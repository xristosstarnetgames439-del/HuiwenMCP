/* 通用沙箱实现：以目录关联文件，用目录修改时间记录使用时间，不维护额外索引文件。 */
#include "SandboxStore.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace
{
void fail(const QString &message)
{
    throw std::runtime_error(message.toStdString());
}

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
    std::sort(result.begin(), result.end(), [](const DocumentItem &a, const DocumentItem &b) {
        return a.lastUsedAt == b.lastUsedAt ? a.taskPath > b.taskPath : a.lastUsedAt > b.lastUsedAt;
    });
    return result;
}
// 设置操作在 NAPI 工作线程运行，使用标准库文件流，避免在 Qt 启动前创建 QFile 对象。
std::filesystem::path fsPath(const QString &path)
{
    return std::filesystem::u8path(path.toUtf8().constData());
}

QByteArray fileDigest(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        fail(QStringLiteral("无法读取迁移文件"));
    QCryptographicHash hash(QCryptographicHash::Sha256);
    char buffer[64 * 1024];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0)
        hash.addData(buffer, static_cast<int>(input.gcount()));
    if (!input.eof())
        fail(QStringLiteral("读取迁移文件失败"));
    return hash.result();
}

void copyTree(const std::filesystem::path &source, const std::filesystem::path &target)
{
    const auto status = std::filesystem::symlink_status(source);
    if (std::filesystem::is_directory(status))
    {
        if (!std::filesystem::create_directory(target))
            fail(QStringLiteral("迁移目标已存在，未覆盖文件"));
        for (const auto &entry : std::filesystem::directory_iterator(source))
            copyTree(entry.path(), target / entry.path().filename());
    }
    else if (std::filesystem::is_regular_file(status))
    {
        if (!std::filesystem::copy_file(source, target) || fileDigest(source) != fileDigest(target))
            fail(QStringLiteral("迁移文件校验失败，原文件已保留"));
    }
    else
        fail(QStringLiteral("工作区包含链接或特殊文件，取消迁移"));
    std::filesystem::last_write_time(target, std::filesystem::last_write_time(source));
}

bool sameTree(const std::filesystem::path &source, const std::filesystem::path &target)
{
    const auto from = std::filesystem::symlink_status(source);
    const auto to = std::filesystem::symlink_status(target);
    if (std::filesystem::is_regular_file(from) && std::filesystem::is_regular_file(to))
        return fileDigest(source) == fileDigest(target);
    if (!std::filesystem::is_directory(from) || !std::filesystem::is_directory(to))
        return false;
    size_t count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(source))
    {
        ++count;
        if (!sameTree(entry.path(), target / entry.path().filename()))
            return false;
    }
    return count == static_cast<size_t>(std::distance(std::filesystem::directory_iterator(target),
                                                      std::filesystem::directory_iterator{}));
}

QStringList taskDirectories(const QString &root)
{
    QStringList result;
    for (const QString &mode : {QString("preview"), QString("edit"), QString("convert")})
    {
        const QString category = root + "/" + mode;
        if (!QFileInfo::exists(category))
            continue;
        if (QFileInfo(category).isSymLink() || QFileInfo(category).canonicalFilePath() != category)
            fail(QStringLiteral("工作区分类目录无效"));
        for (const auto &task :
             QDir(category).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks))
        {
            if (validTaskName(task.fileName()))
                result.push_back(checkedTask(root, task.absoluteFilePath()));
        }
    }
    return result;
}
} // namespace

