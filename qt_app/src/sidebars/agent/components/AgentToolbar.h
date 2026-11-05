/* 助手顶部操作条：仅发出历史、搜索、新建和设置请求。 */
#pragma once
#include <QWidget>

class AgentToolbar : public QWidget
{
    Q_OBJECT
  public:
    explicit AgentToolbar(QWidget *parent = nullptr);
  signals:
    void historyRequested();
    void searchRequested();
    void newConversationRequested();
    void settingsRequested();
};
