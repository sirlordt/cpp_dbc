# CPP_DBC System Patterns

## System Architecture

CPP_DBC follows a layered architecture with clear separation of concerns:

1. **Core Interface Layer**: Abstract base classes defining the API in the `include/cpp_dbc/core/` directory
   - `core/relational/`: Relational database interfaces (`RelationalDBConnection`, `RelationalDBPreparedStatement`, `RelationalDBResultSet`, etc.)
   - `core/document/`: Document database interfaces (`DocumentDBConnection`, `DocumentDBDriver`, `DocumentDBCollection`, `DocumentDBCursor`, `DocumentDBData`)
   - `core/columnar/`: Columnar database interfaces (`ColumnarDBConnection`, `ColumnarDBDriver`)
   - `core/graph/`: Graph database interfaces (placeholder for future)
   - `core/kv/`: Key-Value database interfaces (`KVDBConnection`, `KVDBDriver`)
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
     - `connection_*.cpp`: Connection class implementation (up to 4 files for Firebird)
     - `prepared_statement_*.cpp`: PreparedStatement class implementation (up to 4 files for Firebird)
     - `result_set_*.cpp`: ResultSet class implementation (up to 5 files for SQLite/Firebird)
     - MongoDB-specific: `collection_*.cpp`, `cursor_*.cpp`, `document_*.cpp`
   - **Canonical Method Ordering (Result Sets)**: All result set nothrow methods follow a consistent ordering across all drivers:
     1. `close` → `isEmpty` → `next` → `isBeforeFirst` → `isAfterLast` → `getRow`
     2. Type accessors interleaved: `getInt(index)`, `getInt(name)`, `getLong(index)`, `getLong(name)`, etc.
     3. `getDate`, `getTimestamp`, `getTime` (index/name pairs)
     4. `getColumnNames`, `getColumnCount`
     5. Blob/binary methods in a separate dedicated file
3. **Connection Management Layer**: Connection pooling in `include/cpp_dbc/pool/` and `src/pool/`
   - `pool/`: Pool infrastructure (`DBConnectionPoolBase`, `DBConnectionPool`, `DBConnectionPooled`, `PooledDBConnectionBase` CRTP template) and family-specific thin pool wrappers (`relational/`, `document/`, `columnar/`, `kv/`)
4. **BLOB Layer**: Binary Large Object handling in the `include/cpp_dbc/blob.hpp` (base classes) and database-specific implementations in the driver subfolders (e.g., `drivers/relational/mysql/blob.hpp`)
5. **Key-Value Layer**: Key-Value operations support in `drivers/kv/` directory
6. **JSON Layer**: JSON data type support in database-specific implementations in the `drivers/relational/` directory
7. **Configuration Layer**: Database configuration management in the `include/cpp_dbc/config/` and `src/config/` directories
8. **Documentation Layer**: Developer guides in `libs/cpp_dbc/docs/`:
   - `how_add_new_db_drivers.md`: Comprehensive 5-phase guide for adding new database drivers
   - `error_handling_patterns.md`: DBException, error codes, and exception-free API patterns
   - `shell_script_dependencies.md`: Script call hierarchy and troubleshooting
9. **Code Quality Layer**: Comprehensive warning flags and compile-time checks across all components
10. **Client Application Layer**: User code that interacts with the library

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
- **Single Connection Mutex Model (Firebird):** All statement/result-set registry access uses a single shared `m_connMutex` — eliminates ABBA deadlock between separate registry mutexes and the connection mutex
- **AtomicGuard<T> RAII:** `system_utils::AtomicGuard<T>` provides exception-safe management of `std::atomic<T>` flags (set on construction, restored on destruction, non-copyable)
- **Pool Lock Order Rule:** Connection close operations must happen OUTSIDE pool locks. Pool lock (`m_mutexPool`) is always acquired BEFORE per-connection mutex. Closing inside pool lock inverts this order → Helgrind LockOrder violation
- **Printf-Style Debug Output / `logWithTimesMillis`:** All debug macros (`FIREBIRD_DEBUG`, `SQLITE_DEBUG`, `CP_DEBUG`) use `system_utils::logWithTimesMillis(prefix, msg)` which formats `[HH:MM:SS.mmm] [TID] [prefix] message` and writes atomically via `write()` — `operator<<` is not atomic and causes interleaved output + Helgrind false positives under concurrency
- **`m_resetting` Anti-Deadlock Flag:** Both `FirebirdDBConnection` and `MySQLDBConnection` use `m_resetting` (`atomic<bool>`) to signal `ResultSet::close()` / `PreparedStatement::close()` to skip unregister during `closeAllActiveResultSets()` / `closeAllStatements()` — prevents re-entrant lock acquisition through the close→unregister→lock cycle
- **`MySQLConnectionLock` RAII Helper:** `mysql_internal.hpp` provides `MySQLConnectionLock` that locks connection mutex via `weak_ptr<MySQLDBConnection>`, validates connection is alive and not closed, and returns `expected` on failure. The `MYSQL_CONNECTION_LOCK_OR_RETURN` macro expands to this pattern, replacing repeated `DB_DRIVER_LOCK_GUARD` + closed-check boilerplate
- **Result Set Registry (MySQL):** `MySQLDBConnection::m_activeResultSets` — set of `weak_ptr<MySQLDBResultSet>` with two-phase close pattern in `closeAllActiveResultSets()` (collect shared_ptrs under lock, close outside lock to prevent iterator invalidation). Called by `close()`, `reset()`, and `returnToPool()`
- **Nothrow API — Pure Virtual Base Class Pattern (2026-02-18):**
  - All nothrow methods in `DBConnection`, `DBResultSet`, and `RelationalDBConnection` are `= 0` pure virtual — no default implementations
  - Every driver MUST implement the complete nothrow API surface: `close`, `reset`, `isClosed`, `returnToPool`, `isPooled`, `getURL`, `prepareForPoolReturn`, `prepareForBorrow`
  - Throwing method wrappers delegate to their nothrow counterpart — single code path, no duplication:
    ```cpp
    void MySQLDBConnection::close() {
        auto result = close(std::nothrow);
        if (!result.has_value()) throw result.error();
    }
    ```
  - Connection pool wrappers (`*PooledDBConnection`) also override all nothrow pure virtuals
  - **Rule:** Base class nothrow methods returning "not implemented" errors are a smell — they allow silently broken drivers. Promote to `= 0` immediately.
