cmake_minimum_required(VERSION 3.10)
project(srt_examples)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libsrt package
find_package(PkgConfig REQUIRED)
pkg_check_modules(SRT REQUIRED srt)

# Create executable
add_executable(srt_example srt_example.cpp)

# Link against libsrt
target_link_libraries(srt_example ${SRT_LIBRARIES})
target_include_directories(srt_example PUBLIC ${SRT_INCLUDE_DIRS})
target_link_directories(srt_example PUBLIC ${SRT_LIBRARY_DIRS})
