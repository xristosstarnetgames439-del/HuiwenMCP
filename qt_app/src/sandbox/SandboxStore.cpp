/* 通用沙箱实现：以目录关联文件，用目录修改时间记录使用时间，不维护额外索引文件。 */
#include "SandboxStore.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace
{
void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }

bool validMode(const QString &mode)
{
    return mode == "preview" || mode == "edit" || mode == "convert";
}

QString rootPath(const QString &filesDir, bool create = false)
{
    const QFileInfo info(filesDir);
    if (filesDir.contains(QChar::Null) || !info.isAbsolute() ||
        (create && !QDir().mkpath(filesDir)) || info.canonicalFilePath().isEmpty() || !info.isDir())
        fail(QStringLiteral("应用沙箱目录无效"));
    return info.canonicalFilePath();
}

QString safeName(const QString &name)
{
    if (name.isEmpty() || name == "." || name == ".." || name.contains('/') ||
        name.contains('\\') || name.contains(QChar::Null))
        fail(QStringLiteral("所选文件名无效"));
    QString result = name;
    for (int index = 0; index < result.size(); ++index)
    {
        const QChar character = result.at(index);
        if (character.unicode() < 32 || QStringLiteral(":*?\"<>|").contains(character))
            result[index] = '_';
    }
    while (result.endsWith('.') || result.endsWith(' '))
        result.chop(1);
    if (result.isEmpty() || result.toUtf8().size() > 240)
        fail(QStringLiteral("文件名为空或过长，请更换文件名"));
    return result;
}

QString kindOf(const QString &name)
{
    const QString suffix = QFileInfo(name).suffix().toLower();
    if (suffix == "pdf")
        return "pdf";
    if (suffix == "md" || suffix == "markdown")
        return "md";
    if (suffix == "doc" || suffix == "docx")
        return "word";
    return {};
}

QString ensureChild(const QString &parent, const QString &name)
{
    const QString path = QDir(parent).filePath(name);
    if ((!QDir(parent).mkdir(name) && !QFileInfo(path).isDir()) || QFileInfo(path).isSymLink() ||
        !QFileInfo(path).isDir() || QFileInfo(path).canonicalFilePath() != path)
        fail(QStringLiteral("无法创建安全的沙箱目录：") + path);
    return path;
}

bool validTaskName(const QString &name)
{
    // 首页会在 QApplication 创建前查询任务，避免正则 JIT 使用 Qt 线程局部存储。
    if (name.size() < 17 || name.at(8) != '_' || name.at(15) != '_' || name.at(16) == '\n')
        return false;
    for (int index = 0; index < 15; ++index)
    {
        if (index != 8 && (name.at(index) < QLatin1Char('0') || name.at(index) > QLatin1Char('9')))
            return false;
    }
    return true;
}

QString checkedTask(const QString &root, const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    const QStringList parts = QDir(root).relativeFilePath(canonical).split('/');
    if (!info.isAbsolute() || path.contains(QChar::Null) || path.contains('\\') ||
        QDir::cleanPath(path) != path || canonical.isEmpty() || parts.size() < 2 ||
        !validMode(parts[0]) || !validTaskName(parts[1]))
        fail(QStringLiteral("文件不属于本应用任务目录"));
    const QString category = root + "/" + parts[0];
    const QString task = category + "/" + parts[1];
    if (QFileInfo(category).isSymLink() || QFileInfo(category).canonicalFilePath() != category ||
        QFileInfo(task).isSymLink() || !QFileInfo(task).isDir() ||
        QFileInfo(task).canonicalFilePath() != task)
        fail(QStringLiteral("任务目录已失效"));
    return task;
}

