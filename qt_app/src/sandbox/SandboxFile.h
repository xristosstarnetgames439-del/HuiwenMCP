/* 文件管理只读模型：每条记录对应真实输入或输出文件，不代表整个工作区任务。 */
#pragma once
#include <QString>

struct SandboxFile
{
    QString name;
    QString path;
    QString role;
    QString kind;
    QString taskPath;
    QString workspaceName;
    QString mode;
    QString sourceKind;
    qint64 lastUsedAt = 0;
    qint64 sizeBytes = 0;
};