- **Pool Lifecycle Methods — Protected + Friend Pattern (2026-02-21):**
  - `prepareForPoolReturn()`, `prepareForBorrow()` (and nothrow versions) are pool-internal lifecycle hooks — they must not be part of the public API
  - Moved to `protected` in `RelationalDBConnection` with forward declarations + `friend class RelationalDBConnectionPool` / `friend class RelationalPooledDBConnection`
  - All driver concrete classes (`MySQL`, `PostgreSQL`, `SQLite`, `Firebird`) override them in their `protected` section
  - Pool concrete classes marked `final` — communicates intentional non-inheritance and removes virtual dispatch overhead
  - **Rule:** Methods that are implementation details of a specific usage context (pool lifecycle) should be `protected` + accessed only through `friend` declarations, not exposed in the public API.

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
- Custom `DBException` class for consistent error reporting; inherits `std::exception` (not `std::runtime_error` since 2026-02-26)
- Exceptions are used for error propagation throughout the library
- **`DBException` — Fixed-Size Value Type (2026-02-26):**
  - `m_mark[13]`, `m_message[257]`, `m_full_message[271]` — fixed char arrays, stack-allocatable (~560 bytes)
  - Constructor is `noexcept` — no heap allocations that can throw
  - Call stack stored as `std::shared_ptr<CallStackCapture>` (optional, allocated once, shared across copies)
  - `what()` returns pre-computed `m_full_message` (zero-cost, no concatenation)
  - `what_s()` / `getMark()` return `std::string_view`; `getCallStack()` returns `std::span<const StackFrame>`
  - Long strings left-truncated with `...[TRUNCATED]` marker
- **`system_utils::CallStackCapture` (2026-02-26):**
  - `StackFrame` uses fixed `char file[150]`, `char function[150]` (was `std::string`)
  - `CallStackCapture` holds `StackFrame frames[10]` + `int count` — max 10 frames
  - `captureCallStack()` returns `std::shared_ptr<CallStackCapture>` (was `std::vector<StackFrame>`)
- **Unified `ping()` Interface (2026-02-26):**
  - `virtual bool ping() = 0` and `virtual expected<bool, DBException> ping(std::nothrow_t) noexcept = 0` in `DBConnection` base
  - All database families use the same signature; return type is uniformly `bool`
  - Removed family-specific `ping()` from `KVDBConnection` (returned `std::string "PONG"`) and `DocumentDBConnection`
  - `ping()` also removed from `DocumentDBConnection` abstract interface (2026-03-04) — no longer part of the document connection contract
