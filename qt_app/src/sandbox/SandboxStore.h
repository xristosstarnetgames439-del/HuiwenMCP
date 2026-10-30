/* 通用沙箱目录规则：三类归档、最近使用时间和编辑缓存查询，不依赖鸿蒙 UI。 */
#pragma once
#include "DocumentItem.h"
#include <QVector>

class SandboxStore
{
  public:
    static DocumentItem createDocument(const QString &filesDir, const QString &name,
                                       const QString &kind, const QString &mode);
    static void discardDocument(const DocumentItem &document);
    static QVector<DocumentItem> recentDocuments(const QString &filesDir,
                                                 const QString &mode = "all", int limit = 20);
    static QVector<DocumentItem> findEditDocuments(const QString &filesDir, const QString &name);
    // 重新校验 PDF/MD 预览或 PDF→MD 任务，不创建目录或更新时间。
    static DocumentItem loadTask(const QString &filesDir, const QString &taskPath);
    // 路径可以是任务目录或其中的文件；读取列表本身不更新时间。
    static void markUsed(const QString &filesDir, const QString &path);
    static void markUsed(const DocumentItem &document);
    static QString validateOutput(const QString &filesDir, const QString &path);
    // 接受 preview 任务根目录或 edit/convert 的 input/output，不要求已有输出文件。
    static QString validateTaskDirectory(const QString &filesDir, const QString &path);
};
