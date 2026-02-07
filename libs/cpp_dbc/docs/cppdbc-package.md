# CPPDBC - C++ Database Connectivity Library

This document contains information about the CPPDBC library, inspired by JDBC but for C++. It allows you to connect and work with MySQL, PostgreSQL, SQLite, Firebird, MongoDB, ScyllaDB, and Redis databases using a unified interface. The library includes connection pooling for all database types (relational, document, columnar, and key-value), transaction management with different isolation levels, support for YAML configuration, enhanced stack trace capabilities with libdw, comprehensive benchmarking capabilities for all supported database types, and Doxygen-compatible API documentation with inline code examples across all public headers.

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
- `include/cpp_dbc/drivers/relational/driver_mysql.hpp` - MySQL aggregator header (includes `mysql/*.hpp`)
- `include/cpp_dbc/drivers/relational/driver_postgresql.hpp` - PostgreSQL aggregator header (includes `postgresql/*.hpp`)
- `include/cpp_dbc/drivers/relational/driver_sqlite.hpp` - SQLite aggregator header (includes `sqlite/*.hpp`)
- `include/cpp_dbc/drivers/relational/driver_firebird.hpp` - Firebird aggregator header (includes `firebird/*.hpp`)
- `include/cpp_dbc/drivers/document/driver_mongodb.hpp` - MongoDB aggregator header (includes `mongodb/*.hpp`)
- `include/cpp_dbc/core/columnar/columnar_db_connection.hpp` - Columnar database connection interface
- `include/cpp_dbc/core/columnar/columnar_db_driver.hpp` - Columnar database driver interface
- `include/cpp_dbc/core/columnar/columnar_db_prepared_statement.hpp` - Columnar prepared statement interface
- `include/cpp_dbc/core/columnar/columnar_db_result_set.hpp` - Columnar result set interface
- `include/cpp_dbc/core/columnar/columnar_db_connection_pool.hpp` - Columnar database connection pool interface
- `include/cpp_dbc/drivers/columnar/driver_scylladb.hpp` - ScyllaDB aggregator header (includes `scylladb/*.hpp`)
- `include/cpp_dbc/core/kv/kv_db_connection.hpp` - Key-value database connection interface
- `include/cpp_dbc/core/kv/kv_db_driver.hpp` - Key-value database driver interface
- `include/cpp_dbc/core/kv/kv_db_connection_pool.hpp` - Key-value database connection pool interface
- `include/cpp_dbc/drivers/kv/driver_redis.hpp` - Redis aggregator header (includes `redis/*.hpp`)
- `include/cpp_dbc/drivers/relational/mysql/blob.hpp` - MySQL BLOB implementation (in driver subfolder)
- `include/cpp_dbc/drivers/relational/postgresql/blob.hpp` - PostgreSQL BLOB implementation (in driver subfolder)
- `include/cpp_dbc/drivers/relational/sqlite/blob.hpp` - SQLite BLOB implementation (in driver subfolder)
- `include/cpp_dbc/drivers/relational/firebird/blob.hpp` - Firebird BLOB implementation (in driver subfolder)
- `include/cpp_dbc/core/db_connection_pool.hpp` - Generic connection pool interface for all database types
- `include/cpp_dbc/core/pooled_db_connection.hpp` - Generic pooled connection interface for all database types
- `include/cpp_dbc/core/relational/relational_db_connection_pool.hpp` - Relational database connection pool implementation
- `include/cpp_dbc/transaction_manager.hpp` - Transaction manager for cross-thread transactions
- `include/cpp_dbc/backward.hpp` - Stack trace capture and analysis
- `include/cpp_dbc/common/system_utils.hpp` - System utilities including stack trace functions
- `include/cpp_dbc/config/database_config.hpp` - Database configuration classes
- `include/cpp_dbc/config/yaml_config_loader.hpp` - YAML configuration loader

### Implementation Files

Each database driver implementation is split into multiple focused files within dedicated subdirectories:

**SQLite Driver** (`src/drivers/relational/sqlite/`):
- `sqlite_internal.hpp` - Internal declarations and shared definitions
- `driver_01.cpp` - SQLiteDBDriver implementation
- `connection_01.cpp`, `connection_02.cpp` - SQLiteConnection implementation
- `prepared_statement_01.cpp` to `prepared_statement_04.cpp` - SQLitePreparedStatement implementation
- `result_set_01.cpp` to `result_set_03.cpp` - SQLiteResultSet implementation

**MySQL Driver** (`src/drivers/relational/mysql/`):
- `mysql_internal.hpp` - Internal declarations and shared definitions
- `driver_01.cpp` - MySQLDBDriver implementation
- `connection_01.cpp` to `connection_03.cpp` - MySQLConnection implementation
- `prepared_statement_01.cpp` to `prepared_statement_03.cpp` - MySQLPreparedStatement implementation
- `result_set_01.cpp` to `result_set_03.cpp` - MySQLResultSet implementation