- **Nothrow-First Dual-API Architecture — All 7 Drivers:**
  - All drivers (MySQL, PostgreSQL, SQLite, Firebird, MongoDB, ScyllaDB, Redis) implement the nothrow-first dual-API pattern
  - `#ifdef __cpp_exceptions` guards: all throwing methods guarded; nothrow methods always compile under `-fno-exceptions`
  - Static factory pattern: all connection classes use `create(std::nothrow_t, ...)` factories with private nothrow constructors
  - DBDriver init: all drivers use `std::atomic<bool>` + `std::mutex` double-checked locking (not `std::once_flag`) — re-initializable after `cleanup()`, no `std::system_error` under `-fno-exceptions`
  - Nothrow methods that call only nothrow methods have no try/catch blocks (dead code elimination per conventions)
  - Error deferral pattern: private constructors store errors in `m_initFailed` / `m_initError` members; factory checks and propagates via `unexpected`
  - MySQL-specific (2026-03-06): `MySQLDBConnection` uses `PrivateCtorTag` pattern (public ctor with private tag type) for `std::make_shared` compatibility; `MySQLDBPreparedStatement`/`MySQLDBResultSet` use `new` (not `make_shared`) since their ctors are private; `weak_ptr<MySQLDBConnection>` replaces `weak_ptr<MYSQL>` for accessing native handle and mutex through connection
  - MySQL-specific (2026-03-07): `MySQLBlob` and `MySQLInputStream` upgraded to PrivateCtorTag pattern with `m_initFailed`/`m_initError` and `#ifdef __cpp_exceptions` guards
  - MongoDB-specific (2026-03-04): `DocumentDBCursor` chaining methods (`skip`, `limit`, `sort`) return `expected<std::reference_wrapper<DocumentDBCursor>, DBException>`; `DocumentDBDriver::getURIScheme()` `noexcept` returning full URL prefix; `getDefaultPort()` removed; `MongoDBDocument` ID caching
- **Unified URI API in DBDriver Base (2026-03-07):**
  - `acceptsURL()` renamed to `acceptURI()` — both throwing and nothrow versions in `DBDriver` base (throwing delegates to nothrow)
  - `parseURI(std::nothrow_t)` pure virtual — each driver implements URI parsing, returns `expected<map<string,string>, DBException>`
  - `buildURI(std::nothrow_t)` pure virtual — each driver implements URI building from components
  - `getURIScheme()` pure virtual — returns human-readable URI template (e.g., `"cpp_dbc:mysql://<host>:<port>/<database>"`)
  - Default `acceptURI(std::nothrow_t)` implementation uses `parseURI()`: successful parse = accepted
  - Private helpers (`acceptsURL`, `parseURL`, `buildURL`) removed from `ColumnarDBDriver`, `DocumentDBDriver`, `KVDBDriver` — logic now in concrete drivers
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
- **Direct Handoff — ALL Pool Families (relational 2026-02-21, all others 2026-02-22):**
  - `ConnectionRequest` struct + `m_waitQueue` (`std::deque<ConnectionRequest*>`) for zero-race connection handoff
  - On `returnConnection()`: if `m_waitQueue` is non-empty, connection is assigned directly to the first waiter's `ConnectionRequest::conn` field and `fulfilled = true` — no idle queue push, no race
  - On `getXDBConnection()`: if pool is full, push a stack-local `ConnectionRequest` into `m_waitQueue` and wait on `m_connectionAvailable` CV, then check `request.fulfilled`
  - Single `m_mutexPool` for all pool state (replaces 5 separate mutexes: `m_mutexGetConnection`, `m_mutexReturnConnection`, `m_mutexAllConnections`, `m_mutexIdleConnections`, `m_mutexMaintenance`)
  - Eliminates "stolen wakeup": returning thread cannot re-borrow the connection it just assigned to a waiter
  - All `notify_one()` / `notify_all()` calls inside `m_mutexPool` lock for Helgrind correctness
  - `m_pendingCreations` (size_t, guarded by `m_mutexPool`) prevents pool overshoot when multiple threads create connections simultaneously outside the lock
- **Atomic int64_t for Last-Used Time (all pools, 2026-02-24):**
  - `m_lastUsedTimeMutex` eliminated; `m_lastUsedTimeNs` is `std::atomic<int64_t>` (nanoseconds since epoch)
  - Replaced `std::atomic<time_point>` (not portable to ARM32/MIPS) with `std::atomic<int64_t>` (always lock-free on every supported platform)
  - `static_assert(std::atomic<int64_t>::is_always_lock_free, ...)` enforced at compile time
  - `memory_order_relaxed` for both store and load — sufficient for HikariCP-style validation skip (approximation, stale reads tolerable)
  - `m_creationTime` uses in-class initializer `{std::chrono::steady_clock::now()}`; `m_lastUsedTimeNs` is initialized from `m_creationTime` (safe: declared after in class body)
- **Nothrow Pool Factory Pattern (all pools, 2026-02-24):**
  - `create()` factory returns `expected<shared_ptr<Pool>, DBException>` with `std::nothrow_t` first parameter
  - All internal pool methods (createDBConnection, createPooledDBConnection, validateConnection, returnConnection, initializePool) return `expected<T, DBException>` noexcept
  - `DBConnectionPool` base class declares nothrow pure virtuals for all 6 pool operations
  - `DBConnectionPooled` base class: all 6 interface methods have nothrow signatures
  - Usage pattern:
    ```cpp
    auto result = cpp_dbc::MySQL::MySQLConnectionPool::create(std::nothrow, config);
    if (!result.has_value()) { throw result.error(); }
    auto pool = result.value();
    ```
