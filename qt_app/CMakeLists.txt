cmake_minimum_required(VERSION 3.16)
project(HuiwenWorkspace LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# 鸿蒙由 QPA 加载共享库；桌面系统使用普通可执行入口。
set(workspace_sources
    main.cpp
    resources/file-icons.qrc
    resources/markdown.qrc
    src/workspace/ConversionWindow.cpp
    src/sourceView/SourceDocumentView.cpp
    src/sourceView/PdfPreviewWidget.cpp
    src/sourceView/PdfRenderWindow.cpp
    src/resultView/MarkdownResultView.cpp
    src/resultView/MarkdownPreviewWidget.cpp
    src/components/FileCard.cpp
    src/components/ConvertButton.cpp
    src/components/splitter/WorkspaceSplitter.cpp
    src/components/scrollBar/DocumentScrollBar.cpp
    src/components/functionBar/FunctionBar.cpp
    src/components/functionBar/FileMenu.cpp
    src/components/functionBar/SelectionMenu.cpp
    src/services/ConversionService.cpp
    src/sandbox/SandboxService.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    add_library(huiwen_qt SHARED ${workspace_sources})
    # 使用迁入的预编译 CEF/adapter；只编译公开 C++ 包装层，不重编浏览器和 Qt 本体。
    set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cef/sdk")
    if(NOT EXISTS "${CEF_ROOT}/include/cef_app.h")
        message(FATAL_ERROR "缺少项目内 CEF 环境，请先运行 packaging/ohos/prepare_cef.py")
    endif()
    list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
    find_package(CEF REQUIRED)
    add_subdirectory("${CEF_ROOT}/libcef_dll" "cef_wrapper")
    set_target_properties(libcef_dll_wrapper PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC OFF AUTORCC OFF)
    target_include_directories(libcef_dll_wrapper PRIVATE "${CEF_ROOT}")
    target_include_directories(huiwen_qt PRIVATE "${CEF_ROOT}")
    foreach(library cef adapter_c)
        if(NOT EXISTS "${project_root}/entry/libs/arm64-v8a/lib${library}.so")
            message(FATAL_ERROR "缺少预编译 CEF 运行库：lib${library}.so")
        endif()
        add_library(huiwen_${library} SHARED IMPORTED)
        set_target_properties(huiwen_${library} PROPERTIES
            IMPORTED_LOCATION "${project_root}/entry/libs/arm64-v8a/lib${library}.so")
    endforeach()
    # 原生适配层默认加载 libcefsimple.so 的 CefMain；入口库不链接 Qt 或业务桥。
    add_library(cefsimple SHARED src/platform/harmony/CefSubprocess.cpp)
    set_target_properties(cefsimple PROPERTIES AUTOMOC OFF AUTORCC OFF)
    target_include_directories(cefsimple PRIVATE "${CEF_ROOT}")
    target_link_libraries(cefsimple PRIVATE libcef_dll_wrapper huiwen_cef hilog_ndk.z.so)
    add_custom_command(TARGET cefsimple POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:cefsimple>"
                "${project_root}/entry/libs/arm64-v8a/$<TARGET_FILE_NAME:cefsimple>"
        COMMENT "同步 CEF 子进程入口到 entry/libs/arm64-v8a")
    target_link_libraries(huiwen_qt PRIVATE libcef_dll_wrapper huiwen_cef huiwen_adapter_c cefsimple hilog_ndk.z.so)
    target_link_libraries(huiwen_qt PRIVATE huiwen_native Qt5::QOpenHarmonyPlatformIntegrationPlugin)
    add_custom_command(TARGET huiwen_qt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:huiwen_qt>"
                "${project_root}/entry/libs/arm64-v8a/$<TARGET_FILE_NAME:huiwen_qt>"
        COMMENT "同步 huiwen_qt 动态库到 entry/libs/arm64-v8a")
else()
    # 桌面直接编译通用沙箱规则，不引入鸿蒙 fd/NAPI 适配层。
    add_executable(huiwen_qt ${workspace_sources} src/sandbox/SandboxStore.cpp)
endif()
target_include_directories(huiwen_qt PRIVATE src ../entry/src/main/cpp)
# 工作区使用 PDFium 的公开 C API；桌面构建需指定匹配宿主架构的库。
if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    set(PDFIUM_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/../entry/libs/arm64-v8a/libpdfium.so")
else()
    set(PDFIUM_LIBRARY "" CACHE FILEPATH "宿主架构的 PDFium 动态库")
endif()
if(NOT EXISTS "${PDFIUM_LIBRARY}")
    message(FATAL_ERROR "缺少 PDFium 动态库，请准备 libpdfium.so 或设置 PDFIUM_LIBRARY")
endif()
add_library(huiwen_pdfium SHARED IMPORTED)
set_target_properties(huiwen_pdfium PROPERTIES
    IMPORTED_LOCATION "${PDFIUM_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/pdfium/public")
target_link_libraries(huiwen_qt PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Concurrent huiwen_pdfium)
