# C++ Database Connectivity (CPP_DBC) Library

## For Impatience

If you're in a hurry and want to get started quickly, check out the [Quick Start Guide](QUICKSTART.md) for common commands and examples.

```bash
# Run tests with rebuild, SQLite, PostgreSQL, MySQL, YAML, Valgrind, auto mode, run once
./helper.sh --bk-combo-01

# Clean build with PostgreSQL, MySQL, SQLite, YAML, tests, and examples
./helper.sh --mc-combo-01

# Clean build of distribution with PostgreSQL, MySQL, SQLite, YAML, tests, and examples
./helper.sh --kfc-combo-01
```

---

This project provides a C++ Database Connectivity library inspired by JDBC, with support for MySQL, PostgreSQL, SQLite, Firebird SQL, MongoDB document databases, ScyllaDB columnar databases, and Redis key-value databases. The library includes connection pooling, transaction management, support for different transaction isolation levels, comprehensive BLOB handling with image file support, document database operations for MongoDB, columnar database operations for ScyllaDB, and key-value operations for Redis.

## GOALS
1. Safe and Stable. Using the best C++ practices.
2. The prority is the memory safe and security of the code. 
3. Ergonomic, flexible and easy to use.
4. Easy to learn. Especially with Java/Go Lang/NodeJS backgrounds
5. Keep a "Consistent" DEX across diferent DB families, where is posible.

## NO GOALS
- Be the more fast and memory eficient DB access library from C++ eco system.

## Features

- **Database Abstraction**: Unified API for different database systems
- **Connection Pooling**: Efficient connection management
- **Transaction Management**: Support for distributed transactions
- **Transaction Isolation Levels**: JDBC-compatible isolation levels (READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE) with database-specific implementations
- **Prepared Statements**: Protection against SQL injection
- **Conditional Compilation**: Build with only the database drivers you need
- **Modern C++ Design**: Uses C++23 features and RAII principles
- **Code Quality**: Comprehensive warning flags and compile-time checks for safer, more reliable code
- **YAML Configuration**: Optional support for loading database configurations from YAML files
- **BLOB Support**: Complete implementation of Binary Large Object (BLOB) support for all database drivers, including image file storage and retrieval
- **JSON Support**: Native handling of JSON data types in MySQL and PostgreSQL with comprehensive query capabilities

## Database Support

The library currently supports:

### Relational Databases
- **MySQL**: Full support for MySQL databases (enabled by default)
- **PostgreSQL**: Full support for PostgreSQL databases (disabled by default)
- **SQLite**: Full support for SQLite databases (disabled by default)
- **Firebird SQL**: Full support for Firebird SQL databases (disabled by default)

### Document Databases
- **MongoDB**: Full support for MongoDB document databases (disabled by default)

### Key-Value Databases
- **Redis**: Full support for Redis key-value databases (disabled by default)

### Columnar Databases
- **ScyllaDB/Cassandra**: Full support for ScyllaDB columnar databases (disabled by default)

Each database driver can be enabled or disabled at compile time to reduce dependencies. By default, MySQL support and YAML configuration support are enabled.

## Project Structure

- **Core Components**:
  - `include/cpp_dbc/cpp_dbc.hpp`: Core interfaces and types
  - `include/cpp_dbc/core/db_connection_pool.hpp`: Generic connection pool interface for all database types
  - `include/cpp_dbc/core/pooled_db_connection.hpp`: Generic pooled connection interface
  - `include/cpp_dbc/core/relational/relational_db_connection_pool.hpp` & `src/core/relational/relational_db_connection_pool.cpp`: Relational database connection pooling implementation
  - `include/cpp_dbc/transaction_manager.hpp` & `src/transaction_manager.cpp`: Transaction management implementation
  - `src/driver_manager.cpp`: Driver management implementation
  - `include/cpp_dbc/config/database_config.hpp`: Database configuration classes
  - `include/cpp_dbc/config/yaml_config_loader.hpp` & `src/config/yaml_config_loader.cpp`: YAML configuration loader

- **Database Drivers** (Relational):
  - `include/cpp_dbc/drivers/relational/driver_mysql.hpp` & `src/drivers/relational/mysql/`: MySQL implementation (split into multiple files)
  - `include/cpp_dbc/drivers/relational/driver_postgresql.hpp` & `src/drivers/relational/postgresql/`: PostgreSQL implementation (split into multiple files)
  - `include/cpp_dbc/drivers/relational/driver_sqlite.hpp` & `src/drivers/relational/sqlite/`: SQLite implementation (split into multiple files)
  - `include/cpp_dbc/drivers/relational/driver_firebird.hpp` & `src/drivers/relational/firebird/`: Firebird SQL implementation (split into multiple files)

- **Database Drivers** (Document):
  - `include/cpp_dbc/drivers/document/driver_mongodb.hpp` & `src/drivers/document/mongodb/`: MongoDB implementation (split into multiple files)

- **Database Drivers** (Key-Value):
  - `include/cpp_dbc/drivers/kv/driver_redis.hpp` & `src/drivers/kv/redis/`: Redis implementation (split into multiple files)

