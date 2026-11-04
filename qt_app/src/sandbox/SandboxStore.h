/* 通用沙箱目录规则：三类归档、最近任务与使用时间，不依赖鸿蒙 UI。 */
#pragma once
#include "DocumentItem.h"
#include "SandboxFile.h"
#include <QVector>

class SandboxStore
{
  public:
    static DocumentItem createDocument(const QString &filesDir, const QString &name,
                                       const QString &kind, const QString &mode);
    static void discardDocument(const DocumentItem &document);
    // 按最近使用时间分页；offset 默认为 0，兼容首页原有的最近三条查询。
    static QVector<DocumentItem> recentDocuments(const QString &filesDir,
                                                 const QString &mode = "all", int limit = 20,
                                                 int offset = 0);
    // 一次读取真实输入输出文件快照，按工作区最近使用时间排序，不更新时间。
    static QVector<SandboxFile> listFiles(const QString &filesDir);
    // 重新校验 PDF/MD 预览或 PDF→MD 任务，不创建目录或更新时间。
    static DocumentItem loadTask(const QString &filesDir, const QString &taskPath);
    // 路径可以是任务目录或其中的文件；读取列表本身不更新时间。
    static void markUsed(const QString &filesDir, const QString &path);
    static void markUsed(const DocumentItem &document);
    static QString validateOutput(const QString &filesDir, const QString &path);
    // 接受 preview 任务根目录或 edit/convert 的 input/output，不要求已有输出文件。
    static QString validateTaskDirectory(const QString &filesDir, const QString &path);
    // 设置维护：复制并校验三类工作区；不覆盖已有任务、不修改源目录。
    static QString copyWorkspaces(const QString &sourceRoot, const QString &targetRoot);
    // inputs 仅删 input；duplicates 同模式同名保留最近一份；workspaces 用于迁移后清理源副本。
    static int cleanWorkspaces(const QString &filesDir, const QString &mode,
                               const QString &copiedRoot = {});
};
