# CPP_DBC System Patterns

## System Architecture

CPP_DBC follows a layered architecture with clear separation of concerns:

1. **Interface Layer**: Abstract base classes defining the API (`Connection`, `PreparedStatement`, `ResultSet`, etc.) in the `include/cpp_dbc/` directory
2. **Driver Layer**: Database-specific implementations of the interfaces in the `src/drivers/` directory
3. **Connection Management Layer**: Connection pooling and transaction management in the `src/` directory
4. **BLOB Layer**: Binary Large Object handling in the `include/cpp_dbc/` directory and database-specific implementations in the `drivers/` directory
5. **JSON Layer**: JSON data type support in database-specific implementations in the `drivers/` directory
6. **Configuration Layer**: Database configuration management in the `include/cpp_dbc/config/` and `src/config/` directories
7. **Code Quality Layer**: Comprehensive warning flags and compile-time checks across all components
8. **Client Application Layer**: User code that interacts with the library

The architecture follows this flow:
```
Client Application → DriverManager → Driver → Connection → PreparedStatement/ResultSet
                   → ConnectionPool → PooledConnection → Connection
                   → TransactionManager → Connection
                   → Blob → InputStream/OutputStream
                   → JSON Operations → Database-specific JSON functions
                   → DatabaseConfigManager → DatabaseConfig → Connection
                   → Code Quality Checks → All Components
```

## Design Patterns

### Abstract Factory Pattern
- `DriverManager` acts as a factory for creating database-specific `Driver` instances
- Each `Driver` implementation creates its own `Connection` implementations

### Factory Method Pattern
- `Connection` implementations create their own `PreparedStatement` and `ResultSet` implementations

### Adapter Pattern
- Database-specific implementations adapt the native database client libraries to the common interfaces

### Proxy Pattern
- `PooledConnection` acts as a proxy for the actual `Connection`, adding connection pooling behavior
- When `close()` is called on a `PooledConnection`, it returns to the pool instead of actually closing

### Singleton Pattern
- `DriverManager` is implemented as a singleton with static methods

### Command Pattern
- `PreparedStatement` encapsulates a database operation that can be parameterized and executed later

### Object Pool Pattern
- `ConnectionPool` manages a pool of database connections for efficient reuse

### Strategy Pattern
- Different database drivers implement different strategies for executing SQL operations

### Builder Pattern
- `DatabaseConfig` uses a builder-like pattern with setter methods for constructing database configurations
- This allows for flexible configuration creation with optional parameters

### Bridge Pattern
- The configuration system separates the abstraction (`DatabaseConfigManager`) from implementation (`YamlConfigLoader`)
- This allows for different configuration sources without changing the core configuration classes

### Template Method Pattern
- The `Blob`, `InputStream`, and `OutputStream` abstract classes define template methods
- Concrete implementations like `MemoryBlob`, `FileInputStream`, etc. provide specific implementations

## Key Technical Decisions

### Thread Safety
- `ConnectionPool` uses mutexes and condition variables for thread-safe connection management
- `TransactionManager` provides thread-safe transaction handling across multiple threads
- `SQLiteDriver` implements thread-safe initialization using std::atomic and std::mutex
  - Uses singleton pattern for SQLite configuration to ensure it's only done once
  - Static class members track initialization state for thread safety
  - Prevents race conditions during SQLite library initialization
- Connection objects are not thread-safe and should not be shared between threads

### Resource Management
- Smart pointers (`std::shared_ptr`) are used for automatic resource management
- RAII (Resource Acquisition Is Initialization) principle is followed for resource cleanup

### Error Handling
- Custom `SQLException` class for consistent error reporting
- Exceptions are used for error propagation throughout the library
- Enhanced `DBException` with stack trace capture and unique error marks
- Member variables prefixed with `m_` to avoid shadowing issues in exception handling

### Connection Pooling
- Configurable connection pool with parameters for initial size, max size, validation, etc.
- Automatic connection validation and cleanup
- Background maintenance thread for pool management

### Transaction Management
- Unique transaction IDs for tracking transactions across threads
- Automatic transaction timeout and cleanup
- Support for distributed transactions across multiple operations
- JDBC-compatible transaction isolation levels (READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE)
- Database-specific implementation of isolation levels with appropriate defaults

### Configuration Management
- Optional YAML configuration support with conditional compilation
- Database configurations stored in a central manager for easy access
- Connection pool configurations customizable through YAML
- Test queries configurable for different database types
- Extensible design to support additional configuration formats

### BLOB Management
- Abstract `Blob`, `InputStream`, and `OutputStream` interfaces for consistent binary data handling
- Memory-based implementations for in-memory BLOB operations
- File-based implementations for file I/O operations
- Database-specific BLOB implementations for each supported database
- Streaming support for efficient handling of large binary data

### JSON Management
- Database-specific JSON implementations for MySQL and PostgreSQL
- Support for JSON data types in both databases
- Comprehensive query capabilities for JSON data:
  - MySQL: JSON_EXTRACT, JSON_SEARCH, JSON_CONTAINS, JSON_ARRAYAGG
  - PostgreSQL: JSON operators (@>, <@, ?, ?|, ?&), jsonb_set, jsonb_insert
- Support for indexing JSON fields for performance optimization
- JSON validation functions for data integrity