QVector<DocumentItem> scan(const QString &filesDir, const QString &mode)
{
    if (mode != "all" && !validMode(mode))
        fail(QStringLiteral("需要 preview、edit、convert 或 all 类型"));
    if (!QFileInfo::exists(filesDir))
        return {};
    const QString root = rootPath(filesDir);
    QVector<DocumentItem> result;
    for (const QString &category : {QString("preview"), QString("edit"), QString("convert")})
    {
        if (mode != "all" && mode != category)
            continue;
        const QString categoryPath = root + "/" + category;
        if (QFileInfo(categoryPath).canonicalFilePath() != categoryPath ||
            QFileInfo(categoryPath).isSymLink())
            continue;
        const auto tasks =
            QDir(categoryPath).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        for (const QFileInfo &task : tasks)
        {
            if (!validTaskName(task.fileName()))
                continue;
            const QString inputDir = category == "preview" ? task.absoluteFilePath()
                                                           : task.absoluteFilePath() + "/input";
            if (QFileInfo(inputDir).canonicalFilePath() != inputDir)
                continue;
            const auto inputs =
                QDir(inputDir).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
            for (const QFileInfo &input : inputs)
            {
                const QString kind = kindOf(input.fileName());
                if (kind.isEmpty() || !input.isReadable() ||
                    input.canonicalFilePath() != input.absoluteFilePath())
                    continue;
                DocumentItem item{kind, input.fileName(), input.absoluteFilePath(), {}};
                item.taskPath = task.absoluteFilePath();
                item.mode = category;
                item.lastUsedAt = task.lastModified().toMSecsSinceEpoch();
                item.openPath = item.inputPath;
                if (category != "preview")
                {
                    const QString outputDir = item.taskPath + "/output";
                    if (QFileInfo(outputDir).canonicalFilePath() != outputDir)
                        break;
                    item.outputPath =
                        outputDir + "/" +
                        (category == "edit" ? input.fileName() : input.completeBaseName() + ".md");
                    const QFileInfo output(item.outputPath);
                    if (category == "edit" && (output.exists() || output.isSymLink()))
                    {
                        // 已有编辑结果必须可读，避免悄悄退回原稿而丢失用户对版本的判断。
                        if (!output.isFile() || !output.isReadable() || output.isSymLink() ||
                            output.canonicalFilePath() != item.outputPath)
                            break;
                        item.openPath = item.outputPath;
                    }
                }
                result.push_back(item);
                break;
            }
        }
    }
    std::sort(result.begin(), result.end(),
              [](const DocumentItem &a, const DocumentItem &b) {
                  return a.lastUsedAt == b.lastUsedAt ? a.taskPath > b.taskPath
                                                      : a.lastUsedAt > b.lastUsedAt;
              });
    return result;
}
} // namespace

DocumentItem SandboxStore::createDocument(const QString &filesDir, const QString &name,
                                          const QString &kind, const QString &mode)
{
    if (!validMode(mode) || kindOf(name) != kind || kind.isEmpty() ||
        (mode == "convert" && kind != "pdf"))
        fail(QStringLiteral("文档格式或操作类型不支持"));
    const QString fileName = safeName(name);
    const QString root = rootPath(filesDir, true);
    for (const QString &category : {QString("preview"), QString("edit"), QString("convert")})
        ensureChild(root, category);
    QString stem = QFileInfo(fileName).completeBaseName();
    while (stem.toUtf8().size() > 120)
    {
        stem.chop(1);
        if (!stem.isEmpty() && stem.at(stem.size() - 1).isHighSurrogate())
            stem.chop(1);
    }
    if (stem.isEmpty())
        stem = QStringLiteral("document");
    const QString prefix = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + "_" + stem;
    const QString categoryPath = root + "/" + mode;
    QString taskPath;
    // mkdir 为原子占位，同秒同名追加编号，只为当前导入创建新目录。
    for (int index = 1; index <= 10000; ++index)
    {
        const QString leaf = prefix + (index == 1 ? QString() : QStringLiteral(" (%1)").arg(index));
        if (QDir(categoryPath).mkdir(leaf))
        {
            taskPath = categoryPath + "/" + leaf;
            break;
        }
        if (!QFileInfo::exists(categoryPath + "/" + leaf))
            fail(QStringLiteral("无法创建任务目录"));
    }
    if (taskPath.isEmpty())
        fail(QStringLiteral("同名任务过多，请稍后重试"));
    DocumentItem document{kind, fileName, {}, {}};
    document.taskPath = taskPath;
    document.mode = mode;
    try
    {
        const QString inputDir = mode == "preview" ? taskPath : ensureChild(taskPath, "input");
        document.inputPath = inputDir + "/" + fileName;
        if (mode != "preview")
            document.outputPath =
                ensureChild(taskPath, "output") + "/" +
                (mode == "edit" ? fileName : QFileInfo(fileName).completeBaseName() + ".md");
        document.openPath = document.inputPath;
        document.lastUsedAt = QDateTime::currentMSecsSinceEpoch();
    }
    catch (...)
    {
        discardDocument(document);
        throw;
    }
    return document;
}

void SandboxStore::discardDocument(const DocumentItem &document)
{
    if (!document.inputPath.isEmpty())
        QFile::remove(document.inputPath);
    QDir().rmdir(document.taskPath + "/input");
    QDir().rmdir(document.taskPath + "/output");
    QDir().rmdir(document.taskPath);
}

QVector<DocumentItem> SandboxStore::recentDocuments(const QString &filesDir, const QString &mode,
                                                    int limit)
{
    if (limit < 1 || limit > 100)
        fail(QStringLiteral("最近任务数量必须在 1 到 100 之间"));
    auto documents = scan(filesDir, mode);
    if (documents.size() > limit)
        documents.resize(limit);
    return documents;
}

QVector<DocumentItem> SandboxStore::findEditDocuments(const QString &filesDir, const QString &name)
{
    const QString fileName = safeName(name);
    auto documents = scan(filesDir, "edit");
    documents.erase(std::remove_if(documents.begin(), documents.end(),
                                   [&fileName](const DocumentItem &item)
                                   { return QFileInfo(item.inputPath).fileName() != fileName; }),
                    documents.end());
    return documents;
}

