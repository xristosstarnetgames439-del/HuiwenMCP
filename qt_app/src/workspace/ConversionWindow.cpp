/* 转换工作区：左右独立区域、可拖动分隔线，以及跟随分隔线的悬浮转化按钮。 */
#include "ConversionWindow.h"
#include "components/ConvertButton.h"
#include "components/functionBar/FileMenu.h"
#include "components/functionBar/FunctionBar.h"
#include "components/splitter/WorkspaceSplitter.h"
#include "resultView/MarkdownResultView.h"
#include "sandbox/SandboxService.h"
#include "services/ConversionService.h"
#include "sourceView/SourceDocumentView.h"
#include <QCloseEvent>
#ifdef Q_OS_OPENHARMONY
#include "platform/harmony/qt_bridge.h"
#endif
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QSplitterHandle>
#include <QTimer>
#include <QVBoxLayout>

ConversionWindow::ConversionWindow(QWidget *parent)
    : QWidget(parent), splitter_(new WorkspaceSplitter(this)), source_(new SourceDocumentView),
      result_(new MarkdownResultView), button_(new ConvertButton(this)),
      functionBar_(new FunctionBar(this)), conversion_(new ConversionService(this)),
      sandbox_(new SandboxService(this)), status_(new QLabel(this))
{
    setWindowTitle(QStringLiteral("汇文MCP · 转换工作区"));
    setMinimumSize(640, 480);
    setAutoFillBackground(true);
    QPalette colors = palette();
    colors.setColor(QPalette::Window, QColor("#F5F7FB"));
    // 滚动区域默认使用 Base，和窗口统一为同一种背景色。
    colors.setColor(QPalette::Base, colors.color(QPalette::Window));
    setPalette(colors);
    setObjectName(QStringLiteral("conversionWindow"));
    setAttribute(Qt::WA_StyledBackground);
    // 显式指定所有可见背景，避免平台样式为滚动内容另行填充默认颜色。
    setStyleSheet(QStringLiteral("QWidget#conversionWindow, "
                                 "QWidget#sourceDocumentBackground, QWidget#resultDocumentBackground, "
                                 "QWidget#sourceDocumentViewport, QWidget#resultDocumentViewport "
                                 "{ background-color: %1; }")
                      .arg(colors.color(QPalette::Window).name()));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(functionBar_);
    splitter_->addWidget(source_);
    splitter_->addWidget(result_);
    splitter_->setSizes({600, 600});
    splitter_->installEventFilter(this);
    // 剩余高度全部分配给文档区域，避免空状态栏占据半个窗口。
    layout->addWidget(splitter_, 1);
    status_->setContentsMargins(16, 4, 16, 4);
    status_->setMinimumHeight(28);
    status_->setWordWrap(true);
    status_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(status_, 0);
    connect(splitter_, &QSplitter::splitterMoved, this, [this] { placeConvertButton(); });
    connect(source_, &SourceDocumentView::previewReadyChanged, this, [this] { refreshScrollSync(); });
    connect(result_, &MarkdownResultView::previewReadyChanged, this,
            [this]
            {
                refreshScrollSync();
                placeConvertButton();
            });
    connect(functionBar_, &FunctionBar::scrollSyncChanged, this, &ConversionWindow::refreshScrollSync);
    connect(source_, &SourceDocumentView::scrollPositionChanged, this,
            [this](int page, double fraction, double progress)
            {
                if (scrollSyncEnabled())
                    result_->setScrollPosition(page, fraction, progress);
            });
    connect(result_, &MarkdownResultView::scrollPositionChanged, this,
            [this](int page, double fraction, double progress)
            {
                if (scrollSyncEnabled())
                    source_->setScrollPosition(page, fraction, progress);
            });
    connect(source_, &SourceDocumentView::selectionRequested, this, &ConversionWindow::selectDocument);
    connect(functionBar_->fileMenu(), &FileMenu::replacementRequested, this,
            &ConversionWindow::selectDocument);
    connect(functionBar_->fileMenu(), &FileMenu::previewModeRequested, this,
            [this](bool preview)
            {
                source_->setPreviewMode(preview);
                result_->setPreviewMode(preview);
                refreshScrollSync();
                placeConvertButton();
            });
    connect(functionBar_->fileMenu(), &FileMenu::directoryRequested, this,
            [this](bool output)
            {
                sandbox_->openTaskDirectory(document_, output);
            });
    connect(sandbox_, &SandboxService::documentSelected, this,
            [this](const DocumentItem &document)
            {
                document_ = document;
                retry_ = false;
                status_->clear();
                source_->setDocuments({document});
                result_->setResultFile({});
            });
    connect(sandbox_, &SandboxService::restorationRequested, this, &ConversionWindow::restoreDocument);
    connect(sandbox_, &SandboxService::selectionFinished, this,
            [this]
            {
                picking_ = false;
                refreshBusyState();
            });
    connect(sandbox_, &SandboxService::failed, status_, &QLabel::setText);
    connect(source_, &SourceDocumentView::documentOpened, this,
            [this]
            {
                sandbox_->markUsed(document_);
            });
    connect(result_, &MarkdownResultView::locationRequested, sandbox_, &SandboxService::openResultLocation);
    // 按钮独立接入转换服务，区域之间不互相调用。
    connect(button_, &QPushButton::clicked, this,
            [this]
            {
                converting_ = true;
                refreshBusyState();
                result_->setResultFile({});
                status_->setText(QStringLiteral("正在转化 PDF…"));
                sandbox_->markUsed(document_);
                conversion_->convertPdfToMd(document_.inputPath, document_.outputPath);
            });
    connect(conversion_, &ConversionService::succeeded, this,
            [this](const QString &path, const QString &message)
            {
                converting_ = false;
                retry_ = false;
                sandbox_->markUsed(document_);
                result_->setResultFile(path);
                functionBar_->fileMenu()->setPreviewMode(true);
                status_->setText(QStringLiteral("[code=0] %1 路径：%2").arg(message, path));
                refreshBusyState();
            });
    connect(conversion_, &ConversionService::failed, this,
            [this](int code, const QString &message)
            {
                converting_ = false;
                retry_ = true;
                sandbox_->markUsed(document_);
                status_->setText(QStringLiteral("[code=%1] %2").arg(code).arg(message));
                refreshBusyState();
            });
    functionBar_->fileMenu()->setPreviewMode(true);
}

