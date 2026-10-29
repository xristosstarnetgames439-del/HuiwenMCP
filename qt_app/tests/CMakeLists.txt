cmake_minimum_required(VERSION 3.16)
project(PdfPreviewCheck LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Concurrent)
set(PDFIUM_LIBRARY "" CACHE FILEPATH "匹配本机架构的 PDFium 动态库")
if(NOT EXISTS "${PDFIUM_LIBRARY}")
    message(FATAL_ERROR "请指定匹配本机架构的 PDFIUM_LIBRARY")
endif()
add_executable(pdf_preview_check pdf_preview_check.cpp
    ../src/sourceView/PdfPreviewWidget.cpp ../src/sourceView/PdfPreviewWidget.h
    ../src/sourceView/SourceDocumentView.cpp ../src/sourceView/SourceDocumentView.h
    ../src/components/scrollBar/DocumentScrollBar.cpp
    ../src/components/FileCard.cpp ../resources/file-icons.qrc)
target_include_directories(pdf_preview_check PRIVATE ../src ../../third_party/pdfium/public)
target_link_libraries(pdf_preview_check PRIVATE Qt5::Widgets Qt5::Concurrent "${PDFIUM_LIBRARY}")
