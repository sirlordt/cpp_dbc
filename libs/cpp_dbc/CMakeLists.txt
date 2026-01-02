cmake_minimum_required(VERSION 3.15)
project(cpp_dbc VERSION 0.1.0 LANGUAGES CXX)

# Options for conditional compilation
option(USE_MYSQL "Enable MySQL support" ON)
option(USE_POSTGRESQL "Enable PostgreSQL support" OFF)
option(USE_SQLITE "Enable SQLite support" OFF)
option(USE_FIREBIRD "Enable Firebird SQL support" OFF)
option(USE_MONGODB "Enable MongoDB support" OFF)
option(USE_REDIS "Enable Redis support" OFF)
option(USE_CPP_YAML "Enable YAML configuration support" OFF)
option(BACKWARD_HAS_DW "Enable libdw support for stack traces" ON)

# Thread safety option for database drivers
# Default is ON (thread-safe). Use -DDB_DRIVER_THREAD_SAFE=OFF to disable.
option(DB_DRIVER_THREAD_SAFE "Enable thread-safe database driver operations" ON)

# Debug options
option(DEBUG_CONNECTION_POOL "Enable debug output for ConnectionPool" OFF)
option(DEBUG_TRANSACTION_MANAGER "Enable debug output for TransactionManager" OFF)
option(DEBUG_SQLITE "Enable debug output for SQLite driver" OFF)
option(DEBUG_FIREBIRD "Enable debug output for Firebird driver" OFF)
option(DEBUG_MONGODB "Enable debug output for MongoDB driver" OFF)
option(DEBUG_REDIS "Enable debug output for Redis driver" OFF)
option(DEBUG_ALL "Enable debug output for all drivers and components" OFF)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add the cmake directory to the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Define the library
add_library(cpp_dbc
    src/core/relational/relational_db_connection_pool.cpp
    src/core/document/document_db_connection_pool.cpp
    src/core/kv/kv_db_connection_pool.cpp
    src/transaction_manager.cpp
    src/driver_manager.cpp
    src/config/database_config.cpp
    src/common/system_utils.cpp
)

# Conditionally add YAML config loader and database config implementation
if(USE_CPP_YAML)
    target_sources(cpp_dbc PRIVATE
        src/config/yaml_config_loader.cpp
    )
endif()

# Add database config implementation
# target_sources(cpp_dbc PRIVATE
# )

# Create an alias target for use in the same build tree
add_library(cpp_dbc::cpp_dbc ALIAS cpp_dbc)

# Add compile definitions based on options
target_compile_definitions(cpp_dbc PUBLIC
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
    $<$<BOOL:${USE_REDIS}>:USE_REDIS=1>
    $<$<NOT:$<BOOL:${USE_REDIS}>>:USE_REDIS=0>
    $<$<BOOL:${USE_CPP_YAML}>:USE_CPP_YAML=1>
    $<$<NOT:$<BOOL:${USE_CPP_YAML}>>:USE_CPP_YAML=0>
    $<$<BOOL:${DB_DRIVER_THREAD_SAFE}>:DB_DRIVER_THREAD_SAFE=1>
    $<$<NOT:$<BOOL:${DB_DRIVER_THREAD_SAFE}>>:DB_DRIVER_THREAD_SAFE=0>
    $<$<BOOL:${DEBUG_CONNECTION_POOL}>:DEBUG_CONNECTION_POOL=1>
    $<$<BOOL:${DEBUG_TRANSACTION_MANAGER}>:DEBUG_TRANSACTION_MANAGER=1>
    $<$<BOOL:${DEBUG_SQLITE}>:DEBUG_SQLITE=1>
    $<$<BOOL:${DEBUG_FIREBIRD}>:DEBUG_FIREBIRD=1>
    $<$<BOOL:${DEBUG_MONGODB}>:DEBUG_MONGODB=1>
    $<$<BOOL:${DEBUG_REDIS}>:DEBUG_REDIS=1>
    $<$<BOOL:${DEBUG_ALL}>:DEBUG_ALL=1>
    $<$<BOOL:${BACKWARD_HAS_DW}>:BACKWARD_HAS_DW=1>
    $<$<NOT:$<BOOL:${BACKWARD_HAS_DW}>>:BACKWARD_HAS_DW=0>
)

