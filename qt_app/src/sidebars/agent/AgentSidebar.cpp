/* 助手容器随工作区创建销毁；历史会话独立关联任务，第一阶段只保存消息，不生成模拟回复。 */
#include "AgentSidebar.h"
#include "AgentStore.h"
#include "components/AgentComposer.h"
#include "components/AgentMessageView.h"
#include "components/AgentStyle.h"
#include "components/AgentToolbar.h"
#include "dialogs/AgentHistoryPopup.h"
#include "dialogs/AgentSettingsDialog.h"
#include <QDateTime>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QShortcut>
#include <QUuid>
#include <QVBoxLayout>
#include <QWindow>

AgentSidebar::AgentSidebar(QWidget *parent)
    : QWidget(parent), messages_(new AgentMessageView(this)), composer_(new AgentComposer(this))
{
#ifdef Q_OS_OPENHARMONY
    // 与 PDF／CEF 分开承载，限制在侧栏自身矩形内绘制，避免原生预览盖住聊天控件。
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);
#endif
    setObjectName("agentSidebar");
    setMinimumWidth(500);
    setFocusPolicy(Qt::StrongFocus);
    applyAgentStyle(this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto *toolbar = new AgentToolbar(this);
    layout->addWidget(toolbar);
    layout->addWidget(messages_, 1);
    auto *composerLayout = new QHBoxLayout;
    composerLayout->setContentsMargins(0, 0, 5, 0); // 左侧沿用分隔条留白，仅给右侧补 5px。
    composerLayout->addWidget(composer_);
    layout->addLayout(composerLayout);
    connect(toolbar, &AgentToolbar::historyRequested, this, &AgentSidebar::openHistory);
    connect(toolbar, &AgentToolbar::searchRequested, messages_, &AgentMessageView::showSearch);
    connect(toolbar, &AgentToolbar::newConversationRequested, this, [this] { newConversation(); });
    connect(toolbar, &AgentToolbar::settingsRequested, this, &AgentSidebar::openSettings);
    connect(composer_, &AgentComposer::settingsRequested, this, &AgentSidebar::openSettings);
    connect(composer_, &AgentComposer::submitted, this, &AgentSidebar::submit);
    auto *search = new QShortcut(QKeySequence::Find, this);
    search->setContext(Qt::WidgetWithChildrenShortcut); // 文档区获得焦点时不抢占其 Ctrl+F。
    connect(search, &QShortcut::activated, messages_, &AgentMessageView::showSearch);
    composer_->setModel(AgentStore::modelConfig().model);
    newConversation();
}

void AgentSidebar::setDocument(const DocumentItem &document)
{
    if (document_.taskPath == document.taskPath && document_.name == document.name)
        return;
    document_ = document;
    newConversation(false);
}

QSize AgentSidebar::sizeHint() const
{
    return QSize(minimumWidth(), 480);
}

void AgentSidebar::newConversation(bool clearDraft)
{
    conversation_ = {QUuid::createUuid().toString(QUuid::WithoutBraces),
                     QStringLiteral("新对话"),
                     document_.taskPath,
                     document_.name,
                     QDateTime::currentMSecsSinceEpoch(),
                     {}};
    messages_->setMessages({});
    composer_->setWorkspace(conversation_.workspacePath.isEmpty()
                                ? document_.name
                                : QFileInfo(conversation_.workspacePath).fileName());
    composer_->setStatus({});
    if (clearDraft)
        composer_->clearInput();
}

void AgentSidebar::submit(const QString &text)
{
    auto next = conversation_;
    next.messages.append({QStringLiteral("user"), text});
    if (next.messages.size() == 1)
        next.title = text.simplified().left(40);
    next.updatedAt = QDateTime::currentMSecsSinceEpoch();
    QString error;
    if (!AgentStore::saveConversation(next, error))
    {
        composer_->setStatus(error);
        return; // 保存失败保留输入草稿与原对话。
    }
    conversation_ = next;
    messages_->setMessages(conversation_.messages);
    composer_->clearInput();
    composer_->setStatus({});
    emit sendRequested(conversation_); // TODO：第二阶段连接 AgentClient，不在界面中模拟模型回复。
}

void AgentSidebar::openHistory()
{
    QString error;
    const auto conversations = AgentStore::conversations(error);
    AgentHistoryPopup dialog(conversations, error, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    AgentConversation selected;
    if (!AgentStore::loadConversation(dialog.selectedConversation(), selected, error))
    {
        composer_->setStatus(error);
        return;
    }
    selected.updatedAt = QDateTime::currentMSecsSinceEpoch();
    const bool saved = AgentStore::saveConversation(selected, error);
    conversation_ = selected;
    messages_->setMessages(conversation_.messages);
    composer_->setWorkspace(conversation_.workspacePath.isEmpty()
                                ? conversation_.documentName
                                : QFileInfo(conversation_.workspacePath).fileName());
    composer_->clearInput();
    composer_->setStatus(saved ? QString() : error);
}

void AgentSidebar::openSettings()
{
    AgentSettingsDialog dialog(AgentStore::modelConfig(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QString error;
    const auto config = dialog.modelConfig();
    if (!AgentStore::saveModelConfig(config, error))
    {
        composer_->setStatus(error);
        return;
    }
    composer_->setModel(config.model);
    composer_->setStatus({});
}

void AgentSidebar::raiseAbovePreview()
{
    raise();
#ifdef Q_OS_OPENHARMONY
    if (isVisible() && windowHandle())
        windowHandle()->raise();
#endif
}
