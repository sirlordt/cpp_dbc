cmake_minimum_required(VERSION 3.15)
project(cpp_dbc_benchmarks LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
# Look for Catch2 in the cpp_dbc library's Conan packages
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/${CMAKE_BUILD_TYPE}/generators")
find_package(Catch2 REQUIRED)

# Find yaml-cpp from the main project
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/${CMAKE_BUILD_TYPE}/generators")
find_package(yaml-cpp REQUIRED)

# Define benchmark executable
add_executable(cpp_dbc_benchmarks
    benchmark_main.cpp
    benchmark_common.hpp
    benchmark_common.cpp

    # MySQL benchmarks
    benchmark_mysql_select.cpp
    benchmark_mysql_insert.cpp
    benchmark_mysql_update.cpp
    benchmark_mysql_delete.cpp

    # PostgreSQL benchmarks
    benchmark_postgresql_select.cpp
    benchmark_postgresql_insert.cpp
    benchmark_postgresql_update.cpp
    benchmark_postgresql_delete.cpp

    # SQLite benchmarks
    benchmark_sqlite_select.cpp
    benchmark_sqlite_insert.cpp
    benchmark_sqlite_update.cpp
    benchmark_sqlite_delete.cpp
)

# Link against Catch2, cpp_dbc and yaml-cpp
target_link_libraries(cpp_dbc_benchmarks PRIVATE
    Catch2::Catch2WithMain
    cpp_dbc::cpp_dbc
    yaml-cpp::yaml-cpp
)

# Add compile definitions based on options
target_compile_definitions(cpp_dbc_benchmarks PUBLIC
    $<$<BOOL:${USE_MYSQL}>:USE_MYSQL=1>
    $<$<NOT:$<BOOL:${USE_MYSQL}>>:USE_MYSQL=0>
    $<$<BOOL:${USE_POSTGRESQL}>:USE_POSTGRESQL=1>
    $<$<NOT:$<BOOL:${USE_POSTGRESQL}>>:USE_POSTGRESQL=0>
    $<$<BOOL:${USE_SQLITE}>:USE_SQLITE=1>
    $<$<NOT:$<BOOL:${USE_SQLITE}>>:USE_SQLITE=0>
    $<$<BOOL:${USE_CPP_YAML}>:USE_CPP_YAML=1>
    $<$<NOT:$<BOOL:${USE_CPP_YAML}>>:USE_CPP_YAML=0>
)

# Add include directories
target_include_directories(cpp_dbc_benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../test # Include test directory for helper functions
)

# Copy the benchmark_db_connections.yml file directly to the directory where the executable is
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/benchmark_db_connections.yml
    ${CMAKE_CURRENT_BINARY_DIR}/benchmark_db_connections.yml
    COPYONLY)