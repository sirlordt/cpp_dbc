# CPPDBC - C++ Database Connectivity Library

This document contains information about the CPPDBC library, inspired by JDBC but for C++. It allows you to connect and work with MySQL, PostgreSQL, SQLite, Firebird, and MongoDB databases using a unified interface. The library includes connection pooling, transaction management with different isolation levels, support for YAML configuration, enhanced stack trace capabilities with libdw, and comprehensive benchmarking capabilities for all supported database types.

## File Structure

### Header Files
- `include/cpp_dbc/cpp_dbc.hpp` - Main library interfaces and classes
- `include/cpp_dbc/core/relational/relational_db_connection.hpp` - Relational database connection interface
- `include/cpp_dbc/core/relational/relational_db_driver.hpp` - Relational database driver interface
- `include/cpp_dbc/core/relational/relational_db_prepared_statement.hpp` - Relational prepared statement interface
- `include/cpp_dbc/core/relational/relational_db_result_set.hpp` - Relational result set interface
- `include/cpp_dbc/core/document/document_db_connection.hpp` - Document database connection interface
- `include/cpp_dbc/core/document/document_db_driver.hpp` - Document database driver interface
- `include/cpp_dbc/core/document/document_db_collection.hpp` - Document database collection interface
- `include/cpp_dbc/core/document/document_db_connection_pool.hpp` - Document database connection pool interface
- `include/cpp_dbc/core/document/document_db_cursor.hpp` - Document database cursor interface
- `include/cpp_dbc/core/document/document_db_data.hpp` - Document database data interface
- `include/cpp_dbc/drivers/relational/driver_mysql.hpp` - MySQL-specific definitions
- `include/cpp_dbc/drivers/relational/driver_postgresql.hpp` - PostgreSQL-specific definitions
- `include/cpp_dbc/drivers/relational/driver_sqlite.hpp` - SQLite-specific definitions
- `include/cpp_dbc/drivers/relational/driver_firebird.hpp` - Firebird-specific definitions
- `include/cpp_dbc/drivers/document/driver_mongodb.hpp` - MongoDB-specific definitions
- `include/cpp_dbc/drivers/relational/mysql_blob.hpp` - MySQL BLOB implementation
- `include/cpp_dbc/drivers/relational/postgresql_blob.hpp` - PostgreSQL BLOB implementation
- `include/cpp_dbc/drivers/relational/sqlite_blob.hpp` - SQLite BLOB implementation
- `include/cpp_dbc/drivers/relational/firebird_blob.hpp` - Firebird BLOB implementation
- `include/cpp_dbc/core/db_connection_pool.hpp` - Generic connection pool interface for all database types
- `include/cpp_dbc/core/pooled_db_connection.hpp` - Generic pooled connection interface for all database types
- `include/cpp_dbc/core/relational/relational_db_connection_pool.hpp` - Relational database connection pool implementation
- `include/cpp_dbc/transaction_manager.hpp` - Transaction manager for cross-thread transactions
- `include/cpp_dbc/backward.hpp` - Stack trace capture and analysis
- `include/cpp_dbc/common/system_utils.hpp` - System utilities including stack trace functions
- `include/cpp_dbc/config/database_config.hpp` - Database configuration classes
- `include/cpp_dbc/config/yaml_config_loader.hpp` - YAML configuration loader

### Implementation Files
- `src/drivers/relational/driver_mysql.cpp` - MySQL implementation using libmysqlclient
- `src/drivers/relational/driver_postgresql.cpp` - PostgreSQL implementation using libpq
- `src/drivers/relational/driver_sqlite.cpp` - SQLite implementation using libsqlite3
- `src/drivers/relational/driver_firebird.cpp` - Firebird implementation using libfbclient
- `src/drivers/document/driver_mongodb.cpp` - MongoDB implementation using libmongocxx
- `src/core/document/document_db_connection_pool.cpp` - Document database connection pool implementation
- `benchmark/benchmark_mongodb_select.cpp` - MongoDB select operation benchmarks
- `benchmark/benchmark_mongodb_insert.cpp` - MongoDB insert operation benchmarks
- `benchmark/benchmark_mongodb_update.cpp` - MongoDB update operation benchmarks
- `benchmark/benchmark_mongodb_delete.cpp` - MongoDB delete operation benchmarks
- `test/test_mongodb_real.cpp` - MongoDB integration tests
- `test/test_mongodb_real_blob.cpp` - MongoDB BLOB handling tests
- `test/test_mongodb_real_json.cpp` - MongoDB JSON operation tests
- `test/test_mongodb_real_join.cpp` - MongoDB join operation tests
- `test/test_mongodb_thread_safe.cpp` - MongoDB thread safety tests
- `src/core/relational/relational_db_connection_pool.cpp` - Relational database connection pool implementation
- `src/transaction_manager.cpp` - Transaction manager implementation
- `src/driver_manager.cpp` - Driver manager implementation
- `src/common/system_utils.cpp` - System utilities implementation including stack trace functions
- `src/config/yaml_config_loader.cpp` - YAML configuration loader implementation

