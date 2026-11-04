/* 鸿蒙沙箱 NAPI：异步导入、最近任务／真实文件查询与恢复，提供文件及目录路径校验。 */
#include "harmony_sandbox.h"
#ifdef Q_OS_OPENHARMONY
#include "sandbox/SandboxStore.h"
#include <climits>
#include <fcntl.h>
#include <memory>
#include <napi/native_api.h>
#include <stdexcept>
#include <unistd.h>

namespace
{
enum class Operation
{
    Import,
    Recent,
    Files,
    LoadTask,
    CopyWorkspaces,
    CleanWorkspaces
};

struct SandboxWork
{
    napi_async_work work = nullptr;
    napi_deferred deferred = nullptr;
    Operation operation = Operation::Import;
    QString root;
    QString name;
    QString kind;
    QString mode;
    QString copiedRoot;
    int fd = -1;
    int limit = 20;
    int offset = 0;
    int cleaned = 0;
    HarmonySandboxDocument document;
    QVector<DocumentItem> documents;
    QVector<SandboxFile> files;
    std::string error;

    ~SandboxWork()
    {
        if (fd >= 0)
            close(fd);
    }
};

bool readString(napi_env env, napi_value value, QString &result)
{
    napi_valuetype type;
    size_t length = 0;
    if (napi_typeof(env, value, &type) != napi_ok || type != napi_string ||
        napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok || length == 0 ||
        length > 4096)
        return false;
    QByteArray bytes(static_cast<int>(length + 1), '\0');
    size_t copied = 0;
    if (napi_get_value_string_utf8(env, value, bytes.data(), bytes.size(), &copied) != napi_ok ||
        copied != length || bytes.indexOf('\0') != static_cast<int>(length))
        return false;
    result = QString::fromUtf8(bytes.constData(), static_cast<int>(length));
    return true;
}

napi_value stringValue(napi_env env, const QString &text)
{
    const auto bytes = text.toUtf8();
    napi_value value = nullptr;
    napi_create_string_utf8(env, bytes.constData(), bytes.size(), &value);
    return value;
}

napi_value documentValue(napi_env env, const DocumentItem &document)
{
    napi_value result, timestamp;
    napi_create_object(env, &result);
    napi_set_named_property(env, result, "name", stringValue(env, document.name));
    napi_set_named_property(env, result, "kind", stringValue(env, document.kind));
    napi_set_named_property(env, result, "mode", stringValue(env, document.mode));
    napi_set_named_property(env, result, "taskPath", stringValue(env, document.taskPath));
    napi_set_named_property(env, result, "inputPath", stringValue(env, document.inputPath));
    napi_set_named_property(env, result, "outputPath", stringValue(env, document.outputPath));
    napi_set_named_property(env, result, "openPath", stringValue(env, document.openPath));
    napi_create_double(env, static_cast<double>(document.lastUsedAt), &timestamp);
    napi_set_named_property(env, result, "lastUsedAt", timestamp);
    return result;
}

napi_value fileValue(napi_env env, const SandboxFile &file)
{
    napi_value result, timestamp, size;
    napi_create_object(env, &result);
    napi_set_named_property(env, result, "name", stringValue(env, file.name));
    napi_set_named_property(env, result, "path", stringValue(env, file.path));
    napi_set_named_property(env, result, "role", stringValue(env, file.role));
    napi_set_named_property(env, result, "kind", stringValue(env, file.kind));
    napi_set_named_property(env, result, "taskPath", stringValue(env, file.taskPath));
    napi_set_named_property(env, result, "workspaceName", stringValue(env, file.workspaceName));
    napi_set_named_property(env, result, "mode", stringValue(env, file.mode));
    napi_set_named_property(env, result, "sourceKind", stringValue(env, file.sourceKind));
    napi_create_double(env, static_cast<double>(file.lastUsedAt), &timestamp);
    napi_set_named_property(env, result, "lastUsedAt", timestamp);
    napi_create_double(env, static_cast<double>(file.sizeBytes), &size);
    napi_set_named_property(env, result, "sizeBytes", size);
    return result;
}

void reject(napi_env env, napi_deferred deferred, const std::string &message)
{
    napi_value error;
    napi_create_error(env, stringValue(env, QStringLiteral("HARMONY_SANDBOX_FAILED")),
                      stringValue(env, QString::fromStdString(message)), &error);
    napi_reject_deferred(env, deferred, error);
}

void execute(napi_env, void *data)
{
    auto *task = static_cast<SandboxWork *>(data);
    try
    {
        if (task->operation == Operation::Import)
            task->document = HarmonySandbox::importDocument(task->root, task->name, task->fd,
                                                            task->kind, task->mode);
        else if (task->operation == Operation::Recent)
            task->documents =
                SandboxStore::recentDocuments(task->root, task->mode, task->limit, task->offset);
        else if (task->operation == Operation::Files)
            task->files = SandboxStore::listFiles(task->root);
        else if (task->operation == Operation::CopyWorkspaces)
            task->name = SandboxStore::copyWorkspaces(task->root, task->name);
        else if (task->operation == Operation::CleanWorkspaces)
            task->cleaned = SandboxStore::cleanWorkspaces(task->root, task->mode, task->copiedRoot);
        else
            task->document = SandboxStore::loadTask(task->root, task->name);
    }
    catch (const std::exception &error)
    {
        task->error = error.what();
    }
}

void complete(napi_env env, napi_status status, void *data)
{
    std::unique_ptr<SandboxWork> task(static_cast<SandboxWork *>(data));
    if (status == napi_ok && task->error.empty())
    {
        napi_value result;
        if (task->operation == Operation::Import || task->operation == Operation::LoadTask)
            result = documentValue(env, task->document);
        else if (task->operation == Operation::CopyWorkspaces)
            result = stringValue(env, task->name);
        else if (task->operation == Operation::CleanWorkspaces)
            napi_create_int32(env, task->cleaned, &result);
        else if (task->operation == Operation::Files)
        {
            napi_create_array_with_length(env, task->files.size(), &result);
            for (int index = 0; index < task->files.size(); ++index)
                napi_set_element(env, result, index, fileValue(env, task->files[index]));
        }
        else
        {
            napi_create_array_with_length(env, task->documents.size(), &result);
            for (int index = 0; index < task->documents.size(); ++index)
                napi_set_element(env, result, index, documentValue(env, task->documents[index]));
        }
        napi_resolve_deferred(env, task->deferred, result);
    }
    else
        reject(env, task->deferred, task->error.empty() ? "沙箱任务未完成" : task->error);
    napi_delete_async_work(env, task->work);
}

napi_value queue(napi_env env, std::unique_ptr<SandboxWork> task)
{
    napi_value promise;
    if (napi_create_promise(env, &task->deferred, &promise) != napi_ok)
    {
        napi_throw_error(env, nullptr, "无法创建沙箱任务");
        return nullptr;
    }
    const auto resource = stringValue(env, QStringLiteral("HarmonySandbox"));
    napi_status status =
        napi_create_async_work(env, nullptr, resource, execute, complete, task.get(), &task->work);
    if (status == napi_ok)
        status = napi_queue_async_work(env, task->work);
    if (status != napi_ok)
    {
        reject(env, task->deferred, "无法启动沙箱任务，请重试");
        if (task->work != nullptr)
            napi_delete_async_work(env, task->work);
    }
    else
        task.release();
    return promise;
}

napi_value importDocument(napi_env env, napi_callback_info info)
{
    size_t count = 5;
    napi_value values[5];
    auto task = std::make_unique<SandboxWork>();
    double fd = -1;
    if (napi_get_cb_info(env, info, &count, values, nullptr, nullptr) != napi_ok || count != 5 ||
        !readString(env, values[0], task->root) || !readString(env, values[1], task->name) ||
        napi_get_value_double(env, values[2], &fd) != napi_ok || !(fd >= 0 && fd <= INT_MAX) ||
        fd != static_cast<int>(fd) || !readString(env, values[3], task->kind) ||
        !readString(env, values[4], task->mode))
    {
        napi_throw_type_error(env, "HARMONY_SANDBOX_INVALID_ARGUMENT",
                              "需要沙箱目录、文件名、有效 fd、文档格式及操作类型");
        return nullptr;
    }
    task->fd = fcntl(static_cast<int>(fd), F_DUPFD_CLOEXEC, 0);
    if (task->fd < 0)
    {
        napi_throw_error(env, "HARMONY_SANDBOX_INVALID_FD", "所选文件已无法读取，请重新选择");
        return nullptr;
    }
    return queue(env, std::move(task));
}

napi_value recentDocuments(napi_env env, napi_callback_info info)
{
    size_t count = 4;
    napi_value values[4];
    auto task = std::make_unique<SandboxWork>();
    double limit = 0;
    double offset = 0;
    if (napi_get_cb_info(env, info, &count, values, nullptr, nullptr) != napi_ok ||
        (count != 3 && count != 4) || !readString(env, values[0], task->root) ||
        !readString(env, values[1], task->mode) ||
        napi_get_value_double(env, values[2], &limit) != napi_ok || !(limit >= 1 && limit <= 100) ||
        limit != static_cast<int>(limit) ||
        (count == 4 && (napi_get_value_double(env, values[3], &offset) != napi_ok ||
                        !(offset >= 0 && offset <= INT_MAX) || offset != static_cast<int>(offset))))
    {
        napi_throw_type_error(env, "HARMONY_SANDBOX_INVALID_ARGUMENT",
                              "查询需要沙箱目录、类型，数量为 1 到 100，分页偏移为非负整数");
        return nullptr;
    }
    task->operation = Operation::Recent;
    task->limit = static_cast<int>(limit);
    task->offset = static_cast<int>(offset);
    return queue(env, std::move(task));
}

// 文件管理只读快照：后台扫描一次，不复用最近任务分页，也不触发恢复或转换。
napi_value listFiles(napi_env env, napi_callback_info info)
{
    size_t count = 1;
    napi_value values[1];
    auto task = std::make_unique<SandboxWork>();
    if (napi_get_cb_info(env, info, &count, values, nullptr, nullptr) != napi_ok || count != 1 ||
        !readString(env, values[0], task->root))
    {
        napi_throw_type_error(env, "HARMONY_SANDBOX_INVALID_ARGUMENT", "文件查询需要应用沙箱目录");
        return nullptr;
    }
    task->operation = Operation::Files;
    return queue(env, std::move(task));
}

napi_value loadTask(napi_env env, napi_callback_info info)
{
    size_t count = 2;
    napi_value values[2];
    auto task = std::make_unique<SandboxWork>();
    if (napi_get_cb_info(env, info, &count, values, nullptr, nullptr) != napi_ok || count != 2 ||
        !readString(env, values[0], task->root) || !readString(env, values[1], task->name))
    {
        napi_throw_type_error(env, "HARMONY_SANDBOX_INVALID_ARGUMENT",
                              "恢复任务需要沙箱根目录和任务目录");
        return nullptr;
    }
    task->operation = Operation::LoadTask;
    return queue(env, std::move(task));
}

// 设置维护与文件导入复用异步队列，不在 ArkTS UI 线程复制或删除文件。
napi_value storageMaintenance(napi_env env, napi_callback_info info, Operation operation)
{
    size_t count = 3;
    napi_value values[3];
    auto task = std::make_unique<SandboxWork>();
    if (napi_get_cb_info(env, info, &count, values, nullptr, nullptr) != napi_ok ||
        (count != 2 && count != 3) || !readString(env, values[0], task->root) ||
        !readString(env, values[1], task->name) ||
        (count == 3 && !readString(env, values[2], task->copiedRoot)))
    {
        napi_throw_type_error(env, "HARMONY_SANDBOX_INVALID_ARGUMENT",
                              "需要工作区根目录和目标目录或清理类型");
        return nullptr;
    }
    task->operation = operation;
    task->mode = task->name;
    return queue(env, std::move(task));
}

napi_value copyWorkspaces(napi_env env, napi_callback_info info)
{
    return storageMaintenance(env, info, Operation::CopyWorkspaces);
}

napi_value cleanWorkspaces(napi_env env, napi_callback_info info)
{
    return storageMaintenance(env, info, Operation::CleanWorkspaces);
}

enum class PathOperation
{
    ValidateOutput,
    ValidateTaskDirectory
};

napi_value pathOperation(napi_env env, napi_callback_info info, PathOperation operation)
{
    size_t count = 2;
    napi_value values[2];
    QString root, path;
    if (napi_get_cb_info(env, info, &count, values, nullptr, nullptr) != napi_ok || count != 2 ||
        !readString(env, values[0], root) || !readString(env, values[1], path))
    {
        napi_throw_type_error(env, "HARMONY_SANDBOX_INVALID_ARGUMENT",
                              "需要沙箱目录和文件或目录路径");
        return nullptr;
    }
    try
    {
        if (operation == PathOperation::ValidateOutput)
            return stringValue(env, HarmonySandbox::validateOutput(root, path));
        return stringValue(env, SandboxStore::validateTaskDirectory(root, path));
    }
    catch (const std::exception &error)
    {
        napi_throw_error(env, "HARMONY_SANDBOX_PATH_INVALID", error.what());
        return nullptr;
    }
}

napi_value validateOutput(napi_env env, napi_callback_info info)
{
    return pathOperation(env, info, PathOperation::ValidateOutput);
}

napi_value validateTaskDirectory(napi_env env, napi_callback_info info)
{
    return pathOperation(env, info, PathOperation::ValidateTaskDirectory);
}

} // namespace

void InitHarmonySandbox(napi_env env, napi_value exports)
{
    napi_property_descriptor methods[] = {
        {"harmonyImportDocument", nullptr, importDocument, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"harmonyValidateOutput", nullptr, validateOutput, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"harmonyValidateTaskDirectory", nullptr, validateTaskDirectory, nullptr, nullptr, nullptr,
         napi_default, nullptr},
        {"harmonyRecentDocuments", nullptr, recentDocuments, nullptr, nullptr, nullptr,
         napi_default, nullptr},
        {"harmonyLoadTask", nullptr, loadTask, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"harmonyListFiles", nullptr, listFiles, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"harmonyCopyWorkspaces", nullptr, copyWorkspaces, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"harmonyCleanWorkspaces", nullptr, cleanWorkspaces, nullptr, nullptr, nullptr,
         napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(methods) / sizeof(methods[0]), methods);
}
#endif
