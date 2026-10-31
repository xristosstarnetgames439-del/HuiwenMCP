/* 鸿蒙沙箱 NAPI：异步导入、查询和任务恢复，提供路径校验、编辑缓存及使用时间更新。 */
#include "harmony_sandbox.h"
#ifdef Q_OS_OPENHARMONY
#include "sandbox/SandboxStore.h"
#include <napi/native_api.h>
#include <climits>
#include <fcntl.h>
#include <memory>
#include <stdexcept>
#include <unistd.h>

namespace
{
enum class Operation
{
    Import,
    Recent,
    FindEdits,
    LoadTask
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
    int fd = -1;
    int limit = 20;
    int offset = 0;
    HarmonySandboxDocument document;
    QVector<DocumentItem> documents;
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
        napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok || length == 0 || length > 4096)
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
            task->document =
                HarmonySandbox::importDocument(task->root, task->name, task->fd, task->kind, task->mode);
        else if (task->operation == Operation::Recent)
            task->documents =
                SandboxStore::recentDocuments(task->root, task->mode, task->limit, task->offset);
        else if (task->operation == Operation::FindEdits)
            task->documents = SandboxStore::findEditDocuments(task->root, task->name);
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

napi_value queryDocuments(napi_env env, napi_callback_info info, bool findEdits)
{
    size_t count = 4;
    napi_value values[4];
    auto task = std::make_unique<SandboxWork>();
    double limit = 0;
    double offset = 0;
    if (napi_get_cb_info(env, info, &count, values, nullptr, nullptr) != napi_ok ||
        (findEdits ? count != 2 : (count != 3 && count != 4)) ||
        !readString(env, values[0], task->root) ||
        !readString(env, values[1], findEdits ? task->name : task->mode) ||
        (!findEdits && (napi_get_value_double(env, values[2], &limit) != napi_ok ||
                        !(limit >= 1 && limit <= 100) || limit != static_cast<int>(limit))) ||
        (!findEdits && count == 4 &&
         (napi_get_value_double(env, values[3], &offset) != napi_ok ||
          !(offset >= 0 && offset <= INT_MAX) || offset != static_cast<int>(offset))))
    {
        napi_throw_type_error(
            env, "HARMONY_SANDBOX_INVALID_ARGUMENT",
            "查询需要沙箱目录、类型或文件名，数量为 1 到 100，分页偏移为非负整数");
        return nullptr;
    }
    task->operation = findEdits ? Operation::FindEdits : Operation::Recent;
    task->limit = static_cast<int>(limit);
    task->offset = static_cast<int>(offset);
    return queue(env, std::move(task));
}

napi_value recentDocuments(napi_env env, napi_callback_info info)
{
    return queryDocuments(env, info, false);
}

napi_value loadTask(napi_env env, napi_callback_info info)
{
    size_t count = 2;
    napi_value values[2];
    auto task = std::make_unique<SandboxWork>();
    if (napi_get_cb_info(env, info, &count, values, nullptr, nullptr) != napi_ok || count != 2 ||
        !readString(env, values[0], task->root) || !readString(env, values[1], task->name))
    {
        napi_throw_type_error(env, "HARMONY_SANDBOX_INVALID_ARGUMENT", "恢复任务需要沙箱根目录和任务目录");
        return nullptr;
    }
    task->operation = Operation::LoadTask;
    return queue(env, std::move(task));
}

napi_value findEditCaches(napi_env env, napi_callback_info info)
{
    return queryDocuments(env, info, true);
}

enum class PathOperation
{
    ValidateOutput,
    ValidateTaskDirectory,
    MarkUsed
};

napi_value pathOperation(napi_env env, napi_callback_info info, PathOperation operation)
{
    size_t count = 2;
    napi_value values[2];
    QString root, path;
    if (napi_get_cb_info(env, info, &count, values, nullptr, nullptr) != napi_ok || count != 2 ||
        !readString(env, values[0], root) || !readString(env, values[1], path))
    {
        napi_throw_type_error(env, "HARMONY_SANDBOX_INVALID_ARGUMENT", "需要沙箱目录和文件或目录路径");
        return nullptr;
    }
    try
    {
        if (operation == PathOperation::ValidateOutput)
            return stringValue(env, HarmonySandbox::validateOutput(root, path));
        if (operation == PathOperation::ValidateTaskDirectory)
            return stringValue(env, SandboxStore::validateTaskDirectory(root, path));
        SandboxStore::markUsed(root, path);
        napi_value result;
        napi_get_undefined(env, &result);
        return result;
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

napi_value markUsed(napi_env env, napi_callback_info info)
{
    return pathOperation(env, info, PathOperation::MarkUsed);
}
}

void InitHarmonySandbox(napi_env env, napi_value exports)
{
    napi_property_descriptor methods[] = {
        {"harmonyImportDocument", nullptr, importDocument, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"harmonyValidateOutput", nullptr, validateOutput, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"harmonyValidateTaskDirectory", nullptr, validateTaskDirectory, nullptr, nullptr, nullptr,
         napi_default, nullptr},
        {"harmonyRecentDocuments", nullptr, recentDocuments, nullptr, nullptr, nullptr, napi_default,
         nullptr},
        {"harmonyLoadTask", nullptr, loadTask, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"harmonyFindEditCaches", nullptr, findEditCaches, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"harmonyMarkUsed", nullptr, markUsed, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(methods) / sizeof(methods[0]), methods);
}
#endif
