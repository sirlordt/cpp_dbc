cmake_minimum_required(VERSION 3.15)
project(cpp_dbc_tests LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option for Address Sanitizer
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)

# Enable Address Sanitizer if requested
if(ENABLE_ASAN)
    message(STATUS "Address Sanitizer enabled for tests")

    # Ensure ASAN runtime is linked first
    set(CMAKE_CXX_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer ${CMAKE_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address,undefined ${CMAKE_EXE_LINKER_FLAGS}")

    # Add compile definition to indicate ASAN is enabled
    add_compile_definitions(ASAN_ENABLED)
else()
    message(STATUS "Address Sanitizer disabled for tests")
endif()

# Find required packages
# Look for Catch2 in the cpp_dbc library's Conan packages
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../build/${CMAKE_BUILD_TYPE}/generators")
find_package(Catch2 REQUIRED)

# Find yaml-cpp from the main project
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/${CMAKE_BUILD_TYPE}/generators")
find_package(yaml-cpp REQUIRED)

# Define test executable
add_executable(cpp_dbc_tests
    test_main.cpp
    test_basic.cpp
    test_yaml.cpp
    test_db_config.cpp
    test_mysql_connection.cpp
    test_postgresql_connection.cpp
)

# Link against Catch2, cpp_dbc and yaml-cpp
target_link_libraries(cpp_dbc_tests PRIVATE
    Catch2::Catch2WithMain
    cpp_dbc::cpp_dbc
    yaml-cpp::yaml-cpp
)

# Add include directories
target_include_directories(cpp_dbc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Define CATCH_CONFIG_NO_POSIX_SIGNALS as an extra safety measure
target_compile_definitions(cpp_dbc_tests PRIVATE
    CATCH_CONFIG_NO_POSIX_SIGNALS
)

# Register tests with CTest
include(CTest)
include(Catch)
catch_discover_tests(cpp_dbc_tests)

# Copy the test_db_connections.yml file directly to the directory where the executable is
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test_db_connections.yml
    ${CMAKE_CURRENT_BINARY_DIR}/../test_db_connections.yml
    COPYONLY)

# We'll set environment variables in the run script instead