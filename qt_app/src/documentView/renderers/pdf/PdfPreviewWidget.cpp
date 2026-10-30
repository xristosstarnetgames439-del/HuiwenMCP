/* PDFium 仅在后台任务中调用；窗口线程只排版和绘制 QImage。 */
#include "PdfPreviewWidget.h"
#include "components/rendering/RasterRenderWindow.h"
#include <fpdfview.h>
#include <QFile>
#include <QHideEvent>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QScopedValueRollback>
#include <QShowEvent>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
// PDFium 要求所有 API 串行调用；锁也覆盖初始化和文档释放。
QMutex pdfiumMutex;
int pdfiumUsers = 0;
constexpr int pageMargin = 24;
constexpr int pageGap = 16;
constexpr int maxRenderPixels = 16 * 1024 * 1024;
} // namespace

struct PdfPreviewDocument
{
    FPDF_DOCUMENT handle = nullptr;
    bool initialized = false;
    ~PdfPreviewDocument()
    {
        if (!initialized)
            return;
        QMutexLocker lock(&pdfiumMutex);
        if (handle)
            FPDF_CloseDocument(handle);
        if (--pdfiumUsers == 0)
            FPDF_DestroyLibrary();
    }
};

namespace
{
PdfPreviewResult loadDocument(const std::shared_ptr<PdfPreviewDocument> &document,
                              const QString &path, quint64 generation)
{
    PdfPreviewResult result;
    result.generation = generation;
    QMutexLocker lock(&pdfiumMutex);
    if (pdfiumUsers++ == 0)
    {
        FPDF_LIBRARY_CONFIG config{};
        config.version = 2;
        FPDF_InitLibraryWithConfig(&config);
    }
    document->initialized = true;
    const QByteArray filename = QFile::encodeName(path);
    document->handle = FPDF_LoadDocument(filename.constData(), nullptr);
    if (!document->handle)
    {
        const unsigned long error = FPDF_GetLastError();
        result.error =
            error == FPDF_ERR_PASSWORD
                ? QStringLiteral("该 PDF 需要密码，暂无法预览，请更换文件")
                : QStringLiteral("无法打开 PDF（错误码 %1），请检查文件或重新选择").arg(error);
        return result;
    }
    const int count = FPDF_GetPageCount(document->handle);
    if (count <= 0 || count > 100000)
    {
        result.error = QStringLiteral("PDF 页数为空或超过预览上限");
        return result;
    }
    result.pageSizes.reserve(count);
    for (int page = 0; page < count; ++page)
    {
        FS_SIZEF size{};
        if (!FPDF_GetPageSizeByIndexF(document->handle, page, &size) ||
            !std::isfinite(size.width) || !std::isfinite(size.height) || size.width <= 0 ||
            size.height <= 0)
        {
            result.error = QStringLiteral("PDF 第 %1 页的尺寸无效").arg(page + 1);
            result.pageSizes.clear();
            return result;
        }
        result.pageSizes.append(QSizeF(size.width, size.height));
    }
    return result;
}

PdfPreviewResult renderPage(const std::shared_ptr<PdfPreviewDocument> &document, int page,
                            QSize size, qreal ratio, quint64 generation)
{
    PdfPreviewResult result;
    result.generation = generation;
    result.page = page;
    QMutexLocker lock(&pdfiumMutex);
    const FPDF_PAGE handle = FPDF_LoadPage(document->handle, page);
    if (!handle)
    {
        result.error = QStringLiteral("无法读取 PDF 第 %1 页").arg(page + 1);
        return result;
    }
    result.image = QImage(size, QImage::Format_ARGB32);
    const FPDF_BITMAP bitmap =
        result.image.isNull()
            ? nullptr
            : FPDFBitmap_CreateEx(size.width(), size.height(), FPDFBitmap_BGRA, result.image.bits(),
                                  result.image.bytesPerLine());
    if (bitmap)
    {
        FPDFBitmap_FillRect(bitmap, 0, 0, size.width(), size.height(), 0xFFFFFFFF);
        FPDF_RenderPageBitmap(bitmap, handle, 0, 0, size.width(), size.height(), 0, FPDF_ANNOT);
        FPDFBitmap_Destroy(bitmap);
        result.image.setDevicePixelRatio(ratio);
    }
    else
    {
        result.image = {};
        result.error = QStringLiteral("PDF 第 %1 页的预览图分配失败").arg(page + 1);
    }
    FPDF_ClosePage(handle);
    return result;
}
} // namespace

