cmake_minimum_required(VERSION 3.5.0)
project(HuiWenMCP)

# ArkTS 调用的 C++ NAPI 桥接库，由工程构建生成 .so。
add_library(huiwen_native SHARED napi_bridge.cpp)
target_link_libraries(huiwen_native PRIVATE ace_napi.z.so)
