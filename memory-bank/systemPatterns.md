# CPP_DBC System Patterns

## System Architecture

CPP_DBC follows a layered architecture with clear separation of concerns:

1. **Core Interface Layer**: Abstract base classes defining the API in the `include/cpp_dbc/core/` directory
   - `core/relational/`: Relational database interfaces (`RelationalDBConnection`, `RelationalDBPreparedStatement`, `RelationalDBResultSet`, etc.)
   - `core/document/`: Document database interfaces (`DocumentDBConnection`, `DocumentDBDriver`, `DocumentDBCollection`, `DocumentDBCursor`, `DocumentDBData`)
   - `core/columnar/`: Columnar database interfaces (`ColumnarDBConnection`, `ColumnarDBDriver`, `ColumnarDBConnectionPool`)
   - `core/graph/`: Graph database interfaces (placeholder for future)
   - `core/kv/`: Key-Value database interfaces (`KVDBConnection`, `KVDBDriver`, `KVDBConnectionPool`)
   - `core/timeseries/`: Time-series database interfaces (placeholder for future)
2. **Driver Layer**: Database-specific implementations in the `src/drivers/` and `include/cpp_dbc/drivers/` directories
   - `drivers/relational/`: Relational database drivers (MySQL, PostgreSQL, SQLite, Firebird)
   - `drivers/document/`: Document database drivers (MongoDB)
   - `drivers/columnar/`: Columnar database drivers (ScyllaDB)
   - `drivers/graph/`: Graph database drivers (placeholder for future)
   - `drivers/kv/`: Key-Value database drivers (Redis)
   - `drivers/timeseries/`: Time-series database drivers (placeholder for future)
   - **Header Split Architecture (One-Class-Per-File)**: Each `driver_*.hpp` is split into per-class `.hpp` files in a driver subfolder:
     - `handles.hpp`: RAII custom deleters and smart pointer type aliases
     - `connection.hpp`, `driver.hpp`, `prepared_statement.hpp`, `result_set.hpp`, `blob.hpp`, `input_stream.hpp`
     - Original `driver_*.hpp` serves as a pure aggregator with `#include` directives
     - BLOB classes now in driver subfolders (e.g., `mysql/blob.hpp`) instead of separate `*_blob.hpp` files
   - **Implementation Split Architecture**: Each driver .cpp is split into multiple focused files:
     - `*_internal.hpp`: Internal declarations and shared definitions
     - `driver_*.cpp`: Driver class implementation
     - `connection_*.cpp`: Connection class implementation
     - `prepared_statement_*.cpp`: PreparedStatement class implementation
     - `result_set_*.cpp`: ResultSet class implementation
     - MongoDB-specific: `collection_*.cpp`, `cursor_*.cpp`, `document_*.cpp`
3. **Connection Management Layer**: Connection pooling and transaction management in the `src/` directory
4. **BLOB Layer**: Binary Large Object handling in the `include/cpp_dbc/blob.hpp` (base classes) and database-specific implementations in the driver subfolders (e.g., `drivers/relational/mysql/blob.hpp`)
5. **Key-Value Layer**: Key-Value operations support in `drivers/kv/` directory
6. **JSON Layer**: JSON data type support in database-specific implementations in the `drivers/relational/` directory
6. **Configuration Layer**: Database configuration management in the `include/cpp_dbc/config/` and `src/config/` directories
7. **Code Quality Layer**: Comprehensive warning flags and compile-time checks across all components
8. **Client Application Layer**: User code that interacts with the library

