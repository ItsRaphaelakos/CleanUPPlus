cmake_minimum_required(VERSION 3.22)

project(CleanUpPlusViewers LANGUAGES CXX)

add_executable(Viewer_android WIN32 src/main.cpp)
target_compile_features(Viewer_android PRIVATE cxx_std_17)
target_compile_definitions(Viewer_android PRIVATE VIEWER_ANDROID=1)
target_link_libraries(Viewer_android PRIVATE user32 gdi32)

add_executable(Viewer_ios WIN32 src/main.cpp)
target_compile_features(Viewer_ios PRIVATE cxx_std_17)
target_compile_definitions(Viewer_ios PRIVATE VIEWER_IOS=1)
target_link_libraries(Viewer_ios PRIVATE user32 gdi32)

if (MSVC)
    target_compile_options(Viewer_android PRIVATE /W4 /permissive-)
    target_compile_options(Viewer_ios PRIVATE /W4 /permissive-)
endif()