PdfPreviewWidget::PdfPreviewWidget(QWidget *parent) : RasterDocumentViewport(parent)
{
    setAccessibleName(QStringLiteral("PDF 正文预览"));
    // 尺寸持续变化时只按约一帧处理一次最新布局，避免每个鼠标事件都遍历所有页。
    layoutTimer_.setSingleShot(true);
    layoutTimer_.setInterval(16);
    layoutTimer_.setTimerType(Qt::PreciseTimer);
    connect(&layoutTimer_, &QTimer::timeout, this,
            [this]
            {
                layoutPages();
                renderWindow_->invalidate();
                publishScrollPosition();
            });
    resizeTimer_.setSingleShot(true);
    resizeTimer_.setInterval(150);
    connect(&resizeTimer_, &QTimer::timeout, this,
            [this]
            {
                if (layoutTimer_.isActive())
                {
                    layoutPages();
                    publishScrollPosition();
                }
                // 停止缩放后重绘一次，恢复平滑采样；高清页面继续由后台补齐。
                renderWindow_->invalidate();
                requestRender();
            });
    scrollReportTimer_.setSingleShot(true);
    scrollReportTimer_.setInterval(16);
    connect(&scrollReportTimer_, &QTimer::timeout, this, &PdfPreviewWidget::publishScrollPosition);
    connect(&watcher_, &QFutureWatcherBase::finished, this,
            [this]
            {
                const auto result = watcher_.result();
                if (result.generation == generation_)
                {
                    if (!result.error.isEmpty())
                    {
                        message_ = result.error;
                        emit previewReadyChanged(false);
                        renderWindow_->invalidate();
                    }
                    else if (result.page < 0)
                    {
                        message_.clear();
                        pageSizes_ = result.pageSizes;
                        layoutPages();
                        renderWindow_->invalidate();
                        emit documentOpened(path_);
                        emit previewReadyChanged(isPreviewReady());
                    }
                    else if (result.image.size() == renderSize(result.page) &&
                             qFuzzyCompare(result.image.devicePixelRatio(),
                                           viewport()->devicePixelRatioF()))
                    {
                        images_.insert(result.page, new QImage(result.image),
                                       static_cast<int>(result.image.sizeInBytes()));
                        renderWindow_->invalidate(pageRects_[result.page]
                                                      .translated(0, -verticalScrollBar()->value())
                                                      .toAlignedRect());
                    }
                }
                taskPending_ = false;
                requestRender();
            });
}

PdfPreviewWidget::~PdfPreviewWidget()
{
    // 子窗口晚于业务成员销毁，先解除绘制和输入回调，避免关闭期间回访本对象。
    renderWindow_->stopRendering();
    // 退出窗口时等待当前任务；任务只持有共享文档，不访问 QWidget。
    watcher_.waitForFinished();
}

void PdfPreviewWidget::setDocument(const QString &path)
{
    if (path_ == path)
    {
        if (!path.isEmpty() && !pageSizes_.isEmpty())
            emit documentOpened(path_);
        return;
    }
    ++generation_;
    QScopedValueRollback<bool> applying(applyingScroll_, true);
    stopScrolling();
    scrollReportTimer_.stop();
    path_ = path;
    layoutError_.clear();
    pageSizes_.clear();
    pageRects_.clear();
    images_.clear();
    document_.reset();
    layoutTimer_.stop();
    resizeTimer_.stop();
    verticalScrollBar()->setRange(0, 0);
    emit previewReadyChanged(false);
    loadPending_ = !path.isEmpty();
    message_ = loadPending_ ? QStringLiteral("正在打开 PDF…") : QString();
    if (loadPending_)
        document_ = std::make_shared<PdfPreviewDocument>();
    renderWindow_->invalidate();
    requestRender();
}

bool PdfPreviewWidget::isPreviewReady() const
{
    return isVisible() && !pageRects_.isEmpty() && message_.isEmpty() && layoutError_.isEmpty();
}

void PdfPreviewWidget::publishScrollPosition()
{
    scrollReportTimer_.stop();
    if (!isPreviewReady() || applyingScroll_ || layoutTimer_.isActive())
        return;
    const int page = qMin(firstVisiblePage(), pageRects_.size() - 1);
    const auto &rect = pageRects_[page];
    const auto *bar = verticalScrollBar();
    const double fraction = qBound(0.0, (bar->value() - rect.top()) / rect.height(), 1.0);
    const double progress = bar->maximum() ? double(bar->value()) / bar->maximum() : 0;
    emit scrollPositionChanged(page + 1, fraction, progress);
}

