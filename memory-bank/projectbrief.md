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
- Strict warning flags and compile-time checks for robust code

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
- Relational database implementations in the `drivers/relational/` directory
- Document database implementations in the `drivers/document/` directory
- Columnar database implementations in the `drivers/columnar/` directory
- Key-value database implementations in the `drivers/kv/` directory
- Database-specific BLOB implementations in the `drivers/relational/` directory
- Placeholder directories for future database types:
  - `core/graph/`, `core/timeseries/`
  - `drivers/graph/`, `drivers/timeseries/`
- Connection pooling implementations:
  - `core/relational/relational_db_connection_pool.cpp` for relational databases
  - `core/document/document_db_connection_pool.cpp` for document databases
  - `core/columnar/columnar_db_connection_pool.cpp` for columnar databases
  - `core/kv/kv_db_connection_pool.cpp` for key-value databases
- Transaction management in `transaction_manager.hpp` and `transaction_manager.cpp`
- Example code in the `examples/` directory
- Documentation in the `docs/` directory
