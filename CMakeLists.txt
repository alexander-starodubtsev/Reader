cmake_minimum_required(VERSION 3.20)

project(Reader
    VERSION 0.1.0
    DESCRIPTION "A C++ reader application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(reader_lib
    src/reader.cpp
)

target_include_directories(reader_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(reader
    src/main.cpp
)

target_link_libraries(reader PRIVATE reader_lib)

include(CTest)

if(BUILD_TESTING)
    add_executable(reader_tests
        tests/reader_tests.cpp
    )

    target_link_libraries(reader_tests PRIVATE reader_lib)

    add_test(
        NAME reader_tests
        COMMAND reader_tests
    )
endif()
