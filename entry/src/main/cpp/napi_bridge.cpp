/*
 * 汇文 MCP 的鸿蒙 NAPI 桥接入口。
 * 当前只提供 ping，用于确认 ArkTS 与 C++ 的调用链已连通。
 */
#include "napi/native_api.h"

// 返回固定结果，供首页检查原生模块是否可调用。
static napi_value Ping(napi_env env, napi_callback_info info)
{
    napi_value result;
    napi_create_string_utf8(env, "NAPI OK", NAPI_AUTO_LENGTH, &result);
    return result;
}

// 将 ping 方法暴露给 ArkTS。
static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor descriptor = {"ping", nullptr, Ping, nullptr, nullptr, nullptr, napi_default, nullptr};
    napi_define_properties(env, exports, 1, &descriptor);
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

extern "C" __attribute__((constructor)) void RegisterHuiwenNativeModule()
{
    napi_module_register(&module);
}
