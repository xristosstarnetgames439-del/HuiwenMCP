/* 贴底铺满的输入卡片与独立工作区信息条；只有真实错误占用提示行。 */
#include "AgentComposer.h"
#include "AgentIcons.h"
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

AgentComposer::AgentComposer(QWidget *parent)
    : QWidget(parent), workspace_(new QLabel(this)), status_(new QLabel(this)),
      input_(new QPlainTextEdit(this)), model_(new QToolButton(this)), send_(new QToolButton(this))
{
    setObjectName("agentComposer");
    setAttribute(Qt::WA_StyledBackground); // 外层灰底延伸到下方输入卡片的圆角背后。
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    status_->setObjectName("agentComposerStatus");
    status_->setWordWrap(true);
    auto *workspaceBar = new QFrame(this);
    workspaceBar->setObjectName("agentWorkspaceBar");
    auto *context = new QHBoxLayout(workspaceBar);
    context->setContentsMargins(24, 12, 24, 12);
    context->setSpacing(8);
    auto *folder = new QLabel(workspaceBar);
    folder->setPixmap(agentIcon(AgentIcon::Folder).pixmap(20, 20));
    folder->setFixedSize(20, 20);
    context->addWidget(folder);
    workspace_->setObjectName("agentWorkspaceName");
    workspace_->setTextFormat(Qt::PlainText);
    workspace_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    workspace_->setMinimumWidth(0);
    workspace_->installEventFilter(this);
    context->addWidget(workspace_, 1);
    layout->addWidget(workspaceBar);
    auto *card = new QFrame(this);
    card->setObjectName("agentComposerCard");
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(20);
    auto *inputLayout = new QVBoxLayout;
    inputLayout->setContentsMargins(24, 20, 24, 0);
    inputLayout->setSpacing(12);
    input_->setPlaceholderText(QStringLiteral("向文档助手提问…"));
    input_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    input_->setFixedHeight(128);
    input_->setFrameShape(QFrame::NoFrame);
    input_->setTabChangesFocus(true);
    input_->installEventFilter(this);
    inputLayout->addWidget(input_);
    inputLayout->addWidget(status_);
    cardLayout->addLayout(inputLayout);
    auto *footer = new QWidget(card);
    footer->setObjectName("agentComposerActions");
    auto *actions = new QHBoxLayout(footer);
    actions->setContentsMargins(2, 0, 2, 2);
    actions->setSpacing(8);
    auto *attachment = new QToolButton(footer);
    attachment->setIcon(agentIcon(AgentIcon::Plus, QColor("#AAAAAA")));
    attachment->setIconSize(QSize(22, 22));
    attachment->setFixedSize(44, 44);
    attachment->setEnabled(false);
    attachment->setToolTip(
        QStringLiteral("附件功能尚未接入")); // TODO：接入助手独立附件导入，不替换当前文档。
    attachment->setAccessibleName(QStringLiteral("添加附件，尚未接入"));
    actions->addWidget(attachment);
    model_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    model_->setMinimumWidth(0);
    model_->setFixedHeight(44);
    model_->installEventFilter(this);
    model_->setAccessibleName(QStringLiteral("模型设置"));
    connect(model_, &QToolButton::clicked, this, &AgentComposer::settingsRequested);
    actions->addWidget(model_, 1);
    send_->setObjectName("agentSendButton");
    send_->setIcon(agentIcon(AgentIcon::Send));
    send_->setIconSize(QSize(22, 22));
    send_->setFixedSize(44, 44);
    send_->setEnabled(false);
    send_->setAccessibleName(QStringLiteral("发送消息"));
    connect(send_, &QToolButton::clicked, this, &AgentComposer::submit);
    actions->addWidget(send_);
    cardLayout->addWidget(footer);
    layout->addWidget(card);
    connect(input_, &QPlainTextEdit::textChanged, this, [this] {
        send_->setEnabled(!input_->toPlainText().trimmed().isEmpty());
        input_->setFixedHeight(
            std::clamp(input_->blockCount() * input_->fontMetrics().lineSpacing() + 24, 128, 280));
    });
    setStatus({});
    setWorkspace({});
    setModel({});
}

void AgentComposer::setWorkspace(const QString &name)
{
    workspaceName_ = name.isEmpty() ? QStringLiteral("未关联工作区") : name;
    workspace_->setAccessibleName(workspaceName_);
    refreshLabels();
}

void AgentComposer::setModel(const QString &model)
{
    modelName_ = model.isEmpty() ? QStringLiteral("配置模型") : model;
    refreshLabels();
}

void AgentComposer::setStatus(const QString &text)
{
    status_->setText(text);
    status_->setVisible(!text.isEmpty()); // 空提示完全退出布局，不保留空行和额外间距。
}

void AgentComposer::refreshLabels()
{
    workspace_->setText(workspace_->fontMetrics().elidedText(workspaceName_, Qt::ElideMiddle,
                                                             std::max(0, workspace_->width())));
    model_->setText(model_->fontMetrics().elidedText(modelName_, Qt::ElideRight,
                                                     std::max(0, model_->width() - 8)));
}

void AgentComposer::submit()
{
    const auto text = input_->toPlainText().trimmed();
    if (text.isEmpty())
        return;
    emit submitted(text);
}

void AgentComposer::clearInput()
{
    input_->clear();
    input_->setFocus();
}

bool AgentComposer::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == workspace_ || watched == model_) &&
        (event->type() == QEvent::Resize || event->type() == QEvent::FontChange))
        refreshLabels(); // 按控件实际宽度省略文字，放大后仍不挤占发送按钮。
    if (watched == input_ && event->type() == QEvent::InputMethod)
        composing_ = !static_cast<QInputMethodEvent *>(event)->preeditString().isEmpty();
    if (watched == input_ && event->type() == QEvent::KeyPress)
    {
        const auto *key = static_cast<QKeyEvent *>(event);
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) &&
            !(key->modifiers() & Qt::ShiftModifier) && !composing_)
        {
            submit();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