- **Database Drivers** (Columnar):
  - `include/cpp_dbc/drivers/columnar/driver_scylladb.hpp` & `src/drivers/columnar/scylladb/`: ScyllaDB implementation (split into multiple files)

- **Driver Code Organization**:
  Each database driver implementation is split into multiple focused files within its own subdirectory:
  - `*_internal.hpp`: Internal declarations and shared definitions
  - `driver_*.cpp`: Driver class implementation
  - `connection_*.cpp`: Connection class implementation
  - `prepared_statement_*.cpp`: PreparedStatement class implementation (relational/columnar)
  - `result_set_*.cpp`: ResultSet class implementation (relational/columnar)
  - `collection_*.cpp`, `cursor_*.cpp`, `document_*.cpp`: MongoDB-specific implementations

- **Core Interfaces**:
  - `include/cpp_dbc/core/relational/`: Relational database interfaces
  - `include/cpp_dbc/core/columnar/`: Columnar database interfaces (ScyllaDB connection, driver, prepared statement, result set)
  - `include/cpp_dbc/core/document/`: Document database interfaces (MongoDB connection, collection, cursor, data)
  - `include/cpp_dbc/core/graph/`: Graph database interfaces (placeholder)
  - `include/cpp_dbc/core/kv/`: Key-Value database interfaces (Redis connection, driver, connection pool)
  - `include/cpp_dbc/core/timeseries/`: Time-series database interfaces (placeholder)

- **Examples**:
  - `examples/example.cpp`: Basic usage example
  - `examples/connection_pool_example.cpp`: Connection pool example
  - `examples/transaction_manager_example.cpp`: Transaction management example
  - `examples/config_example.cpp`: YAML configuration example
  - `examples/example_config.yml`: Example YAML configuration file
  - `examples/run_config_example.sh`: Script to run the configuration example

## Building the Library

### Prerequisites

Depending on which database drivers you enable, you'll need:

- For MySQL support: MySQL development libraries (`libmysqlclient-dev` on Debian/Ubuntu, `mysql-devel` on RHEL/CentOS)
- For PostgreSQL support: PostgreSQL development libraries (`libpq-dev` on Debian/Ubuntu, `postgresql-devel` on RHEL/CentOS)
- For SQLite support: SQLite development libraries (`libsqlite3-dev` on Debian/Ubuntu, `sqlite-devel` on RHEL/CentOS)
- For Firebird support: Firebird development libraries (`firebird-dev libfbclient2` on Debian/Ubuntu, `firebird-devel libfbclient2` on RHEL/CentOS/Fedora)
- For MongoDB support: MongoDB C++ driver libraries (`libmongoc-dev libbson-dev libmongocxx-dev libbsoncxx-dev` on Debian/Ubuntu, `mongo-c-driver-devel libbson-devel mongo-cxx-driver-devel` on RHEL/CentOS/Fedora)
- For ScyllaDB support: Cassandra C++ driver libraries (`libcassandra-dev` on Debian/Ubuntu, `cassandra-cpp-driver-devel` on RHEL/CentOS/Fedora)
- For Redis support: Hiredis development libraries (`libhiredis-dev` on Debian/Ubuntu, `hiredis-devel` on RHEL/CentOS/Fedora)

The build script will automatically check for and install these dependencies if needed.

### Build Options

The library supports conditional compilation of database drivers and features:

- `USE_MYSQL`: Enable/disable MySQL support (ON by default)
- `USE_POSTGRESQL`: Enable/disable PostgreSQL support (OFF by default)
- `USE_SQLITE`: Enable/disable SQLite support (OFF by default)
- `USE_FIREBIRD`: Enable/disable Firebird SQL support (OFF by default)
- `USE_MONGODB`: Enable/disable MongoDB support (OFF by default)
- `USE_SCYLLADB`: Enable/disable ScyllaDB support (OFF by default)
- `USE_REDIS`: Enable/disable Redis support (OFF by default)
- `USE_CPP_YAML`: Enable/disable YAML configuration support (ON by default)
- `CPP_DBC_BUILD_EXAMPLES`: Enable/disable building examples (OFF by default)
- `CPP_DBC_BUILD_BENCHMARKS`: Enable/disable building benchmarks (OFF by default)
- `DEBUG_CONNECTION_POOL`: Enable debug output for ConnectionPool (OFF by default)
- `DEBUG_TRANSACTION_MANAGER`: Enable debug output for TransactionManager (OFF by default)
- `DEBUG_SQLITE`: Enable debug output for SQLite driver (OFF by default)
- `DEBUG_MONGODB`: Enable debug output for MongoDB driver (OFF by default)
- `DEBUG_SCYLLADB`: Enable debug output for ScyllaDB driver (OFF by default)
- `DEBUG_REDIS`: Enable debug output for Redis driver (OFF by default)
- `DEBUG_ALL`: Enable all debug output at once (OFF by default)
- `BACKWARD_HAS_DW`: Enable libdw support for enhanced stack traces (ON by default)
- `DB_DRIVER_THREAD_SAFE`: Enable thread-safe database driver operations (ON by default)