QString SandboxStore::copyWorkspaces(const QString &sourceRoot, const QString &targetRoot)
{
    const QString source = rootPath(sourceRoot);
    const QString target = rootPath(targetRoot, true);
    if (source == target)
        return target;
    if (source.startsWith(target + "/") || target.startsWith(source + "/"))
        fail(QStringLiteral("新旧存储目录不能互相包含"));
    const auto tasks = taskDirectories(source);
    // 设置只迁移三类业务目录；已有内容时拒绝合并，避免覆盖另一份工作区。
    for (const QString &mode : {QString("preview"), QString("edit"), QString("convert")})
    {
        const QString directory = ensureChild(target, mode);
        if (!QDir(directory).isEmpty())
            fail(QStringLiteral("目标已有工作区，请选择空目录或先整理目标文件"));
    }
    QStringList created;
    try
    {
        for (const QString &task : tasks)
        {
            const QString destination = target + "/" + QDir(source).relativeFilePath(task);
            // 原子占位成功后才加入回收列表；并发出现的已有目录也不能被误删。
            if (!std::filesystem::create_directory(fsPath(destination)))
                fail(QStringLiteral("迁移目标已存在，未覆盖文件"));
            created.push_back(destination);
            for (const auto &entry : std::filesystem::directory_iterator(fsPath(task)))
                copyTree(entry.path(), fsPath(destination) / entry.path().filename());
            std::filesystem::last_write_time(fsPath(destination),
                                             std::filesystem::last_write_time(fsPath(task)));
        }
    }
    catch (...)
    {
        for (const auto &path : created)
        {
            std::error_code ignored;
            std::filesystem::remove_all(fsPath(path), ignored);
        }
        throw;
    }
    return target;
}

int SandboxStore::cleanWorkspaces(const QString &filesDir, const QString &mode,
                                  const QString &copiedRoot)
{
    const QString root = rootPath(filesDir);
    QStringList removals;
    if (mode == "duplicates")
    {
        QSet<QString> retained;
        // scan 已按最近使用时间排序；同名不等于内容相同，删除由设置页二次确认。
        for (const auto &document : scan(root, "all"))
        {
            const QString key = document.mode + "\n" + document.name;
            if (retained.contains(key))
                removals.push_back(document.taskPath);
            else
                retained.insert(key);
        }
    }
    else if (mode == "inputs" || mode == "workspaces")
    {
        const QString copied = mode == "workspaces" ? rootPath(copiedRoot) : QString();
        if (mode == "workspaces" && copied == root)
            fail(QStringLiteral("不能将源工作区作为迁移副本清理"));
        for (const auto &task : taskDirectories(root))
        {
            const QString path = mode == "inputs" ? task + "/input" : task;
            const QFileInfo info(path);
            if (!info.exists())
                continue;
            if (info.isSymLink() || info.canonicalFilePath() != path)
                fail(QStringLiteral("清理路径无效，未跟随符号链接"));
            if (mode == "workspaces" &&
                !sameTree(fsPath(task), fsPath(copied + "/" + QDir(root).relativeFilePath(task))))
                fail(QStringLiteral("源文件已变化或副本不完整，旧目录已保留"));
            removals.push_back(path);
        }
    }
    else
        fail(QStringLiteral("不支持的清理类型"));
    int count = 0;
    for (const auto &path : removals)
    {
        std::error_code error;
        std::filesystem::remove_all(fsPath(path), error);
        if (error)
            fail(QStringLiteral("部分缓存未清理，请检查目录权限：") + path);
        ++count;
    }
    return count;
}

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
                                                    int limit, int offset)
{
    if (limit < 1 || limit > 100 || offset < 0)
        fail(QStringLiteral("最近任务数量必须在 1 到 100 之间，分页偏移不能为负数"));
    // 仍扫描元数据确定全局时间顺序；只向调用方返回当前页，不更新任务时间。
    const auto documents = scan(filesDir, mode);
    return offset >= documents.size() ? QVector<DocumentItem>{} : documents.mid(offset, limit);
}