# Include directories
target_include_directories(cpp_dbc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Add include directory for source files
target_include_directories(cpp_dbc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Exclude backward.hpp from warnings
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cpp_dbc/backward.hpp
    PROPERTIES
    COMPILE_FLAGS "-Wno-undef -Wno-unused-parameter -Wno-shadow -Wno-conversion"
)

# Define macros for backward.hpp to silence -Wundef warnings
target_compile_definitions(cpp_dbc PUBLIC
    BACKWARD_HAS_UNWIND=0
    BACKWARD_HAS_LIBUNWIND=0
    BACKWARD_HAS_BACKTRACE=0
    BACKWARD_HAS_BFD=0
    BACKWARD_HAS_DWARF=0
    BACKWARD_HAS_BACKTRACE_SYMBOL=0
    BACKWARD_HAS_PDB_SYMBOL=0
)

# Conditionally add MySQL driver files and dependencies
if(USE_MYSQL)
    target_sources(cpp_dbc PRIVATE
        src/drivers/relational/driver_mysql.cpp
    )

    # Find MySQL package
    find_package(MySQL REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${MYSQL_INCLUDE_DIR})
    target_link_libraries(cpp_dbc PUBLIC ${MYSQL_LIBRARIES})
endif()

# Conditionally add PostgreSQL driver files and dependencies
if(USE_POSTGRESQL)
    target_sources(cpp_dbc PRIVATE
        src/drivers/relational/driver_postgresql.cpp
    )

    # Find PostgreSQL package
    find_package(PostgreSQL REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${PostgreSQL_INCLUDE_DIRS})
    target_link_libraries(cpp_dbc PUBLIC ${PostgreSQL_LIBRARIES})
endif()

# Conditionally add SQLite driver files and dependencies
if(USE_SQLITE)
    target_sources(cpp_dbc PRIVATE
        src/drivers/relational/driver_sqlite.cpp
    )

    # Find SQLite package
    find_package(SQLite3 REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(cpp_dbc PUBLIC ${SQLite3_LIBRARIES})
endif()

# Conditionally add Firebird driver files and dependencies
if(USE_FIREBIRD)
    target_sources(cpp_dbc PRIVATE
        src/drivers/relational/driver_firebird.cpp
    )

    # Find Firebird package
    # Required system package: firebird-dev (Debian/Ubuntu) or firebird-devel (RHEL/Fedora)
    # Install with: sudo apt-get install firebird-dev libfbclient2
    find_package(Firebird REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${FIREBIRD_INCLUDE_DIRS})
    target_link_libraries(cpp_dbc PUBLIC ${FIREBIRD_LIBRARIES})
endif()

# Conditionally add MongoDB driver files and dependencies
if(USE_MONGODB)
    target_sources(cpp_dbc PRIVATE
        src/drivers/document/driver_mongodb.cpp
    )

    # Find MongoDB C driver package
    # Required system package: libmongoc-dev libbson-dev (Debian/Ubuntu) or mongo-c-driver-devel (RHEL/Fedora)
    # Install with: sudo apt-get install libmongoc-dev libbson-dev
    find_package(MongoDB REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${MONGODB_INCLUDE_DIRS})
    target_link_libraries(cpp_dbc PUBLIC ${MONGODB_LIBRARIES})
endif()

# Conditionally add Redis driver files and dependencies
if(USE_REDIS)
    target_sources(cpp_dbc PRIVATE
        src/drivers/kv/driver_redis.cpp
    )

    # Find Redis package (hiredis)
    # Required system package: libhiredis-dev (Debian/Ubuntu) or hiredis-devel (RHEL/Fedora)
    # Install with: sudo apt-get install libhiredis-dev
    # Force the use of our custom FindHiredis.cmake module
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF)
    find_package(Hiredis REQUIRED MODULE)
    target_include_directories(cpp_dbc PUBLIC ${HIREDIS_INCLUDE_DIRS})
    target_link_libraries(cpp_dbc PUBLIC ${HIREDIS_LIBRARIES})
endif()

# Conditionally add YAML-CPP dependencies
if(USE_CPP_YAML)
    # Find YAML-CPP package
    # First try to find it using the config file provided by Conan
    if(EXISTS "${CMAKE_BINARY_DIR}/generators/yaml-cpp-config.cmake")
        set(yaml-cpp_DIR "${CMAKE_BINARY_DIR}/generators")
    elseif(EXISTS "${CMAKE_BINARY_DIR}/../../${CMAKE_BUILD_TYPE}/generators/yaml-cpp-config.cmake")
        set(yaml-cpp_DIR "${CMAKE_BINARY_DIR}/../../${CMAKE_BUILD_TYPE}/generators")
    endif()

    find_package(yaml-cpp REQUIRED)
    target_link_libraries(cpp_dbc PUBLIC yaml-cpp::yaml-cpp)
endif()

# Add libdw support for stack traces if enabled
if(BACKWARD_HAS_DW)
    # Find libdw (part of elfutils)
    find_library(LIBDW_LIBRARY NAMES dw)

    if(LIBDW_LIBRARY)
        message(STATUS "Found libdw: ${LIBDW_LIBRARY}")
        target_link_libraries(cpp_dbc PUBLIC ${LIBDW_LIBRARY})
    else()
        message(WARNING "libdw not found. Stack traces may not show detailed information.")
        set(BACKWARD_HAS_DW OFF)

        # Update the compile definition
        target_compile_definitions(cpp_dbc PUBLIC BACKWARD_HAS_DW=0)
    endif()
endif()

# Generate and install export targets
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Installation rules
install(TARGETS cpp_dbc
    EXPORT cpp_dbc-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install header files
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

# Install the export targets
install(EXPORT cpp_dbc-targets
    FILE cpp_dbc-targets.cmake
    NAMESPACE cpp_dbc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)

# Create the config file
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cpp_dbc-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_dbc-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)

# Create the version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_dbc-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install the config and version files
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_dbc-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cpp_dbc-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)