The architecture follows this flow:
```
Relational Databases:
Client Application → DriverManager → Driver → RelationalDBConnection → RelationalDBPreparedStatement/RelationalDBResultSet
                   → RelationalDBConnectionPool → PooledRelationalDBConnection → RelationalDBConnection
                   → TransactionManager → RelationalDBConnection
                   → Blob → InputStream/OutputStream
                   → JSON Operations → Database-specific JSON functions
                   → DatabaseConfigManager → DatabaseConfig → RelationalDBConnection
                   → Code Quality Checks → All Components

Document Databases:
Client Application → DriverManager → DocumentDBDriver → DocumentDBConnection → DocumentDBCollection
                    → DocumentDBCollection → DocumentDBCursor → DocumentDBData
                    → DocumentDBConnectionPool → DocumentPooledDBConnection → DocumentDBConnection
                    → DatabaseConfigManager → DatabaseConfig → DocumentDBConnection
                    → Code Quality Checks → All Components

Key-Value Databases:
Client Application → DriverManager → KVDBDriver → KVDBConnection
                    → KVDBConnection → Key-Value Operations (get, set, hash, list, set, etc.)
                    → KVDBConnectionPool → KVPooledDBConnection → KVDBConnection
                    → DatabaseConfigManager → DatabaseConfig → KVDBConnection
                    → Code Quality Checks → All Components

Columnar Databases:
Client Application → DriverManager → ColumnarDBDriver → ColumnarDBConnection → ColumnarDBPreparedStatement/ColumnarDBResultSet
                    → ColumnarDBConnectionPool → ColumnarPooledDBConnection → ColumnarDBConnection
                    → DatabaseConfigManager → DatabaseConfig → ColumnarDBConnection
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
- Smart pointers are used for automatic resource management:
  - `std::shared_ptr` for connection handles (allows `weak_ptr` references from PreparedStatements)
  - `std::unique_ptr` with custom deleters for result sets and prepared statements
  - `std::weak_ptr` for safe references from PreparedStatements to Connections (detects closed connections)
- Custom deleters ensure proper cleanup of database-specific resources:
  - MySQL: `MySQLDeleter`, `MySQLStmtDeleter`, `MySQLResDeleter`
  - PostgreSQL: `PGconnDeleter`, `PGresultDeleter`
  - SQLite: `SQLiteDbDeleter`, `SQLiteStmtDeleter`
  - Firebird: `FirebirdDbDeleter`, `FirebirdStmtDeleter`
- RAII (Resource Acquisition Is Initialization) principle is followed for resource cleanup even in case of exceptions

### Error Handling
- Custom `SQLException` class for consistent error reporting
- Exceptions are used for error propagation throughout the library
- Enhanced `DBException` with stack trace capture and unique error marks
- Member variables prefixed with `m_` to avoid shadowing issues in exception handling

### Connection Pooling
- Configurable connection pool with parameters for initial size, max size, validation, etc.
- Automatic connection validation and cleanup
- Background maintenance thread for pool management
- Memory safety with smart pointers:
  - `RelationalDBConnectionPool` uses `m_poolAlive` shared atomic flag to track pool lifetime
  - `RelationalDBPooledConnection` uses `weak_ptr` for pool reference
  - Added `isPoolValid()` helper method to check if pool is still alive
  - Pool sets `m_poolAlive` to `false` in `close()` before cleanup
  - Prevents use-after-free when pool is destroyed while connections are in use
  - Graceful handling of connection return when pool is already closed
- Deadlock prevention with `std::scoped_lock`:
  - All connection pool implementations use `std::scoped_lock` for consistent lock ordering
  - Prevents deadlock when acquiring multiple mutexes (`m_mutexAllConnections` and `m_mutexIdleConnections`)
  - Applied to all database types: relational, document, columnar, and key-value pools

### Connection Pooling (continued)
- Connection pool implementations for different database types:
  - `RelationalDBConnectionPool` for relational databases with factory pattern
  - `DocumentDBConnectionPool` for document databases with factory pattern
  - `ColumnarDBConnectionPool` for columnar databases with factory pattern
  - `KVDBConnectionPool` for key-value databases with factory pattern
- Each connection pool implementation follows the same architecture:
  - Abstract base class defining the pool interface
  - Concrete implementations for specific database types (PostgreSQLConnectionPool, MongoDBConnectionPool, ScyllaConnectionPool, RedisConnectionPool, etc.)
  - Factory methods (`create`) for creating pool instances with shared_ptr ownership
  - ConstructorTag pattern (PassKey idiom) to enable `std::make_shared` while enforcing factory pattern:
    - `DBConnectionPool::ConstructorTag` is a protected struct that acts as an access token
    - Constructors are public but require the tag, which can only be created within the class hierarchy
    - Enables single memory allocation with `std::make_shared` instead of separate allocations
  - Resource cleanup with proper exception handling
  - Race condition prevention in connection return flow:
    - `m_closed` flag reset BEFORE `returnConnection()` to prevent race window
    - Catch-all exception handlers ensure correct state on any exception

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
- Memory safety with smart pointers:
  - All BLOB implementations use `weak_ptr` for safe connection references
  - `FirebirdBlob`: Uses `weak_ptr<FirebirdConnection>` with `getConnection()` helper
  - `MySQLBlob`: Uses `weak_ptr<MYSQL>` with `getMySQLConnection()` helper
  - `PostgreSQLBlob`: Uses `weak_ptr<PGconn>` with `getPGConnection()` helper
  - `SQLiteBlob`: Uses `weak_ptr<sqlite3>` with `getSQLiteConnection()` helper
  - All BLOB classes have `isConnectionValid()` method to check connection state
  - Operations throw `DBException` if connection has been closed (prevents use-after-free)

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
- Modern C++ patterns:
  - Use `override` instead of `virtual` for destructors in derived classes
  - Use in-class member initialization for atomic and boolean variables
  - Use `std::scoped_lock` instead of `std::lock_guard<std::mutex>` for CTAD
  - Use `std::unique_lock` with CTAD instead of explicit template arguments
  - Mark methods as `const` when they don't modify state
  - Mark methods as `final` to prevent further overriding when appropriate
  - Catch specific exception types (e.g., `DBException`) instead of generic `std::exception`
  - Use `explicit` keyword for single-argument constructors
  - Avoid redundant member initializations when already initialized at declaration
- SonarCloud static analysis integration:
  - Configuration in `.sonarcloud.properties`
  - Documented rule exclusions for intentional patterns

## Component Relationships

### Relational Driver Components
- `RelationalDBDriver` → Creates → `RelationalDBConnection`
- `RelationalDBConnection` → Creates → `RelationalDBPreparedStatement`
- `RelationalDBConnection` → Creates → `RelationalDBResultSet` (via direct query)
- `RelationalDBPreparedStatement` → Creates → `RelationalDBResultSet` (via prepared query)
- `RelationalDBResultSet` → Creates → `Blob` (via getBlob method)
- `RelationalDBResultSet` → Creates → `InputStream` (via getBinaryStream method)

### Document Driver Components
- `DocumentDBDriver` → Creates → `DocumentDBConnection`
- `DocumentDBConnection` → Creates → `DocumentDBCollection`
- `DocumentDBCollection` → Creates → `DocumentDBCursor` (via find operations)
- `DocumentDBCursor` → Returns → `DocumentDBData` (via iteration)
- `MongoDBDriver` → Implements → `DocumentDBDriver`
- `MongoDBConnection` → Implements → `DocumentDBConnection`
- `MongoDBCollection` → Implements → `DocumentDBCollection`
- `MongoDBCursor` → Implements → `DocumentDBCursor`
- `MongoDBData` → Implements → `DocumentDBData`

### Connection Pool Components
- `RelationalDBConnectionPool` → Manages → `PooledRelationalDBConnection`
- `PooledRelationalDBConnection` → Wraps → `RelationalDBConnection`
- `RelationalDBConnectionPool` → Creates → Physical `RelationalDBConnection` (via Driver)
- `DocumentDBConnectionPool` → Manages → `DocumentPooledDBConnection`
- `DocumentPooledDBConnection` → Wraps → `DocumentDBConnection`
- `DocumentDBConnectionPool` → Creates → Physical `DocumentDBConnection` (via Driver)
- `KVDBConnectionPool` → Manages → `KVPooledDBConnection`
- `KVPooledDBConnection` → Wraps → `KVDBConnection`
- `KVDBConnectionPool` → Creates → Physical `KVDBConnection` (via Driver)

### Transaction Components
- `TransactionManager` → Manages → `TransactionContext`
- `TransactionContext` → Contains → `RelationalDBConnection`
- `TransactionManager` → Uses → `RelationalDBConnectionPool` (to obtain connections)

### Configuration Components
- `DatabaseConfigManager` → Manages → `DatabaseConfig`
- `DatabaseConfig` → Contains → Connection parameters
- `YamlConfigLoader` → Creates → `DatabaseConfigManager` (from YAML file)
- `DBConnectionPoolConfig` → Configures → `RelationalDBConnectionPool`

### BLOB Components
- `Blob` → Uses → `InputStream` (for reading)
- `Blob` → Uses → `OutputStream` (for writing)
- `MemoryBlob` → Implements → `Blob` (for in-memory BLOBs)
- `MySQLBlob` → Implements → `Blob` (for MySQL BLOBs)
- `PostgreSQLBlob` → Implements → `Blob` (for PostgreSQL BLOBs)
- `SQLiteBlob` → Implements → `Blob` (for SQLite BLOBs)
- `FirebirdBlob` → Implements → `Blob` (for Firebird BLOBs)

### Key-Value Driver Components
- `KVDBDriver` → Creates → `KVDBConnection`
- `KVDBConnection` → Performs → Key-Value Operations (get, set, list, hash, etc.)
- `RedisDriver` → Implements → `KVDBDriver`
- `RedisConnection` → Implements → `KVDBConnection`
- `KVDBConnectionPool` → Manages → `KVPooledDBConnection`
- `KVPooledDBConnection` → Wraps → `KVDBConnection`

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

### Document Database Operation Flow (MongoDB)
1. Client obtains a `DocumentDBConnection` via `DriverManager` using MongoDB URL
2. Client gets a `DocumentDBCollection` from the connection
3. Client performs CRUD operations on the collection:
   - **Insert**: `insertOne()` or `insertMany()` with BSON documents
   - **Find**: `find()` with filter to get a `DocumentDBCursor`
   - **Update**: `updateOne()` or `updateMany()` with filter and update operators
   - **Delete**: `deleteOne()` or `deleteMany()` with filter
4. For find operations, client iterates through `DocumentDBCursor` to get `DocumentDBData` objects
5. Client extracts data from `DocumentDBData` using type-specific getters
6. Resources are cleaned up (automatically with smart pointers)

### Configuration Flow
1. Client loads configuration from a YAML file using `YamlConfigLoader`
2. `YamlConfigLoader` parses the YAML file and creates a `DatabaseConfigManager`
3. `DatabaseConfigManager` contains multiple `DatabaseConfig` objects
4. Client retrieves a specific `DatabaseConfig` by name
5. Client uses the `DatabaseConfig` to create a connection string
6. Connection string is used with `DriverManager` to create a connection

### Key-Value Database Operation Flow (Redis)
1. Client obtains a `KVDBConnection` via `DriverManager` using Redis URL
2. Client performs key-value operations on the connection:
   - **String Operations**: `setString()`, `getString()`, `expire()`, etc.
   - **List Operations**: `listPushLeft()`, `listPushRight()`, `listPopLeft()`, `listPopRight()`, `listRange()`, etc.
   - **Hash Operations**: `hashSet()`, `hashGet()`, `hashGetAll()`, `hashDelete()`, etc.
   - **Set Operations**: `setAdd()`, `setRemove()`, `setIsMember()`, `setMembers()`, etc.
   - **Sorted Set Operations**: `sortedSetAdd()`, `sortedSetRemove()`, `sortedSetScore()`, `sortedSetRange()`, etc.
   - **Counter Operations**: `increment()`, `decrement()`
   - **Key Operations**: `deleteKey()`, `deleteKeys()`, `exists()`, `expire()`, `getTTL()`, etc.
   - **Server Operations**: `ping()`, `getServerInfo()`, `flushDB()`, etc.
3. Client can execute arbitrary Redis commands using `executeCommand()`
4. Resources are cleaned up (automatically with smart pointers)

### Connection Pooling with Key-Value Databases Flow
1. Client creates a connection pool using the factory method:
   ```cpp
   auto pool = cpp_dbc::Redis::RedisConnectionPool::create(url, username, password);
   ```
   or with a configuration object:
   ```cpp
   config::DBConnectionPoolConfig config;
   config.setUrl("redis://localhost:6379");
   // Set other configuration options...
   auto pool = cpp_dbc::Redis::RedisConnectionPool::create(config);
   ```
2. Client requests a connection from the pool:
   ```cpp
   auto conn = pool->getKVDBConnection();
   ```
3. Client uses the connection for Redis operations
4. Client returns the connection to the pool when done:
   ```cpp
   conn->close(); // Returns to pool instead of closing
   ```
5. Pool maintains connections and automatically cleans up resources when closed