void PdfPreviewWidget::setScrollPosition(int page, double fraction, double progress)
{
    if (!isPreviewReady() || !std::isfinite(fraction) || !std::isfinite(progress))
        return;
    // 对侧同步定位直接生效，不能被尚未结束的本地滚轮动画覆盖。
    stopScrolling();
    scrollReportTimer_.stop();
    QScopedValueRollback<bool> applying(applyingScroll_, true);
    const int maximum = verticalScrollBar()->maximum();
    double target = qBound(0.0, progress, 1.0) * maximum;
    if (page > 0 && page <= pageRects_.size() && progress > 0 && progress < 1)
        target =
            pageRects_[page - 1].top() + qBound(0.0, fraction, 1.0) * pageRects_[page - 1].height();
    verticalScrollBar()->setValue(qRound(qBound(0.0, target, double(maximum))));
}

int PdfPreviewWidget::firstVisiblePage() const
{
    const qreal top = verticalScrollBar()->value();
    const auto first =
        std::lower_bound(pageRects_.cbegin(), pageRects_.cend(), top,
                         [](const QRectF &rect, qreal value) { return rect.bottom() < value; });
    return static_cast<int>(first - pageRects_.cbegin());
}

void PdfPreviewWidget::layoutPages()
{
    layoutTimer_.stop();
    stopScrolling();
    QScopedValueRollback<bool> applying(applyingScroll_, true);
    layoutError_.clear();
    const int anchor = firstVisiblePage();
    const bool hasAnchor = anchor < pageRects_.size();
    qreal fraction = 0;
    if (hasAnchor)
        fraction =
            (verticalScrollBar()->value() - pageRects_[anchor].top()) / pageRects_[anchor].height();
    const qreal width = std::max(1, viewport()->width() - 2 * pageMargin);
    QVector<QRectF> rectangles;
    rectangles.reserve(pageSizes_.size());
    qreal top = pageMargin;
    for (const QSizeF &size : pageSizes_)
    {
        const qreal height = width * size.height() / size.width();
        if (!std::isfinite(height) ||
            top + height + pageMargin + pageGap > std::numeric_limits<int>::max())
        {
            layoutError_ = QStringLiteral("PDF 页面尺寸超过可显示范围，请更换文件或缩小预览区域");
            pageRects_.clear();
            verticalScrollBar()->setRange(0, 0);
            return;
        }
        rectangles.append(QRectF(pageMargin, top, width, height));
        top += height + pageGap;
    }
    pageRects_ = std::move(rectangles);
    // 预算只随布局变化，避免跨页时因可见页数变化反复使位图缓存失效。
    int maxVisible = 1;
    int end = 0;
    for (int first = 0; first < pageRects_.size(); ++first)
    {
        while (end < pageRects_.size() &&
               pageRects_[end].top() < pageRects_[first].bottom() + viewport()->height())
            ++end;
        maxVisible = std::max(maxVisible, end - first);
    }
    renderPixelBudget_ = maxRenderPixels / maxVisible;
    verticalScrollBar()->setPageStep(viewport()->height());
    const int height =
        pageRects_.isEmpty() ? 0 : static_cast<int>(std::ceil(top - pageGap + pageMargin));
    verticalScrollBar()->setRange(0, std::max(0, height - viewport()->height()));
    if (hasAnchor && anchor < pageRects_.size())
        verticalScrollBar()->setValue(
            static_cast<int>(pageRects_[anchor].top() + fraction * pageRects_[anchor].height()));
}

QSize PdfPreviewWidget::renderSize(int page) const
{
    if (page < 0 || page >= pageRects_.size())
        return {};
    QSizeF size = pageRects_[page].size() * viewport()->devicePixelRatioF();
    const qreal pixelBudget = std::max(1, renderPixelBudget_);
    // ponytail: 可见页共用 64 MiB 位图预算；超长页降采样，需高清长页时改为分块渲染。
    const qreal scale = std::min({1.0, 8192.0 / std::max(size.width(), size.height()),
                                  std::sqrt(pixelBudget / (size.width() * size.height())),
                                  pixelBudget / std::max(size.width(), size.height())});
    size *= scale;
    return QSize(std::max(1, static_cast<int>(size.width())),
                 std::max(1, static_cast<int>(size.height())));
}

