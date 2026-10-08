/*
 * 汇文 MCP 的鸿蒙 NAPI 桥接入口。
 * 提供 ping 和异步 Python 自检，连接 ArkTS 与私有 HNP 运行时。
 */
#include "napi/native_api.h"
#include "python_runner.h"

#include <exception>
#include <string>

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
    std::string result;
    std::string error;
};

// 耗时的进程调用在 NAPI 工作线程执行，避免阻塞 ArkTS 页面。
static void ExecutePython(napi_env env, void *data)
{
    auto *task = static_cast<PythonWork *>(data);
    try
    {
        task->result = RunPythonSmoke();
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
        const std::string message = task->error.empty() ? "Python 自检任务未完成" : task->error;
        napi_value text;
        napi_create_string_utf8(env, message.c_str(), NAPI_AUTO_LENGTH, &text);
        napi_create_error(env, nullptr, text, &value);
        napi_reject_deferred(env, task->deferred, value);
    }
    napi_delete_async_work(env, task->work);
    delete task;
}

// 仅暴露固定的 Python 自检，不接受 ArkTS 传入的命令行。
static napi_value CheckPython(napi_env env, napi_callback_info info)
{
    auto *task = new PythonWork();
    napi_value promise;
    if (napi_create_promise(env, &task->deferred, &promise) != napi_ok)
    {
        delete task;
        napi_throw_error(env, nullptr, "无法创建 Python 自检 Promise");
        return nullptr;
    }
    napi_value name;
    napi_create_string_utf8(env, "HuiwenPythonCheck", NAPI_AUTO_LENGTH, &name);
    napi_status status = napi_create_async_work(env, nullptr, name, ExecutePython, CompletePython, task, &task->work);
    if (status == napi_ok)
    {
        status = napi_queue_async_work(env, task->work);
    }
    if (status != napi_ok)
    {
        napi_value message;
        napi_value error;
        napi_create_string_utf8(env, "无法启动 Python 自检任务", NAPI_AUTO_LENGTH, &message);
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

// 将 ping 和 Python 自检方法暴露给 ArkTS。
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor descriptors[] = {
        {"ping", nullptr, Ping, nullptr, nullptr, nullptr, napi_default, nullptr},
        {"checkPython", nullptr, CheckPython, nullptr, nullptr, nullptr, napi_default, nullptr},
    };
    napi_define_properties(env, exports, sizeof(descriptors) / sizeof(descriptors[0]), descriptors);
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
