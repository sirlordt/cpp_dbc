# CPP_DBC Project Brief

## Project Overview

CPP_DBC is a C++ Database Connectivity library inspired by Java's JDBC (Java Database Connectivity). It provides a unified interface for connecting to and interacting with different database systems, currently supporting MySQL and PostgreSQL.

## Core Goals

1. Provide a consistent API for database operations across different database systems
2. Support prepared statements for secure and efficient database queries
3. Implement connection pooling for efficient connection management
4. Support transaction management across multiple threads with configurable isolation levels
5. Maintain a clean, object-oriented design with proper abstraction layers

## Key Features

- Abstract interfaces for database operations (Connection, PreparedStatement, ResultSet)
- Driver-based architecture for database-specific implementations
- Connection pooling with configurable parameters
- Transaction management with support for distributed transactions and isolation levels
- Support for MySQL and PostgreSQL databases

## Technical Requirements

- C++11 or later
- MySQL client library (for MySQL support)
- PostgreSQL client library (for PostgreSQL support)
- Thread-safe implementation for connection pooling and transaction management

## Project Structure

- Core interfaces and classes in `cpp_dbc.hpp`
- Database-specific implementations in the `drivers/` directory
- Connection pooling implementation in `connection_pool.hpp` and `connection_pool.cpp`
- Transaction management in `transaction_manager.hpp` and `transaction_manager.cpp`
- Example code in the `examples/` directory
- Documentation in the `docs/` directory