### Examples
- `examples/example.cpp` - Basic usage example with MySQL, PostgreSQL, and SQLite
- `examples/connection_pool_example.cpp` - Relational database connection pool usage example
- `examples/document_connection_pool_example.cpp` - Document database connection pool usage example with MongoDB
- `examples/transaction_manager_example.cpp` - Cross-thread transaction example
- `examples/config_example.cpp` - YAML configuration example
- `examples/config_integration_example.cpp` - Database configuration integration example
- `examples/example_config.yml` - Example YAML configuration file

## Building

To build the library and examples, you'll need:

1. A modern C++ compiler with C++23 support
2. MySQL development libraries (libmysqlclient-dev)
3. PostgreSQL development libraries (libpq-dev) (optional)
4. SQLite development libraries (libsqlite3-dev) (optional)
5. Firebird development libraries (libfbclient2, firebird-dev) (optional)
6. MongoDB C++ Driver (libmongoc-dev, libbson-dev, libmongocxx-dev, libbsoncxx-dev) (optional)
7. yaml-cpp development libraries (optional)
8. libdw development libraries (part of elfutils, optional)
9. CMake 3.15 or later
10. Conan for dependency management

### Using the Build Scripts

```bash
# Build with MySQL support only (default)
./build.sh

# Build with MySQL and PostgreSQL support
./build.sh --mysql --postgres

# Build with PostgreSQL support only
./build.sh --mysql-off --postgres

# Build with SQLite support
./build.sh --sqlite

# Build with Firebird support
./build.sh --firebird

# Build with MongoDB support
./build.sh --mongodb

# Build with all database drivers
./build.sh --mysql --postgres --sqlite --firebird --mongodb

# Build with YAML configuration support
./build.sh --yaml

# Build examples
./build.sh --examples

# Build tests
./build.sh --test

# Build in Release mode
./build.sh --release

# Disable libdw support for stack traces
./build.sh --dw-off

# Build Docker image
./build.dist.sh

# Build Docker image with PostgreSQL and YAML support
./build.dist.sh --postgres --yaml

# Build Docker image with SQLite support
./build.dist.sh --sqlite

# Build Docker image with Firebird support
./build.dist.sh --firebird

# Build Docker image with MongoDB support
./build.dist.sh --mongodb

# Build Docker image with all database drivers
./build.dist.sh --mysql --postgres --sqlite --firebird --mongodb --yaml

# Build Docker image without libdw support
./build.dist.sh --dw-off

# Build distribution packages (.deb and .rpm)
./libs/cpp_dbc/build_dist_pkg.sh

# Build packages for multiple distributions
./libs/cpp_dbc/build_dist_pkg.sh --distro=ubuntu:24.04+ubuntu:22.04+debian:12+debian:13+fedora:42+fedora:43

# Build packages with specific options
./libs/cpp_dbc/build_dist_pkg.sh --build=yaml,mysql,postgres,sqlite,firebird,debug,dw,examples

# Build packages with specific version
./libs/cpp_dbc/build_dist_pkg.sh --version=1.0.1
```

The build script:
1. Checks for and installs required dependencies
2. Configures the library with CMake
3. Builds and installs the library to the `build/libs/cpp_dbc` directory
4. Builds the main application

The `build.dist.sh` script:
1. Builds the project with the specified options
2. Automatically detects shared library dependencies
3. Generates a Dockerfile with only the necessary dependencies
4. Builds and tags the Docker image

The `build_dist_pkg.sh` script:
1. Creates Docker containers based on target distributions
2. Builds the cpp_dbc library with specified options
3. Creates distribution packages (.deb for Debian/Ubuntu, .rpm for Fedora)
4. Copies the packages to the build directory

