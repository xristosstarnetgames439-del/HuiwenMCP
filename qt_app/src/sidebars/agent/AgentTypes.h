/* 文档助手的界面数据契约；不依赖模型 SDK，密钥仅在当前进程内使用。 */
#pragma once
#include <QString>
#include <QVector>

struct AgentModelConfig
{
    QString baseUrl;
    QString apiKey;
    QString model;
};

struct AgentMessage
{
    QString role;
    QString text;
};

struct AgentConversation
{
    QString id;
    QString title;
    QString workspacePath;
    QString documentName;
    qint64 updatedAt = 0;
    QVector<AgentMessage> messages;
};
