cmake_minimum_required(VERSION 3.25)

project(Reader
    VERSION 0.1.0
    DESCRIPTION "A modern C++23 reader application"
    LANGUAGES CXX
)

# ==========================================
# C++ Configuration
# ==========================================

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==========================================
# Build Options
# ==========================================

option(READER_BUILD_TESTS "Build tests" ON)

option(READER_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors" ON
)

option(READER_ENABLE_ASAN
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF
)

option(READER_ENABLE_TSAN
    "Enable ThreadSanitizer" OFF
)

# ==========================================
# Validate Configuration
# ==========================================

if(READER_ENABLE_ASAN AND READER_ENABLE_TSAN)
    message(FATAL_ERROR
        "ASan and TSan cannot be enabled simultaneously"
    )
endif()

if((READER_ENABLE_ASAN OR READER_ENABLE_TSAN)
   AND NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    message(FATAL_ERROR
        "Sanitizers require GCC or Clang"
    )
endif()

# ==========================================
# Compiler Warnings
# ==========================================

function(reader_enable_warnings target)

    if(MSVC)

        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /w14242
            /w14254
            /w14263
            /w14265
            /w14287
            /w14311
            /w14545
            /w14546
            /w14547
            /w14549
            /w14555
            /w14619
            /w14640
            /w14826
            /w14905
            /w14906
            /w14928
        )

        if(READER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()

    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")

        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wformat=2
            -Wundef
            -Wcast-align
            -Wnull-dereference
            -Wdouble-promotion
            -Wimplicit-fallthrough
            -Wold-style-cast
            -Woverloaded-virtual
            -Wnon-virtual-dtor
            -Wzero-as-null-pointer-constant
            -Wduplicated-cond
            -Wlogical-op
            -Wuseless-cast
        )

        if(READER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")

        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wsign-conversion
            -Wshadow
            -Wformat=2
            -Wundef
            -Wcast-align
            -Wnull-dereference
            -Wdouble-promotion
            -Wimplicit-fallthrough
            -Wold-style-cast
            -Woverloaded-virtual
            -Wnon-virtual-dtor
            -Wzero-as-null-pointer-constant
            -Wextra-semi
        )

        if(READER_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()

    endif()

endfunction()

# ==========================================
# Sanitizers
# ==========================================

function(reader_enable_sanitizers target)

    if(READER_ENABLE_ASAN)

        message(STATUS "ASan + UBSan enabled")

        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -fno-sanitize-recover=all
            -g
        )

        get_target_property(target_type ${target} TYPE)

        if(NOT target_type STREQUAL "STATIC_LIBRARY")
            target_link_options(${target} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
            )
        endif()

    elseif(READER_ENABLE_TSAN)

        message(STATUS "TSan enabled")

        target_compile_options(${target} PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
            -g
        )

        get_target_property(target_type ${target} TYPE)

        if(NOT target_type STREQUAL "STATIC_LIBRARY")
            target_link_options(${target} PRIVATE
                -fsanitize=thread
                -fno-omit-frame-pointer
            )
        endif()

    endif()

endfunction()

# ==========================================
# Dependencies
# ==========================================

include(FetchContent)

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
    GIT_SHALLOW TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# ==========================================
# Reader Library
# ==========================================

add_library(reader_lib
    src/reader.cpp
    src/logging.cpp
)

target_include_directories(reader_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_include_directories(reader_lib SYSTEM PUBLIC
    ${spdlog_SOURCE_DIR}/include
)

target_link_libraries(reader_lib
    PUBLIC
        spdlog::spdlog
)

target_compile_features(reader_lib
    PUBLIC
        cxx_std_23
)

reader_enable_warnings(reader_lib)
reader_enable_sanitizers(reader_lib)

# ==========================================
# Reader Application
# ==========================================

add_executable(reader
    src/main.cpp
)

target_link_libraries(reader PRIVATE reader_lib)

reader_enable_warnings(reader)
reader_enable_sanitizers(reader)

# ==========================================
# Tests
# ==========================================

include(CTest)

if(BUILD_TESTING AND READER_BUILD_TESTS)

    add_executable(reader_tests
        tests/reader_tests.cpp
    )

    target_link_libraries(reader_tests
        PRIVATE
            reader_lib
            GTest::gtest_main
    )

    reader_enable_warnings(reader_tests)
    reader_enable_sanitizers(reader_tests)

    include(GoogleTest)

    gtest_discover_tests(reader_tests)

endif()

# ==========================================
# Installation
# ==========================================

install(
    TARGETS reader
    RUNTIME DESTINATION bin
)

# ==========================================
# Configuration Summary
# ==========================================

message(STATUS "================================")
message(STATUS " Reader Configuration")
message(STATUS "================================")
message(STATUS " C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS " Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS " Version: ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS " Tests: ${READER_BUILD_TESTS}")
message(STATUS " Warnings: Enabled")
message(STATUS " Warnings as errors: ${READER_WARNINGS_AS_ERRORS}")
message(STATUS " ASan/UBSan: ${READER_ENABLE_ASAN}")
message(STATUS " TSan: ${READER_ENABLE_TSAN}")
message(STATUS "================================")
