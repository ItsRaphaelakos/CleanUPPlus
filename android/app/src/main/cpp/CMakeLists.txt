cmake_minimum_required(VERSION 3.22)

project(cleanupplus_android LANGUAGES CXX)

set(CLEANUP_CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../../common/cleanup_core")

add_library(cleanupplus SHARED
    jni_bridge.cpp
    "${CLEANUP_CORE_DIR}/src/scanner.cpp"
)

target_include_directories(cleanupplus
    PRIVATE
        "${CLEANUP_CORE_DIR}/include"
)

target_compile_features(cleanupplus PRIVATE cxx_std_17)

find_library(log-lib log)
target_link_libraries(cleanupplus PRIVATE ${log-lib})
