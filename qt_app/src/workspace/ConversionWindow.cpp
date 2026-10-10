/* 转换工作区：左右独立区域、可拖动分隔线，以及跟随分隔线的悬浮转化按钮。 */
#include "ConversionWindow.h"
#include "components/ConvertButton.h"
#include "resultView/MarkdownResultView.h"
#include "sandbox/SandboxService.h"
#include "services/ConversionService.h"
#include "sourceView/SourceDocumentView.h"
#include <QLabel>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QSplitter>
#include <QSplitterHandle>
#include <QTimer>
#include <QVBoxLayout>

ConversionWindow::ConversionWindow(QWidget *parent)
    : QWidget(parent), splitter_(new QSplitter(Qt::Horizontal, this)), source_(new SourceDocumentView),
      result_(new MarkdownResultView), button_(new ConvertButton(this)),
      conversion_(new ConversionService(this)), sandbox_(new SandboxService(this)), status_(new QLabel(this))
{
    setWindowTitle(QStringLiteral("汇文 MCP · 转换工作区"));
    setMinimumSize(640, 480);
    setAutoFillBackground(true);
    QPalette colors = palette();
    colors.setColor(QPalette::Window, QColor("#F5F7FB"));
    setPalette(colors);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    splitter_->addWidget(source_);
    splitter_->addWidget(result_);
    splitter_->setChildrenCollapsible(false);
    splitter_->setHandleWidth(7);
    splitter_->setSizes({600, 600});
    splitter_->handle(1)->setCursor(Qt::SplitHCursor);
    splitter_->setStyleSheet("QSplitter::handle:horizontal { background: #D4DEEC; margin: 0 3px; }");
    splitter_->installEventFilter(this);
    // 剩余高度全部分配给文档区域，避免空状态栏占据半个窗口。
    layout->addWidget(splitter_, 1);
    status_->setContentsMargins(16, 4, 16, 4);
    status_->setMinimumHeight(28);
    status_->setWordWrap(true);
    status_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(status_, 0);
    connect(splitter_, &QSplitter::splitterMoved, this, [this] { placeConvertButton(); });
    connect(source_, &SourceDocumentView::selectionRequested, this,
            [this]
            {
                picking_ = true;
                refreshBusyState();
                status_->clear();
                sandbox_->selectPdf();
            });
    connect(sandbox_, &SandboxService::documentSelected, this,
            [this](const DocumentItem &document)
            {
                document_ = document;
                source_->setDocuments({document});
                result_->setResultFile({});
            });
    connect(sandbox_, &SandboxService::selectionFinished, this,
            [this]
            {
                picking_ = false;
                refreshBusyState();
            });
    connect(sandbox_, &SandboxService::failed, status_, &QLabel::setText);
    connect(result_, &MarkdownResultView::locationRequested, sandbox_, &SandboxService::openResultLocation);
    // 按钮独立接入转换服务，区域之间不互相调用。
    connect(button_, &QPushButton::clicked, this,
            [this]
            {
                converting_ = true;
                refreshBusyState();
                result_->setResultFile({});
                status_->setText(QStringLiteral("正在转化 PDF…"));
                conversion_->convertPdfToMd(document_.inputPath, document_.outputPath);
            });
    connect(conversion_, &ConversionService::succeeded, this,
            [this](const QString &path)
            {
                converting_ = false;
                result_->setResultFile(path);
                status_->setText(QStringLiteral("MD 文件已生成：") + path);
                refreshBusyState();
            });
    connect(conversion_, &ConversionService::failed, this,
            [this](const QString &message)
            {
                converting_ = false;
                status_->setText(QStringLiteral("转化失败：") + message);
                refreshBusyState();
            });
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
    const QPoint center = splitter_->handle(1)->mapTo(this, splitter_->handle(1)->rect().center());
    button_->move(center.x() - button_->width() / 2,
                  splitter_->mapTo(this, QPoint(0, splitter_->height())).y() - button_->height() - 32);
    button_->raise();
}

void ConversionWindow::refreshBusyState()
{
    source_->setBusy(picking_ || converting_);
    button_->setState(!document_.inputPath.isEmpty(), picking_ || converting_);
}