**PostgreSQL Driver** (`src/drivers/relational/postgresql/`):
- `postgresql_internal.hpp` - Internal declarations and shared definitions
- `driver_01.cpp` - PostgreSQLDBDriver implementation
- `connection_01.cpp` to `connection_03.cpp` - PostgreSQLConnection implementation
- `prepared_statement_01.cpp` to `prepared_statement_04.cpp` - PostgreSQLPreparedStatement implementation
- `result_set_01.cpp` to `result_set_03.cpp` - PostgreSQLResultSet implementation

**Firebird Driver** (`src/drivers/relational/firebird/`):
- `firebird_internal.hpp` - Internal declarations and shared definitions
- `driver_01.cpp` - FirebirdDBDriver implementation
- `connection_01.cpp` to `connection_03.cpp` - FirebirdConnection implementation
- `prepared_statement_01.cpp` to `prepared_statement_03.cpp` - FirebirdPreparedStatement implementation
- `result_set_01.cpp` to `result_set_04.cpp` - FirebirdResultSet implementation

**MongoDB Driver** (`src/drivers/document/mongodb/`):
- `mongodb_internal.hpp` - Internal declarations and shared definitions
- `driver_01.cpp` - MongoDBDriver implementation
- `connection_01.cpp` to `connection_04.cpp` - MongoDBConnection implementation
- `collection_01.cpp` to `collection_07.cpp` - MongoDBCollection implementation
- `cursor_01.cpp`, `cursor_02.cpp` - MongoDBCursor implementation
- `document_01.cpp` to `document_06.cpp` - MongoDBData implementation

**Redis Driver** (`src/drivers/kv/redis/`):
- `redis_internal.hpp` - Internal declarations and shared definitions
- `driver_01.cpp` - RedisDriver implementation
- `connection_01.cpp` to `connection_06.cpp` - RedisConnection implementation

**ScyllaDB Driver** (`src/drivers/columnar/scylladb/`):
- `scylladb_internal.hpp` - Internal declarations and shared definitions
- `driver_01.cpp` - ScyllaDBDriver implementation
- `connection_01.cpp` - ScyllaDBConnection implementation
- `prepared_statement_01.cpp` to `prepared_statement_03.cpp` - ScyllaDBPreparedStatement implementation
- `result_set_01.cpp` to `result_set_03.cpp` - ScyllaDBResultSet implementation

**Other Implementation Files:**
- `src/core/document/document_db_connection_pool.cpp` - Document database connection pool implementation
- `src/core/columnar/columnar_db_connection_pool.cpp` - Columnar database connection pool implementation
- `src/core/kv/kv_db_connection_pool.cpp` - Key-value database connection pool implementation
- `benchmark/benchmark_mongodb_select.cpp` - MongoDB select operation benchmarks
- `benchmark/benchmark_mongodb_insert.cpp` - MongoDB insert operation benchmarks
- `benchmark/benchmark_mongodb_update.cpp` - MongoDB update operation benchmarks
- `benchmark/benchmark_mongodb_delete.cpp` - MongoDB delete operation benchmarks
- `benchmark/benchmark_scylladb_select.cpp` - ScyllaDB select operation benchmarks
- `benchmark/benchmark_scylladb_insert.cpp` - ScyllaDB insert operation benchmarks
- `benchmark/benchmark_scylladb_update.cpp` - ScyllaDB update operation benchmarks
- `benchmark/benchmark_scylladb_delete.cpp` - ScyllaDB delete operation benchmarks
- `benchmark/benchmark_redis_select.cpp` - Redis select operation benchmarks
- `benchmark/benchmark_redis_insert.cpp` - Redis insert operation benchmarks
- `benchmark/benchmark_redis_update.cpp` - Redis update operation benchmarks
- `benchmark/benchmark_redis_delete.cpp` - Redis delete operation benchmarks
- `test/test_mongodb_real.cpp` - MongoDB integration tests
- `test/test_mongodb_real_blob.cpp` - MongoDB BLOB handling tests
- `test/test_mongodb_real_json.cpp` - MongoDB JSON operation tests
- `test/test_mongodb_real_join.cpp` - MongoDB join operation tests
- `test/test_mongodb_thread_safe.cpp` - MongoDB thread safety tests
- `test/test_scylladb_connection.cpp` - ScyllaDB connection tests
- `test/test_scylladb_connection_pool.cpp` - ScyllaDB connection pool tests
- `test/test_scylladb_real.cpp` - ScyllaDB integration tests
- `test/test_scylladb_thread_safe.cpp` - ScyllaDB thread safety tests
- `test/test_redis_connection.cpp` - Redis connection tests
- `test/test_redis_connection_pool.cpp` - Redis connection pool tests
- `src/core/relational/relational_db_connection_pool.cpp` - Relational database connection pool implementation
- `src/transaction_manager.cpp` - Transaction manager implementation
- `src/driver_manager.cpp` - Driver manager implementation
- `src/common/system_utils.cpp` - System utilities implementation including stack trace functions
- `src/config/yaml_config_loader.cpp` - YAML configuration loader implementation