### Code Quality Management
- Comprehensive warning flags for strict compile-time checks:
  - `-Wall -Wextra -Wpedantic`: Basic warning sets
  - `-Wconversion`: Warns about implicit type conversions
  - `-Wshadow`: Warns about variable shadowing
  - `-Wcast-qual`: Warns about cast that removes type qualifiers
  - `-Wformat=2`: Warns about printf/scanf format string issues
  - `-Wunused`: Warns about unused variables and functions
  - `-Werror=return-type`: Treats missing return statements as errors
  - `-Werror=switch`: Treats missing switch cases as errors
  - `-Wdouble-promotion`: Warns about float to double promotions
  - `-Wfloat-equal`: Warns about floating-point equality comparisons
  - `-Wundef`: Warns about undefined identifiers in preprocessor
  - `-Wpointer-arith`: Warns about pointer arithmetic issues
  - `-Wcast-align`: Warns about pointer cast that increases alignment
- Special handling for third-party headers:
  - Custom handling for backward.hpp to silence -Wundef warnings
- Consistent naming conventions:
  - Member variables prefixed with `m_` to avoid shadowing issues
- Type safety improvements:
  - Using static_cast<> for numeric conversions
  - Changing int return types to uint64_t for executeUpdate() methods

## Component Relationships

### Driver Components
- `Driver` → Creates → `Connection`
- `Connection` → Creates → `PreparedStatement`
- `Connection` → Creates → `ResultSet` (via direct query)
- `PreparedStatement` → Creates → `ResultSet` (via prepared query)
- `ResultSet` → Creates → `Blob` (via getBlob method)
- `ResultSet` → Creates → `InputStream` (via getBinaryStream method)

### Connection Pool Components
- `ConnectionPool` → Manages → `PooledConnection`
- `PooledConnection` → Wraps → `Connection`
- `ConnectionPool` → Creates → Physical `Connection` (via Driver)

### Transaction Components
- `TransactionManager` → Manages → `TransactionContext`
- `TransactionContext` → Contains → `Connection`
- `TransactionManager` → Uses → `ConnectionPool` (to obtain connections)

### Configuration Components
- `DatabaseConfigManager` → Manages → `DatabaseConfig`
- `DatabaseConfig` → Contains → Connection parameters
- `YamlConfigLoader` → Creates → `DatabaseConfigManager` (from YAML file)
- `ConnectionPoolConfig` → Configures → `ConnectionPool`

### BLOB Components
- `Blob` → Uses → `InputStream` (for reading)
- `Blob` → Uses → `OutputStream` (for writing)
- `MemoryBlob` → Implements → `Blob` (for in-memory BLOBs)
- `MySQLBlob` → Implements → `Blob` (for MySQL BLOBs)
- `PostgreSQLBlob` → Implements → `Blob` (for PostgreSQL BLOBs)
- `SQLiteBlob` → Implements → `Blob` (for SQLite BLOBs)

## Critical Implementation Paths

### Database Operation Flow
1. Client obtains a `Connection` (either directly or from a pool)
2. Client creates a `PreparedStatement` with SQL
3. Client sets parameters on the `PreparedStatement` (including BLOB data if needed)
4. Client executes the `PreparedStatement` and gets a `ResultSet`
5. Client processes the `ResultSet` (including retrieving BLOB or JSON data if needed)
6. Resources are cleaned up (automatically with smart pointers)
7. Code quality checks ensure type safety and prevent common C++ issues

### Connection Pooling Flow
1. Client requests a connection from the `ConnectionPool`
2. Pool checks for available idle connections
3. If available, a connection is validated and returned
4. If not available, a new connection is created (if below max size) or the client waits
5. Client uses the connection
6. When client calls `close()`, the connection returns to the pool

### Transaction Management Flow
1. Client begins a transaction via `TransactionManager`
2. Transaction ID is generated and returned to the client
3. Client uses the transaction ID to obtain the associated connection
4. Client can set a specific isolation level on the connection if needed
5. Client performs operations using the connection
6. Client commits or rolls back the transaction using the transaction ID
7. `TransactionManager` handles the actual commit/rollback on the connection

### BLOB Operation Flow
1. Client retrieves a BLOB from a `ResultSet` using `getBlob()`, `getBinaryStream()`, or `getBytes()`
2. Client processes the BLOB data (read, modify, etc.)
3. Client can create a new BLOB using `MemoryBlob` or database-specific BLOB implementations
4. Client sets the BLOB on a `PreparedStatement` using `setBlob()`, `setBinaryStream()`, or `setBytes()`
5. Client executes the `PreparedStatement` to store the BLOB in the database

### JSON Operation Flow
1. Client creates a JSON string using standard JSON formatting
2. Client sets the JSON string on a `PreparedStatement` using `setString()`
3. Client executes the `PreparedStatement` to store the JSON in the database
4. Client retrieves JSON data from a `ResultSet` using `getString()`
5. Client can use database-specific JSON functions and operators in SQL queries
6. Client processes the JSON data using standard JSON parsing techniques

### Configuration Flow
1. Client loads configuration from a YAML file using `YamlConfigLoader`
2. `YamlConfigLoader` parses the YAML file and creates a `DatabaseConfigManager`
3. `DatabaseConfigManager` contains multiple `DatabaseConfig` objects
4. Client retrieves a specific `DatabaseConfig` by name
5. Client uses the `DatabaseConfig` to create a connection string
6. Connection string is used with `DriverManager` to create a connection
