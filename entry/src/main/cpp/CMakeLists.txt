# the minimum version of CMake.
cmake_minimum_required(VERSION 3.5.0)
project(HuiWenMCP)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

list(APPEND CMAKE_FIND_ROOT_PATH ${QT_PREFIX})
find_package(Qt5 5.15.12 EXACT REQUIRED COMPONENTS Core Gui Widgets)

add_library(entry SHARED main.cpp)
target_link_libraries(entry PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::QOpenHarmonyPlatformIntegrationPlugin
)
