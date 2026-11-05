/* 最近对话列表，只选择会话标识；文件读取和会话切换由侧栏协调。 */
#pragma once
#include "../AgentTypes.h"
#include <QDialog>

class AgentHistoryPopup : public QDialog
{
  public:
    explicit AgentHistoryPopup(const QVector<AgentConversation> &conversations,
                               const QString &warning, QWidget *parent = nullptr);
    QString selectedConversation() const;

  private:
    QString selectedId_;
};