### Manual Build with CMake

```bash
# Create build directory for the library
mkdir -p libs/cpp_dbc/build
cd libs/cpp_dbc/build

# Configure with CMake (MySQL enabled, PostgreSQL, SQLite, and Firebird disabled, libdw enabled)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_FIREBIRD=OFF -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configure with Firebird support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=OFF -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_FIREBIRD=ON -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configure without libdw support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=OFF -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Build and install
cmake --build . --target install

# Return to root directory
cd ../../..

# Create build directory for the main application
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

# Build
cmake --build .
```

## Main Features

1. Unified interface for MySQL, PostgreSQL, SQLite, Firebird, and MongoDB
2. Connection, query, and result set management
3. Prepared statement support
4. Thread-safe connection pool
5. Cross-thread transaction manager with UUIDs
6. Transaction isolation levels (JDBC-compatible: READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE)
7. Enhanced stack trace capture and analysis with libdw support
8. YAML configuration support
9. Docker container generation with automatic dependency detection
10. Comprehensive warning flags and compile-time checks for code quality
11. Smart pointer-based resource management with custom deleters for all database drivers
12. Full BLOB support for all relational database drivers including Firebird
13. JOIN operations support (INNER, LEFT, RIGHT, FULL OUTER) for all relational drivers
14. JSON operations support for databases that support it
15. Connection pool memory safety with smart pointers for pool lifetime tracking
16. Document database support with MongoDB driver (CRUD operations, collection management, cursor-based iteration)
17. Comprehensive benchmarking infrastructure for performance testing of all database operations
18. Document database connection pool implementation:
    - Document database connection pooling for MongoDB
    - Thread-safe connection acquisition and release
    - Smart pointer-based pool lifetime tracking for memory safety
    - Connection validation with MongoDB ping command
    - Configurable pool parameters (initial size, max size, min idle, etc.)
    - Background maintenance thread for connection cleanup
    - Comprehensive example in document_connection_pool_example.cpp
    - Thorough test coverage for all pool operations
19. MongoDB benchmark support for select, insert, update, and delete operations with detailed performance metrics:
    - Document insert benchmarks (single and batch operations)
    - Document query benchmarks with different filter complexities
    - Document update benchmarks with various update operators
    - Document delete benchmarks (single and multiple document operations)
    - Memory usage tracking for MongoDB operations
    - Performance comparison against baseline measurements
    - Support for different document sizes and collection scales
20. Extensive testing suite for MongoDB including:
    - JSON operations (basic, nested, arrays)
    - JSON query operators ($eq, $gt, $lt, etc.)
    - JSON updates and modifications
    - JSON aggregation operations
    - Thread safety with multiple connections
    - Thread safety with shared connection
    - Join operations using MongoDB aggregation pipeline
20. Memory usage tracking for all database operations in benchmarks

### Code Quality Features

The library is built with comprehensive warning flags and compile-time checks:

```cmake
# Warning flags used by the library
-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wformat=2 -Wunused
-Werror=return-type -Werror=switch -Wdouble-promotion -Wfloat-equal -Wundef
-Wpointer-arith -Wcast-align
```

These warning flags help catch potential issues:

- `-Wall -Wextra -Wpedantic`: Standard warning flags
- `-Wconversion`: Warns about implicit conversions that may change a value
- `-Wshadow`: Warns when a variable declaration shadows another variable
- `-Wcast-qual`: Warns about casts that remove type qualifiers
- `-Wformat=2`: Enables additional format string checks
- `-Wunused`: Warns about unused variables and functions
- `-Werror=return-type`: Treats missing return statements as errors
- `-Werror=switch`: Treats switch statement issues as errors
- `-Wdouble-promotion`: Warns about implicit float to double promotions
- `-Wfloat-equal`: Warns about floating-point equality comparisons
- `-Wundef`: Warns about undefined identifiers in preprocessor expressions
- `-Wpointer-arith`: Warns about suspicious pointer arithmetic
- `-Wcast-align`: Warns about pointer casts that increase alignment requirements

Code quality improvements include:
- Using m_ prefix for member variables to avoid -Wshadow warnings
- Adding static_cast<> for numeric conversions to avoid -Wconversion warnings
- Changing int return types to uint64_t for executeUpdate() methods
- Improving exception handling to avoid variable shadowing

## Dependencies