// 待迁移到窗口基类：QPA 已将系统关闭转为 closeEvent，这里只通过 NAPI 发出返回首页请求。
void ConversionWindow::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_OPENHARMONY
    event->ignore();
    QtBridge::instance().request("close");
#else
    QWidget::closeEvent(event);
#endif
}

void ConversionWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    placeConvertButton();
}

bool ConversionWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == splitter_ && event->type() == QEvent::Resize)
        QTimer::singleShot(0, this, [this] { placeConvertButton(); });
    return QWidget::eventFilter(watched, event);
}

void ConversionWindow::placeConvertButton()
{
    if (button_->isHidden())
        return;
    const QPoint center = splitter_->handle(1)->mapTo(this, splitter_->handle(1)->rect().center());
    button_->move(center.x() - button_->width() / 2,
                  splitter_->mapTo(this, QPoint(0, splitter_->height())).y() - button_->height() - 72);
    button_->raiseAbovePreview();
}

void ConversionWindow::refreshBusyState()
{
    source_->setBusy(picking_ || converting_);
    functionBar_->fileMenu()->setDocumentState(!document_.inputPath.isEmpty(), picking_ || converting_);
    button_->setState(!document_.inputPath.isEmpty(), picking_ || converting_, retry_);
    // 仅用当前结果控制显隐：选取成功会清空结果，取消选取不会改变结果。
    button_->setVisible(!result_->hasResultFile());
    placeConvertButton();
}

void ConversionWindow::selectDocument()
{
    if (picking_ || converting_)
        return;
    picking_ = true;
    refreshBusyState();
    sandbox_->selectPdf();
}

void ConversionWindow::restoreDocument(const DocumentItem &document)
{
    if (picking_ || converting_)
    {
        sandbox_->finishRestoration(document, QStringLiteral("工作区正在选择或转换文件，请稍后重试"));
        return;
    }
    // 在替换当前工作区前再次确认输入可读，避免列表展示后文件被删除。
    QFile input(document.inputPath);
    if (!input.open(QIODevice::ReadOnly) || input.read(1).isEmpty())
    {
        sandbox_->finishRestoration(document, QStringLiteral("原任务的 PDF 已无法读取"));
        return;
    }
    document_ = document;
    retry_ = false;
    status_->clear();
    source_->setDocuments({document});
    const QFileInfo output(document.outputPath);
    result_->setResultFile(output.isFile() && output.size() > 0 ? document.outputPath : QString());
    functionBar_->fileMenu()->setPreviewMode(true);
    refreshBusyState();
    sandbox_->finishRestoration(document);
}

void ConversionWindow::refreshScrollSync()
{
    if (scrollSyncEnabled())
        source_->publishScrollPosition();
}

bool ConversionWindow::scrollSyncEnabled() const
{
    // 单向滚动不向另一侧转发位置；同步滚动仍要求左右预览均已就绪。
    return functionBar_->isScrollSynchronized() && source_->isPreviewReady() && result_->isPreviewReady();
}