### Examples

Examples are organized by database family using a numeric naming convention: `XX_YZZ_example_<db>_<feature>.cpp`

**Common Examples (10_xxx):**
- `examples/common/10_011_example_config.cpp` - YAML configuration example
- `examples/common/10_012_example_config_integration.cpp` - Database configuration integration example
- `examples/common/example_config.yml` - Example YAML configuration file
- `examples/common/example_common.hpp` - Shared example helper functions

**Relational Database Examples:**
- MySQL (20_xxx): 9 examples in `examples/relational/mysql/`
- PostgreSQL (21_xxx): 9 examples in `examples/relational/postgresql/`
- SQLite (22_xxx): 9 examples in `examples/relational/sqlite/`
- Firebird (23_xxx): 10 examples in `examples/relational/firebird/`

**Key-Value Examples:**
- Redis (24_xxx): 7 examples in `examples/kv/redis/`

**Document Database Examples:**
- MongoDB (25_xxx): 5 examples in `examples/document/mongodb/`

**Columnar Database Examples:**
- ScyllaDB (26_xxx): 7 examples in `examples/columnar/scylladb/`

Each database family includes examples for: basic operations, connection info, connection pooling, transactions (where applicable), JSON operations (where supported), BLOB operations, JOIN operations (relational only), batch operations, and error handling.

Use `./libs/cpp_dbc/run_examples.sh --list` to see all available examples, or `--run='pattern'` to run specific examples with wildcard support.

## Building

To build the library and examples, you'll need:

1. A modern C++ compiler with C++23 support
2. MySQL development libraries (libmysqlclient-dev)
3. PostgreSQL development libraries (libpq-dev) (optional)
4. SQLite development libraries (libsqlite3-dev) (optional)
5. Firebird development libraries (libfbclient2, firebird-dev) (optional)
6. MongoDB C++ Driver (libmongoc-dev, libbson-dev, libmongocxx-dev, libbsoncxx-dev) (optional)
7. Cassandra C++ Driver (libcassandra-dev) for ScyllaDB support (optional)
8. Hiredis library (libhiredis-dev) for Redis support (optional)
9. yaml-cpp development libraries (optional)
10. libdw development libraries (part of elfutils, optional)
11. CMake 3.15 or later
12. Conan for dependency management

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
18. Connection Pool Factory Pattern implementation:
    - **Factory Methods:**
      - Static `create` methods for all connection pool types (both relational and document-based)
      - Multiple overloads accepting configuration objects or direct parameters
      - Protected constructors to enforce creation via factory methods
    - **Memory Safety Improvements:**
      - `std::enable_shared_from_this` inheritance for safe self-reference
      - Two-phase initialization with separate `initializePool()` method called after construction
      - Safer resource management with weak_ptr reference tracking
      - Removal of raw pointer references in pooled connections
    - **API Benefits:**
      - Cleaner client code with explicit ownership semantics
      - Better thread safety in multi-threaded environments
      - Prevents use-after-free issues with pool and connection lifetimes
      - Consistent creation pattern across all database types
    - **Example Usage:**
      ```cpp
      // Using factory method with configuration
      auto config = cpp_dbc::config::DBConnectionPoolConfig();
      config.setUrl("mongodb://localhost:27017/test");
      auto pool = cpp_dbc::MongoDB::MongoDBConnectionPool::create(config);
      
      // Using factory method with direct parameters
      auto pool = cpp_dbc::MongoDB::MongoDBConnectionPool::create(
          "mongodb://localhost:27017/test", "username", "password");
      ```
19. Document database connection pool implementation:
    - **MongoDB-Specific Pool Implementation:**
      - `DocumentDBConnectionPool` base class with MongoDB-specific `MongoDBConnectionPool` implementation
      - Thread-safe connection acquisition and release with mutex protection
      - Connection pooling with minimum idle connections and maximum size limits
    - **Resource Management:**
      - Smart pointer-based pool lifetime tracking for memory safety
      - Automatic connection validation on borrow with ping command
      - Connection cleanup with proper resource release
      - Background maintenance thread for removing idle and expired connections
    - **Configuration Options:**
      - Configurable initial size, maximum size, and minimum idle connections
      - Adjustable timeout settings (max wait, validation, idle, lifetime)
      - Connection validation options (on borrow, on return)
      - Custom validation query support
    - **Documentation and Testing:**
      - Comprehensive example in `document_connection_pool_example.cpp`
      - Thorough test coverage in `test_mongodb_connection_pool.cpp`
      - Performance testing in MongoDB benchmarks
