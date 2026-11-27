cmake_minimum_required(VERSION 3.22.1)

project(goodreader_android_jni LANGUAGES CXX)

set(GOODREADER_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")

add_library(goodreader_jni SHARED
    NativeCoreJni.cpp
    "${GOODREADER_ROOT}/bridge/src/core_c_api.cpp"
    "${GOODREADER_ROOT}/core/reader/document.cpp"
    "${GOODREADER_ROOT}/core/library/types.cpp"
)

target_include_directories(goodreader_jni PRIVATE
    "${GOODREADER_ROOT}/bridge/include"
    "${GOODREADER_ROOT}/core/library/include"
    "${GOODREADER_ROOT}/core/reader/include"
    "${GOODREADER_ROOT}/core/sync/include"
)
target_compile_features(goodreader_jni PRIVATE cxx_std_20)
target_compile_options(goodreader_jni PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(goodreader_jni PRIVATE z)
