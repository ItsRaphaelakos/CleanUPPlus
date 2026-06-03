cmake_minimum_required(VERSION 3.22)

project(cleanup_core LANGUAGES CXX)

add_library(cleanup_core STATIC
    src/scanner.cpp
)

target_include_directories(cleanup_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(cleanup_core PUBLIC cxx_std_17)

if (MSVC)
    target_compile_options(cleanup_core PRIVATE /W4)
else()
    target_compile_options(cleanup_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(CLEANUP_CORE_BUILD_TESTS "Build cleanup_core smoke tests" ON)

if (CLEANUP_CORE_BUILD_TESTS)
    enable_testing()

    add_executable(cleanup_core_smoke_test
        tests/core_smoke_test.cpp
    )
    target_link_libraries(cleanup_core_smoke_test PRIVATE cleanup_core)

    add_test(NAME cleanup_core_smoke_test COMMAND cleanup_core_smoke_test)
endif()
