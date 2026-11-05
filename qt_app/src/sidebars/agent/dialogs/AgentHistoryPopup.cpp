/* 最近会话按使用时间排列；整行可点击，列表超出窗口时由 Qt 原生滚动处理。 */
#include "AgentHistoryPopup.h"
#include "../components/AgentStyle.h"
#include <QDateTime>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

AgentHistoryPopup::AgentHistoryPopup(const QVector<AgentConversation> &conversations,
                                     const QString &warning, QWidget *parent)
    : QDialog(parent)
{
    setObjectName("agentDialog");
    setWindowTitle(QStringLiteral("最近对话"));
    applyAgentStyle(this);
    resize(430, 440);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 16);
    auto *list = new QListWidget(this);
    list->setTextElideMode(Qt::ElideMiddle);
    list->setAccessibleName(QStringLiteral("最近对话列表"));
    for (const auto &conversation : conversations)
    {
        const auto time =
            QDateTime::fromMSecsSinceEpoch(conversation.updatedAt).toString("MM-dd HH:mm");
        auto *item = new QListWidgetItem(
            QStringLiteral("%1\n%2 · %3")
                .arg(conversation.title.isEmpty() ? QStringLiteral("新对话") : conversation.title,
                     time,
                     conversation.documentName.isEmpty() ? QStringLiteral("未关联工作区")
                                                         : conversation.documentName),
            list);
        item->setData(Qt::UserRole, conversation.id);
    }
    const auto activate = [this](QListWidgetItem *item) {
        selectedId_ = item->data(Qt::UserRole).toString();
        accept();
    };
    connect(list, &QListWidget::itemClicked, this, activate);
    connect(list, &QListWidget::itemActivated, this, activate);
    layout->addWidget(list, 1);
    if (!warning.isEmpty() || conversations.isEmpty())
    {
        auto *hint = new QLabel(warning.isEmpty() ? QStringLiteral("暂无最近对话") : warning, this);
        hint->setWordWrap(true);
        layout->addWidget(hint);
    }
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QString AgentHistoryPopup::selectedConversation() const
{
    return selectedId_;
}
