/* 文档助手本地记录：会话独立存为 JSON，模型地址和名称持久化，API Key 不落盘。 */
#pragma once
#include "AgentTypes.h"

class AgentStore
{
  public:
    static AgentModelConfig modelConfig();
    static bool saveModelConfig(const AgentModelConfig &config, QString &error);
    static QVector<AgentConversation> conversations(QString &error);
    static bool loadConversation(const QString &id, AgentConversation &conversation,
                                 QString &error);
    static bool saveConversation(const AgentConversation &conversation, QString &error);
};
