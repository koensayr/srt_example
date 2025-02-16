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

# Installation configuration
include(GNUInstallDirs)

install(TARGETS srt_example
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT runtime)

# Install Python example if requested
option(INSTALL_PYTHON_EXAMPLE "Install Python example script" ON)
if(INSTALL_PYTHON_EXAMPLE)
    install(FILES srt_examples.py
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                       GROUP_READ GROUP_EXECUTE
                       WORLD_READ WORLD_EXECUTE
            DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME})
endif()
