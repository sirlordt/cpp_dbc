# CPP_DBC System Patterns

## System Architecture

CPP_DBC follows a layered architecture with clear separation of concerns:

1. **Interface Layer**: Abstract base classes defining the API (`Connection`, `PreparedStatement`, `ResultSet`, etc.) in the `include/cpp_dbc/` directory
2. **Driver Layer**: Database-specific implementations of the interfaces in the `src/drivers/` directory
3. **Connection Management Layer**: Connection pooling and transaction management in the `src/` directory
4. **Client Application Layer**: User code that interacts with the library

The architecture follows this flow:
```
Client Application → DriverManager → Driver → Connection → PreparedStatement/ResultSet
                   → ConnectionPool → PooledConnection → Connection
                   → TransactionManager → Connection
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

## Key Technical Decisions

### Thread Safety
- `ConnectionPool` uses mutexes and condition variables for thread-safe connection management
- `TransactionManager` provides thread-safe transaction handling across multiple threads
- Connection objects are not thread-safe and should not be shared between threads

### Resource Management
- Smart pointers (`std::shared_ptr`) are used for automatic resource management
- RAII (Resource Acquisition Is Initialization) principle is followed for resource cleanup

### Error Handling
- Custom `SQLException` class for consistent error reporting
- Exceptions are used for error propagation throughout the library

### Connection Pooling
- Configurable connection pool with parameters for initial size, max size, validation, etc.
- Automatic connection validation and cleanup
- Background maintenance thread for pool management

### Transaction Management
- Unique transaction IDs for tracking transactions across threads
- Automatic transaction timeout and cleanup
- Support for distributed transactions across multiple operations

## Component Relationships

### Driver Components
- `Driver` → Creates → `Connection`
- `Connection` → Creates → `PreparedStatement`
- `Connection` → Creates → `ResultSet` (via direct query)
- `PreparedStatement` → Creates → `ResultSet` (via prepared query)

### Connection Pool Components
- `ConnectionPool` → Manages → `PooledConnection`
- `PooledConnection` → Wraps → `Connection`
- `ConnectionPool` → Creates → Physical `Connection` (via Driver)

### Transaction Components
- `TransactionManager` → Manages → `TransactionContext`
- `TransactionContext` → Contains → `Connection`
- `TransactionManager` → Uses → `ConnectionPool` (to obtain connections)

## Critical Implementation Paths

### Database Operation Flow
1. Client obtains a `Connection` (either directly or from a pool)
2. Client creates a `PreparedStatement` with SQL
3. Client sets parameters on the `PreparedStatement`
4. Client executes the `PreparedStatement` and gets a `ResultSet`
5. Client processes the `ResultSet`
6. Resources are cleaned up (automatically with smart pointers)

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
4. Client performs operations using the connection
5. Client commits or rolls back the transaction using the transaction ID
6. `TransactionManager` handles the actual commit/rollback on the connection