20. MongoDB benchmark support for select, insert, update, and delete operations with detailed performance metrics:
    - Document insert benchmarks (single and batch operations)
    - Document query benchmarks with different filter complexities
    - Document update benchmarks with various update operators
    - Document delete benchmarks (single and multiple document operations)
    - Memory usage tracking for MongoDB operations
    - Performance comparison against baseline measurements
    - Support for different document sizes and collection scales
21. Extensive testing suite for MongoDB including:
    - JSON operations (basic, nested, arrays)
    - JSON query operators ($eq, $gt, $lt, etc.)
    - JSON updates and modifications
    - JSON aggregation operations
    - Thread safety with multiple connections
    - Thread safety with shared connection
    - Join operations using MongoDB aggregation pipeline
22. Memory usage tracking for all database operations in benchmarks

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
│       │           │   ├── driver_mysql.hpp         (aggregator)
│       │           │   ├── driver_postgresql.hpp    (aggregator)
│       │           │   ├── driver_sqlite.hpp        (aggregator)
│       │           │   ├── driver_firebird.hpp      (aggregator)
│       │           │   ├── mysql/
│       │           │   │   ├── handles.hpp
│       │           │   │   ├── input_stream.hpp
│       │           │   │   ├── blob.hpp
│       │           │   │   ├── result_set.hpp
│       │           │   │   ├── prepared_statement.hpp
│       │           │   │   ├── connection.hpp
│       │           │   │   └── driver.hpp
│       │           │   ├── postgresql/
│       │           │   │   ├── handles.hpp
│       │           │   │   ├── input_stream.hpp
│       │           │   │   ├── blob.hpp
│       │           │   │   ├── result_set.hpp
│       │           │   │   ├── prepared_statement.hpp
│       │           │   │   ├── connection.hpp
│       │           │   │   └── driver.hpp
│       │           │   ├── sqlite/
│       │           │   │   ├── handles.hpp
│       │           │   │   ├── input_stream.hpp
│       │           │   │   ├── blob.hpp
│       │           │   │   ├── result_set.hpp
│       │           │   │   ├── prepared_statement.hpp
│       │           │   │   ├── connection.hpp
│       │           │   │   └── driver.hpp
│       │           │   └── firebird/
│       │           │       ├── handles.hpp
│       │           │       ├── input_stream.hpp
│       │           │       ├── blob.hpp
│       │           │       ├── result_set.hpp
│       │           │       ├── prepared_statement.hpp
│       │           │       ├── connection.hpp
│       │           │       └── driver.hpp
│       │           ├── columnar/
│       │           │   ├── driver_scylladb.hpp      (aggregator)
│       │           │   └── scylladb/
│       │           │       ├── handles.hpp
│       │           │       ├── memory_input_stream.hpp
│       │           │       ├── result_set.hpp
│       │           │       ├── prepared_statement.hpp
│       │           │       ├── connection.hpp
│       │           │       └── driver.hpp
│       │           ├── document/
│       │           │   ├── driver_mongodb.hpp       (aggregator)
│       │           │   └── mongodb/
│       │           │       ├── handles.hpp
│       │           │       ├── document.hpp
│       │           │       ├── cursor.hpp
│       │           │       ├── collection.hpp
│       │           │       ├── connection.hpp
│       │           │       └── driver.hpp
│       │           ├── graph/
│       │           ├── kv/
│       │           │   ├── driver_redis.hpp         (aggregator)
│       │           │   └── redis/
│       │           │       ├── handles.hpp
│       │           │       ├── reply_handle.hpp
│       │           │       ├── connection.hpp
│       │           │       └── driver.hpp
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
│       │   ├── common/
│       │   │   ├── 10_011_example_config.cpp
│       │   │   ├── 10_012_example_config_integration.cpp
│       │   │   ├── example_common.hpp
│       │   │   ├── example_config.yml
│       │   │   └── test.jpg
│       │   ├── relational/
│       │   │   ├── mysql/           # 20_xxx series - 9 examples
│       │   │   ├── postgresql/      # 21_xxx series - 9 examples
│       │   │   ├── sqlite/          # 22_xxx series - 9 examples
│       │   │   ├── firebird/        # 23_xxx series - 10 examples
│       │   │   └── example_relational_common.hpp
│       │   ├── kv/
│       │   │   └── redis/           # 24_xxx series - 7 examples
│       │   ├── document/
│       │   │   └── mongodb/         # 25_xxx series - 5 examples
│       │   ├── columnar/
│       │   │   └── scylladb/        # 26_xxx series - 7 examples
│       │   ├── graph/               # Placeholder for future graph DB examples
│       │   ├── timeseries/          # Placeholder for future time-series DB examples
│       │   └── run_examples.sh      # Example discovery and execution script
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
