cmake_minimum_required(VERSION 3.16)
project(HuiwenMCP)

# 头文件、工具和配置使用项目内开发包；库入口指向 entry/libs。
get_filename_component(project_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(QT_PREFIX "${project_root}/third_party/qt")
if(NOT EXISTS "${QT_PREFIX}/lib/cmake/Qt5/Qt5Config.cmake")
    message(FATAL_ERROR "缺少项目内 Qt 开发包，请运行 packaging/ohos/prepare_qt.py")
endif()
list(APPEND CMAKE_FIND_ROOT_PATH "${QT_PREFIX}")
set(Qt5_DIR "${QT_PREFIX}/lib/cmake/Qt5" CACHE PATH "项目内 Qt 配置" FORCE)
foreach(component Core Gui Widgets Concurrent)
    set(Qt5${component}_DIR "${QT_PREFIX}/lib/cmake/Qt5${component}" CACHE PATH "项目内 Qt 模块" FORCE)
endforeach()
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt5 5.15.12 EXACT REQUIRED COMPONENTS Core Gui Widgets Concurrent)
set(workspace_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../../qt_app")

# ArkTS 调用的 C++ NAPI 桥接库，由工程构建生成 .so。
add_library(huiwen_native SHARED napi_bridge.cpp python_runner.cpp
            "${workspace_root}/src/platform/harmony/qt_bridge.cpp")
if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    # 通用目录规则进入 NAPI 库，Qt 工作区复用同一份实现。
    target_sources(huiwen_native PRIVATE
        "${workspace_root}/src/sandbox/SandboxStore.cpp"
        "${workspace_root}/src/platform/harmony/harmony_sandbox.cpp"
        "${workspace_root}/src/platform/harmony/harmony_sandbox_napi.cpp")
endif()
target_include_directories(huiwen_native PRIVATE "${workspace_root}/src")
target_link_libraries(huiwen_native PRIVATE ace_napi.z.so hilog_ndk.z.so rawfile.z.so Qt5::Core)
add_subdirectory("${workspace_root}" "${CMAKE_CURRENT_BINARY_DIR}/qt_app")

# 更新本项目生成的库；Hvigor 同时从原生输出和 libs 收集运行库。
add_custom_command(TARGET huiwen_native POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:huiwen_native>"
            "${project_root}/entry/libs/arm64-v8a/$<TARGET_FILE_NAME:huiwen_native>"
    COMMENT "同步 huiwen_native 动态库到 entry/libs/arm64-v8a")

# PDFium 由 Run 的准备任务生成并部署，Qt 工作区链接该库显示正文预览。
if(NOT EXISTS "${project_root}/entry/libs/arm64-v8a/libpdfium.so")
    message(FATAL_ERROR "缺少 libpdfium.so，请运行 packaging/ohos/build_pdfium.py")
endif()
