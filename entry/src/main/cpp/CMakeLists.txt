cmake_minimum_required(VERSION 3.16)
project(HuiWenMCP)

# Qt SDK 路径只放在环境变量或被忽略的 local.properties，不写入跨机器配置。
if(NOT QT_PREFIX)
    set(QT_PREFIX "$ENV{QT_PREFIX}")
endif()
if(NOT QT_PREFIX)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../../../../local.properties" qt_config
         REGEX "^qt.sdk.dir=")
    string(REGEX REPLACE "^qt.sdk.dir=" "" QT_PREFIX "${qt_config}")
endif()
if(NOT EXISTS "${QT_PREFIX}/lib/cmake/Qt5/Qt5Config.cmake")
    message(FATAL_ERROR "请设置 QT_PREFIX 或 local.properties 中的 qt.sdk.dir 为鸿蒙 Qt SDK 目录")
endif()
list(APPEND CMAKE_FIND_ROOT_PATH "${QT_PREFIX}")
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt5 5.15.12 EXACT REQUIRED COMPONENTS Core Gui Widgets Concurrent)
set(workspace_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../../qt_app")

# ArkTS 调用的 C++ NAPI 桥接库，由工程构建生成 .so。
add_library(huiwen_native SHARED napi_bridge.cpp python_runner.cpp
            "${workspace_root}/src/platform/harmony/qt_bridge.cpp")
target_include_directories(huiwen_native PRIVATE "${workspace_root}/src")
target_link_libraries(huiwen_native PRIVATE ace_napi.z.so Qt5::Core)
add_subdirectory("${workspace_root}" "${CMAKE_CURRENT_BINARY_DIR}/qt_app")

# 只部署所需运行时，不复制整套 Qt；目录是被 Git 忽略的本机构建输入。
set(runtime_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../../libs/arm64-v8a")
file(MAKE_DIRECTORY "${runtime_dir}")
foreach(component Core Gui Widgets Concurrent)
    configure_file("${QT_PREFIX}/lib/libQt5${component}.so" "${runtime_dir}/libQt5${component}.so" COPYONLY)
endforeach()
configure_file("${QT_PREFIX}/plugins/platforms/libplugins_platforms_qopenharmony.so"
               "${runtime_dir}/libplugins_platforms_qopenharmony.so" COPYONLY)
