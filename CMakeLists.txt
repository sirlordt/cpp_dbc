cmake_minimum_required(VERSION 3.15)
project(cpp_dbc_demo VERSION 0.1.0 LANGUAGES CXX)

# Generate compile_commands.json for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options for conditional compilation of database drivers
option(USE_MYSQL "Enable MySQL support" ON)
option(USE_POSTGRESQL "Enable PostgreSQL support" OFF)
option(USE_SQLITE "Enable SQLite support" OFF)
option(USE_FIREBIRD "Enable Firebird support" OFF)
option(USE_MONGODB "Enable MongoDB support" OFF)
option(USE_SCYLLADB "Enable ScyllaDB support" OFF)
option(USE_CPP_YAML "Enable YAML support" OFF)

# Load binary name from .dist_build
file(STRINGS ".dist_build" DIST_BUILD_CONTENT)

foreach(LINE ${DIST_BUILD_CONTENT})
    if(LINE MATCHES "^Container_Bin_Name=\"([^\"]+)\"$")
        set(APP_BIN_NAME ${CMAKE_MATCH_1})
    endif()
endforeach()

if(NOT DEFINED APP_BIN_NAME)
    set(APP_BIN_NAME "cpp_dbc_demo")
    message(WARNING "Container_Bin_Name not found in .dist_build, using default: ${APP_BIN_NAME}")
endif()

message(STATUS "Building executable: ${APP_BIN_NAME}")
message(STATUS "MySQL support: ${USE_MYSQL}")
message(STATUS "PostgreSQL support: ${USE_POSTGRESQL}")
message(STATUS "SQLite support: ${USE_SQLITE}")
message(STATUS "Firebird support: ${USE_FIREBIRD}")
message(STATUS "MongoDB support: ${USE_MONGODB}")
message(STATUS "ScyllaDB support: ${USE_SCYLLADB}")
message(STATUS "YAML support: ${USE_CPP_YAML}")

# Find dependencies with Conan
if(EXISTS "${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
    include("${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
endif()

# Add Conan generators directory to include path
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CONAN_GENERATORS_DIR "${CMAKE_BINARY_DIR}/Debug/generators")
else()
    set(CONAN_GENERATORS_DIR "${CMAKE_BINARY_DIR}/Release/generators")
endif()

message(STATUS "Conan generators directory: ${CONAN_GENERATORS_DIR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Find Conan packages
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)

# Add the path where the cpp_dbc library is installed
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build/libs/cpp_dbc")

# Add the path to the cpp_dbc source
set(CPP_DBC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/cpp_dbc")

# Find the cpp_dbc library
find_package(cpp_dbc REQUIRED)

# Main executable
add_executable(${APP_BIN_NAME} src/main.cpp)

# Link against the cpp_dbc library and other dependencies
target_link_libraries(${APP_BIN_NAME} PRIVATE
    cpp_dbc::cpp_dbc
    fmt::fmt
    nlohmann_json::nlohmann_json
    yaml-cpp::yaml-cpp
)

# Add include directories for the main executable
target_include_directories(${APP_BIN_NAME} PRIVATE
    ${CONAN_GENERATORS_DIR}
)

# Note: Compile definitions for database drivers (USE_MYSQL, USE_POSTGRESQL, etc.)
# are automatically added by linking to cpp_dbc::cpp_dbc through INTERFACE properties.
# They are defined in the cpp_dbc-config.cmake file based on how the library was built.

# Installation rules
install(TARGETS ${APP_BIN_NAME}
    DESTINATION bin)

# No tests
