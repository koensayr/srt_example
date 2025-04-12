cmake_minimum_required(VERSION 3.10)
project(visca_srt)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(SRT REQUIRED srt)

# Add nlohmann_json as a submodule if not found
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Add include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Create executables
add_executable(visca_srt_server visca_srt_server.cpp)
add_executable(visca_srt_client visca_srt_client.cpp)

# Link dependencies
foreach(TARGET visca_srt_server visca_srt_client)
    target_link_libraries(${TARGET} PRIVATE
        ${SRT_LIBRARIES}
        nlohmann_json::nlohmann_json
        pthread
    )
    target_include_directories(${TARGET} PRIVATE ${SRT_INCLUDE_DIRS})
    target_link_directories(${TARGET} PRIVATE ${SRT_LIBRARY_DIRS})
endforeach()

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


# Install binaries
install(TARGETS visca_srt_server visca_srt_client
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT runtime)

# Install configuration files
install(FILES 
        ${CMAKE_CURRENT_SOURCE_DIR}/config/server_config.json
        ${CMAKE_CURRENT_SOURCE_DIR}/config/client_config.json
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/visca_srt)

# Install and set permissions for NDI tally test script
install(FILES 
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts/ndi_tally_test.py
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                   GROUP_READ GROUP_EXECUTE
                   WORLD_READ WORLD_EXECUTE
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        RENAME ndi-tally-test)

# Enable testing
enable_testing()

# Setup testing
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Add test executable
add_executable(test_ndi_tally tests/test_ndi_tally.cpp)
target_link_libraries(test_ndi_tally PRIVATE gtest gtest_main)
target_include_directories(test_ndi_tally PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add tests
add_test(NAME server_help COMMAND visca_srt_server --help)
add_test(NAME client_help COMMAND visca_srt_client --help)
add_test(NAME ndi_tally_tests COMMAND test_ndi_tally)

# Enable test discovery
gtest_discover_tests(test_ndi_tally)
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