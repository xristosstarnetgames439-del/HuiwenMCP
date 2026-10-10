cmake_minimum_required(VERSION 3.16)
project(HuiwenWorkspace LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# 鸿蒙由 QPA 加载共享库；桌面系统使用普通可执行入口。
set(workspace_sources
    main.cpp
    src/workspace/ConversionWindow.cpp
    src/sourceView/SourceDocumentView.cpp
    src/resultView/MarkdownResultView.cpp
    src/components/FileCard.cpp
    src/components/ConvertButton.cpp
    src/services/ConversionService.cpp
    src/sandbox/SandboxService.cpp)
if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    add_library(huiwen_qt SHARED ${workspace_sources})
    target_link_libraries(huiwen_qt PRIVATE huiwen_native Qt5::QOpenHarmonyPlatformIntegrationPlugin)
else()
    add_executable(huiwen_qt ${workspace_sources})
endif()
target_include_directories(huiwen_qt PRIVATE src ../entry/src/main/cpp)
target_link_libraries(huiwen_qt PRIVATE Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Concurrent)
