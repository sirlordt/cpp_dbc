cmake_minimum_required(VERSION 3.15)
project(cpp_dbc_demo VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options for conditional compilation of database drivers
option(USE_MYSQL "Enable MySQL support" ON)
option(USE_POSTGRESQL "Enable PostgreSQL support" OFF)

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
)

# Add include directories for the main executable
target_include_directories(${APP_BIN_NAME} PRIVATE
    ${CONAN_GENERATORS_DIR}
)

# Add compile definitions for the database drivers
target_compile_definitions(${APP_BIN_NAME} PRIVATE
    $<$<BOOL:${USE_MYSQL}>:USE_MYSQL=1>
    $<$<NOT:$<BOOL:${USE_MYSQL}>>:USE_MYSQL=0>
    $<$<BOOL:${USE_POSTGRESQL}>:USE_POSTGRESQL=1>
    $<$<NOT:$<BOOL:${USE_POSTGRESQL}>>:USE_POSTGRESQL=0>
)

# Installation rules
install(TARGETS ${APP_BIN_NAME}
    DESTINATION bin)

# No tests