QVector<SandboxFile> SandboxStore::listFiles(const QString &filesDir)
{
    const QString root = rootPath(filesDir);
    QVector<SandboxFile> result;
    for (const QString &mode : {QString("preview"), QString("edit"), QString("convert")})
    {
        const QString category = root + "/" + mode;
        if (QFileInfo(category).isSymLink() || QFileInfo(category).canonicalFilePath() != category)
            continue;
        const auto tasks = QDir(category).entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
        for (const QFileInfo &task : tasks)
        {
            if (!validTaskName(task.fileName()) ||
                task.canonicalFilePath() != task.absoluteFilePath())
                continue;
            QString sourceKind;
            for (const QString &role : {QString("input"), QString("output")})
            {
                if (mode == "preview" && role == "output")
                    continue;
                const QString directory = mode == "preview" ? task.absoluteFilePath()
                                                            : task.absoluteFilePath() + "/" + role;
                const QFileInfo directoryInfo(directory);
                if (directoryInfo.isSymLink() || directoryInfo.canonicalFilePath() != directory)
                    continue;
                const auto files =
                    QDir(directory).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
                for (const QFileInfo &file : files)
                {
                    const QString kind = kindOf(file.fileName());
                    if (kind.isEmpty() || !file.isReadable() ||
                        file.canonicalFilePath() != file.absoluteFilePath())
                        continue;
                    if (role == "input" && sourceKind.isEmpty())
                        sourceKind = kind;
                    SandboxFile item;
                    item.name = file.fileName();
                    item.path = file.absoluteFilePath();
                    item.role = role;
                    item.kind = kind;
                    item.taskPath = task.absoluteFilePath();
                    item.workspaceName = task.fileName();
                    item.mode = mode;
                    item.sourceKind = sourceKind;
                    item.lastUsedAt = task.lastModified().toMSecsSinceEpoch();
                    item.sizeBytes = file.size();
                    result.push_back(item);
                }
            }
        }
    }
    // 时间相同时用实际路径稳定排序，避免同次输入输出在刷新时交换位置。
    std::sort(result.begin(), result.end(), [](const SandboxFile &a, const SandboxFile &b) {
        return a.lastUsedAt == b.lastUsedAt ? a.path < b.path : a.lastUsedAt > b.lastUsedAt;
    });
    return result;
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
    if (task != taskPath || (mode != "preview" && mode != "convert" && mode != "edit"))
        fail(QStringLiteral("当前仅支持 PDF/MD/Word 预览、MD 编辑和 PDF 转 MD 任务"));
    // 编辑 input 保留原件，output 是持续编辑版本；恢复校验不创建目录。
    const QString inputDir =
        mode == "preview" ? task : validateTaskDirectory(root, task + "/input");
    DocumentItem document;
    const auto inputs = QDir(inputDir).entryInfoList(QDir::Files | QDir::NoSymLinks, QDir::Name);
    for (const QFileInfo &input : inputs)
    {
        const QString kind = kindOf(input.fileName());
        const bool supportedPreview = mode == "preview" && (kind == "md" || kind == "word");
        const bool supportedEdit = mode == "edit" && kind == "md";
        if ((mode == "edit" && !supportedEdit) || (kind != "pdf" && !supportedPreview && !supportedEdit))
            continue;
        // 恢复前校验运行在 NAPI 工作线程；QFile 是 QObject，会提前占用 Qt 主线程标识。
        const QByteArray path = input.absoluteFilePath().toUtf8();
        std::ifstream file(std::filesystem::u8path(path.constData()), std::ios::binary);
        char firstByte = 0;
        if (!document.inputPath.isEmpty() || !input.isReadable() || (!supportedEdit && input.size() <= 0) ||
            input.canonicalFilePath() != input.absoluteFilePath() || (!supportedEdit && !file.get(firstByte)))
            fail(QStringLiteral("任务输入不唯一、已失效或无法读取"));
        document.kind = kind;
        document.name = input.fileName();
        document.inputPath = input.absoluteFilePath();
    }
    if (document.inputPath.isEmpty())
        fail(QStringLiteral("原任务的文档已被删除或无法读取"));
    document.openPath = document.inputPath;
    if (mode == "convert" || mode == "edit")
    {
        const QString outputDir = validateTaskDirectory(root, task + "/output");
        document.outputPath = outputDir + "/" +
            (mode == "edit" ? document.name : QFileInfo(document.name).completeBaseName() + ".md");
        const QFileInfo output(document.outputPath);
        if (output.isSymLink() ||
            (output.exists() && (!output.isFile() || !output.isReadable() ||
                                 output.canonicalFilePath() != document.outputPath)))
            fail(QStringLiteral("原任务的 Markdown 路径已失效"));
        if (mode == "edit" && output.exists())
        {
            const QByteArray path = document.outputPath.toUtf8();
            std::ifstream version(std::filesystem::u8path(path.constData()), std::ios::binary);
            if (!version.is_open())
                fail(QStringLiteral("已编辑的 Markdown 无法读取，未退回 input 原件"));
            // An empty output is a valid saved edit, not a missing result.
            document.openPath = document.outputPath;
        }
    }
    document.taskPath = task;
    document.mode = mode;
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