The library also includes comprehensive warning flags and compile-time checks:

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

### Using the build scripts

#### Library Build Script

The `libs/cpp_dbc/build_cpp_dbc.sh` script handles dependencies and builds the cpp_dbc library:

```bash
# Build with MySQL support only (default)
./libs/cpp_dbc/build_cpp_dbc.sh

# Enable PostgreSQL support
./libs/cpp_dbc/build_cpp_dbc.sh --postgres

# Enable both MySQL and PostgreSQL
./libs/cpp_dbc/build_cpp_dbc.sh --mysql --postgres

# Enable SQLite support
./libs/cpp_dbc/build_cpp_dbc.sh --sqlite

# Enable Firebird SQL support
./libs/cpp_dbc/build_cpp_dbc.sh --firebird

# Enable MongoDB support
./libs/cpp_dbc/build_cpp_dbc.sh --mongodb

# Enable ScyllaDB support
./libs/cpp_dbc/build_cpp_dbc.sh --scylla

# Enable Redis support
./libs/cpp_dbc/build_cpp_dbc.sh --redis

# Enable all database drivers
./libs/cpp_dbc/build_cpp_dbc.sh --mysql --postgres --sqlite --firebird --mongodb --scylla --redis

# Disable MySQL support
./libs/cpp_dbc/build_cpp_dbc.sh --mysql-off

# Build in Release mode (default is Debug)
./libs/cpp_dbc/build_cpp_dbc.sh --release

# Enable YAML configuration support
./libs/cpp_dbc/build_cpp_dbc.sh --yaml

# Build examples
./libs/cpp_dbc/build_cpp_dbc.sh --examples

# Enable debug output for ConnectionPool
./libs/cpp_dbc/build_cpp_dbc.sh --debug-pool

# Enable debug output for TransactionManager
./libs/cpp_dbc/build_cpp_dbc.sh --debug-txmgr

# Enable debug output for SQLite driver
./libs/cpp_dbc/build_cpp_dbc.sh --debug-sqlite

# Enable debug output for MongoDB driver
./libs/cpp_dbc/build_cpp_dbc.sh --debug-mongodb

# Enable debug output for ScyllaDB driver
./libs/cpp_dbc/build_cpp_dbc.sh --debug-scylla

# Enable debug output for Redis driver
./libs/cpp_dbc/build_cpp_dbc.sh --debug-redis

# Enable all debug output
./libs/cpp_dbc/build_cpp_dbc.sh --debug-all

# Disable libdw support for stack traces
./libs/cpp_dbc/build_cpp_dbc.sh --dw-off

# Disable thread-safe database driver operations (for single-threaded performance)
./libs/cpp_dbc/build_cpp_dbc.sh --db-driver-thread-safe-off

# Enable YAML and build examples
./libs/cpp_dbc/build_cpp_dbc.sh --yaml --examples

# Show help
./libs/cpp_dbc/build_cpp_dbc.sh --help
```

This script:
1. Checks for and installs required development libraries
2. Configures the library with CMake
3. Builds and installs the library to the `build/libs/cpp_dbc` directory

#### Main Build Script

The `build.sh` script builds the main application, passing all parameters to the library build script:

```bash
# Build with MySQL support only (default in Debug mode)
./build.sh

# Enable PostgreSQL support
./build.sh --postgres

# Enable both MySQL and PostgreSQL
./build.sh --mysql --postgres

# Enable SQLite support
./build.sh --sqlite

# Enable Firebird SQL support
./build.sh --firebird

# Enable MongoDB support
./build.sh --mongodb

# Enable ScyllaDB support
./build.sh --scylla

# Enable Redis support
./build.sh --redis

# Enable all database drivers
./build.sh --mysql --postgres --sqlite --firebird --mongodb --scylla --redis

# Disable MySQL support
./build.sh --mysql-off

# Build in Release mode
./build.sh --release

# Enable PostgreSQL and disable MySQL
./build.sh --postgres --mysql-off

# Enable YAML configuration support
./build.sh --yaml

# Build examples
./build.sh --examples

# Enable YAML and build examples
./build.sh --yaml --examples

# Enable debug output for ConnectionPool
./build.sh --debug-pool

# Enable debug output for TransactionManager
./build.sh --debug-txmgr

# Enable debug output for SQLite driver
./build.sh --debug-sqlite

# Enable debug output for MongoDB driver
./build.sh --debug-mongodb

# Enable debug output for ScyllaDB driver
./build.sh --debug-scylla

# Enable debug output for Redis driver
./build.sh --debug-redis

# Enable all debug output
./build.sh --debug-all

# Disable libdw support for stack traces
./build.sh --dw-off

# Disable thread-safe database driver operations (for single-threaded performance)
./build.sh --db-driver-thread-safe-off

# Show help
./build.sh --help
```

The main build script forwards command-line arguments to the library build script and stops if the library build fails.

