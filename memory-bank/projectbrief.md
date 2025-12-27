# CPP_DBC Project Brief

## Project Overview

CPP_DBC is a C++ Database Connectivity library inspired by Java's JDBC (Java Database Connectivity). It provides a unified interface for connecting to and interacting with different database systems, currently supporting MySQL, PostgreSQL, SQLite, and Firebird SQL.

## Core Goals

1. Provide a consistent API for database operations across different database systems
2. Support prepared statements for secure and efficient database queries
3. Implement connection pooling for efficient connection management
4. Support transaction management across multiple threads with configurable isolation levels
5. Provide comprehensive BLOB (Binary Large Object) support for all database systems
6. Support JSON data types in MySQL and PostgreSQL
7. Maintain a clean, object-oriented design with proper abstraction layers
8. Ensure high code quality through comprehensive warning flags and compile-time checks

## Key Features

- Abstract interfaces for database operations (Connection, PreparedStatement, ResultSet)
- Driver-based architecture for database-specific implementations
- Connection pooling with configurable parameters
- Transaction management with support for distributed transactions and isolation levels
- Comprehensive BLOB support with streaming capabilities
- Native JSON data type support for MySQL and PostgreSQL
- Support for MySQL, PostgreSQL, SQLite, and Firebird SQL databases
- Strict warning flags and compile-time checks for robust code

## Technical Requirements

- C++23 or later
- MySQL client library (for MySQL support)
- PostgreSQL client library (for PostgreSQL support)
- SQLite library (for SQLite support)
- Firebird client library (for Firebird SQL support)
- Thread-safe implementation for connection pooling and transaction management
- Comprehensive warning flags: -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wformat=2 -Wunused -Werror=return-type -Werror=switch -Wdouble-promotion -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align

## Project Structure

- Core interfaces and classes in `cpp_dbc.hpp`
- Core relational interfaces in `core/relational/` directory
- BLOB support in `blob.hpp`
- Database-specific implementations in the `drivers/relational/` directory
- Database-specific BLOB implementations in the `drivers/relational/` directory
- Placeholder directories for future database types:
  - `core/columnar/`, `core/document/`, `core/graph/`, `core/kv/`, `core/timeseries/`
  - `drivers/columnar/`, `drivers/document/`, `drivers/graph/`, `drivers/kv/`, `drivers/timeseries/`
- Connection pooling implementation in `connection_pool.hpp` and `connection_pool.cpp`
- Transaction management in `transaction_manager.hpp` and `transaction_manager.cpp`
- Example code in the `examples/` directory
- Documentation in the `docs/` directory