void PdfPreviewWidget::requestRender()
{
    // finished 信号可能尚在事件队列中，不能仅凭 isRunning() 复用 watcher。
    if (!isVisible() || taskPending_ || !document_)
        return;
    const auto document = document_;
    const quint64 generation = generation_;
    if (loadPending_)
    {
        loadPending_ = false;
        taskPending_ = true;
        const QString path = path_;
        watcher_.setFuture(QtConcurrent::run([document, path, generation]
                                             { return loadDocument(document, path, generation); }));
        return;
    }
    if (!message_.isEmpty() || !layoutError_.isEmpty() || resizeTimer_.isActive())
        return;
    const qreal bottom = verticalScrollBar()->value() + viewport()->height();
    const int first = firstVisiblePage();
    QVector<int> pages;
    int end = first;
    while (end < pageRects_.size() && pageRects_[end].top() < bottom)
        pages.append(end++);
    // 可见页全部就绪后才预取下一页、上一页；内存不足时放弃预取。
    pages.append(end);
    pages.append(first - 1);
    qint64 nearbyCost = 0;
    for (int page : pages)
    {
        if (page < 0 || page >= pageRects_.size())
            continue;
        const QSize size = renderSize(page);
        const qint64 cost = qint64(size.width()) * size.height() * 4;
        if ((page < first || page >= end) && nearbyCost + cost > images_.maxCost())
            continue;
        nearbyCost += cost;
        const qreal ratio = viewport()->devicePixelRatioF();
        const QImage *image = images_.object(page);
        if (image && image->size() == size && qFuzzyCompare(image->devicePixelRatio(), ratio))
            continue;
        taskPending_ = true;
        watcher_.setFuture(
            QtConcurrent::run([document, page, size, ratio, generation]
                              { return renderPage(document, page, size, ratio, generation); }));
        return;
    }
}

void PdfPreviewWidget::paintDocument(QPainter &painter, const QRegion &region)
{
    for (const QRect &rect : region)
        painter.fillRect(rect, palette().color(QPalette::Window));
    painter.setPen(QColor("#667B98"));
    const QString message = layoutError_.isEmpty() ? message_ : layoutError_;
    if (!message.isEmpty())
    {
        painter.drawText(
            viewport()->rect().adjusted(pageMargin, pageMargin, -pageMargin, -pageMargin),
            Qt::AlignCenter | Qt::TextWordWrap, message);
        return;
    }
    const int offset = verticalScrollBar()->value();
    const qreal bottom = offset + viewport()->height();
    // 拖动时快速绘制缓存位图，避免界面线程持续执行大图平滑缩放。
    painter.setRenderHint(QPainter::SmoothPixmapTransform, !resizeTimer_.isActive());
    for (int page = firstVisiblePage(); page < pageRects_.size() && pageRects_[page].top() < bottom;
         ++page)
    {
        const QRectF rect = pageRects_[page].translated(0, -offset);
        if (!region.intersects(rect.toAlignedRect()))
            continue;
        painter.fillRect(rect, Qt::white);
        if (const QImage *image = images_.object(page))
            painter.drawImage(rect, *image);
        else
            painter.drawText(rect, Qt::AlignCenter,
                             QStringLiteral("正在渲染第 %1 页…").arg(page + 1));
    }
    requestRender();
}

void PdfPreviewWidget::resizeEvent(QResizeEvent *event)
{
    RasterDocumentViewport::resizeEvent(event);
    resizeTimer_.start();
    if (!layoutTimer_.isActive())
        layoutTimer_.start();
}

void PdfPreviewWidget::showEvent(QShowEvent *event)
{
    RasterDocumentViewport::showEvent(event);
    layoutPages();
    requestRender();
    emit previewReadyChanged(isPreviewReady());
}

void PdfPreviewWidget::hideEvent(QHideEvent *event)
{
    layoutTimer_.stop();
    resizeTimer_.stop();
    scrollReportTimer_.stop();
    emit previewReadyChanged(false);
    RasterDocumentViewport::hideEvent(event);
}

void PdfPreviewWidget::scrollContentsBy(int dx, int dy)
{
    if (isPreviewReady())
        renderWindow_->scrollBy(dx, dy);
    else
        renderWindow_->invalidate();
    requestRender();
    if (!applyingScroll_ && !scrollReportTimer_.isActive())
        scrollReportTimer_.start();
}
