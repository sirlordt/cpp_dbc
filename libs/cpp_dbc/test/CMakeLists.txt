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
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/${CMAKE_BUILD_TYPE}/generators")
find_package(Catch2 REQUIRED)

# Find yaml-cpp from the main project only if YAML support is enabled
if(USE_CPP_YAML)
    set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/${CMAKE_BUILD_TYPE}/generators")
    find_package(yaml-cpp REQUIRED)
endif()

# Define test executable
add_executable(cpp_dbc_tests
    test_main.cpp
    test_basic.cpp
    test_yaml.cpp
    test_db_config.cpp
    test_mysql_connection.cpp
    test_postgresql_connection.cpp
    test_sqlite_connection.cpp
    test_connection_pool.cpp
    test_transaction_manager.cpp
    test_database_config.cpp
    test_drivers.cpp
    test_integration.cpp
    test_mysql_common.cpp
    test_postgresql_common.cpp
    test_sqlite_common.cpp
    test_firebird_common.cpp
    test_mongodb_common.cpp
    test_mysql_real.cpp
    test_mysql_real_inner_join.cpp
    test_mysql_real_left_join.cpp
    test_mysql_real_right_join.cpp
    test_mysql_real_full_join.cpp
    test_mysql_real_blob.cpp
    test_postgresql_real.cpp
    test_postgresql_real_inner_join.cpp
    test_postgresql_real_left_join.cpp
    test_postgresql_real_right_join.cpp
    test_postgresql_real_full_join.cpp
    test_postgresql_real_blob.cpp
    test_postgresql_real_json.cpp
    test_mysql_real_json.cpp
    test_sqlite_real.cpp
    test_sqlite_real_inner_join.cpp
    test_sqlite_real_left_join.cpp
    test_sqlite_real_blob.cpp
    test_firebird_real.cpp
    test_firebird_connection.cpp
    test_firebird_real_blob.cpp
    test_firebird_real_inner_join.cpp
    test_firebird_real_left_join.cpp
    test_firebird_real_right_join.cpp
    test_firebird_real_full_join.cpp
    test_firebird_real_json.cpp
    test_firebird_thread_safe.cpp
    test_connection_pool_real.cpp
    test_transaction_manager_real.cpp
    test_transaction_isolation.cpp
    test_mysql_thread_safe.cpp
    test_postgresql_thread_safe.cpp
    test_sqlite_thread_safe.cpp
    test_mongodb_real.cpp
    test_mongodb_connection.cpp
    test_mongodb_thread_safe.cpp
    test_mongodb_real_blob.cpp
    test_mongodb_real_json.cpp
    test_mongodb_real_join.cpp
)

# Link against Catch2 and cpp_dbc
target_link_libraries(cpp_dbc_tests PRIVATE
    Catch2::Catch2WithMain
    cpp_dbc::cpp_dbc
)

# Conditionally link against yaml-cpp
if(USE_CPP_YAML)
    target_link_libraries(cpp_dbc_tests PRIVATE
        yaml-cpp::yaml-cpp
    )
endif()

# Add compile definitions based on options
target_compile_definitions(cpp_dbc_tests PUBLIC
    $<$<BOOL:${USE_MYSQL}>:USE_MYSQL=1>
    $<$<NOT:$<BOOL:${USE_MYSQL}>>:USE_MYSQL=0>
    $<$<BOOL:${USE_POSTGRESQL}>:USE_POSTGRESQL=1>
    $<$<NOT:$<BOOL:${USE_POSTGRESQL}>>:USE_POSTGRESQL=0>
    $<$<BOOL:${USE_SQLITE}>:USE_SQLITE=1>
    $<$<NOT:$<BOOL:${USE_SQLITE}>>:USE_SQLITE=0>
    $<$<BOOL:${USE_FIREBIRD}>:USE_FIREBIRD=1>
    $<$<NOT:$<BOOL:${USE_FIREBIRD}>>:USE_FIREBIRD=0>
    $<$<BOOL:${USE_MONGODB}>:USE_MONGODB=1>
    $<$<NOT:$<BOOL:${USE_MONGODB}>>:USE_MONGODB=0>
    $<$<BOOL:${USE_CPP_YAML}>:USE_CPP_YAML=1>
    $<$<NOT:$<BOOL:${USE_CPP_YAML}>>:USE_CPP_YAML=0>
    $<$<BOOL:${DB_DRIVER_THREAD_SAFE}>:DB_DRIVER_THREAD_SAFE=1>
    $<$<NOT:$<BOOL:${DB_DRIVER_THREAD_SAFE}>>:DB_DRIVER_THREAD_SAFE=0>
)

# Define macros for backward.hpp to silence -Wundef warnings
target_compile_definitions(cpp_dbc_tests PUBLIC
    BACKWARD_HAS_UNWIND=0
    BACKWARD_HAS_LIBUNWIND=0
    BACKWARD_HAS_BACKTRACE=0
    BACKWARD_HAS_BFD=0
    BACKWARD_HAS_DWARF=0
    BACKWARD_HAS_BACKTRACE_SYMBOL=0
    BACKWARD_HAS_PDB_SYMBOL=0
)

# Add include directories
target_include_directories(cpp_dbc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# Exclude backward.hpp from warnings
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/cpp_dbc/backward.hpp
    PROPERTIES
    COMPILE_FLAGS "-Wno-undef -Wno-unused-parameter -Wno-shadow -Wno-conversion"
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
    ${CMAKE_CURRENT_BINARY_DIR}/test_db_connections.yml
    COPYONLY)

# Copy the test.jpg file directly to the directory where the executable is
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test.jpg
    ${CMAKE_CURRENT_BINARY_DIR}/test.jpg
    COPYONLY)

# We'll set environment variables in the run script instead