void SandboxStore::markUsed(const QString &filesDir, const QString &path)
{
    const QString task = checkedTask(rootPath(filesDir), path);
    const QByteArray bytes = task.toUtf8();
    std::error_code error;
    std::filesystem::last_write_time(std::filesystem::u8path(bytes.constData()),
                                     std::filesystem::file_time_type::clock::now(), error);
    if (error)
        fail(QStringLiteral("无法更新任务使用时间：") + QString::fromStdString(error.message()));
}

DocumentItem SandboxStore::loadTask(const QString &filesDir, const QString &taskPath)
{
    const QString root = rootPath(filesDir);
    const QString task = checkedTask(root, taskPath);
    const QString mode = QDir(root).relativeFilePath(task).section('/', 0, 0);
    if (task != taskPath || (mode != "preview" && mode != "convert"))
        fail(QStringLiteral("当前仅支持 PDF/MD 预览和 PDF 转 MD 任务"));
    // 预览文档直接位于任务目录，转化任务继续使用 input/output；校验不创建目录。
    const QString inputDir =
        mode == "preview" ? task : validateTaskDirectory(root, task + "/input");
    DocumentItem document;
    const auto inputs = QDir(inputDir).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &input : inputs)
    {
        const QString kind = kindOf(input.fileName());
        if (kind != "pdf" && !(mode == "preview" && kind == "md"))
            continue;
        // 恢复前校验运行在 NAPI 工作线程；QFile 是 QObject，会提前占用 Qt 主线程标识。
        const QByteArray path = input.absoluteFilePath().toUtf8();
        std::ifstream file(std::filesystem::u8path(path.constData()), std::ios::binary);
        char firstByte = 0;
        if (!document.inputPath.isEmpty() || !input.isReadable() || input.size() <= 0 ||
            input.canonicalFilePath() != input.absoluteFilePath() || !file.get(firstByte))
            fail(QStringLiteral("任务输入不唯一、已失效或无法读取"));
        document.kind = kind;
        document.name = input.fileName();
        document.inputPath = input.absoluteFilePath();
    }
    if (document.inputPath.isEmpty())
        fail(QStringLiteral("原任务的文档已被删除或无法读取"));
    if (mode == "convert")
    {
        const QString outputDir = validateTaskDirectory(root, task + "/output");
        document.outputPath = outputDir + "/" + QFileInfo(document.name).completeBaseName() + ".md";
        const QFileInfo output(document.outputPath);
        if (output.isSymLink() ||
            (output.exists() && (!output.isFile() || !output.isReadable() ||
                                 output.canonicalFilePath() != document.outputPath)))
            fail(QStringLiteral("原任务的 Markdown 路径已失效"));
    }
    document.taskPath = task;
    document.mode = mode;
    document.openPath = document.inputPath;
    document.lastUsedAt = QFileInfo(task).lastModified().toMSecsSinceEpoch();
    return document;
}

void SandboxStore::markUsed(const DocumentItem &document)
{
    if (!document.taskPath.isEmpty())
        markUsed(QFileInfo(QFileInfo(document.taskPath).absolutePath()).absolutePath(),
                 document.taskPath);
}

QString SandboxStore::validateOutput(const QString &filesDir, const QString &path)
{
    const QString root = rootPath(filesDir);
    const QFileInfo output(path);
    const QString canonical = output.canonicalFilePath();
    const QStringList parts = QDir(root).relativeFilePath(canonical).split('/');
    const bool legacy = canonical.startsWith(root + "/outputs/");
    const bool current = parts.size() == 4 && (parts[0] == "convert" || parts[0] == "edit") &&
                         validTaskName(parts[1]) && parts[2] == "output";
    QFile file(path);
    if (path.contains(QChar::Null) || path.contains('\\') || !output.isAbsolute() ||
        QDir::cleanPath(path) != path || (!legacy && !current) ||
        output.suffix().compare(QStringLiteral("md"), Qt::CaseInsensitive) != 0 ||
        !output.isFile() || output.size() <= 0 || !file.open(QIODevice::ReadOnly) ||
        file.read(1).isEmpty())
        fail(QStringLiteral("Markdown 文件不属于本应用输出目录，或文件不存在、为空、无法读取"));
    if (current)
        checkedTask(root, canonical);
    return output.absoluteFilePath();
}

QString SandboxStore::validateTaskDirectory(const QString &filesDir, const QString &path)
{
    const QString root = rootPath(filesDir);
    const QFileInfo directory(path);
    if (!directory.isAbsolute() || !directory.isDir() || !directory.isReadable() ||
        directory.isSymLink() || directory.canonicalFilePath() != path)
        fail(QStringLiteral("任务目录不存在、不可读取或包含路径别名"));
    const QString task = checkedTask(root, path);
    const QString mode = QFileInfo(QFileInfo(task).absolutePath()).fileName();
    const QString leaf = QDir(task).relativeFilePath(path);
    if (mode == "preview" && path == task)
        return path;
    if ((mode != "edit" && mode != "convert") || (leaf != "input" && leaf != "output"))
        fail(QStringLiteral("仅允许打开预览任务目录或编辑／转化任务的输入输出目录"));
    return path;
}