- **Qualified Destructor Close Pattern:**
  - Pool destructors call `XDBConnectionPool::close()` (qualified name) rather than `close()` (virtual dispatch)
  - Avoids S1699: calling virtual method in destructor bypasses derived class overrides and may access destroyed state
  - Rule: `close()` logic that must run in destructor should either be inlined or called with the fully qualified class name

### Connection Pooling (continued)
- **Unified Pool Base Class (`DBConnectionPoolBase`, 2026-03-06):**
  - All pool infrastructure extracted into `pool/connection_pool.hpp` + `pool/connection_pool.cpp` (955 lines)
  - Contains: connection lifecycle, maintenance thread, direct handoff, HikariCP validation skip, phase-based lock protocol
  - Pure virtual `createPooledDBConnection(std::nothrow_t)` — derived classes override to create family-specific pooled wrappers
  - Protected `acquireConnection()` — core borrow logic; `initializePool()` — called by factory after `make_shared`
  - Pool headers/sources moved from `core/` to `pool/` directory
- **CRTP Pooled Connection Base (`PooledDBConnectionBase<D,C,P>`, 2026-03-06):**
  - `pool/pooled_db_connection_base.hpp` + `.cpp` (~485 lines) — extracts close/returnToPool (race-condition fix), destructor cleanup, and pool metadata from all 4 family pooled connection wrappers
  - `*Impl` methods for diamond-ambiguous DBConnection methods (close, isClosed, returnToPool, isPooled, getURL, reset, ping, prepareForPoolReturn, prepareForBorrow)
  - `*Throw` methods under `#ifdef __cpp_exceptions` for throwing delegators
  - DBConnectionPooled interface overrides directly (no diamond): `isPoolValid`, `getCreationTime`, `getLastUsedTime`, `setActive`, `isActive`, `getUnderlyingConnection`, `markPoolClosed`, `isPoolClosed`, `updateLastUsedTime`
  - Explicit template instantiations for all 4 families in `.cpp`
  - Family pooled connections provide one-line inline delegators to resolve diamond inheritance
  - Friend declarations in all family connection classes and `DBConnectionPoolBase` for CRTP access
- Connection pool implementations — thin derived classes inheriting from `DBConnectionPoolBase`:
  - `RelationalDBConnectionPool` for relational databases
  - `DocumentDBConnectionPool` for document databases
  - `ColumnarDBConnectionPool` for columnar databases
  - `KVDBConnectionPool` for key-value databases
- Each connection pool implementation follows the same architecture:
  - Inherits from `DBConnectionPoolBase` (all pool logic) — only overrides `createPooledDBConnection()` and adds typed getter
  - Concrete implementations for specific database types (PostgreSQLConnectionPool, MongoDBConnectionPool, ScyllaConnectionPool, RedisDBConnectionPool, etc.)
  - Factory methods (`create`) return `expected<shared_ptr<Pool>, DBException>` with `std::nothrow_t` — exception-free pool creation
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
  - Use `[[nodiscard]]` attribute for functions returning `expected<T, E>` to ensure error handling
  - Use `int64_t` instead of `long` for portable 64-bit integer types
  - Use platform-specific preprocessor guards (`#ifdef _WIN32`) for cross-platform compatibility
  - Catch and log exceptions in destructors instead of allowing them to propagate
- Doxygen API documentation:
  - All public headers include `/** @brief ... */` documentation blocks with inline code examples
  - `@param`, `@return`, `@throws`, `@see` tags for cross-referencing
  - Ready for Doxygen HTML/PDF generation
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
- `MongoDBDriver` → Implements → `DocumentDBDriver` (double-checked locking for C library init)
- `MongoDBConnection` → Implements → `DocumentDBConnection` (static factory: `create(std::nothrow_t)`)
- `MongoDBCollection` → Implements → `DocumentDBCollection` (`getCollectionHandle(std::nothrow_t)` helper)
- `MongoDBCursor` → Implements → `DocumentDBCursor` (nothrow constructor, `std::reference_wrapper` chaining)
- `MongoDBDocument` → Implements → `DocumentDBData` (static factory, ID caching with `m_idCached`/`m_cachedId`)

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
- `RedisDBDriver` → Implements → `KVDBDriver`
- `RedisDBConnection` → Implements → `KVDBConnection`
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
   auto pool = cpp_dbc::Redis::RedisDBConnectionPool::create(url, username, password);
   ```
   or with a configuration object:
   ```cpp
   config::DBConnectionPoolConfig config;
   config.setUrl("redis://localhost:6379");
   // Set other configuration options...
   auto pool = cpp_dbc::Redis::RedisDBConnectionPool::create(config);
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
