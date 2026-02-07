cmake_minimum_required(VERSION 3.15)
project(cpp_dbc_tests LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options for conditional compilation (inherited from parent project)
option(USE_MYSQL "Enable MySQL support" ON)
option(USE_POSTGRESQL "Enable PostgreSQL support" OFF)
option(USE_SQLITE "Enable SQLite support" OFF)
option(USE_FIREBIRD "Enable Firebird SQL support" OFF)
option(USE_MONGODB "Enable MongoDB support" OFF)
option(USE_SCYLLADB "Enable ScyllaDB support" OFF)
option(USE_REDIS "Enable Redis support" OFF)
option(USE_CPP_YAML "Enable YAML configuration support" OFF)

# Thread safety option for database drivers
option(DB_DRIVER_THREAD_SAFE "Enable thread-safe database driver operations" ON)

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

    # Core test files (common/)
    common/10_000_test_main.cpp
    common/10_001_test_database_config.cpp
    common/10_011_test_db_config.cpp
    common/10_021_test_db_connection_pool_config.cpp
    common/10_031_test_db_exception.cpp
    common/10_041_test_expected.cpp
    common/10_051_test_yaml.cpp
    common/10_061_test_integration.cpp

    # MySQL tests (relational/mysql/)
    relational/mysql/20_001_test_mysql_real_common.cpp
    relational/mysql/20_011_test_mysql_real_db_config.cpp
    relational/mysql/20_021_test_mysql_real_driver.cpp
    relational/mysql/20_031_test_mysql_real.cpp
    relational/mysql/20_041_test_mysql_real_connection.cpp
    relational/mysql/20_051_test_mysql_real_json.cpp
    relational/mysql/20_061_test_mysql_real_blob.cpp
    relational/mysql/20_071_test_mysql_real_inner_join.cpp
    relational/mysql/20_081_test_mysql_real_left_join.cpp
    relational/mysql/20_091_test_mysql_real_right_join.cpp
    relational/mysql/20_101_test_mysql_real_full_join.cpp
    relational/mysql/20_111_test_mysql_real_thread_safe.cpp
    relational/mysql/20_121_test_mysql_real_transaction_isolation.cpp
    relational/mysql/20_131_test_mysql_real_transaction_manager.cpp
    relational/mysql/20_141_test_mysql_real_connection_pool.cpp

    # PostgreSQL tests (relational/postgresql/)
    relational/postgresql/21_001_test_postgresql_real_common.cpp
    relational/postgresql/21_011_test_postgresql_real_db_config.cpp
    relational/postgresql/21_021_test_postgresql_real_driver.cpp
    relational/postgresql/21_031_test_postgresql_real.cpp
    relational/postgresql/21_041_test_postgresql_real_connection.cpp
    relational/postgresql/21_051_test_postgresql_real_json.cpp
    relational/postgresql/21_061_test_postgresql_real_blob.cpp
    relational/postgresql/21_071_test_postgresql_real_inner_join.cpp
    relational/postgresql/21_081_test_postgresql_real_left_join.cpp
    relational/postgresql/21_091_test_postgresql_real_right_join.cpp
    relational/postgresql/21_101_test_postgresql_real_full_join.cpp
    relational/postgresql/21_111_test_postgresql_real_thread_safe.cpp
    relational/postgresql/21_121_test_postgresql_real_transaction_isolation.cpp
    relational/postgresql/21_131_test_postgresql_real_transaction_manager.cpp
    relational/postgresql/21_141_test_postgresql_real_connection_pool.cpp

    # SQLite tests (relational/sqlite/)
    relational/sqlite/22_001_test_sqlite_real_common.cpp
    relational/sqlite/22_011_test_sqlite_real_db_config.cpp
    relational/sqlite/22_021_test_sqlite_real_driver.cpp
    relational/sqlite/22_031_test_sqlite_real.cpp
    relational/sqlite/22_041_test_sqlite_real_connection.cpp
    # relational/sqlite/22_051_test_sqlite_real_json.cpp
    relational/sqlite/22_061_test_sqlite_real_blob.cpp
    relational/sqlite/22_071_test_sqlite_real_inner_join.cpp
    relational/sqlite/22_081_test_sqlite_real_left_join.cpp
    # relational/sqlite/22_091_test_sqlite_real_right_join.cpp
    # relational/sqlite/22_101_test_sqlite_real_full_join.cpp
    relational/sqlite/22_111_test_sqlite_real_thread_safe.cpp
    # relational/sqlite/22_121_test_sqlite_real_transaction_isolation.cpp
    relational/sqlite/22_131_test_sqlite_real_transaction_manager.cpp
    relational/sqlite/22_141_test_sqlite_real_connection_pool.cpp

    # Firebird tests (relational/firebird/)
    relational/firebird/23_001_test_firebird_real_common.cpp
    relational/firebird/23_011_test_firebird_real_db_config.cpp
    relational/firebird/23_021_test_firebird_real_driver.cpp
    relational/firebird/23_031_test_firebird_real.cpp
    relational/firebird/23_041_test_firebird_real_connection.cpp
    relational/firebird/23_051_test_firebird_real_json.cpp
    relational/firebird/23_061_test_firebird_real_blob.cpp
    relational/firebird/23_071_test_firebird_real_inner_join.cpp
    relational/firebird/23_081_test_firebird_real_left_join.cpp
    relational/firebird/23_091_test_firebird_real_right_join.cpp
    relational/firebird/23_101_test_firebird_real_full_join.cpp
    relational/firebird/23_111_test_firebird_real_thread_safe.cpp
    relational/firebird/23_121_test_firebird_real_transaction_isolation.cpp
    relational/firebird/23_131_test_firebird_real_transaction_manager.cpp
    relational/firebird/23_141_test_firebird_real_connection_pool.cpp

    # Redis tests (kv/redis/)
    kv/redis/24_001_test_redis_real_common.cpp
    # kv/redis/24_011_test_redis_real_db_config.cpp
    kv/redis/24_021_test_redis_real_driver.cpp
    # kv/redis/24_031_test_redis_real.cpp
    kv/redis/24_041_test_redis_real_connection.cpp
    # kv/redis/24_051_test_redis_real_json.cpp
    # kv/redis/24_061_test_redis_real_blob.cpp
    # kv/redis/24_071_test_redis_real_inner_join.cpp
    # kv/redis/24_081_test_redis_real_left_join.cpp
    # kv/redis/24_091_test_redis_real_right_join.cpp
    # kv/redis/24_101_test_redis_real_full_join.cpp
    # kv/redis/24_111_test_redis_real_thread_safe.cpp
    # kv/redis/24_121_test_redis_real_transaction_isolation.cpp
    # kv/redis/24_131_test_redis_real_transaction_manager.cpp
    kv/redis/24_141_test_redis_real_connection_pool.cpp

    # MongoDB tests (document/mongodb/)
    document/mongodb/25_001_test_mongodb_real_common.cpp
    document/mongodb/25_011_test_mongodb_real_db_config.cpp
    document/mongodb/25_021_test_mongodb_real_driver.cpp
    document/mongodb/25_031_test_mongodb_real.cpp
    document/mongodb/25_041_test_mongodb_real_connection.cpp
    document/mongodb/25_051_test_mongodb_real_json.cpp
    document/mongodb/25_061_test_mongodb_real_blob.cpp
    document/mongodb/25_071_test_mongodb_real_inner_join.cpp
    document/mongodb/25_081_test_mongodb_real_left_join.cpp
    document/mongodb/25_091_test_mongodb_real_right_join.cpp
    document/mongodb/25_101_test_mongodb_real_full_join.cpp
    document/mongodb/25_111_test_mongodb_real_thread_safe.cpp
    # document/mongodb/25_121_test_mongodb_real_transaction_isolation.cpp
    # document/mongodb/25_131_test_mongodb_real_transaction_manager.cpp
    document/mongodb/25_141_test_mongodb_real_connection_pool.cpp

    # Exclusive tests for MongoDB (document-specific APIs)
    document/mongodb/25_521_test_mongodb_real_cursor_api.cpp

    # ScyllaDB tests (columnar/scylladb/)
    columnar/scylladb/26_001_test_scylladb_real_common.cpp
    columnar/scylladb/26_011_test_scylladb_real_db_config.cpp
    columnar/scylladb/26_021_test_scylladb_real_driver.cpp
    columnar/scylladb/26_031_test_scylladb_real.cpp
    columnar/scylladb/26_041_test_scylladb_real_connection.cpp
    columnar/scylladb/26_051_test_scylladb_real_json.cpp
    columnar/scylladb/26_061_test_scylladb_real_blob.cpp
    columnar/scylladb/26_071_test_scylladb_real_inner_join.cpp
    columnar/scylladb/26_081_test_scylladb_real_left_join.cpp
    columnar/scylladb/26_091_test_scylladb_real_right_join.cpp
    columnar/scylladb/26_101_test_scylladb_real_full_join.cpp
    columnar/scylladb/26_111_test_scylladb_real_thread_safe.cpp
    # columnar/scylladb/26_121_test_scylladb_real_transaction_isolation.cpp
    # columnar/scylladb/26_131_test_scylladb_real_transaction_manager.cpp
    columnar/scylladb/26_141_test_scylladb_real_connection_pool.cpp
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

# Conditionally link against Hiredis for Redis tests
if(USE_REDIS)
    target_link_libraries(cpp_dbc_tests PRIVATE
        ${HIREDIS_LIBRARIES}
    )
    target_include_directories(cpp_dbc_tests PRIVATE
        ${HIREDIS_INCLUDE_DIRS}
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
    $<$<BOOL:${USE_SCYLLADB}>:USE_SCYLLADB=1>
    $<$<NOT:$<BOOL:${USE_SCYLLADB}>>:USE_SCYLLADB=0>
    $<$<BOOL:${USE_REDIS}>:USE_REDIS=1>
    $<$<NOT:$<BOOL:${USE_REDIS}>>:USE_REDIS=0>
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
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/mysql
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/postgresql
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/sqlite
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/firebird
    ${CMAKE_CURRENT_SOURCE_DIR}/kv/redis
    ${CMAKE_CURRENT_SOURCE_DIR}/document/mongodb
    ${CMAKE_CURRENT_SOURCE_DIR}/columnar/scylladb
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
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/common/test_db_connections.yml
    ${CMAKE_CURRENT_BINARY_DIR}/test_db_connections.yml
    COPYONLY)

# Copy the test.jpg file directly to the directory where the executable is
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/common/test.jpg
    ${CMAKE_CURRENT_BINARY_DIR}/test.jpg
    COPYONLY)

# We'll set environment variables in the run script instead