- libmysqlclient for MySQL connections
- libpq for PostgreSQL connections (optional)
- libsqlite3 for SQLite connections (optional)
- libfbclient for Firebird connections (optional)
- libmongocxx/libbsoncxx for MongoDB connections (optional)
- yaml-cpp for YAML configuration support (optional)
- libdw (part of elfutils) for enhanced stack traces (optional)
- C++23 standard library for threads, mutexes, and condition variables
- CMake for build system
- Conan for dependency management

## Project Structure

```
cpp_dbc/
├── libs/
│   └── cpp_dbc/
│       ├── include/
│       │   └── cpp_dbc/
│       │       ├── cpp_dbc.hpp
│       │       ├── connection_pool.hpp
│       │       ├── transaction_manager.hpp
│       │       ├── blob.hpp
│       │       ├── config/
│       │       │   ├── database_config.hpp
│       │       │   └── yaml_config_loader.hpp
│       │       ├── core/
│       │       │   ├── db_connection.hpp
│       │       │   ├── db_driver.hpp
│       │       │   ├── db_exception.hpp
│       │       │   ├── db_result_set.hpp
│       │       │   ├── db_types.hpp
│       │       │   ├── relational/
│       │       │   │   ├── relational_db_connection.hpp
│       │       │   │   ├── relational_db_driver.hpp
│       │       │   │   ├── relational_db_prepared_statement.hpp
│       │       │   │   └── relational_db_result_set.hpp
│       │       │   ├── columnar/
│       │       │   ├── document/
│       │       │   │   ├── document_db_collection.hpp
│       │       │   │   ├── document_db_connection.hpp
│       │       │   │   ├── document_db_cursor.hpp
│       │       │   │   ├── document_db_data.hpp
│       │       │   │   └── document_db_driver.hpp
│       │       │   ├── graph/
│       │       │   ├── kv/
│       │       │   └── timeseries/
│       │       └── drivers/
│       │           ├── relational/
│       │           │   ├── driver_mysql.hpp
│       │           │   ├── driver_postgresql.hpp
│       │           │   ├── driver_sqlite.hpp
│       │           │   ├── driver_firebird.hpp
│       │           │   ├── mysql_blob.hpp
│       │           │   ├── postgresql_blob.hpp
│       │           │   ├── sqlite_blob.hpp
│       │           │   └── firebird_blob.hpp
│       │           ├── columnar/
│       │           ├── document/
│       │           │   └── driver_mongodb.hpp
│       │           ├── graph/
│       │           ├── kv/
│       │           └── timeseries/
│       ├── src/
│       │   ├── connection_pool.cpp
│       │   ├── transaction_manager.cpp
│       │   ├── driver_manager.cpp
│       │   ├── config/
│       │   │   └── yaml_config_loader.cpp
│       │   └── drivers/
│       │       ├── relational/
│       │       │   ├── driver_mysql.cpp
│       │       │   ├── driver_postgresql.cpp
│       │       │   ├── driver_sqlite.cpp
│       │       │   └── driver_firebird.cpp
│       │       └── document/
│       │           └── driver_mongodb.cpp
│       ├── examples/
│       │   ├── example.cpp
│       │   ├── connection_pool_example.cpp
│       │   ├── transaction_manager_example.cpp
│       │   ├── config_example.cpp
│       │   ├── example_config.yml
│       │   └── run_config_example.sh
│       ├── test/
│       │   ├── test_basic.cpp
│       │   ├── test_db_config.cpp
│       │   ├── test_yaml.cpp
│       │   ├── test_main.cpp
│       │   ├── test_db_connections.yml
│       │   ├── test_sqlite_connection.cpp
│       │   └── test_sqlite_real.cpp
│       ├── cmake/
│       │   ├── cpp_dbc-config.cmake.in
│       │   ├── FindMySQL.cmake
│       │   ├── FindPostgreSQL.cmake
│       │   └── FindSQLite3.cmake
│       ├── CMakeLists.txt
│       ├── build_cpp_dbc.sh
│       ├── build_test_cpp_dbc.sh
│       └── run_test_cpp_dbc.sh
├── src/
│   └── main.cpp
├── tests/
│   ├── catch_main.cpp
│   ├── catch_test.cpp
│   ├── json_test.cpp
│   └── main_test.cpp
├── build/
│   └── libs/
│       └── cpp_dbc/
├── CMakeLists.txt
├── build.sh
├── build.dist.sh
├── helper.sh
└── run_test.sh