The script will:

1. Check for and install required development libraries
2. Create the build directory if it doesn't exist
3. Generate Conan files
4. Configure the project with CMake using the specified options
5. Build the project in Debug mode by default (or Release mode if specified)

#### Test Build and Execution

The project includes scripts for building and running tests:

```bash
# Build with tests enabled
./build.sh --test

# Run the tests
./run_test.sh

# Run tests with specific options
./run_test.sh --valgrind  # Run tests with Valgrind
./run_test.sh --asan      # Run tests with AddressSanitizer
./run_test.sh --ctest     # Run tests using CTest
./run_test.sh --rebuild   # Force rebuild of tests before running
./run_test.sh --run-test="tag1+tag2"  # Run specific tests by tag
./run_test.sh --skip-build  # Skip the build step
./run_test.sh --list        # List tests without running them
./run_test.sh --debug-pool  # Enable debug output for ConnectionPool
./run_test.sh --debug-txmgr  # Enable debug output for TransactionManager
./run_test.sh --debug-sqlite  # Enable debug output for SQLite driver
./run_test.sh --debug-mongodb  # Enable debug output for MongoDB driver
./run_test.sh --debug-all  # Enable all debug output
./run_test.sh --db-driver-thread-safe-off  # Disable thread-safe driver operations
```

All test output is automatically logged to files in the `logs/test/` directory with timestamps in the filenames. The system automatically rotates logs, keeping the 4 most recent files.

#### Parallel Test Execution

The project supports running test prefixes (10_, 20_, 21_, 23_, 26_, etc.) in parallel for faster test execution:

```bash
# Run 4 test prefixes in parallel
./helper.sh --run-test=parallel=4

# Run 2 prefixes in parallel, prioritizing 23_ tests (slow tests first)
./helper.sh --run-test=parallel=2,parallel-order=23_

# Run with TUI progress display (real-time status of each parallel test)
./helper.sh --run-test=parallel=4,progress

# Run parallel tests with Valgrind
./helper.sh --run-test=parallel=4,valgrind

# Summarize past test runs from log directories
./helper.sh --run-test=summarize
```

Parallel execution features:
- Each prefix runs independently with separate log files
- If a prefix fails, it stops but others continue running
- Logs saved to `logs/test/YYYY-MM-DD-HH-MM-SS/PREFIX_RUNXX.log`
- TUI mode provides split panel view with real-time test output
- Summarize mode shows summary of all past parallel test runs
- Valgrind error detection support
- Color-coded output for test status (pass/fail)

### Running Benchmarks

The project includes benchmarks for database operations using Google Benchmark:

```bash
# Build with benchmarks enabled
./build.sh --benchmarks

# Run the benchmarks
./helper.sh --run-benchmarks

# Run only MySQL benchmarks
./helper.sh --run-benchmarks=mysql

# Disable MySQL benchmarks
./helper.sh --run-benchmarks=mysql-off

# Run only PostgreSQL benchmarks
./helper.sh --run-benchmarks=postgresql

# Run only SQLite benchmarks
./helper.sh --run-benchmarks=sqlite

# Run MySQL and PostgreSQL benchmarks
./helper.sh --run-benchmarks=mysql+postgresql

# Run benchmarks with memory usage tracking
./helper.sh --run-benchmarks=mysql,postgresql,memory-usage

# Run benchmarks and create a baseline
./helper.sh --run-benchmarks=mysql,postgresql,base-line
```

The benchmarks measure the performance of SELECT, INSERT, UPDATE, and DELETE operations for different database systems with varying data sizes (10, 100, 1000, and 10000 rows). All benchmark output is automatically logged to files in the `logs/benchmark/` directory with timestamps in the filenames.

You can configure benchmark execution with additional parameters:

```bash
# Set minimum time per iteration (in seconds)
./helper.sh --run-benchmarks --min-time=1.0

# Set number of repetitions for each benchmark
./helper.sh --run-benchmarks --repetitions=5
```

### Benchmark Baselines and Comparison

The project includes tools for creating and comparing benchmark baselines:

```bash
# Create a baseline from a benchmark log file
./libs/cpp_dbc/create_benchmark_cpp_dbc_base_line.sh --log-file=logs/benchmark/output-YYYYMMDD-HHMMSS.log

# Compare two baseline files
./libs/cpp_dbc/compare_benchmark_cpp_dbc_base_line.sh \
  --benchmarkA=base_line/SystemConfig/benchmark-A.data \
  --benchmarkB=base_line/SystemConfig/benchmark-B.data

# Use automatic detection to find the latest baseline files
./libs/cpp_dbc/compare_benchmark_cpp_dbc_base_line.sh \
  --benchmarkA=base_line/SystemConfig/detect \
  --benchmarkB=base_line/SystemConfig/detect

# Filter comparison by benchmark name
./libs/cpp_dbc/compare_benchmark_cpp_dbc_base_line.sh \
  --benchmarkA=base_line/SystemConfig/detect \
  --benchmarkB=base_line/SystemConfig/detect \
  --filter=MySQL
```

