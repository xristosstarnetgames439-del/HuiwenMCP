/* 浅色助手操作条：30px 图标配 50px 按钮，与会话和模型服务解耦。 */
#include "AgentToolbar.h"
#include "AgentIcons.h"
#include <QHBoxLayout>
#include <QToolButton>

AgentToolbar::AgentToolbar(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(21, 18, 21, 12);
    layout->setSpacing(6);
    const auto addButton = [this, layout](AgentIcon icon, const QString &label) {
        auto *button = new QToolButton(this);
        button->setIcon(agentIcon(icon));
        button->setIconSize(QSize(30, 30));
        button->setFixedSize(50, 50);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(label);
        button->setAccessibleName(label);
        layout->addWidget(button);
        return button;
    };
    connect(addButton(AgentIcon::Sidebar, QStringLiteral("最近对话")), &QToolButton::clicked, this,
            &AgentToolbar::historyRequested);
    connect(addButton(AgentIcon::Search, QStringLiteral("搜索当前对话（Ctrl+F）")),
            &QToolButton::clicked, this, &AgentToolbar::searchRequested);
    layout->addStretch();
    connect(addButton(AgentIcon::Plus, QStringLiteral("新建对话")), &QToolButton::clicked, this,
            &AgentToolbar::newConversationRequested);
    connect(addButton(AgentIcon::Settings, QStringLiteral("助手设置")), &QToolButton::clicked, this,
            &AgentToolbar::settingsRequested);
}
