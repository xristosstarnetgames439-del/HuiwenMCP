cmake_minimum_required(VERSION 3.16)
project(huiwen_pillow C)

# Python extension symbols resolve from the HNP Python executable at import.
# build_pdf_ocr.py checks all remaining undefined symbols against target exports.
string(REPLACE "-Wl,--no-undefined" "" CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS}")
include("${PILLOW_SOURCE_LIST}")
add_library(_imaging MODULE ${PILLOW_SOURCES})
set_target_properties(_imaging PROPERTIES PREFIX "" SUFFIX ".so"
    BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "$ORIGIN/../../..")
target_include_directories(_imaging PRIVATE
    "${PYTHON_RUNTIME}/include/python3.14"
    "${PILLOW_SOURCE}/src" "${PILLOW_SOURCE}/src/libImaging"
    "${JPEG_SOURCE}/src" "${JPEG_BUILD}")
target_compile_definitions(_imaging PRIVATE HAVE_LIBJPEG HAVE_LIBZ PILLOW_VERSION="12.3.0")
target_link_libraries(_imaging PRIVATE "${JPEG_BUILD}/libjpeg.a" "${PYTHON_RUNTIME}/lib/libz.so" m)