Baseline files are stored in `libs/cpp_dbc/benchmark/base_line/` organized by system configuration (CPU, RAM, OS). Each baseline file contains:
- Benchmark results (time, CPU time, iterations, items per second)
- Memory usage per database type
- Time command output with detailed resource usage

You can analyze test logs for failures and memory issues:

```bash
# Check the most recent test log file for failures and memory issues
./helper.sh --check-test-log

# Check a specific test log file
./helper.sh --check-test-log=logs/test/output-20251109-152030.log
```

### BLOB Support

The library provides comprehensive support for Binary Large Object (BLOB) operations:

1. **Core BLOB Classes**:
   - Base classes for BLOB handling with consistent interfaces
   - Memory-based and file-based implementations
   - Support for reading and writing binary data

2. **Database-Specific Implementations**:
   - MySQL: Support for BLOB data types with MySQLBlob implementation
   - PostgreSQL: Support for BYTEA data type with PostgreSQLBlob implementation
   - SQLite: Support for BLOB data type with SQLiteBlob implementation
   - Firebird: Support for BLOB data type with FirebirdBlob implementation

3. **Image File Support**:
   - Helper functions for reading and writing binary files
   - Support for storing and retrieving image files
   - Integrity verification for binary data
   - Comprehensive test cases with test.jpg file

### JSON Support

The library provides comprehensive support for JSON data types:

1. **MySQL JSON Support**:
   - Support for the JSON data type
   - JSON functions: JSON_EXTRACT, JSON_SEARCH, JSON_CONTAINS, JSON_ARRAYAGG
   - JSON indexing for performance optimization
   - JSON validation and error handling

2. **PostgreSQL JSON Support**:
   - Support for both JSON and JSONB data types
   - JSON operators: @>, <@, ?, ?|, ?&
   - JSON modification functions: jsonb_set, jsonb_insert
   - GIN indexing for JSONB fields
   - JSON path expressions and transformations

3. **Testing**:
   - Comprehensive test cases in test_mysql_real_json.cpp and test_postgresql_real_json.cpp
   - Random JSON data generation for performance testing
   - Tests for various JSON operations, validation, and error handling

### Memory Leak Prevention

The project includes comprehensive smart pointer usage to prevent memory leaks:

1. **Smart Pointer Migration for All Database Drivers**:
   - All database drivers now use smart pointers (unique_ptr, shared_ptr, weak_ptr) instead of raw pointers
   - Custom deleters ensure proper cleanup of database-specific resources
   - RAII principles guarantee resource cleanup even in case of exceptions

2. **MySQL Driver Smart Pointers**:
   - `MySQLHandle` (shared_ptr<MYSQL>) for connection management
   - `MySQLStmtHandle` (unique_ptr<MYSQL_STMT>) for prepared statements
   - `MySQLResHandle` (unique_ptr<MYSQL_RES>) for result sets
   - `weak_ptr<MYSQL>` in PreparedStatement for safe connection reference

3. **PostgreSQL Driver Smart Pointers**:
   - `PGconnHandle` (shared_ptr<PGconn>) for connection management
   - `PGresultHandle` (unique_ptr<PGresult>) for result sets
   - `weak_ptr<PGconn>` in PreparedStatement for safe connection reference

4. **SQLite Driver Smart Pointers**:
   - `shared_ptr<sqlite3>` for connection management
   - `SQLiteStmtHandle` (unique_ptr<sqlite3_stmt>) for prepared statements
   - `weak_ptr<sqlite3>` in PreparedStatement for safe connection reference
   - `weak_ptr` tracking for active statements to avoid preventing destruction

5. **Firebird Driver Smart Pointers**:
   - `shared_ptr<isc_db_handle>` for connection management
   - Custom deleters for statement handles and database handles
   - `weak_ptr<isc_db_handle>` in PreparedStatement for safe connection reference
   - SQLDA structure management with proper memory allocation/deallocation

6. **BLOB Memory Safety**:
   - All BLOB implementations use `weak_ptr` for safe connection references
   - `FirebirdBlob`: Uses `weak_ptr<FirebirdConnection>` with `getConnection()` helper
   - `MySQLBlob`: Uses `weak_ptr<MYSQL>` with `getMySQLConnection()` helper
   - `PostgreSQLBlob`: Uses `weak_ptr<PGconn>` with `getPGConnection()` helper
   - `SQLiteBlob`: Uses `weak_ptr<sqlite3>` with `getSQLiteConnection()` helper
   - All BLOB classes have `isConnectionValid()` method to check connection state
   - Operations throw `DBException` if connection has been closed

7. **Connection Pool Memory Safety and Factory Pattern**:
   - `RelationalDBConnectionPool` uses `std::enable_shared_from_this<RelationalDBConnectionPool>` for safe self-reference
   - Added factory pattern with static `create()` methods for all connection pools
   - Made constructors protected to enforce creation via factory methods
   - Added `initializePool()` method that's called after shared_ptr construction
   - `RelationalDBPooledConnection` uses `weak_ptr` for pool reference
   - Uses `m_poolAlive` shared atomic flag to track pool lifetime
   - Added `isPoolValid()` helper method to check if pool is still alive
   - Pool sets `m_poolAlive` to `false` in `close()` before cleanup
   - Prevents use-after-free when pool is destroyed while connections are in use
   - Graceful handling of connection return when pool is already closed

