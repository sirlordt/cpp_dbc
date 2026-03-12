# CPP_DBC Project Brief

## Project Overview

CPP_DBC is a C++ Database Connectivity library inspired by Java's JDBC (Java Database Connectivity). It provides a unified interface for connecting to and interacting with different database systems, currently supporting:
- **Relational databases**: MySQL, PostgreSQL, SQLite, and Firebird SQL
- **Document databases**: MongoDB
- **Columnar databases**: ScyllaDB
- **Key-value databases**: Redis

## Core Goals

1. Provide a consistent API for database operations across different database systems
2. Support relational, document, columnar, and key-value database paradigms
3. Support prepared statements for secure and efficient database queries
4. Implement connection pooling for efficient connection management (all database types)
5. Support transaction management across multiple threads with configurable isolation levels
6. Provide comprehensive BLOB (Binary Large Object) support for all relational database systems
7. Support document operations (CRUD) for document databases
8. Implement columnar operations for ScyllaDB and other columnar databases
9. Implement key-value operations for Redis and other key-value databases
10. Support JSON data types in MySQL and PostgreSQL
11. Maintain a clean, object-oriented design with proper abstraction layers
12. Ensure high code quality through comprehensive warning flags and compile-time checks
13. Provide Doxygen-compatible API documentation with inline code examples across all public headers

## Key Features

- Abstract interfaces for database operations (Connection, PreparedStatement, ResultSet)
- Document database interfaces (DocumentDBConnection, DocumentDBCollection, DocumentDBCursor)
- Columnar database interfaces (ColumnarDBConnection, ColumnarDBDriver, ColumnarDBConnectionPool)
- Key-value database interfaces (KVDBConnection, KVDBDriver)
- Driver-based architecture for database-specific implementations
- Connection pooling with configurable parameters for all database types
- Transaction management with support for distributed transactions and isolation levels
- Comprehensive BLOB support with streaming capabilities
- Native JSON data type support for MySQL and PostgreSQL
- Document CRUD operations for MongoDB
- Columnar operations for ScyllaDB (CQL queries, prepared statements)
- Key-value operations for Redis (strings, lists, hashes, sets, sorted sets)
- Support for MySQL, PostgreSQL, SQLite, Firebird SQL, MongoDB, ScyllaDB, and Redis databases
- Exception-free API with nothrow variants returning `expected<T, DBException>`
- `-fno-exceptions` compatibility: all 7 drivers guard throwing methods with `#ifdef __cpp_exceptions`; nothrow API always compiles
- Static factory pattern for all connection classes (e.g., `MySQLDBConnection::create`, `FirebirdDBConnection::create`, `MongoDBConnection::create`, `ScyllaDBConnection::create`, `RedisDBConnection::create`)
- Double-checked locking for driver initialization (all drivers use `std::atomic<bool>` + `std::mutex`, not `std::once_flag`)
- Unified URI API in `DBDriver` base class: `acceptURI`, `parseURI`, `buildURI`, `getURIScheme` pure virtuals (2026-03-07)
- Unified version/info API: `getDriverVersion()` in `DBDriver`, `getServerVersion()` + `getServerInfo()` in `DBConnection` — all 7 drivers return driver-specific metadata maps (2026-03-08)
- Strict warning flags and compile-time checks for robust code
- Doxygen-compatible API documentation with inline code examples across all public headers

## Technical Requirements

- C++23 or later
- MySQL client library (for MySQL support)
- PostgreSQL client library (for PostgreSQL support)
- SQLite library (for SQLite support)
- Firebird client library (for Firebird SQL support)
- MongoDB C++ driver (for MongoDB support)
- Cassandra C++ driver (for ScyllaDB support)
- Hiredis library (for Redis support)
- Thread-safe implementation for connection pooling and transaction management
- Comprehensive warning flags: -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wformat=2 -Wunused -Werror=return-type -Werror=switch -Wdouble-promotion -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align

## Project Structure

- Core interfaces and classes in `cpp_dbc.hpp`
- Core relational interfaces in `core/relational/` directory
- Core document interfaces in `core/document/` directory
- Core columnar interfaces in `core/columnar/` directory
- Core key-value interfaces in `core/kv/` directory
- BLOB support in `blob.hpp`
- Relational database implementations in the `drivers/relational/` directory (split into subdirectories)
- Document database implementations in the `drivers/document/` directory (split into subdirectories)
- Columnar database implementations in the `drivers/columnar/` directory (split into subdirectories)
- Key-value database implementations in the `drivers/kv/` directory (split into subdirectories)
- Each driver header (`driver_*.hpp`) is split into per-class `.hpp` files in a driver subfolder:
  - `handles.hpp`: RAII custom deleters and smart pointer type aliases
  - Per-class headers: `connection.hpp`, `driver.hpp`, `prepared_statement.hpp`, `result_set.hpp`, `blob.hpp`, etc.
  - Original `driver_*.hpp` serves as a pure aggregator with `#include` directives
- Each driver has a dedicated subdirectory with split implementation files:
  - `*_internal.hpp`: Internal declarations and shared definitions
  - `driver_*.cpp`, `connection_*.cpp`, `prepared_statement_*.cpp`, `result_set_*.cpp`
- Database-specific BLOB implementations in the driver subfolders (e.g., `drivers/relational/mysql/blob.hpp`)
- Placeholder directories for future database types:
  - `core/graph/`, `core/timeseries/`
  - `drivers/graph/`, `drivers/timeseries/`
- Connection pooling implementations:
  - `pool/connection_pool.hpp` + `pool/connection_pool.cpp` — unified base class (`DBConnectionPoolBase`) with all pool infrastructure
  - `pool/pooled_db_connection_base.hpp` + `.cpp` — CRTP base `PooledDBConnectionBase<D,C,P>` with common pooled connection logic (close/returnToPool race-condition fix, destructor cleanup, pool metadata)
  - `pool/relational/relational_db_connection_pool.cpp` for relational databases (thin CRTP delegator + pool derived class)
  - `pool/document/document_db_connection_pool.cpp` for document databases (thin CRTP delegator + pool derived class)
  - `pool/columnar/columnar_db_connection_pool.cpp` for columnar databases (thin CRTP delegator + pool derived class)
  - `pool/kv/kv_db_connection_pool.cpp` for key-value databases (thin CRTP delegator + pool derived class)
- Transaction management in `transaction_manager.hpp` and `transaction_manager.cpp`
- Example code in the `examples/` directory
- Documentation in the `docs/` directory