# Install the Find*.cmake modules
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindMySQL.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindPostgreSQL.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindSQLite3.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindFirebird.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindMongoDB.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindHiredis.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)

# Add tests subdirectory if testing is enabled
option(CPP_DBC_BUILD_TESTS "Build cpp_dbc tests" OFF)

if(CPP_DBC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Add benchmarks subdirectory if benchmarking is enabled
option(CPP_DBC_BUILD_BENCHMARKS "Build cpp_dbc benchmarks" OFF)

if(CPP_DBC_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

# Add examples
option(CPP_DBC_BUILD_EXAMPLES "Build cpp_dbc examples" OFF)

if(CPP_DBC_BUILD_EXAMPLES)
    # Basic example
    add_executable(cpp_dbc_example examples/example.cpp)
    target_link_libraries(cpp_dbc_example PRIVATE cpp_dbc)

    # Connection pool example
    add_executable(connection_pool_example examples/connection_pool_example.cpp)
    target_link_libraries(connection_pool_example PRIVATE cpp_dbc)

    # Transaction manager example
    add_executable(transaction_manager_example examples/transaction_manager_example.cpp)
    target_link_libraries(transaction_manager_example PRIVATE cpp_dbc)

    # Configuration example
    add_executable(config_example examples/config_example.cpp)
    target_link_libraries(config_example PRIVATE cpp_dbc)

    # Configuration integration example
    add_executable(config_integration_example examples/config_integration_example.cpp)
    target_link_libraries(config_integration_example PRIVATE cpp_dbc)

    # JSON operations example
    add_executable(json_operations_example examples/json_operations_example.cpp)
    target_link_libraries(json_operations_example PRIVATE cpp_dbc)

    # BLOB operations example
    add_executable(blob_operations_example examples/blob_operations_example.cpp)
    target_link_libraries(blob_operations_example PRIVATE cpp_dbc)

    # JOIN operations example
    add_executable(join_operations_example examples/join_operations_example.cpp)
    target_link_libraries(join_operations_example PRIVATE cpp_dbc)

    # Batch operations example
    add_executable(batch_operations_example examples/batch_operations_example.cpp)
    target_link_libraries(batch_operations_example PRIVATE cpp_dbc)

    # Error handling example
    add_executable(error_handling_example examples/error_handling_example.cpp)
    target_link_libraries(error_handling_example PRIVATE cpp_dbc)

    # Connection info example
    add_executable(connection_info_example examples/connection_info_example.cpp)
    target_link_libraries(connection_info_example PRIVATE cpp_dbc)

    # Firebird examples (only if Firebird is enabled)
    if(USE_FIREBIRD)
        add_executable(firebird_reserved_word_example examples/firebird_reserved_word_example.cpp)
        target_link_libraries(firebird_reserved_word_example PRIVATE cpp_dbc)

        add_executable(firebird_example examples/firebird_example.cpp)
        target_link_libraries(firebird_example PRIVATE cpp_dbc)
    endif()

    # MongoDB examples (only if MongoDB is enabled)
    if(USE_MONGODB)
        add_executable(mongodb_example examples/mongodb_example.cpp)
        target_link_libraries(mongodb_example PRIVATE cpp_dbc)

        # Document database connection pool example
        add_executable(document_connection_pool_example examples/document_connection_pool_example.cpp)
        target_link_libraries(document_connection_pool_example PRIVATE cpp_dbc)
    endif()

    # Redis examples (only if Redis is enabled)
    if(USE_REDIS)
        add_executable(redis_example examples/redis_example.cpp)
        target_link_libraries(redis_example PRIVATE cpp_dbc)

        # Key-value database connection pool example
        add_executable(kv_connection_pool_example examples/kv_connection_pool_example.cpp)
        target_link_libraries(kv_connection_pool_example PRIVATE cpp_dbc)
    endif()

    # Copy necessary files for examples
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/examples/example_config.yml
        ${CMAKE_CURRENT_BINARY_DIR}/example_config.yml COPYONLY)

    # Copy test.jpg for BLOB example
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/test/test.jpg
        ${CMAKE_CURRENT_BINARY_DIR}/test.jpg COPYONLY)
endif()