8. **Benefits**:
   - Automatic resource cleanup through RAII
   - Safe detection of closed connections via weak_ptr
   - Clear ownership semantics documented in code
   - Elimination of manual delete/free calls
   - Prevention of double-free errors

To run memory leak checks:

```bash
# Run tests with Valgrind
./run_test.sh --valgrind

# Run tests with AddressSanitizer
./run_test.sh --asan
```

For more details about memory leak issues and their solutions, see the [Valgrind PostgreSQL Memory Leak documentation](memory-bank/valgrind_postgresql_memory_leak.md) and [AddressSanitizer Issues documentation](memory-bank/asan_issues.md).

### Exception Handling and Stack Trace Capture

The library includes a robust exception handling system with stack trace capture capabilities:

1. **Enhanced DBException Class**:
   - Unique error marks for better error identification
   - Separated error marks from error messages
   - Call stack capture for better debugging
   - Methods to retrieve and print stack traces
   - Método `what_s()` que devuelve un `std::string&` para evitar problemas de seguridad con punteros `const char*`
   - Destructor virtual para una correcta jerarquía de herencia

2. **Stack Trace Capture with libdw Support**:
   - Integration with backward-cpp library for stack trace capture
   - Support for libdw (part of elfutils) for enhanced stack trace information
   - Configurable with `--dw-off` option to disable libdw when needed
   - Automatic detection and installation of libdw development libraries
   - Improved function name and source file information in stack traces

2. **Stack Trace Capture**:
   - Integration with backward-cpp library for stack trace capture
   - Automatic capture of call stack when exceptions are thrown
   - Filtering of irrelevant stack frames
   - Human-readable stack trace output

3. **Error Identification**:
   - Unique alphanumeric error marks for each error location
   - Consistent error format across all database drivers
   - Improved error messages with context information
   - Better debugging capabilities with file and line information

4. **Usage Example**:
   ```cpp
   try {
       // Database operations
   } catch (const cpp_dbc::DBException& e) {
       std::cerr << "Error: " << e.what_s() << std::endl;
       std::cerr << "Error Mark: " << e.getMark() << std::endl;
       e.printCallStack();
   }
   ```

The `run_test.sh` script will automatically build the project and tests if they haven't been built yet.

#### Running the YAML Configuration Example

To run the YAML configuration example:

```bash
# First, build the library with YAML support and examples
./build.sh --yaml --examples

# Then run the example
./libs/cpp_dbc/examples/run_config_example.sh
```

This will load the example YAML configuration file and display the database configurations.

#### Helper Script

The project includes a helper script (`helper.sh`) that provides various utilities:

```bash
# Build the project
./helper.sh --build

# Clean the build directory
./helper.sh --clean-build

# Clear the Conan cache
./helper.sh --clean-conan-cache

# Build the tests
./helper.sh --test

# Run the tests
./helper.sh --run-test

# Check shared library dependencies of the executable inside the container
./helper.sh --ldd-bin-ctr

# Check shared library dependencies of the executable locally
./helper.sh --ldd-build-bin

# Run the executable
./helper.sh --run-build-bin

# Multiple commands can be combined
./helper.sh --clean-build --clean-conan-cache --build
```

### VSCode Integration

The project includes VSCode configuration files in the `.vscode` directory:

- `c_cpp_properties.json`: Configures C/C++ IntelliSense with proper include paths and preprocessor definitions
- `tasks.json`: Defines build tasks for the project
- `build_with_props.sh`: Script to extract preprocessor definitions from `c_cpp_properties.json` and pass them to build.sh

To build the project using VSCode tasks:

1. Press `Ctrl+Shift+B` to run the default build task
2. Select "CMake: build" to build using the default configuration
3. Select "Build with C++ Properties" to build using the preprocessor definitions from `c_cpp_properties.json`

The "Auto install extensions" task runs automatically when opening the folder and installs recommended extensions.

### Manual CMake configuration

You can also configure the build manually with CMake:

```bash
mkdir -p build && cd build
# Default: MySQL enabled, PostgreSQL disabled
cmake ..

# Enable PostgreSQL
cmake .. -DUSE_POSTGRESQL=ON

# Disable MySQL, enable PostgreSQL
cmake .. -DUSE_MYSQL=OFF -DUSE_POSTGRESQL=ON

# Build the project
cmake --build .
```

## Using as a Library in Other CMake Projects

The CPP_DBC library is designed to be used as an external dependency in other CMake projects. After installing the library, you can use it in your CMake project as follows:

```cmake
# In your CMakeLists.txt
find_package(cpp_dbc REQUIRED)

# Create your executable or library
add_executable(your_app main.cpp)

# Link against cpp_dbc
target_link_libraries(your_app PRIVATE cpp_dbc::cpp_dbc)

# Add compile definitions for the database drivers
target_compile_definitions(your_app PRIVATE
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
)
```

The library exports the following CMake variables that you can use to check which database drivers are enabled:

- `CPP_DBC_USE_MYSQL`: Set to ON if MySQL support is enabled
- `CPP_DBC_USE_POSTGRESQL`: Set to ON if PostgreSQL support is enabled
- `CPP_DBC_USE_SQLITE`: Set to ON if SQLite support is enabled
- `CPP_DBC_USE_FIREBIRD`: Set to ON if Firebird SQL support is enabled
- `CPP_DBC_USE_MONGODB`: Set to ON if MongoDB support is enabled

You can use these variables to conditionally include code in your project:

```cmake
if(CPP_DBC_USE_MYSQL)
    # MySQL-specific code
endif()

if(CPP_DBC_USE_POSTGRESQL)
    # PostgreSQL-specific code
endif()

if(CPP_DBC_USE_SQLITE)
    # SQLite-specific code
endif()

if(CPP_DBC_USE_FIREBIRD)
    # Firebird-specific code
endif()

if(CPP_DBC_USE_MONGODB)
    # MongoDB-specific code
endif()
```

### Include Paths

When using the library as an external dependency, include the headers as follows:

```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif
#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif
#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif
#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#endif
```

## Usage Example

```cpp
#include "cpp_dbc/cpp_dbc.hpp"

#if USE_MYSQL
#include "cpp_dbc/drivers/relational/driver_mysql.hpp"
#endif

#if USE_POSTGRESQL
#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"
#endif

#if USE_SQLITE
#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#endif

#if USE_FIREBIRD
#include "cpp_dbc/drivers/relational/driver_firebird.hpp"
#endif

#if USE_MONGODB
#include "cpp_dbc/drivers/document/driver_mongodb.hpp"
#endif

#if USE_REDIS
#include "cpp_dbc/drivers/kv/driver_redis.hpp"
#endif

int main() {
    // Register available drivers
#if USE_MYSQL
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
#endif

#if USE_POSTGRESQL
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());
#endif

#if USE_SQLITE
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());
#endif

#if USE_FIREBIRD
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());
#endif

#if USE_MONGODB
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>());
#endif

#if USE_REDIS
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Redis::RedisDriver>());
#endif

    // Get a relational database connection
    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
        cpp_dbc::DriverManager::getDBConnection(
            "cpp_dbc:mysql://localhost:3306/testdb",
            "username",
            "password"
        ));

    // Use the connection
    auto resultSet = conn->executeQuery("SELECT * FROM users");
    
    while (resultSet->next()) {
        std::cout << resultSet->getInt("id") << ": "
                  << resultSet->getString("name") << std::endl;
    }
    
    conn->close();

#if USE_MONGODB
    // Get a MongoDB document database connection
    auto mongoDriver = std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>();
    auto mongoConn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(
        mongoDriver->connectDocument(
            "mongodb://localhost:27017/testdb",
            "username",
            "password"
        ));

    // Use the MongoDB connection
    auto collection = mongoConn->getCollection("users");
    
    // Insert a document
    collection->insertOne(R"({"name": "John", "age": 30})");
    
    // Find documents
    auto cursor = collection->find(R"({"age": {"$gte": 18}})");
    while (cursor->next()) {
        auto doc = cursor->current();
        std::cout << doc->getString("name") << ": "
                  << doc->getInt("age") << std::endl;
    }
    
    mongoConn->close();
#endif

#if USE_REDIS
    // Get a Redis key-value database connection
    auto redisDriver = std::make_shared<cpp_dbc::Redis::RedisDriver>();
    auto redisConn = std::dynamic_pointer_cast<cpp_dbc::KVDBConnection>(
        redisDriver->connectKV(
            "cpp_dbc:redis://localhost:6379",
            "",
            ""
        ));

    // Use the Redis connection
    redisConn->setString("greeting", "Hello, Redis!");
    std::string greeting = redisConn->getString("greeting");
    std::cout << "Redis greeting: " << greeting << std::endl;

    // Use Redis list operations
    redisConn->listPushRight("mylist", "item1");
    redisConn->listPushRight("mylist", "item2");
    std::cout << "List length: " << redisConn->listLength("mylist") << std::endl;

    // Use Redis hash operations
    redisConn->hashSet("myhash", "field1", "value1");
    std::cout << "Hash value: " << redisConn->hashGet("myhash", "field1") << std::endl;

    redisConn->close();
#endif
    
    return 0;
}
```

## YAML Configuration Example

The library provides optional support for loading database configurations from YAML files:

```cpp
#include "cpp_dbc/config/database_config.hpp"

#ifdef USE_CPP_YAML
#include "cpp_dbc/config/yaml_config_loader.hpp"
#endif

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file.yml>" << std::endl;
        return 1;
    }

#ifdef USE_CPP_YAML
    try {
        // Load configuration from YAML file
        std::cout << "Loading configuration from YAML file: " << argv[1] << std::endl;
        cpp_dbc::config::DatabaseConfigManager configManager =
            cpp_dbc::config::YamlConfigLoader::loadFromFile(argv[1]);
        
        // Get a specific database configuration
        auto dbConfigOpt = configManager.getDatabaseByName("dev_mysql");
        if (dbConfigOpt) {
            // Use the configuration to create a connection
            const auto& dbConfig = dbConfigOpt->get();
            std::string connStr = dbConfig.createConnectionString();
            std::cout << "Connection String: " << connStr << std::endl;
            std::cout << "Username: " << dbConfig.getUsername() << std::endl;
            std::cout << "Password: " << dbConfig.getPassword() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cerr << "YAML support not enabled. Rebuild with --yaml option." << std::endl;
    return 1;
#endif

    return 0;
}
```

Example YAML configuration file:

```yaml
# Database configurations
databases:
  - name: dev_mysql
    type: mysql
    host: localhost
    port: 3306
    database: TestDB
    username: root
    password: password
    options:
      connect_timeout: 5
      charset: utf8mb4
      autocommit: true

  - name: prod_postgresql
    type: postgresql
    host: db.example.com
    port: 5432
    database: ProdDB
    username: prod_user
    password: prod_password
    options:
      connect_timeout: 10
      application_name: cpp_dbc_prod
      client_encoding: UTF8
      sslmode: require

  - name: dev_mongodb
    type: mongodb
    host: localhost
    port: 27017
    database: TestDB
    username: root
    password: password
    options:
      connect_timeout: 5000
      server_selection_timeout: 5000
      auth_source: admin
      direct_connection: true

  - name: dev_redis
    type: redis
    host: localhost
    port: 6379
    database: 0
    username:
    password:
    options:
      connect_timeout: 5000
      tcp_keepalive: true

# Connection pool configurations
connection_pool:
  default:
    initial_size: 5
    max_size: 20
    connection_timeout: 5000
    idle_timeout: 60000
    validation_interval: 30000
```

## Distribution

### Docker Image

The `build.dist.sh` script generates a Dockerfile for the application and builds the Docker image:

```bash
# Build Docker image with default options
./build.dist.sh

# Enable PostgreSQL support
./build.dist.sh --postgres

# Enable YAML configuration support
./build.dist.sh --yaml

# Build in Release mode
./build.dist.sh --release

# Build with tests
./build.dist.sh --test

# Build with examples
./build.dist.sh --examples

# Show help
./build.dist.sh --help
```

The script performs the following steps:

1. Processes variables from `.dist_build` and `.env_dist` files
2. Builds the project with the specified options
3. Automatically analyzes the executable to detect shared library dependencies
4. Maps libraries to their corresponding Debian packages
5. Generates a Dockerfile with only the necessary dependencies
6. Builds and tags the Docker image

The automatic dependency detection ensures that the Docker container includes all required libraries for the executable to run properly, regardless of which database drivers or features are enabled.

You can use the `--ldd` option in the helper script to check the shared library dependencies inside the container:

```bash
./helper.sh --ldd
```

This will run the `ldd` command inside the Docker container on the executable, showing all shared libraries that the executable depends on within the container environment.

### Distribution Packages (DEB and RPM)

The `libs/cpp_dbc/build_dist_pkg.sh` script builds distribution packages (.deb and .rpm) for the cpp_dbc library for multiple distributions:

```bash
# Build package for Ubuntu 24.04 with default options
./libs/cpp_dbc/build_dist_pkg.sh

# Build for multiple distributions (Debian, Ubuntu, and Fedora)
./libs/cpp_dbc/build_dist_pkg.sh --distro=ubuntu:24.04+ubuntu:22.04+debian:12+debian:13+fedora:42+fedora:43

# Specify build options
./libs/cpp_dbc/build_dist_pkg.sh --build=yaml,mysql,postgres,sqlite,debug,dw,examples

# Specify a version instead of using a timestamp
./libs/cpp_dbc/build_dist_pkg.sh --version=1.0.1

# Show help
./libs/cpp_dbc/build_dist_pkg.sh --help
```

The script performs the following steps for each specified distribution:

1. Creates a Docker container based on the target distribution
2. Builds the cpp_dbc library with the specified options
3. Creates a Debian (.deb) or RPM (.rpm) package with proper dependencies
4. Copies the package to the build directory

Supported distributions:
- Debian: 12, 13 (.deb packages)
- Ubuntu: 22.04, 24.04 (.deb packages)
- Fedora: 42, 43 (.rpm packages)

The resulting packages include:
- The static library file (libcpp_dbc.a)
- Header files
- CMake configuration files for easy integration with other projects
- Documentation and examples

After installing the package, you can use the library in your CMake projects as described in the [Using as a Library in Other CMake Projects](#using-as-a-library-in-other-cmake-projects) section.

For more details on using the library with CMake, see the [cmake_usage.md](libs/cpp_dbc/docs/cmake_usage.md) documentation.
