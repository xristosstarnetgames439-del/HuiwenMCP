/* 预览和转化共用工作区；模式只改变内容栏数、文档类型和菜单，窗口交接保持一致。 */
#include "ConversionWindow.h"
#include "components/ConvertButton.h"
#include "components/functionBar/FileMenu.h"
#include "components/functionBar/FunctionBar.h"
#include "components/splitter/WorkspaceSplitter.h"
#include "documentView/DocumentView.h"
#include "documentView/renderers/markdown/MarkdownPreviewWidget.h"
#include "documentView/renderers/pdf/PdfPreviewWidget.h"
#include "sandbox/SandboxService.h"
#include "services/ConversionService.h"
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

ConversionWindow::ConversionWindow(const QString &mode, const QString &kind, QWidget *parent)
    : QWidget(parent), mode_(mode), kind_(kind), splitter_(new WorkspaceSplitter(this)),
      source_(
          kind == "md"
              ? new DocumentView(new MarkdownPreviewWidget, QStringLiteral("请选择 Markdown"), true)
              : new DocumentView(new PdfPreviewWidget, QStringLiteral("请选择 PDF"), true)),
      functionBar_(new FunctionBar(mode, this)), sandbox_(new SandboxService(this)),
      status_(new QLabel(this))
{
    const bool conversion = mode_ == "convert";
    // 单栏只创建实际需要的一个渲染器；保留现有工作区及分栏容器，不另建预览窗口。
    if (conversion)
    {
        result_ = new DocumentView(new MarkdownPreviewWidget, QStringLiteral("待转化 PDF"), false);
        button_ = new ConvertButton(this);
        conversion_ = new ConversionService(this);
    }
    setWindowTitle(
        conversion ? QStringLiteral("汇文MCP · 转换工作区")
                   : QStringLiteral("汇文MCP · %1 预览").arg(kind_ == "md" ? "Markdown" : "PDF"));
    setMinimumSize(640, 480);
    setAutoFillBackground(true);
    QPalette colors = palette();
    colors.setColor(QPalette::Window, QColor("#F5F7FB"));
    colors.setColor(QPalette::Base, colors.color(QPalette::Window));
    setPalette(colors);
    setObjectName(QStringLiteral("conversionWindow"));
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral("QWidget#conversionWindow, "
                                 "QWidget#documentCardBackground, QWidget#documentCardViewport "
                                 "{ background-color: %1; }")
                      .arg(colors.color(QPalette::Window).name()));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(functionBar_);
    splitter_->addWidget(source_);
    if (result_)
    {
        splitter_->addWidget(result_);
        splitter_->setSizes({600, 600});
    }
    splitter_->installEventFilter(this);
    // 只有一个子控件时 QSplitter 不显示分隔条，文档占满原有内容区。
    layout->addWidget(splitter_, 1);
    status_->setContentsMargins(16, 4, 16, 4);
    status_->setMinimumHeight(28);
    status_->setWordWrap(true);
    status_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(status_, 0);
    connect(splitter_, &QSplitter::splitterMoved, this, [this] { placeConvertButton(); });
    const auto refreshPreview = [this]
    {
        splitter_->raiseAbovePreview();
        refreshScrollSync();
        placeConvertButton();
    };
    connect(source_, &DocumentView::previewReadyChanged, this, refreshPreview);
    connect(source_, &DocumentView::cardActivated, this, &ConversionWindow::selectDocument);
    connect(source_, &DocumentView::documentOpened, this,
            [this] { sandbox_->markUsed(document_); });
    connect(functionBar_, &FunctionBar::scrollSyncChanged, this,
            &ConversionWindow::refreshScrollSync);
    connect(functionBar_->fileMenu(), &FileMenu::replacementRequested, this,
            &ConversionWindow::selectDocument);
    connect(functionBar_->fileMenu(), &FileMenu::previewModeRequested, this,
            [this](bool preview)
            {
                source_->setPreviewMode(preview);
                if (result_)
                    result_->setPreviewMode(preview);
                refreshScrollSync();
                placeConvertButton();
            });
    connect(functionBar_->fileMenu(), &FileMenu::directoryRequested, this,
            [this](bool output) { sandbox_->openTaskDirectory(document_, output); });
    connect(sandbox_, &SandboxService::documentSelected, this, &ConversionWindow::showDocument);
    connect(sandbox_, &SandboxService::restorationRequested, this,
            &ConversionWindow::restoreDocument);
    connect(sandbox_, &SandboxService::selectionFinished, this,
            [this]
            {
                picking_ = false;
                refreshBusyState();
            });
    connect(sandbox_, &SandboxService::failed, status_, &QLabel::setText);
    if (result_)
    {
        connect(result_, &DocumentView::previewReadyChanged, this, refreshPreview);
        connect(source_, &DocumentView::scrollPositionChanged, this,
                [this](int page, double fraction, double progress)
                {
                    if (scrollSyncEnabled())
                        result_->setScrollPosition(page, fraction, progress);
                });
        connect(result_, &DocumentView::scrollPositionChanged, this,
                [this](int page, double fraction, double progress)
                {
                    if (scrollSyncEnabled())
                        source_->setScrollPosition(page, fraction, progress);
                });
        connect(result_, &DocumentView::cardActivated, sandbox_,
                &SandboxService::openResultLocation);
        // 按钮独立接入转换服务，区域之间不互相调用。
        connect(button_, &QPushButton::clicked, this,
                [this]
                {
                    converting_ = true;
                    refreshBusyState();
                    result_->setDocument({}, QStringLiteral("md"));
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
                    result_->setDocument(path, QStringLiteral("md"));
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
    }
    functionBar_->fileMenu()->setPreviewMode(true);
    refreshBusyState();
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
    if (!button_ || button_->isHidden())
        return;
    const QPoint center = splitter_->handle(1)->mapTo(this, splitter_->handle(1)->rect().center());
    button_->move(center.x() - button_->width() / 2,
                  splitter_->mapTo(this, QPoint(0, splitter_->height())).y() - button_->height() -
                      72);
    button_->raiseAbovePreview();
}

void ConversionWindow::refreshBusyState()
{
    source_->setBusy(picking_ || converting_);
    functionBar_->fileMenu()->setDocumentState(!document_.inputPath.isEmpty(),
                                               picking_ || converting_);
    if (button_)
    {
        button_->setState(!document_.inputPath.isEmpty(), picking_ || converting_, retry_);
        // 单栏不创建按钮；双栏仅用结果文件控制显隐，取消选取不改变结果。
        button_->setVisible(!result_->hasDocument());
    }
    placeConvertButton();
}

void ConversionWindow::selectDocument()
{
    if (picking_ || converting_)
        return;
    picking_ = true;
    refreshBusyState();
    sandbox_->selectDocument(kind_, mode_);
}

void ConversionWindow::showDocument(const DocumentItem &document)
{
    document_ = document;
    retry_ = false;
    status_->clear();
    source_->setDocument(document.inputPath, document.kind, document.name);
    if (result_)
    {
        const QFileInfo output(document.outputPath);
        result_->setDocument(output.isFile() && output.size() > 0 ? document.outputPath : QString(),
                             QStringLiteral("md"));
    }
}

void ConversionWindow::restoreDocument(const DocumentItem &document)
{
    if (picking_ || converting_)
    {
        sandbox_->finishRestoration(document,
                                    QStringLiteral("工作区正在选择或转换文件，请稍后重试"));
        return;
    }
    if (document.mode != mode_ || document.kind != kind_)
    {
        sandbox_->finishRestoration(document, QStringLiteral("任务类型与当前工作区不一致"));
        return;
    }
    // 在替换当前工作区前再次确认输入可读，避免列表展示后文件被删除。
    QFile input(document.inputPath);
    if (!input.open(QIODevice::ReadOnly) || input.read(1).isEmpty())
    {
        sandbox_->finishRestoration(document, QStringLiteral("原任务的文档已无法读取"));
        return;
    }
    showDocument(document);
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
    return result_ && functionBar_->isScrollSynchronized() && source_->isPreviewReady() &&
           result_->isPreviewReady();
}
