/*
 * 汇文 MCP 的鸿蒙 NAPI 桥接入口。
 * 提供 ping、Python 自检和异步 PDF 转换，连接 ArkTS 与私有 HNP 运行时。
 */
#include "napi/native_api.h"
#include "python_runner.h"
#include "platform/harmony/qt_bridge.h"

#include <exception>
#include <string>
#include <vector>

// 返回固定结果，供首页检查原生模块是否可调用。
static napi_value Ping(napi_env env, napi_callback_info info)
{
    napi_value result;
    napi_create_string_utf8(env, "NAPI OK", NAPI_AUTO_LENGTH, &result);
    return result;
}

struct PythonWork
{
    napi_async_work work = nullptr;
    napi_deferred deferred = nullptr;
    bool convert = false;
    std::string inputPath;
    std::string outputPath;
    std::string result;
    std::string error;
};

// 耗时的进程调用在 NAPI 工作线程执行，避免阻塞 ArkTS 页面。
static void ExecutePython(napi_env env, void *data)
{
    auto *task = static_cast<PythonWork *>(data);
    try
    {
        task->result = task->convert ? RunPdfConversion(task->inputPath, task->outputPath) : RunPythonSmoke();
    }
    catch (const std::exception &ex)
    {
        task->error = ex.what();
    }
}

static void CompletePython(napi_env env, napi_status status, void *data)
{
    auto *task = static_cast<PythonWork *>(data);
    napi_value value;
    if (status == napi_ok && task->error.empty())
    {
        napi_create_string_utf8(env, task->result.c_str(), NAPI_AUTO_LENGTH, &value);
        napi_resolve_deferred(env, task->deferred, value);
    }
    else
    {
        const std::string message = task->error.empty() ? "Python 任务未完成" : task->error;
        napi_value text;
        napi_create_string_utf8(env, message.c_str(), NAPI_AUTO_LENGTH, &text);
        napi_create_error(env, nullptr, text, &value);
        napi_reject_deferred(env, task->deferred, value);
    }
    napi_delete_async_work(env, task->work);
    delete task;
}

static napi_value QueuePythonWork(napi_env env, PythonWork *task, const char *name)
{
    napi_value promise;
    if (napi_create_promise(env, &task->deferred, &promise) != napi_ok)
    {
        delete task;
        napi_throw_error(env, nullptr, "无法创建 Python 任务 Promise");
        return nullptr;
    }
    napi_value resourceName;
    napi_create_string_utf8(env, name, NAPI_AUTO_LENGTH, &resourceName);
    napi_status status = napi_create_async_work(env, nullptr, resourceName, ExecutePython, CompletePython, task, &task->work);
    if (status == napi_ok)
    {
        status = napi_queue_async_work(env, task->work);
    }
    if (status != napi_ok)
    {
        napi_value message;
        napi_value error;
        napi_create_string_utf8(env, "无法启动 Python 任务", NAPI_AUTO_LENGTH, &message);
        napi_create_error(env, nullptr, message, &error);
        napi_reject_deferred(env, task->deferred, error);
        if (task->work != nullptr)
        {
            napi_delete_async_work(env, task->work);
        }
        delete task;
    }
    return promise;
}

// 自检与转换共用异步进程任务，路径只作为固定脚本的参数。
static napi_value CheckPython(napi_env env, napi_callback_info info)
{
    return QueuePythonWork(env, new PythonWork(), "HuiwenPythonCheck");
}

static bool ReadPath(napi_env env, napi_value value, std::string &path)
{
    napi_valuetype type;
    size_t length = 0;
    if (napi_typeof(env, value, &type) != napi_ok || type != napi_string ||
        napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok || length == 0 || length > 4096)
    {
        return false;
    }
    std::vector<char> buffer(length + 1);
    size_t copied = 0;
    if (napi_get_value_string_utf8(env, value, buffer.data(), buffer.size(), &copied) != napi_ok || copied != length)
    {
        return false;
    }
    path.assign(buffer.data(), copied);
    return path.find('\0') == std::string::npos;
}

// 项目对 ArkTS 暴露的 PDF 转 MD API：只接收输入、输出路径并异步执行。
static napi_value ConvertPdf(napi_env env, napi_callback_info info)
{
    size_t count = 2;
    napi_value values[2];
    if (napi_get_cb_info(env, info, &count, values, nullptr, nullptr) != napi_ok || count != 2)
    {
        napi_throw_type_error(env, nullptr, "需要 PDF 输入路径和 MD 输出路径");
        return nullptr;
    }
    auto *task = new PythonWork();
    task->convert = true;
    if (!ReadPath(env, values[0], task->inputPath) || !ReadPath(env, values[1], task->outputPath))
    {
        delete task;
        napi_throw_type_error(env, nullptr, "文件路径必须是有效字符串");
        return nullptr;
    }
    return QueuePythonWork(env, task, "HuiwenPdfConvert");
}

// 将首页需要的能力暴露给 ArkTS。
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor descriptors[] = {
        {"ping", nullptr, Ping, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"checkPython", nullptr, CheckPython, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"convertPdf", nullptr, ConvertPdf, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
    // 工作区桥接使用同一模块，Qt 与 ArkTS 共享回调和 PythonRunner。
    InitQtBridge(env, exports);
    return exports;
}

// 模块名与 CMake 目标名、ArkTS 导入名保持一致。
static napi_module module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "huiwen_native",
    .nm_priv = nullptr,
    .reserved = {0},
};

extern "C" __attribute__((constructor)) void RegisterHuiwenNativeModule() { napi_module_register(&module); }
