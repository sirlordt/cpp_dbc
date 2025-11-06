# CPPDBC - C++ Database Connectivity Library

This document contains information about the CPPDBC library, inspired by JDBC but for C++. It allows you to connect and work with MySQL, PostgreSQL, and SQLite databases using a unified interface. The library includes connection pooling, transaction management with different isolation levels, and support for YAML configuration.

## File Structure

### Header Files
- `include/cpp_dbc/cpp_dbc.hpp` - Main library interfaces and classes
- `include/cpp_dbc/drivers/driver_mysql.hpp` - MySQL-specific definitions
- `include/cpp_dbc/drivers/driver_postgresql.hpp` - PostgreSQL-specific definitions
- `include/cpp_dbc/drivers/driver_sqlite.hpp` - SQLite-specific definitions
- `include/cpp_dbc/connection_pool.hpp` - Connection pool with thread-safety support
- `include/cpp_dbc/transaction_manager.hpp` - Transaction manager for cross-thread transactions
- `include/cpp_dbc/config/database_config.hpp` - Database configuration classes
- `include/cpp_dbc/config/yaml_config_loader.hpp` - YAML configuration loader

### Implementation Files
- `src/drivers/driver_mysql.cpp` - MySQL implementation using libmysqlclient
- `src/drivers/driver_postgresql.cpp` - PostgreSQL implementation using libpq
- `src/drivers/driver_sqlite.cpp` - SQLite implementation using libsqlite3
- `src/connection_pool.cpp` - Connection pool implementation
- `src/transaction_manager.cpp` - Transaction manager implementation
- `src/driver_manager.cpp` - Driver manager implementation
- `src/config/yaml_config_loader.cpp` - YAML configuration loader implementation

### Examples
- `examples/example.cpp` - Basic usage example with MySQL, PostgreSQL, and SQLite
- `examples/connection_pool_example.cpp` - Connection pool usage example
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
4. yaml-cpp development libraries (optional)
5. CMake 3.15 or later
6. Conan for dependency management

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

# Build with all database drivers
./build.sh --mysql --postgres --sqlite

# Build with YAML configuration support
./build.sh --yaml

# Build examples
./build.sh --examples

# Build tests
./build.sh --test

# Build in Release mode
./build.sh --release

# Build Docker image
./build.dist.sh

# Build Docker image with PostgreSQL and YAML support
./build.dist.sh --postgres --yaml

# Build Docker image with SQLite support
./build.dist.sh --sqlite

# Build Docker image with all database drivers
./build.dist.sh --mysql --postgres --sqlite --yaml
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

### Manual Build with CMake

```bash
# Create build directory for the library
mkdir -p libs/cpp_dbc/build
cd libs/cpp_dbc/build

# Configure with CMake (MySQL enabled, PostgreSQL and SQLite disabled)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=OFF -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

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

1. Unified interface for MySQL, PostgreSQL, and SQLite
2. Connection, query, and result set management
3. Prepared statement support
4. Thread-safe connection pool
5. Cross-thread transaction manager with UUIDs
6. Transaction isolation levels (JDBC-compatible: READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE)
7. YAML configuration support
8. Docker container generation with automatic dependency detection

## Dependencies

- libmysqlclient for MySQL connections
- libpq for PostgreSQL connections (optional)
- libsqlite3 for SQLite connections (optional)
- yaml-cpp for YAML configuration support (optional)
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
│       │       ├── config/
│       │       │   ├── database_config.hpp
│       │       │   └── yaml_config_loader.hpp
│       │       └── drivers/
│       │           ├── driver_mysql.hpp
│       │           ├── driver_postgresql.hpp
│       │           └── driver_sqlite.hpp
│       ├── src/
│       │   ├── connection_pool.cpp
│       │   ├── transaction_manager.cpp
│       │   ├── driver_manager.cpp
│       │   ├── config/
│       │   │   └── yaml_config_loader.cpp
│       │   └── drivers/
│       │       ├── driver_mysql.cpp
│       │       ├── driver_postgresql.cpp
│       │       └── driver_sqlite.cpp
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
