/* 文档区域只处理展示状态；选择文件、打开目录、转换等行为由工作区接收信号后执行。 */
#include "DocumentView.h"
#include "components/FileCard.h"
#include <QFileInfo>
#include <QScrollArea>
#include <QVBoxLayout>

DocumentView::DocumentView(QWidget *rendererWidget, DocumentRenderer *renderer,
                           const QString &emptyPrompt, bool selectableWhenEmpty, QWidget *parent)
    : QStackedWidget(parent), card_(new FileCard), rendererWidget_(rendererWidget),
      renderer_(renderer), emptyPrompt_(emptyPrompt), selectableWhenEmpty_(selectableWhenEmpty)
{
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setBackgroundRole(QPalette::Window);
    scroll->viewport()->setBackgroundRole(QPalette::Window);
    scroll->viewport()->setObjectName(QStringLiteral("documentCardViewport"));
    scroll->viewport()->setAutoFillBackground(true);
    auto *content = new QWidget;
    content->setObjectName(QStringLiteral("documentCardBackground"));
    content->setBackgroundRole(QPalette::Window);
    content->setAutoFillBackground(true);
    auto *cards = new QVBoxLayout(content);
    cards->setContentsMargins(32, 32, 32, 112);
    cards->addStretch();
    cards->addWidget(card_, 0, Qt::AlignHCenter);
    cards->addStretch();
    scroll->setWidget(content);
    addWidget(scroll);
    addWidget(rendererWidget_);
    setMinimumWidth(260);
    card_->setPrompt(emptyPrompt_, selectableWhenEmpty_);
    connect(card_, &QPushButton::clicked, this, [this] { emit cardActivated(path_); });
}

void DocumentView::setDocument(const QString &path, const QString &kind, const QString &name)
{
    // 先切回卡片，再清理旧渲染任务／CEF；新的文件只在预览模式下打开。
    setCurrentIndex(0);
    renderer_->setDocument({});
    path_ = path;
    if (path_.isEmpty())
        card_->setPrompt(emptyPrompt_, selectableWhenEmpty_);
    else
        card_->setFile(kind, name.isEmpty() ? QFileInfo(path_).fileName() : name);
    setBusy(busy_);
    setPreviewMode(previewMode_);
}

void DocumentView::setBusy(bool busy)
{
    busy_ = busy;
    card_->setEnabled(!busy && (selectableWhenEmpty_ || hasDocument()));
}

void DocumentView::setPreviewMode(bool preview)
{
    previewMode_ = preview;
    if (!preview || path_.isEmpty())
        setCurrentIndex(0);
    else if (currentWidget() != rendererWidget_)
    {
        renderer_->setDocument(path_);
        setCurrentWidget(rendererWidget_);
    }
}

bool DocumentView::isPreviewReady() const
{
    return currentWidget() == rendererWidget_ && renderer_->isPreviewReady();
}

void DocumentView::setScrollPosition(int page, double fraction, double progress)
{
    if (isPreviewReady())
        renderer_->setScrollPosition(page, fraction, progress);
}

void DocumentView::publishScrollPosition()
{
    if (isPreviewReady())
        renderer_->publishScrollPosition();
}

void DocumentView::setViewOptions(const DocumentViewOptions &options)
{
    renderer_->setViewOptions(options);
}
