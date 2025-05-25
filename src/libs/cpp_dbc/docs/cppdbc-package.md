# CPPDBC - C++ Database Connectivity Library

This document contains information about the CPPDBC library, inspired by JDBC but for C++. It allows you to connect and work with MySQL and PostgreSQL databases using a unified interface.

## File Structure

### Header Files
- `cpp_dbc.hpp` - Main library interfaces and classes
- `drivers/driver_mysql.hpp` - MySQL-specific definitions
- `drivers/driver_postgresql.hpp` - PostgreSQL-specific definitions
- `connection_pool.hpp` - Connection pool with thread-safety support
- `transaction_manager.hpp` - Transaction manager for cross-thread transactions

### Implementation Files
- `drivers/driver_mysql.cpp` - MySQL implementation using libmysqlclient
- `drivers/driver_postgresql.cpp` - PostgreSQL implementation using libpq
- `connection_pool.cpp` - Connection pool implementation
- `transaction_manager.cpp` - Transaction manager implementation

### Examples
- `examples/basic_example.cpp` - Basic usage example with MySQL and PostgreSQL
- `examples/connection_pool_example.cpp` - Connection pool usage example
- `examples/transaction_manager_example.cpp` - Cross-thread transaction example

## Building

To build the library and examples, you'll need:

1. A modern C++ compiler with C++23 support
2. MySQL development libraries (libmysqlclient-dev)
3. PostgreSQL development libraries (libpq-dev)
4. CMake 3.15 or later
5. Conan for dependency management

### Using the Build Scripts

```bash
# Build with MySQL support only (default)
./build.sh

# Build with MySQL and PostgreSQL support
./build.sh --mysql --postgres

# Build with PostgreSQL support only
./build.sh --mysql-off --postgres
```

The build script:
1. Checks for and installs required dependencies
2. Configures the library with CMake
3. Builds and installs the library to the `build/cpp_dbc` directory
4. Builds the main application

### Manual Build with CMake

```bash
# Create build directory for the library
mkdir -p src/libs/cpp_dbc/build
cd src/libs/cpp_dbc/build

# Configure with CMake (MySQL enabled, PostgreSQL disabled)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DCMAKE_INSTALL_PREFIX="../../../../build/cpp_dbc"

# Build and install
cmake --build . --target install

# Return to root directory
cd ../../../..

# Create build directory for the main application
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DCMAKE_PREFIX_PATH=../build/cpp_dbc

# Build
cmake --build .
```

## Main Features

1. Unified interface for MySQL and PostgreSQL
2. Connection, query, and result set management
3. Prepared statement support
4. Thread-safe connection pool
5. Cross-thread transaction manager with UUIDs

## Dependencies

- libmysqlclient for MySQL connections
- libpq for PostgreSQL connections
- C++17 standard library for threads, mutexes, and condition variables
- CMake for build system
- Conan for dependency management

## Project Structure

```
cpp_dbc/
├── src/
│   ├── libs/
│   │   └── cpp_dbc/
│   │       ├── cpp_dbc.hpp
│   │       ├── connection_pool.hpp
│   │       ├── connection_pool.cpp
│   │       ├── transaction_manager.hpp
│   │       ├── transaction_manager.cpp
│   │       ├── drivers/
│   │       │   ├── driver_mysql.hpp
│   │       │   ├── driver_mysql.cpp
│   │       │   ├── driver_postgresql.hpp
│   │       │   └── driver_postgresql.cpp
│   │       ├── cmake/
│   │       │   ├── cpp_dbc-config.cmake.in
│   │       │   ├── FindMySQL.cmake
│   │       │   └── FindPostgreSQL.cmake
│   │       ├── CMakeLists.txt
│   │       └── build_cpp_dbc.sh
│   └── install/
├── build/
│   └── cpp_dbc/
├── main.cpp
├── CMakeLists.txt
└── build.sh
