/* 欢迎态与消息展示：只渲染会话内容，搜索范围限定为当前对话。 */
#pragma once
#include "../AgentTypes.h"
#include <QWidget>
class QLabel;
class QLineEdit;
class QStackedWidget;
class QTextBrowser;

class AgentMessageView : public QWidget
{
  public:
    explicit AgentMessageView(QWidget *parent = nullptr);
    void setMessages(const QVector<AgentMessage> &messages);
    void showSearch();

  private:
    void findMessage(bool backward, bool fromStart = false);
    QWidget *searchBar_;
    QLineEdit *search_;
    QLabel *searchStatus_;
    QStackedWidget *pages_;
    QTextBrowser *messages_;
};
