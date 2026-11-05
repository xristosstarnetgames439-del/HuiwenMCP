/* 文档助手私有数据存储；写盘失败保留旧文件，读取失败的会话不会被空内容覆盖。 */
#include "AgentStore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <cmath>

namespace
{
QString sessionApiKey; // 首版只在进程内保留；后端阶段再接平台安全存储。

QString dataRoot()
{
    const auto base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base.isEmpty() ? QString() : QDir(base).filePath(QStringLiteral("agent"));
}

QString conversationPath(const QString &id)
{
    // 文件名只能是本模块生成的 UUID，不接受历史 JSON 中的任意相对路径。
    if (id.isEmpty() || QUuid(id).toString(QUuid::WithoutBraces) != id || dataRoot().isEmpty())
        return {};
    return QDir(dataRoot()).filePath(QStringLiteral("conversations/%1.json").arg(id));
}
} // namespace

AgentModelConfig AgentStore::modelConfig()
{
    if (dataRoot().isEmpty())
        return {{}, sessionApiKey, {}};
    QSettings settings(QDir(dataRoot()).filePath("model.ini"), QSettings::IniFormat);
    return {settings.value("baseUrl").toString(), sessionApiKey,
            settings.value("model").toString()};
}

bool AgentStore::saveModelConfig(const AgentModelConfig &config, QString &error)
{
    if (dataRoot().isEmpty() || !QDir().mkpath(dataRoot()))
    {
        error = QStringLiteral("无法创建助手配置目录");
        return false;
    }
    QSettings settings(QDir(dataRoot()).filePath("model.ini"), QSettings::IniFormat);
    settings.setValue("baseUrl", config.baseUrl);
    settings.setValue("model", config.model);
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        error = QStringLiteral("模型配置保存失败");
        return false;
    }
    sessionApiKey = config.apiKey;
    return true;
}

bool AgentStore::loadConversation(const QString &id, AgentConversation &conversation,
                                  QString &error)
{
    const auto path = conversationPath(id);
    QFile file(path);
    if (path.isEmpty() || QFileInfo(path).isSymLink() || !file.open(QIODevice::ReadOnly) ||
        file.size() > 8 * 1024 * 1024)
    {
        error = QStringLiteral("无法读取会话记录，原文件已保留");
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const auto object = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
        object.value("id").toString() != id || !object.value("messages").isArray() ||
        !object.value("title").isString() || !object.value("workspacePath").isString() ||
        !object.value("documentName").isString() || !object.value("updatedAt").isDouble() ||
        !std::isfinite(object.value("updatedAt").toDouble()) ||
        object.value("updatedAt").toDouble() < 0 ||
        object.value("updatedAt").toDouble() > 9007199254740991.0)
    {
        error = QStringLiteral("会话记录格式错误，原文件已保留");
        return false;
    }
    AgentConversation loaded{id,
                             object.value("title").toString(),
                             object.value("workspacePath").toString(),
                             object.value("documentName").toString(),
                             static_cast<qint64>(object.value("updatedAt").toDouble()),
                             {}};
    for (const auto &value : object.value("messages").toArray())
    {
        const auto item = value.toObject();
        const auto role = item.value("role").toString();
        if (!item.value("text").isString() ||
            (role != "user" && role != "assistant" && role != "notice"))
        {
            error = QStringLiteral("会话消息格式错误，原文件已保留");
            return false;
        }
        loaded.messages.append({role, item.value("text").toString()});
    }
    conversation = loaded;
    return true;
}

QVector<AgentConversation> AgentStore::conversations(QString &error)
{
    QVector<AgentConversation> result;
    if (dataRoot().isEmpty())
        return result;
    // ponytail: 首版按 JSON 文件扫描会话；记录量大后再加元数据索引。
    const QDir directory(QDir(dataRoot()).filePath("conversations"));
    for (const auto &name : directory.entryList({"*.json"}, QDir::Files, QDir::Time))
    {
        AgentConversation conversation;
        QString failure;
        if (loadConversation(name.left(name.size() - 5), conversation, failure))
            result.append(conversation);
        else
            error = QStringLiteral("部分会话无法读取，原记录已保留");
    }
    std::sort(result.begin(), result.end(),
              [](const AgentConversation &left, const AgentConversation &right) {
                  return left.updatedAt > right.updatedAt;
              });
    return result;
}

bool AgentStore::saveConversation(const AgentConversation &conversation, QString &error)
{
    const auto path = conversationPath(conversation.id);
    if (path.isEmpty() || !QDir().mkpath(QDir(dataRoot()).filePath("conversations")))
    {
        error = QStringLiteral("无法创建助手会话目录");
        return false;
    }
    QJsonArray messages;
    for (const auto &message : conversation.messages)
        messages.append(QJsonObject{{"role", message.role}, {"text", message.text}});
    const QJsonObject object{{"id", conversation.id},
                             {"title", conversation.title},
                             {"workspacePath", conversation.workspacePath},
                             {"documentName", conversation.documentName},
                             {"updatedAt", static_cast<double>(conversation.updatedAt)},
                             {"messages", messages}};
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    QSaveFile file(path);
    if (bytes.size() > 8 * 1024 * 1024 || !file.open(QIODevice::WriteOnly) ||
        file.write(bytes) != bytes.size() || !file.commit())
    {
        error = QStringLiteral("会话保存失败，原记录已保留");
        return false;
    }
    return true;
}
