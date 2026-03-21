# How to Add New Database Drivers to cpp_dbc

This guide explains how to add support for a new database management system (DBMS) to the cpp_dbc library. It is designed for both human contributors and LLM assistants.

## Table of Contents

1. [Overview](#overview)
2. [Code Reference](#code-reference)
   - [Base Interfaces](#base-interfaces)
   - [Family-Specific Interfaces](#family-specific-interfaces)
   - [Utilities](#utilities)
   - [Thread Safety Macros](#thread-safety-macros)
   - [Reference Implementations](#reference-implementations)
3. [Phase 1: Create Driver Files](#phase-1-create-driver-files)
   - [Identify the Driver Family](#identify-the-driver-family)
   - [Create Header Files (.hpp)](#create-header-files-hpp)
   - [Create Source Files (.cpp)](#create-source-files-cpp)
   - [Update Main Header (cpp_dbc.hpp)](#update-main-header-cpp_dbchpp)
   - [Register in DriverManager::initDrivers](#register-in-drivermanagerinitdrivers)
   - [Integrate with Connection Pool](#integrate-with-connection-pool)
   - [Update Class Hierarchy Diagram](#update-class-hierarchy-diagram)
4. [Phase 2: Update Build Configuration](#phase-2-update-build-configuration)
   - [Update CMakeLists.txt](#update-cmakeliststxt)
   - [Create FindModule for CMake](#create-findmodule-for-cmake)
   - [Update CMake Config Files](#update-cmake-config-files)
   - [Update Benchmark Common Header](#update-benchmark-common-header)
   - [Update Shell Scripts](#update-shell-scripts)
   - [Update Distribution Packages](#update-distribution-packages)
   - [Source Compilation Fallback](#source-compilation-fallback)
5. [Phase 3: Create Tests](#phase-3-create-tests)
   - [Test File Naming Convention](#test-file-naming-convention)
   - [Test Patterns by Family](#test-patterns-by-family)
   - [Update test/CMakeLists.txt](#update-testcmakeliststxt)
6. [Phase 4: Create Benchmarks](#phase-4-create-benchmarks)
   - [Benchmark File Structure](#benchmark-file-structure)
   - [Update benchmark/CMakeLists.txt](#update-benchmarkcmakeliststxt)
7. [Phase 5: Create Examples](#phase-5-create-examples)
   - [Example Files by Family](#example-files-by-family)
   - [Update CMakeLists.txt for Examples](#update-cmakeliststxt-for-examples)
8. [Checklist Summary](#checklist-summary)
9. [Common Mistakes to Avoid](#common-mistakes-to-avoid)
10. [Architectural Patterns](#architectural-patterns)
    - [PrivateCtorTag Pattern — Throw-Free Construction with `std::make_shared`](#privatectortag-pattern--throw-free-construction-with-stdmake_shared)
    - [DBDriver Singleton + Connection Registry Pattern](#dbdriver-singleton--connection-registry-pattern)
    - [Mutex Sharing — Connection and Child Classes](#mutex-sharing--connection-and-child-classes)
11. [Related Documentation](#related-documentation)

---

## Related Documentation

This guide is a high-level map. For deeper details, see these companion documents:

| Document | Description |
|----------|-------------|
| [shell_script_dependencies.md](shell_script_dependencies.md) | Detailed call hierarchy and dependencies between all shell scripts |
| [error_handling_patterns.md](error_handling_patterns.md) | Complete guide to DBException usage, error codes, and nothrow API patterns |

---

## Overview

The cpp_dbc library organizes database drivers by **family**:

| Family     | Description                        | Examples                        |
|------------|------------------------------------|---------------------------------|
| relational | SQL-based relational databases     | MySQL, PostgreSQL, SQLite, Firebird |
| document   | Document-oriented NoSQL databases  | MongoDB                         |
| columnar   | Column-family/wide-column databases| ScyllaDB (Cassandra)            |
| kv         | Key-value stores                   | Redis                           |
| graph      | Graph databases                    | (Reserved for future)           |
| timeseries | Time-series databases              | (Reserved for future)           |

Each driver consists of:
- **Header files** in `libs/cpp_dbc/include/cpp_dbc/drivers/<family>/<driver_name>/`
- **Source files** in `libs/cpp_dbc/src/drivers/<family>/<driver_name>/`
- **An umbrella header** at `libs/cpp_dbc/include/cpp_dbc/drivers/<family>/driver_<driver_name>.hpp`

---

## Code Reference

Before implementing a new driver, study the core interfaces in `libs/cpp_dbc/include/cpp_dbc/`. This section provides a map of all relevant files.

### Base Interfaces

These are the fundamental classes that all drivers inherit from:

| File | Class | Description |
|------|-------|-------------|
| `core/db_driver.hpp` | `DBDriver` | Base class for all drivers. Defines `connect()`, `acceptURI()`, `getDBType()`, `getName()` |
| `core/db_connection.hpp` | `DBConnection` | Base class for all connections. Defines `close()`, `isClosed()`, `returnToPool()`, `getURI()` |
| `core/db_result_set.hpp` | `DBResultSet` | Base class for result sets. Defines `close()`, `isEmpty()` |
| `core/db_exception.hpp` | `DBException` | Exception class with 12-char error codes and call stack capture |
| `core/db_types.hpp` | `DBType`, `Types`, `TransactionIsolationLevel` | Core enums for database types, SQL types, and isolation levels |
| `core/db_expected.hpp` | `expected<T,E>` | Error handling type for nothrow API pattern |
| `pool/db_connection_pool.hpp` | `DBConnectionPool` | Base class for connection pools |
| `pool/db_connection_pooled.hpp` | `DBConnectionPooled` | Wrapper for pooled connections |
| `pool/connection_pool.hpp` | `DBConnectionPoolBase` | Unified pool base class (all pool infrastructure) |
| `pool/pooled_db_connection_base.hpp` | `PooledDBConnectionBase<D,C,P>` | CRTP base for all family-specific pooled connection wrappers (close/returnToPool race-condition fix, destructor cleanup, pool metadata) |
| `pool/<family>/<family>_db_connection_pool.hpp` | `<Family>DBConnectionPool`, `<Family>PooledDBConnection` | Family-specific pool class + pooled connection wrapper |
| `core/streams.hpp` | `InputStream` | Stream interface for BLOB data |

### Family-Specific Interfaces

Each database family has abstract interfaces in `libs/cpp_dbc/include/cpp_dbc/core/<family>/`. **Read the .hpp files in these directories to see the virtual methods you must implement.**

| Family | Interface Directory | Status |
|--------|---------------------|--------|
| **relational** | `core/relational/` | ✅ Implemented |
| **document** | `core/document/` | ✅ Implemented |
| **kv** | `core/kv/` | ✅ Implemented |
| **columnar** | `core/columnar/` | ✅ Implemented |
| **graph** | `core/graph/` | ❌ Not yet implemented |
| **timeseries** | `core/timeseries/` | ❌ Not yet implemented |

> **Important**: For `graph` or `timeseries`, you must first create the base interfaces in the corresponding `core/` directory and add the type to `DBType` enum in `core/db_types.hpp`.

### Utilities

| File | Description |
|------|-------------|
| `common/system_utils.hpp` | `captureCallStack()`, `parseDBURI()`, `safePrint()`, logging functions |
| `config/database_config.hpp` | Database configuration structures |
| `config/yaml_config_loader.hpp` | YAML configuration file loader |
| `blob.hpp` | BLOB type definitions |
| `transaction_manager.hpp` | Transaction management utilities |
| `backward.hpp` | Stack trace support (third-party) |

### Thread Safety Macros

Each driver defines thread-safety macros in its internal header file at `src/drivers/<family>/<driver>/<driver>_internal.hpp`:

```cpp
// Thread-safety macros for conditional mutex locking
// Using recursive_mutex to allow the same thread to acquire the lock multiple times
#if DB_DRIVER_THREAD_SAFE
#define DB_DRIVER_LOCK_GUARD(mutex) std::scoped_lock<std::recursive_mutex> lock(mutex)
#else
#define DB_DRIVER_LOCK_GUARD(mutex) (void)0
#endif

// Debug output macro (controlled by -DDEBUG_<DRIVER>=1 or -DDEBUG_ALL=1)
#if (defined(DEBUG_<DRIVER>) && DEBUG_<DRIVER>) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define <DRIVER>_DEBUG(format, ...)                                                          \
    do                                                                                       \
    {                                                                                        \
        char debug_buffer[1024];                                                             \
        int debug_n = std::snprintf(debug_buffer, sizeof(debug_buffer), format, ##__VA_ARGS__); \
        if (debug_n >= static_cast<int>(sizeof(debug_buffer)))                               \
        {                                                                                    \
            static constexpr const char trunc[] = "...[TRUNCATED]";                          \
            std::memcpy(debug_buffer + sizeof(debug_buffer) - sizeof(trunc),                 \
                        trunc, sizeof(trunc));                                               \
        }                                                                                    \
        cpp_dbc::system_utils::logWithTimesMillis("<Driver>", debug_buffer);                 \
    } while (0)
#else
#define <DRIVER>_DEBUG(...) ((void)0)
#endif
```

Create a similar `<driver>_internal.hpp` file for your new driver in `src/drivers/<family>/<driver>/`.

### Reference Implementations

Study these existing drivers as templates for your implementation:

| Family | Driver | Directory | Notes |
|--------|--------|-----------|-------|
| **relational** | MySQL | `drivers/relational/mysql/` | Most complete example with all features |
| **relational** | PostgreSQL | `drivers/relational/postgresql/` | Good example of native type handling |
| **relational** | SQLite | `drivers/relational/sqlite/` | Example of embedded database |
| **relational** | Firebird | `drivers/relational/firebird/` | Example with special transaction handling |
| **document** | MongoDB | `drivers/document/mongodb/` | Complete document store implementation |
| **kv** | Redis | `drivers/kv/redis/` | Complete key-value store implementation |
| **columnar** | ScyllaDB | `drivers/columnar/scylladb/` | Complete columnar database implementation |

### External Resources

In addition to studying the codebase, you will need to consult **official documentation** for the target database:

| Resource Type | What to Look For | Examples |
|---------------|------------------|----------|
| **C/C++ Client Library Docs** | API functions, connection handling, query execution, result iteration | libmysqlclient docs, libpq docs, MongoDB C Driver docs |
| **Native Data Types** | Type mappings between the database and C/C++ types | `MYSQL_TYPE_*`, `Oid` in PostgreSQL, `BSON` types |
| **Error Handling** | Error codes, error retrieval functions, connection state | `mysql_errno()`, `PQerrorMessage()`, `mongoc_error_t` |
| **Thread Safety** | Whether the client library is thread-safe, connection sharing rules | Per-connection vs per-statement thread safety |
| **Memory Management** | Who owns allocated memory, when to free resources | Result set lifecycle, prepared statement cleanup |

> **Important for LLM Assistants**: This project cannot document every possible database API. If you are unsure about:
> - Which C/C++ client library to use for the target database
> - Where to find the official documentation
> - The correct GitHub repository or official project URL
>
> **You MUST ask the user for clarification** before proceeding. Request:
> 1. The name of the C/C++ client library to use (e.g., `libpq`, `libmysqlclient`, `hiredis`)
> 2. Official documentation URLs
> 3. GitHub repository or source code URLs
> 4. Any other relevant references the user can provide
>
> Do not guess or assume which library to use—different databases may have multiple client libraries with different APIs and trade-offs.

---

## Phase 1: Create Driver Files

### Identify the Driver Family

First, determine which family your new driver belongs to:

- **relational**: Traditional SQL databases (MySQL, PostgreSQL, SQLite, Firebird)
- **document**: Document stores (MongoDB)
- **columnar**: Wide-column stores (ScyllaDB/Cassandra)
- **kv**: Key-value stores (Redis)
- **graph**: Graph databases (future)
- **timeseries**: Time-series databases (future)

For this guide, we'll use `sqlserver` (Microsoft SQL Server) as an example relational driver.

### Create Header Files (.hpp)

#### Implementation Order

**Always implement in this order** (due to dependencies):

1. `handles.hpp` - RAII handles (no internal dependencies)
2. `*_internal.hpp` - Thread-safety macros (in `src/` directory)
3. `result_set.hpp` / `cursor.hpp` - Data retrieval (depends on handles)
4. `prepared_statement.hpp` - Query execution (depends on handles, result_set)
5. `connection.hpp` - Connection management (depends on all above)
6. `driver.hpp` - Factory class (depends on connection)

#### Directory Structure

```text
libs/cpp_dbc/include/cpp_dbc/drivers/relational/
├── driver_sqlserver.hpp          # Umbrella header (includes all components)
└── sqlserver/
    ├── handles.hpp               # RAII handles for native resources
    ├── input_stream.hpp          # Input stream for BLOB handling (optional)
    ├── blob.hpp                  # BLOB type definitions (optional)
    ├── result_set.hpp            # Result set class
    ├── prepared_statement.hpp    # Prepared statement class
    ├── connection.hpp            # Connection class
    └── driver.hpp                # Driver factory class
```

#### Umbrella Header Pattern

Create `driver_sqlserver.hpp`:

```cpp
/**
 * @file driver_sqlserver.hpp
 * @brief SQL Server database driver implementation
 */

#ifndef CPP_DBC_DRIVER_SQLSERVER_HPP
#define CPP_DBC_DRIVER_SQLSERVER_HPP

#include "sqlserver/handles.hpp"
#include "sqlserver/input_stream.hpp"
#include "sqlserver/blob.hpp"
#include "sqlserver/result_set.hpp"
#include "sqlserver/prepared_statement.hpp"
#include "sqlserver/connection.hpp"
#include "sqlserver/driver.hpp"

#endif // CPP_DBC_DRIVER_SQLSERVER_HPP
```

#### Component Classes by Family

Each family has different required components. **For exact `#include` dependencies and class inheritance, read the existing driver headers in the reference implementations.**

| Family | Reference Directory | Key Files |
|--------|---------------------|-----------|
| **relational** | `drivers/relational/mysql/` | `handles.hpp`, `result_set.hpp`, `prepared_statement.hpp`, `connection.hpp`, `driver.hpp` |
| **document** | `drivers/document/mongodb/` | `handles.hpp`, `document.hpp`, `cursor.hpp`, `collection.hpp`, `connection.hpp`, `driver.hpp` |
| **kv** | `drivers/kv/redis/` | `handles.hpp`, `reply_handle.hpp`, `connection.hpp`, `driver.hpp` |
| **columnar** | `drivers/columnar/scylladb/` | `handles.hpp`, `memory_input_stream.hpp`, `result_set.hpp`, `prepared_statement.hpp`, `connection.hpp`, `driver.hpp` |

> **Tip**: Open the reference driver's `driver_<name>.hpp` umbrella header to see the exact include order and dependencies.

### Create Source Files (.cpp)

#### Directory Structure

```text
libs/cpp_dbc/src/drivers/relational/sqlserver/
├── driver_01.cpp                 # Driver factory implementation
├── connection_01.cpp             # Connection: constructor, destructor, basic ops
├── connection_02.cpp             # Connection: throwing wrappers
├── connection_03.cpp             # Connection: nothrow methods (part 1)
├── connection_04.cpp             # Connection: nothrow methods (part 2)
├── prepared_statement_01.cpp     # PreparedStatement: setup and binding
├── prepared_statement_02.cpp     # PreparedStatement: nothrow setters (part 1)
├── prepared_statement_03.cpp     # PreparedStatement: nothrow setters (part 2)
├── prepared_statement_04.cpp     # PreparedStatement: nothrow execute/close
├── result_set_01.cpp             # ResultSet: throwing wrappers
├── result_set_02.cpp             # ResultSet: nothrow methods (close, nav, basic types)
├── result_set_03.cpp             # ResultSet: nothrow methods (string, bool, date/time, metadata)
├── result_set_04.cpp             # ResultSet: nothrow methods (blob/binary)
└── result_set_05.cpp             # ResultSet: nothrow methods (additional, if needed)
```

#### File Splitting Convention

Source files are split by functionality to:
1. Reduce compilation time (parallel builds)
2. Improve code organization
3. Allow incremental changes

**Naming Pattern**: `<component>_<number>.cpp`

| Component          | Files                                     |
|--------------------|-------------------------------------------|
| Driver             | `driver_01.cpp`                           |
| Connection         | `connection_01.cpp` to `connection_04.cpp`|
| PreparedStatement  | `prepared_statement_01.cpp` to `..._04.cpp`|
| ResultSet          | `result_set_01.cpp` to `result_set_05.cpp`|

#### Canonical Method Ordering (Result Sets)

All nothrow result set methods must follow this canonical ordering:

1. `close` → `isEmpty` → `next` → `isBeforeFirst` → `isAfterLast` → `getRow`
2. Type accessors interleaved by-index/by-name: `getInt(index)`, `getInt(name)`, `getLong(index)`, `getLong(name)`, etc.
3. Date/time: `getDate`, `getTimestamp`, `getTime` (index/name pairs)
4. Metadata: `getColumnNames`, `getColumnCount`
5. Blob/binary methods in a separate file (e.g., `result_set_04.cpp`)

### Update Main Header (cpp_dbc.hpp)

**IMPORTANT**: Update `libs/cpp_dbc/include/cpp_dbc/cpp_dbc.hpp` to include the new driver.

#### 1. Add Default Macro Definition

Add the `USE_<DRIVER>` macro with the other driver macros (around line 25-51):

```cpp
#ifndef USE_SQLSERVER
#define USE_SQLSERVER 0 // NOSONAR - Macro required for conditional compilation
#endif
```

#### 2. Add Conditional Include

Add the conditional include for the new driver header (around line 80-107):

```cpp
#if USE_SQLSERVER
#include "cpp_dbc/drivers/relational/driver_sqlserver.hpp"
#endif
```

**Note**: The include path depends on the driver family:
- Relational: `cpp_dbc/drivers/relational/driver_<name>.hpp`
- Document: `cpp_dbc/drivers/document/driver_<name>.hpp`
- Key-Value: `cpp_dbc/drivers/kv/driver_<name>.hpp`
- Columnar: `cpp_dbc/drivers/columnar/driver_<name>.hpp`

### Register in DriverManager::initDrivers

Every new driver must register itself in `DriverManager::initDrivers` so that callers using the high-level `DriverManager::getDBConnection()` API can discover and use it without manual driver registration.

**File**: `libs/cpp_dbc/src/driver_manager.cpp`

Add a new `#if USE_<DRIVER>` block inside `DriverManager::initDrivers(std::nothrow_t)`, following the identical pattern used by all existing drivers:

```cpp
#if USE_SQLSERVER
    if (!drivers.contains("sqlserver"))
    {
        auto result = cpp_dbc::SQLServer::SQLServerDBDriver::getInstance(std::nothrow);
        if (result.has_value())
        {
            registerDriver(result.value());
        }
        else
        {
            errors.push_back(result.error());
        }
    }
#endif
```

**Key design points**:

- **`!drivers.contains("sqlserver")` guard**: Makes `initDrivers` idempotent. If the driver is already registered (e.g. from a previous call, or from explicit `registerDriver()` by the caller), the entire block is skipped — no constructor runs, no C library is touched.
- **`getInstance(std::nothrow)`**: All drivers use the `weak_ptr`-based singleton pattern. `getInstance` returns the live instance if one already exists, or creates a new one. The singleton ensures only one driver object exists per process, even if `initDrivers` is called multiple times.
- **Error accumulation**: Failures are collected into `errors` and returned at the end — other drivers continue to initialize even if one fails.
- **`#if USE_<DRIVER>` guard**: The block is compiled only when the driver is enabled in the build. The driver name string (`"sqlserver"`) must match exactly what `XxxDBDriver::getName()` returns.

**`getName()` contract**: The string key passed to `drivers.contains()` must be identical to the value returned by the driver's `getName() const noexcept override`. Check the driver's `getName()` implementation before writing the string literal.

### Integrate with Connection Pool

Every driver must be visible to the connection pool system. The required work depends on whether the driver belongs to an **existing family** or introduces a **new family**.

#### Connection Pool Architecture

The pool system has three layers:

```text
DBConnectionPoolBase (connection_pool.hpp / connection_pool.cpp)
  └── <Family>DBConnectionPool (pool/<family>/<family>_db_connection_pool.hpp/.cpp)
        └── <Driver>ConnectionPool (same file, e.g. ScyllaDBConnectionPool)

PooledDBConnectionBase<Derived, ConnType, PoolType>  (CRTP template)
  └── <Family>PooledDBConnection (inline delegators in pool/<family>/ header)
```

| File | Role |
|------|------|
| `pool/connection_pool.hpp` / `.cpp` | `DBConnectionPoolBase` — all shared pool infrastructure (maintenance thread, direct handoff, phase-based lock protocol). **Do not modify when adding a driver.** |
| `pool/pooled_db_connection_base.hpp` / `.cpp` | `PooledDBConnectionBase<D,C,P>` — CRTP base with close/returnToPool (race-condition fix), destructor cleanup, pool metadata. Explicit template instantiations for each family at the bottom of the `.cpp`. |
| `pool/<family>/<family>_db_connection_pool.hpp` / `.cpp` | Family pool class + pooled connection wrapper + per-driver pool subclasses (e.g. `ScyllaDBConnectionPool`). |

#### Scenario A: Adding a Driver to an Existing Family

When adding a driver to an existing family (relational, kv, document, columnar), the `<Family>PooledDBConnection` and `<Family>DBConnectionPool` already exist. You only need to add a **driver subclass pool**.

##### 1. Add the Pool Subclass in the Family Pool Header

Add a new class at the bottom of `pool/<family>/<family>_db_connection_pool.hpp`, inside the driver's namespace:

```cpp
// In pool/relational/relational_db_connection_pool.hpp
namespace cpp_dbc::SQLServer
{
    class SQLServerConnectionPool final : public RelationalDBConnectionPool
    {
    public:
        SQLServerConnectionPool(DBConnectionPool::PrivateCtorTag,
                                const std::string &uri,
                                const std::string &username,
                                const std::string &password) noexcept;

        explicit SQLServerConnectionPool(DBConnectionPool::PrivateCtorTag,
                                         const config::DBConnectionPoolConfig &config) noexcept;

        ~SQLServerConnectionPool() override = default;

        SQLServerConnectionPool(const SQLServerConnectionPool &) = delete;
        SQLServerConnectionPool &operator=(const SQLServerConnectionPool &) = delete;

#ifdef __cpp_exceptions
        static std::shared_ptr<SQLServerConnectionPool> create(
            const std::string &uri, const std::string &username, const std::string &password);
        static std::shared_ptr<SQLServerConnectionPool> create(
            const config::DBConnectionPoolConfig &config);
#endif

        static cpp_dbc::expected<std::shared_ptr<SQLServerConnectionPool>, DBException>
        create(std::nothrow_t, const std::string &uri, const std::string &username,
               const std::string &password) noexcept;
        static cpp_dbc::expected<std::shared_ptr<SQLServerConnectionPool>, DBException>
        create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;
    };
} // namespace cpp_dbc::SQLServer
```

##### 2. Implement the Pool Subclass in the Family Pool .cpp

Add the constructors and factory methods at the bottom of the family pool `.cpp` file. Follow the existing pattern (e.g., `ScyllaDBConnectionPool` in `columnar_db_connection_pool.cpp`, `RedisDBConnectionPool` in `kv_db_connection_pool.cpp`):

```cpp
// In pool/relational/relational_db_connection_pool.cpp
namespace cpp_dbc::SQLServer
{
    SQLServerConnectionPool::SQLServerConnectionPool(DBConnectionPool::PrivateCtorTag,
                                                     const std::string &uri,
                                                     const std::string &username,
                                                     const std::string &password)
        : RelationalDBConnectionPool(DBConnectionPool::PrivateCtorTag{}, uri, username, password)
    {
        // Driver-specific initialization if needed
    }

    SQLServerConnectionPool::SQLServerConnectionPool(DBConnectionPool::PrivateCtorTag,
                                                     const config::DBConnectionPoolConfig &config) noexcept
        : RelationalDBConnectionPool(DBConnectionPool::PrivateCtorTag{}, config)
    {
        // Driver-specific initialization if needed
    }

#ifdef __cpp_exceptions
    std::shared_ptr<SQLServerConnectionPool> SQLServerConnectionPool::create(
        const std::string &uri, const std::string &username, const std::string &password)
    {
        auto result = create(std::nothrow, uri, username, password);
        if (!result.has_value()) { throw result.error(); }
        return result.value();
    }

    std::shared_ptr<SQLServerConnectionPool> SQLServerConnectionPool::create(
        const config::DBConnectionPoolConfig &config)
    {
        auto result = create(std::nothrow, config);
        if (!result.has_value()) { throw result.error(); }
        return result.value();
    }
#endif

    cpp_dbc::expected<std::shared_ptr<SQLServerConnectionPool>, DBException>
    SQLServerConnectionPool::create(std::nothrow_t, const std::string &uri,
                                    const std::string &username,
                                    const std::string &password) noexcept
    {
        auto pool = std::make_shared<SQLServerConnectionPool>(
            DBConnectionPool::PrivateCtorTag{}, uri, username, password);
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return pool;
    }

    cpp_dbc::expected<std::shared_ptr<SQLServerConnectionPool>, DBException>
    SQLServerConnectionPool::create(std::nothrow_t,
                                    const config::DBConnectionPoolConfig &config) noexcept
    {
        auto pool = std::make_shared<SQLServerConnectionPool>(
            DBConnectionPool::PrivateCtorTag{}, config);
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return pool;
    }
} // namespace cpp_dbc::SQLServer
```

> **Important**: The `create(std::nothrow_t, ...)` factory methods must NOT use try/catch. The only possible exception is `std::bad_alloc` from `std::make_shared`, which is a death-sentence exception (see project conventions). The `initializePool(std::nothrow)` call is `noexcept` and returns `expected`.

##### 3. No Changes Needed To

- `pool/pooled_db_connection_base.hpp/.cpp` — the CRTP base is already instantiated for the family
- `pool/connection_pool.hpp/.cpp` — the unified pool base doesn't change per driver
- The family connection base class (e.g., `RelationalDBConnection`) — friend declarations already cover the family

#### Scenario B: Adding a New Family (e.g., Graph, TimeSeries)

When introducing an entirely new family, you must create the full pool stack from scratch.

##### 1. Create the Family Connection Base Class with Friend Declarations

In `core/<family>/<family>_db_connection.hpp`, include the CRTP friend:

```cpp
class GraphDBConnection : public DBConnection
{
    friend class GraphDBConnectionPool;
    friend class GraphPooledDBConnection;
    template <typename, typename, typename> friend class PooledDBConnectionBase;

    // ... protected and public members ...
};
```

The `PooledDBConnectionBase` friend declaration is **required** because the CRTP base calls `m_conn->prepareForPoolReturn(std::nothrow, ...)` and `m_conn->prepareForBorrow(std::nothrow)`, which are `protected` in the connection class.

##### 2. Create the Family Pool Header

Create `pool/<family>/<family>_db_connection_pool.hpp` with:

1. **Forward declaration** of the family `DBConnectionPool`
2. **`<Family>PooledDBConnection`** — uses the CRTP base with inline delegators
3. **`<Family>DBConnectionPool`** — inherits from `DBConnectionPoolBase`
4. **Driver subclass pools** (e.g., `Neo4jConnectionPool`)

Use an existing family pool header (e.g., `pool/columnar/columnar_db_connection_pool.hpp`) as the template. Key elements:

```cpp
#include "cpp_dbc/pool/connection_pool.hpp"
#include "cpp_dbc/pool/pooled_db_connection_base.hpp"
#include "cpp_dbc/core/<family>/<family>_db_connection.hpp"

namespace cpp_dbc
{
    class GraphDBConnectionPool;

    // ── Pooled Connection Wrapper (CRTP) ────────────────────────────────────
    class GraphPooledDBConnection final
        : public PooledDBConnectionBase<GraphPooledDBConnection, GraphDBConnection, GraphDBConnectionPool>,
          public GraphDBConnection,
          public std::enable_shared_from_this<GraphPooledDBConnection>
    {
        using Base = PooledDBConnectionBase<GraphPooledDBConnection, GraphDBConnection, GraphDBConnectionPool>;

    public:
        GraphPooledDBConnection(
            std::shared_ptr<GraphDBConnection> connection,
            std::weak_ptr<GraphDBConnectionPool> connectionPool,
            std::shared_ptr<std::atomic<bool>> poolAlive) noexcept;
        ~GraphPooledDBConnection() override = default;

        // Delete copy/move
        GraphPooledDBConnection(const GraphPooledDBConnection &) = delete;
        GraphPooledDBConnection &operator=(const GraphPooledDBConnection &) = delete;

#ifdef __cpp_exceptions
        // ── Diamond-resolving throwing delegators ──
        void close() final { this->closeThrow(); }
        bool isClosed() const override { return this->isClosedThrow(); }
        void returnToPool() final { this->returnToPoolThrow(); }
        bool isPooled() const override { return this->isPooledThrow(); }
        std::string getURI() const override { return this->getURIThrow(); }
        void reset() override { this->resetThrow(); }
        bool ping() override { return this->pingThrow(); }

        // ── Family-specific throwing methods ──
        // ... graph-specific method throwing delegators ...
#endif

        // ── Diamond-resolving nothrow delegators ──
        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override
        { return this->closeImpl(std::nothrow); }
        // ... (same pattern for all diamond methods) ...

        // ── Pool return/borrow delegators ──
    protected:
        cpp_dbc::expected<void, DBException>
        prepareForPoolReturn(std::nothrow_t,
            TransactionIsolationLevel il = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override
        { return this->prepareForPoolReturnImpl(std::nothrow, il); }

        cpp_dbc::expected<void, DBException>
        prepareForBorrow(std::nothrow_t) noexcept override
        { return this->prepareForBorrowImpl(std::nothrow); }

    public:
        // ── Family-specific nothrow methods ──
        // ... graph-specific method declarations ...
    };

    // ── Family Pool Class ───────────────────────────────────────────────────
    class GraphDBConnectionPool : public DBConnectionPoolBase
    {
        // ... same pattern as ColumnarDBConnectionPool ...
    };

} // namespace cpp_dbc
```

> **Key point — Diamond inheritance**: Each `PooledDBConnection` has TWO `DBConnection` subobjects (one via `DBConnectionPooled`, one via the family connection class). The CRTP base overrides the `DBConnectionPooled` path directly, and provides `*Impl` methods for diamond-ambiguous methods (`close`, `isClosed`, `returnToPool`, `isPooled`, `getURI`, `reset`, `ping`, `prepareForPoolReturn`, `prepareForBorrow`). The derived class provides one-line inline delegators that forward to these `*Impl` methods, resolving the diamond.

##### 3. Create the Family Pool Source File

Create `pool/<family>/<family>_db_connection_pool.cpp` with:

- `<Family>PooledDBConnection` constructor (delegates to CRTP `Base`)
- `<Family>DBConnectionPool` methods: constructors, destructor, `createDBConnection()`, `createPooledDBConnection()`, factory methods, family-specific getter
- `<Family>PooledDBConnection` family-specific method implementations
- Driver subclass pool implementations (e.g., `Neo4jConnectionPool`)

Use `pool/columnar/columnar_db_connection_pool.cpp` as the reference implementation.

##### 4. Add CRTP Explicit Instantiation

In `pool/pooled_db_connection_base.cpp`, add the new family's explicit template instantiation at the bottom:

```cpp
// ── Explicit instantiations ───────────────────────────────────────────────
template class PooledDBConnectionBase<RelationalPooledDBConnection, RelationalDBConnection, RelationalDBConnectionPool>;
template class PooledDBConnectionBase<KVPooledDBConnection, KVDBConnection, KVDBConnectionPool>;
template class PooledDBConnectionBase<DocumentPooledDBConnection, DocumentDBConnection, DocumentDBConnectionPool>;
template class PooledDBConnectionBase<ColumnarPooledDBConnection, ColumnarDBConnection, ColumnarDBConnectionPool>;
template class PooledDBConnectionBase<GraphPooledDBConnection, GraphDBConnection, GraphDBConnectionPool>;  // NEW
```

Also add the corresponding `#include` at the top of that file:

```cpp
#include "cpp_dbc/pool/<family>/<family>_db_connection_pool.hpp"
```

##### 5. Add Friend Declaration in DBConnectionPoolBase

In `pool/connection_pool.hpp`, the `PooledDBConnectionBase` template friend is already declared:

```cpp
template <typename, typename, typename> friend class PooledDBConnectionBase;
```

No changes needed here — the friend covers all instantiations.

##### 6. Register the New Pool Source in CMakeLists.txt

Add the new pool `.cpp` file to `libs/cpp_dbc/CMakeLists.txt`:

```cmake
target_sources(cpp_dbc PRIVATE
    # ... existing pool sources ...
    src/pool/<family>/<family>_db_connection_pool.cpp
)
```

##### Summary: Files Modified for a New Family Pool

| File | Action |
|------|--------|
| `core/<family>/<family>_db_connection.hpp` | Add `template <typename,typename,typename> friend class PooledDBConnectionBase;` |
| `pool/<family>/<family>_db_connection_pool.hpp` | **CREATE** — `<Family>PooledDBConnection` (CRTP) + `<Family>DBConnectionPool` + driver subclass pools |
| `pool/<family>/<family>_db_connection_pool.cpp` | **CREATE** — all implementations |
| `pool/pooled_db_connection_base.cpp` | Add `#include` and explicit template instantiation for the new family |
| `libs/cpp_dbc/CMakeLists.txt` | Add `src/pool/<family>/<family>_db_connection_pool.cpp` to sources |

### Update Class Hierarchy Diagram

Update the class hierarchy diagram at `libs/cpp_dbc/docs/class_hierarchy.drawio` to include all new classes.

**Important**: The `.drawio` file contains an XML comment block at the very top of the file with the complete layout specification. **Read that comment first** — it documents the exact X/Y coordinates, spacing rules, color codes, and how to position new boxes.

Key rules from the specification:

- **Box size**: All boxes are 250 × 50 (uniform, no exceptions).
- **Sub-column gap**: 290px between Blue (Root Abstract) → Green (Family Abstract) → Yellow (Concrete).
- **Inter-zone gap**: 430px between zones.
- **Y rows by family**: Columnar=12, Document=88.75, KV=165, Relational=240/320.
- **Concrete box style**: `fillColor=#fff2cc;strokeColor=#d6b656;fontSize=11` (or `fontSize=10` for long class names).
- **No arrows or lines** — inheritance is implied by position (same Y = same family, leftmost = most abstract).

For each new class, add a box in the corresponding zone's Yellow (concrete) sub-column at the Y row matching the driver's family. If a new family is created (e.g., Graph, TimeSeries), assign it the next available Y position (y = previous max + 76.75).

---

## Phase 2: Update Build Configuration

### Update CMakeLists.txt

Edit `libs/cpp_dbc/CMakeLists.txt`:

#### 1. Add the option for the new driver

```cmake
option(USE_SQLSERVER "Enable SQL Server support" OFF)
```

#### 2. Add compile definitions

```cmake
target_compile_definitions(cpp_dbc PUBLIC
    # ... existing definitions ...
    $<$<BOOL:${USE_SQLSERVER}>:USE_SQLSERVER=1>
    $<$<NOT:$<BOOL:${USE_SQLSERVER}>>:USE_SQLSERVER=0>
)
```

#### 3. Add conditional source files

```cmake
# Conditionally add SQL Server driver files and dependencies
if(USE_SQLSERVER)
    target_sources(cpp_dbc PRIVATE
        # SQLServerDBResultSet
        src/drivers/relational/sqlserver/result_set_01.cpp
        src/drivers/relational/sqlserver/result_set_02.cpp
        src/drivers/relational/sqlserver/result_set_03.cpp
        src/drivers/relational/sqlserver/result_set_04.cpp

        # SQLServerDBPreparedStatement
        src/drivers/relational/sqlserver/prepared_statement_01.cpp
        src/drivers/relational/sqlserver/prepared_statement_02.cpp
        src/drivers/relational/sqlserver/prepared_statement_03.cpp
        src/drivers/relational/sqlserver/prepared_statement_04.cpp

        # SQLServerDBConnection
        src/drivers/relational/sqlserver/connection_01.cpp
        src/drivers/relational/sqlserver/connection_02.cpp
        src/drivers/relational/sqlserver/connection_03.cpp
        src/drivers/relational/sqlserver/connection_04.cpp

        # SQLServerDBDriver
        src/drivers/relational/sqlserver/driver_01.cpp
    )

    # Find SQL Server package
    find_package(SQLServer REQUIRED)
    target_include_directories(cpp_dbc PUBLIC ${SQLSERVER_INCLUDE_DIR})
    target_link_libraries(cpp_dbc PUBLIC ${SQLSERVER_LIBRARIES})
endif()
```

#### 4. Install the FindModule

```cmake
install(FILES
    # ... existing files ...
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindSQLServer.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cpp_dbc
)
```

### Create FindModule for CMake

Create `libs/cpp_dbc/cmake/FindSQLServer.cmake`:

```cmake
# FindSQLServer.cmake
# Find the SQL Server client library
#
# This module defines:
#   SQLSERVER_FOUND        - True if SQL Server was found
#   SQLSERVER_INCLUDE_DIR  - Include directories
#   SQLSERVER_LIBRARIES    - Libraries to link

find_path(SQLSERVER_INCLUDE_DIR
    NAMES sql.h sqlext.h
    PATHS
        /usr/include
        /usr/local/include
        /opt/mssql-tools/include
)

find_library(SQLSERVER_LIBRARIES
    NAMES odbc
    PATHS
        /usr/lib
        /usr/local/lib
        /opt/mssql-tools/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SQLServer
    DEFAULT_MSG
    SQLSERVER_LIBRARIES
    SQLSERVER_INCLUDE_DIR
)

mark_as_advanced(SQLSERVER_INCLUDE_DIR SQLSERVER_LIBRARIES)
```

### Update CMake Config Files

These files are used when cpp_dbc is installed as a package and consumed by other projects.

#### 1. Update `libs/cpp_dbc/cmake/cpp_dbc-config.cmake`

This file provides the installed package configuration:

```cmake
# Add variable for the new driver
set(CPP_DBC_USE_SQLSERVER @USE_SQLSERVER@)

# Add compile definitions
if(CPP_DBC_USE_SQLSERVER)
    target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_SQLSERVER=1)
else()
    target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_SQLSERVER=0)
endif()

# Add find_package and linking (if applicable)
if(CPP_DBC_USE_SQLSERVER)
    include("${CMAKE_CURRENT_LIST_DIR}/FindSQLServer.cmake")
    if(NOT SQLServer_FOUND)
        message(FATAL_ERROR "SQL Server library not found.")
    endif()
    target_include_directories(cpp_dbc::cpp_dbc INTERFACE ${SQLSERVER_INCLUDE_DIR})
    target_link_libraries(cpp_dbc::cpp_dbc INTERFACE ${SQLSERVER_LIBRARIES})
endif()
```

#### 2. Update `libs/cpp_dbc/cmake/FindCPPDBC.cmake`

This file auto-detects available drivers for fallback discovery:

```cmake
# Check for SQL Server
find_package(SQLServer QUIET)
if(SQLServer_FOUND)
    target_link_libraries(cpp_dbc::cpp_dbc INTERFACE ${SQLSERVER_LIBRARIES})
    target_include_directories(cpp_dbc::cpp_dbc INTERFACE ${SQLSERVER_INCLUDE_DIR})
    target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_SQLSERVER=1)
else()
    target_compile_definitions(cpp_dbc::cpp_dbc INTERFACE USE_SQLSERVER=0)
endif()
```

### Update Benchmark Common Header

Update `libs/cpp_dbc/benchmark/benchmark_common.hpp` to include the new driver:

```cpp
#if USE_SQLSERVER
#include <cpp_dbc/drivers/relational/driver_sqlserver.hpp>
#endif
```

### Update Shell Scripts

**CRITICAL**: This is where mistakes are most common. Update ALL of these files.

> **See also**: [shell_script_dependencies.md](shell_script_dependencies.md) for a detailed diagram showing how these scripts call each other.

#### 1. `libs/cpp_dbc/build_cpp_dbc.sh`

Add the new driver option:

```bash
# Default values section (around line 15-35)
USE_SQLSERVER=OFF

# Parse command line arguments section (add case)
--sqlserver|--sqlserver-on)
USE_SQLSERVER=ON
shift
;;
--sqlserver-off)
USE_SQLSERVER=OFF
shift
;;

# Help message section
echo "  --sqlserver, --sqlserver-on  Enable SQL Server support"
echo "  --sqlserver-off              Disable SQL Server support"

# Dependency check section
if [ "$USE_SQLSERVER" = "ON" ]; then
    echo "Checking for SQL Server ODBC driver..."
    # Add package manager detection logic
fi

# Echo configuration section
echo "  SQL Server support: $USE_SQLSERVER"

# CMake call section
-DUSE_SQLSERVER=$USE_SQLSERVER \
```

#### 2. `libs/cpp_dbc/build_test_cpp_dbc.sh`

Same pattern as above - add option parsing, help text, and CMake variable.

#### 3. `libs/cpp_dbc/run_test_cpp_dbc.sh`

Add test running support for the new driver.

#### 4. `libs/cpp_dbc/run_benchmarks_cpp_dbc.sh`

Add benchmark support for the new driver.

#### 5. `libs/cpp_dbc/build_dist_pkg.sh`

Add distro package building support:

```bash
# Add in options parsing
sqlserver)
    CMAKE_SQLSERVER_OPTION="-DCPP_DBC_WITH_SQLSERVER=ON"
    USE_SQLSERVER="ON"
    BUILD_FLAGS="$BUILD_FLAGS --sqlserver"
    ;;

# Add package dependency handling
SQLSERVER_DEV_PKG="unixodbc-dev"  # or appropriate package
```

#### 6. Root `helper.sh`

Add the driver to:
- `show_usage()` function
- `cmd_run_build()` function
- `cmd_run_test()` function
- `cmd_run_benchmarks()` function
- Various `--bk-combo-XX`, `--mc-combo-XX` shortcuts

#### 7. Root `run_test.sh`

Add option handling for the new driver.

### Update Distribution Packages

**CRITICAL**: Don't forget the distros folder!

#### For each distribution in `libs/cpp_dbc/distros/`:

1. **Update `Dockerfile`** - Add package placeholder:
```dockerfile
RUN apt-get install -y \
    # ... existing packages ...
    __SQLSERVER_DEV_PKG__
```

2. **Update `build_script.sh`** - Add parameter handling:
```bash
if [ "__USE_SQLSERVER__" = "ON" ]; then
    SQLSERVER_PARAM="--sqlserver"
else
    SQLSERVER_PARAM="--sqlserver-off"
fi
```

#### Supported distributions:

- `debian_12/`
- `debian_13/`
- `ubuntu_22_04/`
- `ubuntu_24_04/`
- `fedora_42/`
- `fedora_43/`

For each, update both `Dockerfile` and `build_script.sh`.

### Source Compilation Fallback

Some database drivers don't have official `.deb` or `.rpm` packages. Examples include:
- **ScyllaDB/Cassandra C++ Driver** - No official packages, only source code
- **MongoDB C++ Driver** (on some distributions) - May require source compilation

For these cases, create a `download_and_setup_<driver>.sh` script that:
1. Checks if the driver is already installed
2. Downloads and compiles from source if not found
3. Installs to `/usr/local`

#### When to Create a Setup Script

Create a `download_and_setup_<driver>.sh` script when:
- No official `.deb`/`.rpm` package exists for the driver
- The driver must be compiled from source on most distributions
- Different distributions require different build configurations

#### Script Location and Naming

Place the script at:
```text
libs/cpp_dbc/download_and_setup_<driver>_driver.sh
```

Examples:
- `download_and_setup_cassandra_driver.sh` (for ScyllaDB/Cassandra)
- `download_and_setup_sqlserver_driver.sh` (hypothetical)

#### Script Structure

Follow this template based on `download_and_setup_cassandra_driver.sh`:

```bash
#!/bin/bash
set -e

# ============================================
# 1. UTILITY FUNCTIONS
# ============================================

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# ============================================
# 2. PRIVILEGE CHECK
# ============================================

if [ "$(id -u)" -eq 0 ]; then
    SUDO=""
else
    if command_exists sudo; then
        SUDO="sudo"
    else
        echo "Error: This script requires root privileges or sudo."
        exit 1
    fi
fi

# ============================================
# 3. CHECK IF DRIVER ALREADY INSTALLED
# ============================================

echo "Checking for existing <Driver> installation..."

DRIVER_FOUND=0

# Check pkg-config
if command_exists pkg-config; then
    if pkg-config --exists <driver_name>; then
        echo "<Driver> found via pkg-config."
        DRIVER_FOUND=1
    fi
fi

# Check headers
HEADER_FOUND=0
if [ -f "/usr/local/include/<driver>.h" ] || [ -f "/usr/include/<driver>.h" ]; then
    HEADER_FOUND=1
fi

# Check libraries
LIB_FOUND=0
if ls /usr/local/lib/lib<driver>.* 1> /dev/null 2>&1 || \
   ls /usr/lib/lib<driver>.* 1> /dev/null 2>&1 || \
   ls /usr/lib/x86_64-linux-gnu/lib<driver>.* 1> /dev/null 2>&1 || \
   ls /usr/local/lib64/lib<driver>.* 1> /dev/null 2>&1 || \
   ls /usr/lib64/lib<driver>.* 1> /dev/null 2>&1; then
    LIB_FOUND=1
fi

# Exit early if already installed
if [ $DRIVER_FOUND -eq 1 ] || ( [ $HEADER_FOUND -eq 1 ] && [ $LIB_FOUND -eq 1 ] ); then
    echo "<Driver> appears to be already installed."
    exit 0
fi

echo "<Driver> not found. Starting installation..."

# ============================================
# 4. INSTALL BUILD DEPENDENCIES
# ============================================

echo "Installing build dependencies..."
if command_exists apt-get; then
    export DEBIAN_FRONTEND=noninteractive
    $SUDO apt-get update
    $SUDO apt-get install -y cmake make g++ <dep1> <dep2> wget pkg-config
elif command_exists dnf; then
    $SUDO dnf install -y cmake make gcc-c++ <dep1-fedora> <dep2-fedora> wget pkgconf-pkg-config
elif command_exists yum; then
    $SUDO yum install -y cmake make gcc-c++ <dep1-rhel> <dep2-rhel> wget pkgconfig
else
    echo "Warning: Could not detect package manager."
fi

# ============================================
# 5. CREATE TEMPORARY BUILD DIRECTORY
# ============================================

TEMP_DIR=$(mktemp -d -p /tmp <driver>_build.XXXXXX)
echo "Using temporary directory: $TEMP_DIR"

cleanup() {
    if [ -d "$TEMP_DIR" ]; then
        echo "Cleaning up temporary directory..."
        rm -rf "$TEMP_DIR"
    fi
}
trap cleanup EXIT

cd "$TEMP_DIR"

# ============================================
# 6. DOWNLOAD SOURCE CODE
# ============================================

echo "Downloading <Driver> version X.Y.Z..."
wget https://github.com/<org>/<repo>/archive/refs/tags/X.Y.Z.tar.gz -O <driver>.tar.gz

echo "Extracting..."
tar xzf <driver>.tar.gz
cd <repo>-X.Y.Z

# ============================================
# 7. BUILD
# ============================================

echo "Building <Driver>..."
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)

# ============================================
# 8. INSTALL
# ============================================

echo "Installing <Driver>..."
$SUDO make install

if command_exists ldconfig; then
    $SUDO ldconfig
fi

echo "<Driver> installation complete."
```

#### Key Implementation Details

1. **Installation Detection**: Check multiple locations:
   - `/usr/local/include/` and `/usr/include/` for headers
   - `/usr/local/lib/`, `/usr/lib/`, `/usr/lib/x86_64-linux-gnu/`, `/usr/lib64/` for libraries
   - Use `pkg-config` when available

2. **Package Manager Detection**: Support all major package managers:
   - `apt-get` for Debian/Ubuntu
   - `dnf` for Fedora
   - `yum` for RHEL/CentOS

3. **Build Dependencies**: Map dependencies to each package manager:
   ```bash
   # Debian/Ubuntu
   apt-get install -y libuv1-dev libssl-dev

   # Fedora
   dnf install -y libuv-devel openssl-devel
   ```

4. **Temporary Directory**: Use `mktemp` with cleanup trap:
   ```bash
   TEMP_DIR=$(mktemp -d -p /tmp <driver>_build.XXXXXX)
   trap cleanup EXIT
   ```

5. **Install Location**: Always install to `/usr/local`:
   ```bash
   cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
   ```

#### Integrating with build_cpp_dbc.sh

In `build_cpp_dbc.sh`, call the setup script before CMake configuration:

```bash
# In the dependency check section of build_cpp_dbc.sh
if [ "$USE_SCYLLADB" = "ON" ]; then
    echo "Checking for ScyllaDB/Cassandra driver..."

    if ! pkg-config --exists cassandra 2>/dev/null; then
        SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
        if [ -f "$SCRIPT_DIR/download_and_setup_cassandra_driver.sh" ]; then
            echo "ScyllaDB/Cassandra driver not found. Running setup script..."
            bash "$SCRIPT_DIR/download_and_setup_cassandra_driver.sh"
        else
            echo "Error: ScyllaDB driver not found and setup script missing."
            exit 1
        fi
    fi
fi
```

#### Distribution Package Considerations

When creating distro packages for drivers that require source compilation:

1. **In Dockerfile**: Include build dependencies but not the driver itself:
   ```dockerfile
   RUN apt-get install -y cmake make g++ libuv1-dev libssl-dev wget
   ```

2. **In build_script.sh**: Run the setup script before building cpp_dbc:
   ```bash
   if [ "__USE_SCYLLADB__" = "ON" ]; then
       /app/libs/cpp_dbc/download_and_setup_cassandra_driver.sh
   fi
   ```

#### Reference Implementation

See `libs/cpp_dbc/download_and_setup_cassandra_driver.sh` for a complete working example.

---

## Phase 3: Create Tests

### Test Directory Structure

**CRITICAL**: Tests are organized by database family following the same pattern as examples:

```text
libs/cpp_dbc/test/
├── common/                           # Core tests (10_*)
│   ├── 10_000_test_main.cpp
│   ├── 10_001_test_database_config.cpp
│   ├── 10_011_test_db_config.cpp
│   ├── 10_021_test_db_connection_pool_config.cpp
│   ├── 10_031_test_db_exception.cpp
│   ├── 10_041_test_expected.cpp
│   ├── 10_051_test_yaml.cpp
│   ├── 10_061_test_integration.cpp
│   ├── test_db_connections.yml
│   └── test.jpg
├── relational/
│   ├── mysql/                        # MySQL tests (20_*)
│   ├── postgresql/                   # PostgreSQL tests (21_*)
│   ├── sqlite/                       # SQLite tests (22_*)
│   └── firebird/                     # Firebird tests (23_*)
├── kv/
│   └── redis/                        # Redis tests (24_*)
├── document/
│   └── mongodb/                      # MongoDB tests (25_*)
├── columnar/
│   └── scylladb/                     # ScyllaDB tests (26_*)
├── graph/
│   └── .git_keep                     # Reserved for future
└── timeseries/
    └── .git_keep                     # Reserved for future
```

### Test File Naming Convention

Test files follow this pattern: `XX_YYY_test_<driver>_<category>.cpp`

Where:
- `XX` = Driver prefix (see table below)
- `YYY` = Sequential number within driver
- `<driver>` = Driver name (mysql, postgresql, sqlite, firebird, redis, mongodb, scylladb)
- `<category>` = Test category

#### Driver Prefix Assignment

| Prefix | Driver     | Family     | Directory                        |
|--------|------------|------------|----------------------------------|
| 10     | Core       | N/A        | `test/common/`                   |
| 20     | MySQL      | relational | `test/relational/mysql/`         |
| 21     | PostgreSQL | relational | `test/relational/postgresql/`    |
| 22     | SQLite     | relational | `test/relational/sqlite/`        |
| 23     | Firebird   | relational | `test/relational/firebird/`      |
| 24     | Redis      | kv         | `test/kv/redis/`                 |
| 25     | MongoDB    | document   | `test/document/mongodb/`         |
| 26     | ScyllaDB   | columnar   | `test/columnar/scylladb/`        |
| 27     | (Next)     | (Next)     | `test/<family>/<driver>/`        |

For a new relational driver like SQL Server, you would use prefix `27_` and place files in `test/relational/sqlserver/`.

#### Test Categories

**Standard Test Categories (XX_0YY):**

| Number | Category                    |
|--------|----------------------------|
| 001    | common (test helpers)      |
| 011    | db_config                  |
| 021    | driver                     |
| 031    | basic operations           |
| 041    | connection                 |
| 051    | json                       |
| 061    | blob                       |
| 071    | inner_join                 |
| 081    | left_join                  |
| 091    | right_join                 |
| 101    | full_join                  |
| 111    | thread_safe                |
| 121    | transaction_isolation      |
| 131    | transaction_manager        |
| 141    | connection_pool            |

**Exclusive Test Categories (XX_5YY):**

For tests that are specific to a database family and cannot be shared with other drivers, use the `5YY` range:

| Number | Category                       | Notes                                    |
|--------|-------------------------------|------------------------------------------|
| 521    | Driver-exclusive API tests    | Tests for family-specific APIs (e.g., MongoDB's `hasNext()`, `nextDocument()`) |
| 522-599| Reserved for future exclusive | Additional exclusive test categories     |

**Examples:**
- `25_521_test_mongodb_real_cursor_api.cpp` - Tests MongoDB-specific cursor methods
- `24_521_test_redis_real_pipeline_api.cpp` - Tests Redis-specific pipelining (hypothetical)

### Test Patterns by Family

#### Relational Database Tests

Required files in `test/relational/sqlserver/`:
```text
test/relational/sqlserver/27_001_test_sqlserver_real_common.cpp     # Test helpers
test/relational/sqlserver/27_001_test_sqlserver_real_common.hpp     # Test helpers header
test/relational/sqlserver/27_011_test_sqlserver_real_db_config.cpp
test/relational/sqlserver/27_021_test_sqlserver_real_driver.cpp
test/relational/sqlserver/27_031_test_sqlserver_real.cpp
test/relational/sqlserver/27_041_test_sqlserver_real_connection.cpp
test/relational/sqlserver/27_051_test_sqlserver_real_json.cpp
test/relational/sqlserver/27_061_test_sqlserver_real_blob.cpp
test/relational/sqlserver/27_071_test_sqlserver_real_inner_join.cpp
test/relational/sqlserver/27_081_test_sqlserver_real_left_join.cpp
test/relational/sqlserver/27_091_test_sqlserver_real_right_join.cpp
test/relational/sqlserver/27_101_test_sqlserver_real_full_join.cpp
test/relational/sqlserver/27_111_test_sqlserver_real_thread_safe.cpp
test/relational/sqlserver/27_121_test_sqlserver_real_transaction_isolation.cpp
test/relational/sqlserver/27_131_test_sqlserver_real_transaction_manager.cpp
test/relational/sqlserver/27_141_test_sqlserver_real_connection_pool.cpp
```

#### Key-Value Database Tests

Redis-style tests (simpler, no JOINs):
```text
24_001_test_redis_real_common.cpp
24_021_test_redis_real_driver.cpp
24_041_test_redis_real_connection.cpp
24_141_test_redis_real_connection_pool.cpp
```

#### Document Database Tests

MongoDB-style tests:
```text
25_001_test_mongodb_real_common.cpp
25_011_test_mongodb_real_db_config.cpp
25_021_test_mongodb_real_driver.cpp
25_031_test_mongodb_real.cpp
25_041_test_mongodb_real_connection.cpp
25_051_test_mongodb_real_json.cpp
25_061_test_mongodb_real_blob.cpp
25_071_test_mongodb_real_inner_join.cpp  # Aggregation pipeline
25_081_test_mongodb_real_left_join.cpp
25_091_test_mongodb_real_right_join.cpp
25_101_test_mongodb_real_full_join.cpp
25_111_test_mongodb_real_thread_safe.cpp
25_141_test_mongodb_real_connection_pool.cpp
```

#### Columnar Database Tests

ScyllaDB-style tests:
```text
26_001_test_scylladb_real_common.cpp
26_011_test_scylladb_real_db_config.cpp
26_021_test_scylladb_real_driver.cpp
26_031_test_scylladb_real.cpp
26_041_test_scylladb_real_connection.cpp
26_051_test_scylladb_real_json.cpp
26_061_test_scylladb_real_blob.cpp
26_071_test_scylladb_real_inner_join.cpp
26_081_test_scylladb_real_left_join.cpp
26_091_test_scylladb_real_right_join.cpp
26_101_test_scylladb_real_full_join.cpp
26_111_test_scylladb_real_thread_safe.cpp
26_141_test_scylladb_real_connection_pool.cpp
```

### Update Test Configuration YAML

**Single file for all drivers**: `libs/cpp_dbc/test/test_db_connections.yml`

Add `dev_<driver>` and `test_<driver>` entries following the existing patterns in that file.

### Update test/CMakeLists.txt

**IMPORTANT**: Test files must use family-prefixed paths in CMakeLists.txt.

#### 1. Add the option:
```cmake
option(USE_SQLSERVER "Enable SQL Server support" OFF)
```

#### 2. Add the test files with family paths:
```cmake
# SQL Server tests (relational/sqlserver/)
relational/sqlserver/27_001_test_sqlserver_real_common.cpp
relational/sqlserver/27_011_test_sqlserver_real_db_config.cpp
relational/sqlserver/27_021_test_sqlserver_real_driver.cpp
relational/sqlserver/27_031_test_sqlserver_real.cpp
relational/sqlserver/27_041_test_sqlserver_real_connection.cpp
relational/sqlserver/27_051_test_sqlserver_real_json.cpp
relational/sqlserver/27_061_test_sqlserver_real_blob.cpp
relational/sqlserver/27_071_test_sqlserver_real_inner_join.cpp
relational/sqlserver/27_081_test_sqlserver_real_left_join.cpp
relational/sqlserver/27_091_test_sqlserver_real_right_join.cpp
relational/sqlserver/27_101_test_sqlserver_real_full_join.cpp
relational/sqlserver/27_111_test_sqlserver_real_thread_safe.cpp
relational/sqlserver/27_121_test_sqlserver_real_transaction_isolation.cpp
relational/sqlserver/27_131_test_sqlserver_real_transaction_manager.cpp
relational/sqlserver/27_141_test_sqlserver_real_connection_pool.cpp
```

#### 3. Add compile definition:
```cmake
$<$<BOOL:${USE_SQLSERVER}>:USE_SQLSERVER=1>
$<$<NOT:$<BOOL:${USE_SQLSERVER}>>:USE_SQLSERVER=0>
```

#### 4. Add include directories for test headers:
```cmake
# Add include directories (add after existing target_include_directories)
target_include_directories(cpp_dbc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/mysql
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/postgresql
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/sqlite
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/firebird
    ${CMAKE_CURRENT_SOURCE_DIR}/relational/sqlserver    # Add your new driver here
    ${CMAKE_CURRENT_SOURCE_DIR}/kv/redis
    ${CMAKE_CURRENT_SOURCE_DIR}/document/mongodb
    ${CMAKE_CURRENT_SOURCE_DIR}/columnar/scylladb
)
```

This allows test files to `#include "XX_001_test_<driver>_real_common.hpp"` without specifying the full path.

#### 5. Update file copy operations for common resources:
```cmake
# Copy the test_db_connections.yml file from common/
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/common/test_db_connections.yml
    ${CMAKE_CURRENT_BINARY_DIR}/test_db_connections.yml
    COPYONLY)

# Copy the test.jpg file from common/
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/common/test.jpg
    ${CMAKE_CURRENT_BINARY_DIR}/test.jpg
    COPYONLY)
```

#### 6. Update common integration tests (if applicable):

If you need to add support for the new driver in `common/10_061_test_integration.cpp`, wrap it with `USE_<DRIVER>`:

```cpp
#if USE_SQLSERVER
#include "27_001_test_sqlserver_real_common.hpp"
#endif

// Later in the test file:
#if USE_SQLSERVER
TEST_CASE("SQL Server Integration", "[10_061_07_sqlserver_integration]")
{
    auto driverResult = cpp_dbc::SQLServer::SQLServerDBDriver::getInstance(std::nothrow);
    REQUIRE(driverResult.has_value());
    auto driver = driverResult.value();
    // ... test code ...
}
#endif
```

---

## Phase 4: Create Benchmarks

### Benchmark File Structure

Benchmarks test CRUD operations:
```text
benchmark_sqlserver_select.cpp
benchmark_sqlserver_insert.cpp
benchmark_sqlserver_update.cpp
benchmark_sqlserver_delete.cpp
```

#### Benchmark Naming Convention

Benchmark functions follow the pattern:
```cpp
static void BM_SQLServer_Select_Small(benchmark::State& state) { ... }
static void BM_SQLServer_Select_Medium(benchmark::State& state) { ... }
static void BM_SQLServer_Select_Large(benchmark::State& state) { ... }

BENCHMARK(BM_SQLServer_Select_Small);
BENCHMARK(BM_SQLServer_Select_Medium);
BENCHMARK(BM_SQLServer_Select_Large);
```

### Update Benchmark Configuration YAML

**Single file for all drivers**: `libs/cpp_dbc/benchmark/benchmark_db_connections.yml`

Add entries for your driver following the existing patterns.

### Update benchmark/CMakeLists.txt

Add the benchmark files:
```cmake
# SQL Server benchmarks
benchmark_sqlserver_select.cpp
benchmark_sqlserver_insert.cpp
benchmark_sqlserver_update.cpp
benchmark_sqlserver_delete.cpp
```

Add compile definition:
```cmake
$<$<BOOL:${USE_SQLSERVER}>:USE_SQLSERVER=1>
$<$<NOT:$<BOOL:${USE_SQLSERVER}>>:USE_SQLSERVER=0>
```

---

## Phase 5: Create Examples

Examples demonstrate how to use the new driver and serve as documentation for users. The examples vary significantly based on the driver family since each has a different API.

### Example Files by Family

#### Relational Database Examples

For relational databases (MySQL, PostgreSQL, SQLite, Firebird, SQL Server), create examples following the numeric naming convention `XX_YZZ_example_<db>_<feature>.cpp`:

```text
# Example structure for a new relational driver (e.g., SQL Server = family 27)
libs/cpp_dbc/examples/relational/sqlserver/27_001_example_sqlserver_basic.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_021_example_sqlserver_connection_info.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_031_example_sqlserver_connection_pool.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_041_example_sqlserver_transaction_manager.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_051_example_sqlserver_json.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_061_example_sqlserver_blob.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_071_example_sqlserver_join.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_081_example_sqlserver_batch.cpp
libs/cpp_dbc/examples/relational/sqlserver/27_091_example_sqlserver_error_handling.cpp
```

**Naming Convention:**
- **XX**: Database family number (20=MySQL, 21=PostgreSQL, 22=SQLite, 23=Firebird, 27=new driver)
- **YZZ**: Feature category (001=basic, 021=connection_info, 031=pool, 041=transaction, 051=json, 061=blob, 071=join, 081=batch, 091=error_handling)

**Key operations to demonstrate:**
- Connection and configuration
- Basic CRUD (SELECT, INSERT, UPDATE, DELETE)
- Prepared statements with parameter binding
- Transaction management (BEGIN, COMMIT, ROLLBACK)
- JOIN operations
- BLOB/binary data handling
- JSON operations (if supported)

**Example structure:**
```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_SQLSERVER
#include <cpp_dbc/drivers/relational/driver_sqlserver.hpp>
#endif

#if USE_SQLSERVER
void demonstrateSQLServer()
{
    // 1. Create driver and connect
    // 2. Create table
    // 3. Insert data
    // 4. Query data
    // 5. Update data
    // 6. Delete data
    // 7. Transaction example
    // 8. Prepared statement example
}
#endif

int main()
{
#if USE_SQLSERVER
    demonstrateSQLServer();
#else
    std::cout << "SQL Server support not enabled" << std::endl;
#endif
    return 0;
}
```

#### Key-Value Database Examples

For key-value stores (Redis), create examples following the numeric naming convention:

```text
# Example structure for a new KV driver (e.g., Memcached = family 28)
libs/cpp_dbc/examples/kv/memcached/28_001_example_memcached_basic.cpp
libs/cpp_dbc/examples/kv/memcached/28_021_example_memcached_connection_info.cpp
libs/cpp_dbc/examples/kv/memcached/28_031_example_memcached_connection_pool.cpp
libs/cpp_dbc/examples/kv/memcached/28_041_example_memcached_transaction.cpp
libs/cpp_dbc/examples/kv/memcached/28_061_example_memcached_blob.cpp
libs/cpp_dbc/examples/kv/memcached/28_081_example_memcached_batch.cpp
libs/cpp_dbc/examples/kv/memcached/28_091_example_memcached_error_handling.cpp
```

**Existing KV Examples:**
- Redis uses family 24 (24_xxx series)

**Key operations to demonstrate:**
- Connection (URL-based)
- String operations (SET, GET, DEL)
- Expiration (TTL)
- List operations (LPUSH, RPUSH, LRANGE)
- Hash operations (HSET, HGET, HGETALL)
- Set operations (SADD, SMEMBERS)

**Example structure:**
```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_NEWKV
#include <cpp_dbc/drivers/kv/driver_newkv.hpp>
#endif

int main()
{
#if USE_NEWKV
    auto driverResult = NewKVDriver::getInstance(std::nothrow);
    if (!driverResult.has_value()) { return 1; }
    auto driver = driverResult.value();
    auto conn = driver->connectKV("newkv://localhost:1234", "", "");

    // String operations
    conn->setString("key", "value");
    std::string val = conn->getString("key");

    // With expiration
    conn->setString("key_exp", "value", 60);

    // Other KV operations...
#endif
    return 0;
}
```

#### Document Database Examples

For document databases (MongoDB), create examples following the numeric naming convention:

```text
# Example structure for a new document driver (e.g., CouchDB = family 29)
libs/cpp_dbc/examples/document/couchdb/29_001_example_couchdb_basic.cpp
libs/cpp_dbc/examples/document/couchdb/29_021_example_couchdb_connection_info.cpp
libs/cpp_dbc/examples/document/couchdb/29_031_example_couchdb_connection_pool.cpp
libs/cpp_dbc/examples/document/couchdb/29_081_example_couchdb_batch.cpp
libs/cpp_dbc/examples/document/couchdb/29_091_example_couchdb_error_handling.cpp
```

**Existing Document Examples:**
- MongoDB uses family 25 (25_xxx series)

**Key operations to demonstrate:**
- Connection and database selection
- Collection operations
- Document CRUD (insert, find, update, delete)
- Query with filters
- Aggregation pipelines (for JOINs)
- BSON/JSON handling

**Example structure:**
```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_NEWDOC
#include <cpp_dbc/drivers/document/driver_newdoc.hpp>
#endif

#if USE_NEWDOC
void demonstrateDocumentDB()
{
    auto driverResult = NewDocDriver::getInstance(std::nothrow);
    if (!driverResult.has_value()) { return; }
    auto driver = driverResult.value();
    auto conn = driver->connectDocument("newdoc://localhost:27017", "database");

    // Insert document
    auto doc = conn->createDocument();
    doc->set("name", "John");
    doc->set("age", 30);
    conn->insertOne("collection", doc);

    // Query documents
    auto cursor = conn->find("collection", {});
    while (cursor->next()) {
        auto result = cursor->current();
        std::cout << result->toJson() << std::endl;
    }
}
#endif
```

#### Columnar Database Examples

For columnar/wide-column databases (ScyllaDB/Cassandra), create examples following the numeric naming convention:

```text
# Example structure for a new columnar driver (e.g., Cassandra = family 30)
libs/cpp_dbc/examples/columnar/cassandra/30_001_example_cassandra_basic.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_021_example_cassandra_connection_info.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_031_example_cassandra_connection_pool.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_051_example_cassandra_json.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_061_example_cassandra_blob.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_081_example_cassandra_batch.cpp
libs/cpp_dbc/examples/columnar/cassandra/30_091_example_cassandra_error_handling.cpp
```

**Existing Columnar Examples:**
- ScyllaDB uses family 26 (26_xxx series)

**Key operations to demonstrate:**
- Connection with contact points
- Keyspace creation and selection
- Table creation with partition/clustering keys
- CQL queries (INSERT, SELECT, UPDATE, DELETE)
- Prepared statements
- BLOB handling
- JSON/map column types

**Example structure:**
```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_NEWCOLUMNAR
#include <cpp_dbc/drivers/columnar/driver_newcolumnar.hpp>
#endif

#if USE_NEWCOLUMNAR
void demonstrateColumnarDB(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    // Create keyspace
    conn->executeUpdate("CREATE KEYSPACE IF NOT EXISTS test_ks ...");

    // Create table with partition key
    conn->executeUpdate("CREATE TABLE test_ks.users (id int PRIMARY KEY, name text)");

    // Insert with prepared statement
    auto stmt = conn->prepareStatement("INSERT INTO test_ks.users (id, name) VALUES (?, ?)");
    stmt->setInt(1, 1);
    stmt->setString(2, "John");
    stmt->executeUpdate();

    // Query
    auto rs = conn->executeQuery("SELECT * FROM test_ks.users");
    while (rs->next()) {
        std::cout << rs->getInt("id") << ": " << rs->getString("name") << std::endl;
    }
}
#endif
```

#### Graph Database Examples (Future)

For graph databases, examples should demonstrate:
- Node creation
- Edge/relationship creation
- Graph traversal queries
- Path finding
- Pattern matching

#### Time-Series Database Examples (Future)

For time-series databases, examples should demonstrate:
- Time-based data insertion
- Time-range queries
- Aggregations over time windows
- Downsampling

### Update CMakeLists.txt for Examples

Add the example executables in `libs/cpp_dbc/CMakeLists.txt` following the numeric naming convention:

```cmake
# ============================================================================
# SQL Server Examples (27_xxx) - examples/relational/sqlserver/
# ============================================================================
if(USE_SQLSERVER AND CPP_DBC_BUILD_EXAMPLES)
    # Basic operations
    add_executable(27_001_example_sqlserver_basic
        examples/relational/sqlserver/27_001_example_sqlserver_basic.cpp)
    target_link_libraries(27_001_example_sqlserver_basic PRIVATE cpp_dbc)

    # Connection info
    add_executable(27_021_example_sqlserver_connection_info
        examples/relational/sqlserver/27_021_example_sqlserver_connection_info.cpp)
    target_link_libraries(27_021_example_sqlserver_connection_info PRIVATE cpp_dbc)

    # Connection pool
    add_executable(27_031_example_sqlserver_connection_pool
        examples/relational/sqlserver/27_031_example_sqlserver_connection_pool.cpp)
    target_link_libraries(27_031_example_sqlserver_connection_pool PRIVATE cpp_dbc)

    # Transaction manager
    add_executable(27_041_example_sqlserver_transaction_manager
        examples/relational/sqlserver/27_041_example_sqlserver_transaction_manager.cpp)
    target_link_libraries(27_041_example_sqlserver_transaction_manager PRIVATE cpp_dbc)

    # JSON operations
    add_executable(27_051_example_sqlserver_json
        examples/relational/sqlserver/27_051_example_sqlserver_json.cpp)
    target_link_libraries(27_051_example_sqlserver_json PRIVATE cpp_dbc)

    # BLOB operations
    add_executable(27_061_example_sqlserver_blob
        examples/relational/sqlserver/27_061_example_sqlserver_blob.cpp)
    target_link_libraries(27_061_example_sqlserver_blob PRIVATE cpp_dbc)

    # JOIN operations
    add_executable(27_071_example_sqlserver_join
        examples/relational/sqlserver/27_071_example_sqlserver_join.cpp)
    target_link_libraries(27_071_example_sqlserver_join PRIVATE cpp_dbc)

    # Batch operations
    add_executable(27_081_example_sqlserver_batch
        examples/relational/sqlserver/27_081_example_sqlserver_batch.cpp)
    target_link_libraries(27_081_example_sqlserver_batch PRIVATE cpp_dbc)

    # Error handling
    add_executable(27_091_example_sqlserver_error_handling
        examples/relational/sqlserver/27_091_example_sqlserver_error_handling.cpp)
    target_link_libraries(27_091_example_sqlserver_error_handling PRIVATE cpp_dbc)
endif()
```

**Important**:
- Wrap driver-specific examples in `if(USE_<DRIVER> AND CPP_DBC_BUILD_EXAMPLES)` blocks
- Use the numeric prefix system: `XX_YZZ_example_<db>_<feature>.cpp`
- Organize by database family folder: `examples/relational/`, `examples/kv/`, `examples/document/`, `examples/columnar/`
- For a new relational driver, use the next available family number (e.g., 27 for SQL Server)

---

## Checklist Summary

### Phase 1: Driver Files
- [ ] Created `libs/cpp_dbc/include/cpp_dbc/drivers/<family>/driver_<name>.hpp`
- [ ] Created `libs/cpp_dbc/include/cpp_dbc/drivers/<family>/<name>/` with all .hpp files
- [ ] Created `libs/cpp_dbc/src/drivers/<family>/<name>/` with all .cpp files
- [ ] All files follow C++17+ conventions and project coding standards
- [ ] RAII handles for all external resources
- [ ] Thread safety with `DB_DRIVER_LOCK_GUARD(*m_connMutex)` for connection methods and `*_STMT_LOCK_OR_*` macros for statement/result set methods
- [ ] Updated `libs/cpp_dbc/include/cpp_dbc/cpp_dbc.hpp`
  - [ ] Added `#ifndef USE_<DRIVER>` macro definition
  - [ ] Added `#if USE_<DRIVER>` conditional include
- [ ] Registered in `DriverManager::initDrivers` in `libs/cpp_dbc/src/driver_manager.cpp`
  - [ ] Added `#if USE_<DRIVER>` block with `!drivers.contains("<name>")` guard
  - [ ] Used `XxxDBDriver::getInstance(std::nothrow)` (singleton, not `make_shared`)
  - [ ] Driver name string matches `XxxDBDriver::getName()` exactly
- [ ] Updated class hierarchy diagram `libs/cpp_dbc/docs/class_hierarchy.drawio`
  - [ ] Read the XML comment at the top of the file for the layout specification (zones, Y rows, spacing rules)
  - [ ] Added a box (250×50) for each new class in its corresponding zone (Driver, Connection, ResultSet, etc.)
  - [ ] Used the correct Y row for the database family (Columnar=12, Document=88.75, KV=165, Relational=240/320)
  - [ ] Used the correct sub-column (Yellow concrete) at 290px spacing from the Family (Green) column
  - [ ] Used the concrete style: `fillColor=#fff2cc;strokeColor=#d6b656;fontSize=11`
- [ ] Connection Pool Integration
  - **Existing family** (relational, kv, document, columnar):
    - [ ] Added driver subclass pool class in `pool/<family>/<family>_db_connection_pool.hpp`
    - [ ] Added driver subclass pool implementation in `pool/<family>/<family>_db_connection_pool.cpp`
  - **New family** (graph, timeseries, etc.) — all of the above, plus:
    - [ ] Added `template <typename,typename,typename> friend class PooledDBConnectionBase;` to `core/<family>/<family>_db_connection.hpp`
    - [ ] Created `pool/<family>/<family>_db_connection_pool.hpp` with `<Family>PooledDBConnection` (CRTP) + `<Family>DBConnectionPool`
    - [ ] Created `pool/<family>/<family>_db_connection_pool.cpp` with all implementations
    - [ ] Added explicit template instantiation + `#include` in `pool/pooled_db_connection_base.cpp`
    - [ ] Added `src/pool/<family>/<family>_db_connection_pool.cpp` to `libs/cpp_dbc/CMakeLists.txt`

### Phase 2: Build Configuration
- [ ] Updated `libs/cpp_dbc/CMakeLists.txt`
  - [ ] Added option
  - [ ] Added compile definitions
  - [ ] Added source files
  - [ ] Added find_package
  - [ ] Added to install files
- [ ] Created `libs/cpp_dbc/cmake/Find<Name>.cmake`
- [ ] Updated `libs/cpp_dbc/cmake/cpp_dbc-config.cmake`
  - [ ] Added `CPP_DBC_USE_<DRIVER>` variable
  - [ ] Added compile definitions
  - [ ] Added find_package and linking
- [ ] Updated `libs/cpp_dbc/cmake/FindCPPDBC.cmake`
  - [ ] Added driver detection and linking
- [ ] Updated `libs/cpp_dbc/benchmark/benchmark_common.hpp`
  - [ ] Added `#if USE_<DRIVER>` include
- [ ] Updated `libs/cpp_dbc/build_cpp_dbc.sh`
- [ ] Updated `libs/cpp_dbc/build_test_cpp_dbc.sh`
- [ ] Updated `libs/cpp_dbc/run_test_cpp_dbc.sh`
- [ ] Updated `libs/cpp_dbc/run_benchmarks_cpp_dbc.sh`
- [ ] Updated `libs/cpp_dbc/build_dist_pkg.sh`
- [ ] Updated root `helper.sh`
  - [ ] Added driver option to `show_usage()` function
  - [ ] Added driver option to `cmd_run_build()` function
  - [ ] Added driver option to `cmd_run_test()` function
  - [ ] Added driver option to `cmd_run_benchmarks()` function
  - [ ] Updated `--mc-combo-XX` shortcuts to include new driver
  - [ ] Updated `--bk-combo-XX` shortcuts to include new driver
- [ ] Updated root `run_test.sh`
- [ ] Updated ALL distro folders:
  - [ ] `distros/debian_12/Dockerfile`
  - [ ] `distros/debian_12/build_script.sh`
  - [ ] `distros/debian_13/Dockerfile`
  - [ ] `distros/debian_13/build_script.sh`
  - [ ] `distros/ubuntu_22_04/Dockerfile`
  - [ ] `distros/ubuntu_22_04/build_script.sh`
  - [ ] `distros/ubuntu_24_04/Dockerfile`
  - [ ] `distros/ubuntu_24_04/build_script.sh`
  - [ ] `distros/fedora_42/Dockerfile`
  - [ ] `distros/fedora_42/build_script.sh`
  - [ ] `distros/fedora_43/Dockerfile`
  - [ ] `distros/fedora_43/build_script.sh`
- [ ] (If no official package) Created `libs/cpp_dbc/download_and_setup_<driver>_driver.sh`
  - [ ] Checks for existing installation (pkg-config, headers, libraries)
  - [ ] Installs build dependencies for apt-get, dnf, yum
  - [ ] Downloads, builds, and installs to /usr/local
  - [ ] Integrated with build_cpp_dbc.sh

#### ⚠️ CHECKPOINT: Validate Before Phase 3

**DO NOT proceed to Phase 3 until you verify the library compiles successfully.**

```bash
# Option 1: Use a combo shortcut (after updating it to include the new driver)
./helper.sh --mc-combo-01

# Option 2: Use --run-build with specific options for the new driver
./helper.sh --run-build=clean,<new_driver>,test
```

- [ ] Library compiles without errors
- [ ] No warnings with `-Wall -Wextra -Wpedantic`
- [ ] Driver can be instantiated (basic smoke test)

### Phase 3: Tests
- [ ] Compiles without warnings with `-Wall -Wextra -Wpedantic`
- [ ] Created test directory structure: `test/<family>/<driver>/`
  - Relational: `test/relational/<driver>/`
  - KV: `test/kv/<driver>/`
  - Document: `test/document/<driver>/`
  - Columnar: `test/columnar/<driver>/`
- [ ] Created test helper files (`XX_001_...common.cpp/hpp`) in appropriate family directory
- [ ] Created all required test files in appropriate family directory
- [ ] Updated `libs/cpp_dbc/test/CMakeLists.txt`
  - [ ] Added option for the new driver
  - [ ] Added test files with family-prefixed paths (e.g., `relational/sqlserver/27_001_...`)
  - [ ] Added compile definitions
  - [ ] Added include directory for the new driver (e.g., `${CMAKE_CURRENT_SOURCE_DIR}/relational/sqlserver`)
- [ ] Updated `common/10_061_test_integration.cpp` (if applicable)
  - [ ] Added `#if USE_<DRIVER>` include for driver's common header
  - [ ] Added integration test case wrapped in `#if USE_<DRIVER>`
- [ ] Added entries to `libs/cpp_dbc/test/common/test_db_connections.yml` (single file in common/, add new driver entries)
- [ ] All tests pass

#### ⚠️ CHECKPOINT: Validate Before Phase 4

**DO NOT proceed to Phase 4 until all tests pass.**

```bash
# Option 1: Use a combo shortcut (after updating it to include the new driver)
./helper.sh --bk-combo-01

# Option 2: Use --run-test with specific options for the new driver
./helper.sh --run-test=rebuild,<new_driver>,auto,run=1
```

- [ ] All tests compile without errors
- [ ] All tests pass (no failures)
- [ ] No memory leaks detected (if using valgrind)

### Phase 4: Benchmarks
- [ ] Compiles without warnings
- [ ] Created all benchmark files (select, insert, update, delete)
- [ ] Updated `libs/cpp_dbc/benchmark/CMakeLists.txt`
- [ ] Added entries to `libs/cpp_dbc/benchmark/benchmark_db_connections.yml` (single file, add new driver entries)
- [ ] All benchmarks run successfully

#### ⚠️ CHECKPOINT: Validate Before Phase 5

**DO NOT proceed to Phase 5 until all benchmarks run successfully.**

```bash
# Use a combo shortcut for benchmarks
./helper.sh --dk-combo-01

# Or use --run-benchmarks with specific options
./helper.sh --run-benchmarks=rebuild,<new_driver>,memory-usage
```

- [ ] All benchmarks compile without errors
- [ ] All benchmarks execute successfully
- [ ] Performance is reasonable for the driver

### Phase 5: Examples
- [ ] Created example files following numeric naming convention `XX_YZZ_example_<db>_<feature>.cpp`
  - [ ] Basic example: `XX_001_example_<driver>_basic.cpp`
  - [ ] Connection info: `XX_021_example_<driver>_connection_info.cpp`
  - [ ] Connection pool: `XX_031_example_<driver>_connection_pool.cpp`
  - [ ] Additional feature examples (transaction, JSON, BLOB, JOIN, batch, error handling) as applicable
- [ ] Created examples in appropriate family folder:
  - Relational: `examples/relational/<driver>/`
  - KV: `examples/kv/<driver>/`
  - Document: `examples/document/<driver>/`
  - Columnar: `examples/columnar/<driver>/`
- [ ] Updated `libs/cpp_dbc/CMakeLists.txt` to add example executables
- [ ] Examples wrapped in `if(USE_<DRIVER> AND CPP_DBC_BUILD_EXAMPLES)` blocks
- [ ] Examples compile and run successfully
- [ ] Examples are well-documented with comments
- [ ] Used next available family number (e.g., 27 for SQL Server)

---

## Common Mistakes to Avoid

### 1. Forgetting Distro Files
The most common mistake is updating the main build scripts but forgetting the distro folder files:
- `libs/cpp_dbc/distros/<distro>/Dockerfile`
- `libs/cpp_dbc/distros/<distro>/build_script.sh`

### 2. Inconsistent Option Names
Ensure the option name is consistent across all files:
- CMake: `USE_SQLSERVER`
- Shell: `--sqlserver`, `--sqlserver-on`, `--sqlserver-off`
- Debug: `DEBUG_SQLSERVER`

### 3. Missing Compile Definitions
Don't forget to add compile definitions in:
- `libs/cpp_dbc/CMakeLists.txt`
- `libs/cpp_dbc/test/CMakeLists.txt`
- `libs/cpp_dbc/benchmark/CMakeLists.txt`

### 4. Missing Debug Options
If your driver needs debug output, add:
- `DEBUG_<DRIVER>` option to CMake
- `--debug-<driver>` option to shell scripts
- Handler in `helper.sh`

### 5. Not Using RAII
Always use RAII handles for external resources:
```cpp
// Good - RAII handle
using SQLServerStmtHandle = std::unique_ptr<SQLHSTMT, SQLServerStmtDeleter>;

// Bad - raw handle
SQLHSTMT stmt;  // Must remember to free manually
```

### 6. Thread Safety
Use the macro for conditional locking in connection methods:
```cpp
DB_DRIVER_LOCK_GUARD(*m_connMutex);
```
For PreparedStatement/ResultSet methods, use the RAII helper macros (e.g., `MY_STMT_LOCK_OR_RETURN`).

### 7. DBException Error Codes

> **See also**: [error_handling_patterns.md](error_handling_patterns.md) for complete patterns including exception-based and nothrow API usage.

Error codes must be **12-character uppercase alphanumeric** strings with:
- At least 5 letters (A-Z)
- No more than 4 consecutive repeated characters
- Unique across the entire project

```cpp
// Correct - generated code
throw DBException("7K3F9J2B5Z8D", "Connection failed", system_utils::captureCallStack());

// Incorrect examples
throw DBException("INVALID_KEY", ...);   // Not alphanumeric only
throw DBException("001", ...);           // Too short
throw DBException("AAAAAB123456", ...);  // 5 consecutive 'A's
```

Generate unique codes with:
```bash
./generate_dbexception_code.sh /path/to/project      # Generate 1 code
./generate_dbexception_code.sh /path/to/project 10   # Generate 10 codes
```

See `generate_dbexception_code.sh` for implementation details.

### 8. Missing Source Compilation Script
For drivers without official `.deb`/`.rpm` packages, you must create a `download_and_setup_<driver>_driver.sh` script. Don't assume the developer will manually install the driver. See [Source Compilation Fallback](#source-compilation-fallback).

### 9. Forgetting Combo Commands in helper.sh
The root `helper.sh` has combo shortcuts that build/test multiple drivers at once:
- `--mc-combo-01`, `--mc-combo-02`, etc. (build combos)
- `--bk-combo-01`, `--bk-combo-02`, etc. (benchmark combos)

When adding a new driver, update these combos to include support for compiling and testing with the new database.

### 10. Missing or Incomplete Examples
Examples are essential documentation for users. Don't skip Phase 5:
- Each driver needs examples following the numeric naming convention `XX_YZZ_example_<db>_<feature>.cpp`
- At minimum, create basic and connection pool examples
- Examples must be in appropriate family folder: `examples/<family>/<driver>/`
- Examples must be wrapped in `#if USE_<DRIVER>` preprocessor guards
- Examples must be added to `CMakeLists.txt` inside `if(USE_<DRIVER> AND CPP_DBC_BUILD_EXAMPLES)` blocks
- The API demonstrated should match the driver's family (don't show SQL for a KV store)
- Use the next available family number for the new driver

### 11. Forgetting Connection Pool Integration
A new driver is useless without pool support. For **existing families**, add the driver subclass pool (e.g., `SQLServerConnectionPool`) in the family pool header/cpp. For **new families**, you must create the entire pool stack:
- `<Family>PooledDBConnection` using CRTP base `PooledDBConnectionBase<D,C,P>`
- `<Family>DBConnectionPool` inheriting from `DBConnectionPoolBase`
- Explicit template instantiation in `pooled_db_connection_base.cpp`
- `PooledDBConnectionBase` friend declaration in the family connection class

See [Integrate with Connection Pool](#integrate-with-connection-pool) for full details.

### 12. Forgetting CMake Config Files
The installed package configuration files also need updates:
- `libs/cpp_dbc/cmake/cpp_dbc-config.cmake` - For package consumers
- `libs/cpp_dbc/cmake/FindCPPDBC.cmake` - For fallback discovery
- `libs/cpp_dbc/benchmark/benchmark_common.hpp` - For benchmark includes

Without these updates, users who install cpp_dbc as a package won't be able to use the new driver.

---

## Architectural Patterns

This section documents cross-cutting patterns that apply to **all drivers regardless of family**. Every new driver must implement these patterns.

### PrivateCtorTag Pattern — Throw-Free Construction with `std::make_shared`

#### Problem

Driver classes (Connection, PreparedStatement, ResultSet) need:
1. **Throw-free constructors** — errors must be captured, not thrown, to support `-fno-exceptions` builds
2. **`std::make_shared` compatibility** — for efficient single-allocation construction
3. **Prevention of direct construction** — only static `create()` factories should instantiate objects

A private constructor satisfies (1) and (3) but breaks (2): `std::make_shared` cannot call private constructors. The **PrivateCtorTag** pattern (a variant of the PassKey idiom) resolves this conflict.

#### Solution

Define a private nested struct `PrivateCtorTag` inside the class. The constructor is **public** (so `std::make_shared` can call it) but requires a `PrivateCtorTag` instance as its first parameter. Since the tag type is private, external code cannot construct it — only the class's own static factory methods can.

Two member variables track construction outcome:
- `m_initFailed` (`bool`, default `false`) — set to `true` if initialization fails
- `m_initError` (`std::unique_ptr<DBException>`, default `nullptr`) — stores the error for deferred delivery, allocated **only** on the failure path

**Why `std::unique_ptr<DBException>` instead of a bare `DBException`**: A `DBException` object is ~256 bytes. Storing it inline means every instance pays that cost even when construction succeeds (the vast majority of cases). With `std::unique_ptr`, successful instances cost only 8 bytes (the pointer), and the full `DBException` is heap-allocated only when there is an actual error. `std::make_unique` can throw `std::bad_alloc`, but this happens inside a catch block on an already-failed constructor — if the system cannot allocate ~256 bytes, it is a death sentence regardless.

The nothrow constructor wraps all initialization in try/catch and stores errors instead of throwing. The `create()` factory checks `m_initFailed` after construction and returns `std::unexpected` if set.

#### Universal Rule

**ALL driver classes** — Connection, PreparedStatement, ResultSet, Blob, InputStream, Cursor, Collection, Document — use the PrivateCtorTag pattern without exception. There is no "private constructor + `new`" variant. Every class follows the same structure:

1. **Public `noexcept` constructor** guarded by `PrivateCtorTag` — prevents external construction while enabling `std::make_shared`
2. **`m_initFailed` / `m_initError`** member variables — capture construction errors for deferred delivery
3. **Static `create` factory** (nothrow + throwing wrapper) — the only way to instantiate objects
4. **`std::make_shared`** in the factory — if it throws `std::bad_alloc`, that is a death sentence (no try/catch)

This applies **inexorably and strictly** to every class in every driver.

#### Public Section Layout — Three Regions

The `public:` section of every class with a dual throw/nothrow API **must** be organized as exactly three regions, in this order:

1. **Constructor, destructor, deleted ops** (always compiled) — the public `noexcept` constructor guarded by `PrivateCtorTag`, the destructor, and deleted copy/move operators. These appear before any `#ifdef` guard.
2. **Throwing block** — a **single** `#ifdef __cpp_exceptions` / `#endif` guard containing **ALL** throwing public methods (factory, getters, setters, lifecycle). No throwing method may appear outside this block. No nothrow method may appear inside it.
3. **Nothrow block** — immediately after the `#endif`, containing **ALL** nothrow public methods (factory, getters, setters, lifecycle). This region is always compiled, regardless of exception support.

```cpp
public:
    // Constructor, destructor, deleted ops (always compiled)
    MyClass(PrivateCtorTag, std::nothrow_t, ...) noexcept;
    ~MyClass() override;
    MyClass(const MyClass &) = delete;
    MyClass &operator=(const MyClass &) = delete;

    // ══════════════════════════════════════════════════════════════
    // BLOCK 1: ALL throwing methods — single #ifdef, nothing else
    // ══════════════════════════════════════════════════════════════
#ifdef __cpp_exceptions
    static std::shared_ptr<MyClass> create(...);
    void connect();
    void close();
    Result query(const std::string &sql);
#endif

    // ══════════════════════════════════════════════════════════════
    // BLOCK 2: ALL nothrow methods — always compiled
    // ══════════════════════════════════════════════════════════════
    static cpp_dbc::expected<std::shared_ptr<MyClass>, DBException>
    create(std::nothrow_t, ...) noexcept;
    cpp_dbc::expected<void, DBException> connect(std::nothrow_t) noexcept;
    cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept;
    cpp_dbc::expected<Result, DBException> query(std::nothrow_t, ...) noexcept;
```

**Forbidden**: interleaving throwing and nothrow declarations, multiple `#ifdef __cpp_exceptions` blocks in the same `public:` section, throwing methods outside the guard, nothrow methods inside the guard.

The order of methods **within** each block must match across throwing and nothrow — if the throwing block declares `close()` then `query()`, the nothrow block must declare `close(std::nothrow_t)` then `query(std::nothrow_t)` in the same order.

#### Pattern: PrivateCtorTag (Connection)

**Header** — tag definition + construction state + public constructor + factories:

```cpp
class MyDBConnection final : public RelationalDBConnection,
                              public std::enable_shared_from_this<MyDBConnection>
{
    // ── PrivateCtorTag ────────────────────────────────────────────────────
    // Private tag for the passkey idiom — enables std::make_shared from
    // static factory methods while keeping the constructor effectively
    // private (external code cannot construct PrivateCtorTag).
    struct PrivateCtorTag
    {
        explicit PrivateCtorTag() = default;
    };

    // ── Member variables ──────────────────────────────────────────────────
    MyHandle m_handle;
    std::atomic<bool> m_closed{true};
    // ... other members ...

    // ── Construction state ────────────────────────────────────────────────
    // Flag indicating constructor initialization failed. Set by the nothrow
    // constructor when connection setup fails. Inspected by create(nothrow_t)
    // to propagate the error via expected.
    bool m_initFailed{false};

    // Error captured when constructor initialization fails. Only allocated
    // on the failure path (~256 bytes saved per successful instance).
    std::unique_ptr<DBException> m_initError{nullptr};

    // ── Private helpers ───────────────────────────────────────────────────
    // ... private nothrow methods ...

public:
    // ── Public nothrow constructor (guarded by PrivateCtorTag) ─────────────
    MyDBConnection(PrivateCtorTag,
                   std::nothrow_t,
                   const std::string &host,
                   int port,
                   const std::string &database,
                   const std::string &user,
                   const std::string &password,
                   const std::map<std::string, std::string> &options) noexcept;

    ~MyDBConnection() override;

#ifdef __cpp_exceptions
    // ── Throwing factory ──────────────────────────────────────────────────
    static std::shared_ptr<MyDBConnection>
    create(const std::string &host, int port, const std::string &database,
           const std::string &user, const std::string &password,
           const std::map<std::string, std::string> &options = {})
    {
        auto r = create(std::nothrow, host, port, database, user, password, options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }
#endif

    // ── Nothrow factory ───────────────────────────────────────────────────
    static cpp_dbc::expected<std::shared_ptr<MyDBConnection>, DBException>
    create(std::nothrow_t,
           const std::string &host, int port, const std::string &database,
           const std::string &user, const std::string &password,
           const std::map<std::string, std::string> &options = {}) noexcept
    {
        auto obj = std::make_shared<MyDBConnection>(
            PrivateCtorTag{}, std::nothrow, host, port, database, user, password, options);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }
};
```

**Implementation** — constructor captures errors instead of throwing:

```cpp
MyDBConnection::MyDBConnection(PrivateCtorTag,
                               std::nothrow_t,
                               const std::string &host,
                               int port,
                               const std::string &database,
                               const std::string &user,
                               const std::string &password,
                               const std::map<std::string, std::string> &options) noexcept
#if DB_DRIVER_THREAD_SAFE
    : m_connMutex(std::make_shared<std::recursive_mutex>())
#endif
{
    try
    {
        m_handle = c_api_connect(host, port, database, user, password);
        if (!m_handle)
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>(
                "XXXXXXXXXXXX", "Failed to connect",
                system_utils::captureCallStack());
            return;
        }
        m_closed.store(false, std::memory_order_seq_cst);
    }
    catch (const std::exception &ex)
    {
        m_initFailed = true;
        m_initError = std::make_unique<DBException>(
            "XXXXXXXXXXXX", ex.what(),
            system_utils::captureCallStack());
    }
    catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
    {
        m_initFailed = true;
        m_initError = std::make_unique<DBException>(
            "XXXXXXXXXXXX", "Unknown error in constructor",
            system_utils::captureCallStack());
    }
}
```

#### Pattern: PrivateCtorTag (PreparedStatement / ResultSet / Other Children)

Child classes follow the **exact same PrivateCtorTag pattern** as Connection. The only structural difference is the constructor parameters (they receive a weak reference to the parent Connection instead of host/port/etc.):

```cpp
class MyDBPreparedStatement final : public RelationalDBPreparedStatement
{
    // ── PrivateCtorTag ────────────────────────────────────────────────────
    // Same passkey idiom as Connection — prevents external construction
    // while enabling std::make_shared from the static factory.
    struct PrivateCtorTag
    {
        explicit PrivateCtorTag() = default;
    };

    // ── Member variables ──────────────────────────────────────────────────
    std::weak_ptr<MyDBConnection> m_connection;
    std::string m_sql;
    // ...

    // ── Construction state ────────────────────────────────────────────────
    bool m_initFailed{false};
    std::unique_ptr<DBException> m_initError{nullptr};

    // ── Private helpers ───────────────────────────────────────────────────
    // ... private nothrow methods ...

public:
    // ── Public nothrow constructor (guarded by PrivateCtorTag) ─────────────
    MyDBPreparedStatement(PrivateCtorTag,
                          std::nothrow_t,
                          std::weak_ptr<MyDBConnection> conn,
                          const std::string &sql) noexcept;

    ~MyDBPreparedStatement() override;

#ifdef __cpp_exceptions
    static std::shared_ptr<MyDBPreparedStatement>
    create(std::weak_ptr<MyDBConnection> conn, const std::string &sql)
    {
        auto r = create(std::nothrow, std::move(conn), sql);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }
#endif

    static cpp_dbc::expected<std::shared_ptr<MyDBPreparedStatement>, DBException>
    create(std::nothrow_t,
           std::weak_ptr<MyDBConnection> conn,
           const std::string &sql) noexcept
    {
        // std::make_shared — if bad_alloc is thrown, it is a death sentence
        // (unrecoverable). No try/catch needed.
        auto obj = std::make_shared<MyDBPreparedStatement>(
            PrivateCtorTag{}, std::nothrow, std::move(conn), sql);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }
};
```

#### Reference Implementations

All driver classes use `PrivateCtorTag` + `std::make_shared`. There is no alternative pattern.

| Driver | Connection | PreparedStatement | ResultSet |
|--------|-----------|-------------------|-----------|
| **Firebird** | `PrivateCtorTag` + `make_shared` | `PrivateCtorTag` + `make_shared` | `PrivateCtorTag` + `make_shared` |
| **MySQL** | `PrivateCtorTag` + `make_shared` | `PrivateCtorTag` + `make_shared` | `PrivateCtorTag` + `make_shared` |
| **MongoDB** | `PrivateCtorTag` + `make_shared` | `PrivateCtorTag` + `make_shared` (Cursor, Collection, Document) | — |
| **Redis** | `PrivateCtorTag` + `make_shared` | — | — |
| **ScyllaDB** | `PrivateCtorTag` + `make_shared` | `PrivateCtorTag` + `make_shared` | `PrivateCtorTag` + `make_shared` |
| **SQLite** | Public ctor + `make_shared` (legacy) | Public ctor + `make_shared` (legacy) | Public ctor + `make_shared` (legacy) |

> **Note**: SQLite uses an older pattern with public throwing constructors. New drivers **must** follow the `PrivateCtorTag` pattern for **all** classes.

---

### DBDriver Singleton + Connection Registry Pattern

Every `*DBDriver` class (e.g. `MySQLDBDriver`, `MongoDBDriver`) implements two tightly-coupled mechanisms: a **`weak_ptr`-based singleton** that enforces a single live driver instance per process, and a **connection registry** that tracks all open connections for lifecycle management.

#### Why a Singleton?

A `DBDriver` wraps a C library that typically requires one-time global initialization (e.g. `mysql_library_init`, `mongoc_init`). Creating multiple driver instances in the same process would either double-initialize the library or leave a stale `s_initialized` flag after the first instance is destroyed — both incorrect.

The `weak_ptr` singleton is the right tool here because:
- It allows the driver to be destroyed (C library cleaned up) when no one holds a `shared_ptr` to it.
- The next `getInstance()` call transparently creates a fresh instance, reinitializing the library.
- No global `shared_ptr` is kept alive forever — memory and library resources are released when the driver is no longer needed.

#### Required Static Members

Add these static members to every `*DBDriver` class header (inside `#if USE_<DRIVER>`):

```cpp
// ── Initialization state ───────────────────────────────────────────────────
// atomic<bool> + mutex instead of std::once_flag because once_flag cannot be
// reset, but cleanup() must allow re-initialization after driver destruction.
// Also, std::call_once can throw std::system_error, incompatible with -fno-exceptions.
static std::atomic<bool> s_initialized;
static std::mutex        s_initMutex;

// ── Singleton state ────────────────────────────────────────────────────────
static std::weak_ptr<XxxDBDriver> s_instance;
static std::mutex                 s_instanceMutex;

// ── Connection registry ────────────────────────────────────────────────────
static std::mutex                                                      s_registryMutex;
static std::set<std::weak_ptr<XxxDBConnection>,
                std::owner_less<std::weak_ptr<XxxDBConnection>>>       s_connectionRegistry;
```

Add these private methods:

```cpp
static cpp_dbc::expected<bool, DBException> initialize(std::nothrow_t) noexcept;
static void registerConnection(std::nothrow_t, std::weak_ptr<XxxDBConnection> conn) noexcept;
static void unregisterConnection(std::nothrow_t, const std::weak_ptr<XxxDBConnection> &conn) noexcept;
friend class XxxDBConnection;  // so XxxDBConnection can call unregisterConnection
```

Add these public methods:

```cpp
static std::shared_ptr<XxxDBDriver> getInstance();                                          // throwing
static cpp_dbc::expected<std::shared_ptr<XxxDBDriver>, DBException> getInstance(std::nothrow_t) noexcept;
static void cleanup();
static size_t getConnectionAlive() noexcept;
```

#### `getInstance(std::nothrow_t)` Implementation

```cpp
cpp_dbc::expected<std::shared_ptr<XxxDBDriver>, DBException>
XxxDBDriver::getInstance(std::nothrow_t) noexcept
{
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    auto existing = s_instance.lock();
    if (existing)
    {
        return existing;
    }
    try
    {
        auto inst = std::make_shared<XxxDBDriver>();
        s_instance = inst;
        return inst;
    }
    catch (const DBException &ex)
    {
        return cpp_dbc::unexpected(ex);
    }
    catch (const std::exception &ex)
    {
        return cpp_dbc::unexpected(DBException("XXXXXXXXXXXX",
            std::string("Failed to initialize Xxx driver singleton: ") + ex.what(),
            system_utils::captureCallStack()));
    }
    catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
    {
        return cpp_dbc::unexpected(DBException("XXXXXXXXXXXX",
            "Unknown error while initializing Xxx driver singleton",
            system_utils::captureCallStack()));
    }
}
```

#### Connection Registry

The registry uses `std::set<std::weak_ptr<XxxDBConnection>, std::owner_less<...>>` so that expired weak pointers remain comparable. `registerConnection` is called by `connectXxx()` after successful connection creation. `unregisterConnection` is called by `XxxDBConnection::close()` using the `m_self` weak pointer stored during `create()`.

**In the driver's `connectXxx()` method** — call `registerConnection` after success:

```cpp
auto conn = XxxDBConnection::create(std::nothrow, host, port, db, user, password, options);
if (!conn.has_value())
{
    return cpp_dbc::unexpected(conn.error());
}
registerConnection(std::nothrow, conn.value());
return conn.value();
```

**In `XxxDBConnection`** — add `m_self` and call unregister in `close()`:

```cpp
// In the class header:
std::weak_ptr<XxxDBConnection> m_self;

// In create() factory — set m_self after construction succeeds:
obj->m_self = obj;

// In close(std::nothrow_t) — unregister before closing:
XxxDBDriver::unregisterConnection(std::nothrow, m_self);
```

`m_self` must be set in the factory (not the constructor) because `shared_from_this()` is unavailable inside a constructor. The `owner_less` comparator in the registry set compares by control block identity, so a `weak_ptr<XxxDBConnection>` can be erased even after the object is partially destroyed.

#### `getConnectionAlive()` Implementation

```cpp
size_t XxxDBDriver::getConnectionAlive() noexcept
{
    std::lock_guard<std::mutex> lock(s_registryMutex);
    return static_cast<size_t>(std::count_if(
        s_connectionRegistry.begin(), s_connectionRegistry.end(),
        [](const auto &w) { return !w.expired(); }));
}
```

---

### Mutex Sharing — Connection and Child Classes

#### Overview

Every Connection class creates a mutex that protects operations on the underlying C API handle. Child classes (PreparedStatement, ResultSet, Blob, Cursor, Collection, Document, etc.) that depend semantically and/or functionally on the Connection may share this mutex or create their own, depending on the database's execution model. Note: `*InputStream` classes are NOT child objects of Connection — they are pure in-memory byte buffers with no connection reference. Blob is created by ResultSet (via `getBlob()`) but its lifecycle dependency is directly on Connection, not on ResultSet.

The key architectural invariant is:

1. **Connection owns the mutex** — created as `std::shared_ptr<std::recursive_mutex>` during construction
2. **Children hold a `std::weak_ptr` back to Connection** — not a raw pointer, to detect parent destruction safely
3. **Connection tracks all children** — via `std::set<std::weak_ptr<Child>, std::owner_less<...>>` registries
4. **When Connection closes, all children close** — releases C API resources and marks them as closed
5. **Children reject operations when the parent connection is closed** — every child object must verify that the parent connection is **both alive and open** before processing any request. This is a double check:
   - `weak_ptr` expired → connection destroyed → child marks itself closed, returns error
   - `conn->m_closed == true` → connection explicitly closed but still alive → child returns error

   This applies **even to child objects that operate purely in-memory** (e.g., MySQL/PostgreSQL ResultSets in the Lax model where all data is in client memory). A closed connection means the session is over — continuing to serve data from a closed session violates the lifecycle contract. The `*_STMT_LOCK_OR_RETURN` macros (both thread-safe and non-thread-safe variants) enforce this automatically via the `*ConnectionLock` RAII helper or the `m_closed` + `m_connection.expired()` checks.

#### Mutex Strictness Levels

Different databases require different levels of mutex sharing depending on their C API threading model:

| Level | Description | Example Drivers |
|-------|-------------|-----------------|
| **Strict** | ONE mutex shared by Connection + PreparedStatement + ResultSet | Firebird |
| **Extra-strict** | ONE mutex per connection + ONE mutex per database file (multiple connections to the same local file share a file-level mutex) | SQLite |
| **Lax** | ONE mutex shared by Connection + PreparedStatement; ResultSet has its OWN independent mutex | MySQL, PostgreSQL |

The strictness level is determined by how the C API handles concurrent access:

##### Strict (Firebird)

Firebird uses a **cursor-based** execution model. `isc_dsql_fetch()` communicates with the database handle for EACH row. All operations — connection, statement, and result set — must be serialized through a single mutex per connection.

```text
Connection ─── m_connMutex (shared_ptr<recursive_mutex>)
  │
  ├── PreparedStatement ─── accesses mutex via m_connection.lock()->getConnectionMutex(std::nothrow)
  ├── ResultSet ─────────── accesses mutex via m_connection.lock()->getConnectionMutex(std::nothrow)
  └── Blob ──────────────── accesses mutex via m_connection.lock()->getConnectionMutex(std::nothrow)
                            (created by ResultSet, but depends on Connection)
```

> **Note**: `*InputStream` is not shown — it is a pure in-memory byte buffer with no connection dependency.

##### Extra-Strict (SQLite)

SQLite has the same cursor-based model as Firebird, but adds a unique constraint: it is a **local file-based database**. Multiple connections to the **same database file** from different threads must also be serialized, because SQLite uses POSIX file locks internally for synchronization — which sanitizers (ThreadSanitizer, Helgrind) cannot detect.

```text
FileMutexRegistry (singleton)
  │
  ├── "/path/to/db1.sqlite" ─── globalFileMutex_A (shared_ptr<recursive_mutex>)
  │     │
  │     ├── Connection_1 ─── m_globalFileMutex = globalFileMutex_A
  │     │     ├── PreparedStatement ─── via m_connection.lock()->getConnectionMutex(std::nothrow)
  │     │     ├── ResultSet ─────────── via m_connection.lock()->getConnectionMutex(std::nothrow)
  │     │     └── Blob ──────────────── via m_connection.lock()->getConnectionMutex(std::nothrow)
  │     │
  │     └── Connection_2 ─── m_globalFileMutex = globalFileMutex_A  (SAME mutex!)
  │           ├── PreparedStatement ─── via m_connection.lock()->getConnectionMutex(std::nothrow)
  │           ├── ResultSet ─────────── via m_connection.lock()->getConnectionMutex(std::nothrow)
  │           └── Blob ──────────────── via m_connection.lock()->getConnectionMutex(std::nothrow)
  │
  └── "/path/to/db2.sqlite" ─── globalFileMutex_B (different mutex)
        │
        └── Connection_3 ─── m_globalFileMutex = globalFileMutex_B
```

This pattern is specific to **local file-based databases**. Network-based databases (MySQL, PostgreSQL, Firebird) do not need file-level mutexes because the server handles concurrency. The `FileMutexRegistry` normalizes paths (`/tmp/db`, `/tmp//db`, `/tmp/./db` all map to the same mutex) and uses `weak_ptr` for automatic cleanup when all connections to a file are closed.

Special case: `:memory:` databases get a local per-connection mutex instead of a registry-based one, since there is no shared file.

> **Note**: The file-level mutex is controlled by the CMake option `ENABLE_FILE_MUTEX_REGISTRY`. It is enabled for sanitizer testing and disabled in production (zero overhead). See `libs/cpp_dbc/include/cpp_dbc/common/file_mutex_registry.hpp`.

##### Lax (MySQL, PostgreSQL)

MySQL and PostgreSQL use a **"store result"** model. `mysql_store_result()` / `PQexec()` fetches ALL data into client memory (`MYSQL_RES*` / `PGresult*`). After that, ResultSet operations (`next()`, `getString()`, etc.) only read in-memory structures — they do NOT communicate with the connection handle. Therefore, ResultSet can use its own independent `std::recursive_mutex` (`m_mutex`) instead of sharing the connection's mutex.

However, PreparedStatement operations (`mysql_stmt_prepare()`, `mysql_stmt_execute()`, `mysql_stmt_close()`) DO communicate with the connection handle. They must share the connection's mutex.

> **If your new driver's C API fetches all result rows into client memory at construction** (like MySQL's `mysql_store_result()` or PostgreSQL's `PQexec()`), your ResultSet should follow this Lax model:
> - Use its own independent `std::recursive_mutex m_mutex` for thread-safe serialization of in-memory reads
> - Do **NOT** use `*ConnectionLock` or `*_STMT_LOCK_OR_RETURN` (those acquire the connection mutex)
> - Still hold `std::weak_ptr<*DBConnection> m_connection` for lifecycle checks
> - Still check `m_closed` + `m_connection.expired()` before every operation (lifecycle contract)
> - Update `.claude/rules/cpp_dbc_conventions.md` rule #3 exception list and `.claude/rules/cpp_dbc_conventions_violations_how_report_them.md` checklist to include your driver alongside MySQL and PostgreSQL
>
> If your driver's `next()` communicates with the server for each row (like Firebird's `isc_dsql_fetch()` or SQLite's `sqlite3_step()`), you **must** use the Strict or Extra-Strict model instead.

```text
Connection ─── m_connMutex (shared_ptr<recursive_mutex>)
  │
  ├── PreparedStatement ─── accesses mutex via m_connection.lock()->getConnectionMutex(std::nothrow)
  │                         (shares connection's mutex)
  │
  ├── ResultSet ─────────── m_mutex (own independent recursive_mutex)
  │                         (does NOT share connection's mutex — but still checks
  │                          parent connection state via m_connection weak_ptr)
  │
  └── Blob ──────────────── accesses mutex via m_connection.lock()->getConnectionMutex(std::nothrow)
                            (created by ResultSet, depends on Connection — ensureLoaded/save use native API)
```

##### Blob — Always Synchronized (Created by ResultSet, Depends on Connection)

Blob classes are **created by ResultSet** (via `getBlob()`) but their lifecycle dependency is **directly on Connection**, not on ResultSet. They perform native database API calls (`ensureLoaded()`, `save()`) and must follow the same synchronization pattern as PreparedStatement:

| Requirement | Description |
|-------------|-------------|
| `m_connection` | `std::weak_ptr<*DBConnection>` — **not** `weak_ptr<NativeHandle>` |
| `m_closed` | `mutable std::atomic<bool>{false}` — for double-checked locking |
| `friend` | `friend class *ConnectionLock` — so the RAII helper can access `m_connection` |
| Lock before native calls | `*_STMT_LOCK_OR_RETURN` / `*_STMT_LOCK_OR_THROW` before any native DB API call |
| Handle access | Obtain native handle through connection (e.g., `conn->m_conn`) under the lock |

This applies regardless of the mutex strictness level (Strict, Extra-Strict, or Lax). The Blob does not need to know the type of mutex or where it comes from — it acquires it through the connection, identical to PreparedStatement.

**`*InputStream` is NOT a child object of Connection.** All current `*InputStream` implementations are pure in-memory byte buffers (`std::vector<uint8_t>` + position cursor) with no `m_connection`, no `weak_ptr`, and no native DB API calls. They copy data at construction and are fully self-contained. They do not appear in the ownership diagrams above.

#### Child Registry Pattern

Connection tracks its children using weak pointer sets with owner-based comparison:

```cpp
// In Connection header
std::set<std::weak_ptr<MyDBPreparedStatement>,
         std::owner_less<std::weak_ptr<MyDBPreparedStatement>>> m_activeStatements;
std::set<std::weak_ptr<MyDBResultSet>,
         std::owner_less<std::weak_ptr<MyDBResultSet>>> m_activeResultSets;
```

**Why `std::weak_ptr`**: The Connection does not own its children. Users hold the `shared_ptr`. When the user releases a PreparedStatement, it is destroyed naturally. The registry only tracks — it does not extend lifetime.

**Why `std::owner_less`**: Standard `operator<` on `weak_ptr` compares the managed pointer, which can change across `lock()` calls. `std::owner_less` compares ownership (the control block), which is stable.

**Periodic cleanup**: Expired entries are cleaned lazily — when the set exceeds a threshold (e.g., 50 entries), `std::erase_if` removes expired weak pointers:

```cpp
cpp_dbc::expected<void, DBException>
MyDBConnection::registerStatement(std::nothrow_t,
                                  std::weak_ptr<MyDBPreparedStatement> stmt) noexcept
{
    LOCK_OR_RETURN("XXXXXXXXXXXX", "Connection closed");
    if (m_activeStatements.size() > 50)
    {
        std::erase_if(m_activeStatements, [](const auto &w) { return w.expired(); });
    }
    m_activeStatements.insert(stmt);
    return {};
}
```

#### Connection Close Cascade

When a Connection closes, it must close all its children BEFORE releasing C API resources. The pattern uses a **two-phase approach** to avoid iterator invalidation and deadlocks:

1. **Phase 1** (under lock): Collect live children into a temporary vector, then clear the registry
2. **Phase 2** (outside lock): Close each child

```cpp
cpp_dbc::expected<void, DBException>
MyDBConnection::closeAllStatements(std::nothrow_t) noexcept
{
    std::vector<std::shared_ptr<MyDBPreparedStatement>> stmtsToClose;

    {
        LOCK_OR_RETURN("XXXXXXXXXXXX", "Connection closed");
        for (auto &weakStmt : m_activeStatements)
        {
            if (auto stmt = weakStmt.lock())
            {
                stmtsToClose.push_back(stmt);
            }
        }
        m_activeStatements.clear();  // Clear BEFORE releasing lock
    }

    // Close outside the lock — prevents deadlock if close() re-enters the connection
    for (auto &stmt : stmtsToClose)
    {
        [[maybe_unused]] auto closeResult = stmt->notifyConnClosing(std::nothrow);
    }
    return {};
}
```

**Why two phases**: `close()` on a child may call `unregisterStatement()` on the Connection, which would modify `m_activeStatements` while iterating — causing iterator invalidation. By clearing the set first and closing outside the lock, this race is eliminated.

#### Child Back-Reference via `std::weak_ptr`

Every child class holds a `std::weak_ptr<MyDBConnection>` back to its parent. This is critical for:

1. **Lifecycle safety**: `weak_ptr::lock()` detects when the Connection is destroyed. Methods can return an error ("connection destroyed") instead of dereferencing a dangling pointer.
2. **Mutex access**: Children acquire the connection's mutex through `m_connection.lock()->getConnectionMutex(std::nothrow)`.
3. **C API access**: Children access C API resources (transaction handle, database handle) through the locked connection.

```cpp
// In PreparedStatement methods:
auto conn = m_connection.lock();
if (!conn)
{
    return cpp_dbc::unexpected(
        DBException("XXXXXXXXXXXX", "Connection destroyed",
                   system_utils::captureCallStack()));
}
// Safe: connection is alive while conn shared_ptr exists
auto &tr = conn->getTransactionHandle();
c_api_execute(tr, m_stmt, ...);
```

> **CRITICAL**: Never store a raw pointer to the Connection or its members. Use `weak_ptr` and lock it before every access. Raw pointers cause USE-AFTER-FREE bugs when the Connection is destroyed while children still exist.

#### Connection Lock Helper (All Drivers)

All drivers use an RAII helper class that acquires the connection's mutex through `weak_ptr<Connection>` with double-checked locking. This is **required** for all Statement/ResultSet-level macros:

```cpp
class MyConnectionLock
{
    std::shared_ptr<MyDBConnection> m_conn;  // Keeps connection alive while lock is held
    std::unique_lock<std::recursive_mutex> m_lock;
    bool m_acquired{false};

public:
    template <typename T>
    MyConnectionLock(T *obj, std::atomic<bool> &closed)
    {
        // FIRST CHECK: fast path — already closed, no lock needed
        if (closed.load(std::memory_order_seq_cst))
        {
            return;
        }

        // Obtain connection and KEEP IT ALIVE
        m_conn = obj->m_connection.lock();
        if (!m_conn)
        {
            closed.store(true, std::memory_order_seq_cst);
            return;
        }

        // Acquire connection's mutex (connection stays alive via m_conn)
        m_lock = std::unique_lock<std::recursive_mutex>(m_conn->getConnectionMutex(std::nothrow));

        // SECOND CHECK: another thread may have closed between first check and lock
        if (closed.load(std::memory_order_seq_cst))
        {
            return;
        }

        m_acquired = true;
    }

    bool isAcquired() const noexcept { return m_acquired; }
    explicit operator bool() const noexcept { return m_acquired; }
};
```

The `getConnectionMutex(std::nothrow_t)` method must be declared in the **`protected`** section of the Connection class — it is an implementation detail, not part of the public API. The `*ConnectionLock` RAII helper accesses it through `friend class *ConnectionLock` declared in the Connection class:

```cpp
protected:
    std::recursive_mutex &getConnectionMutex(std::nothrow_t) noexcept
    {
        return *m_connMutex;
    }
```

This helper is typically used via **six macros** organized in two levels. Each driver defines its own set with the driver prefix (e.g. `MYSQL_`, `FIREBIRD_`, `SQLITE_`). See `src/drivers/relational/mysql/mysql_internal.hpp` and `src/drivers/relational/firebird/firebird_internal.hpp` for complete reference implementations:

**Connection-level macros** — used inside `*DBConnection` methods. Acquire the connection's own mutex (`m_connMutex`, a `SharedConnMutex` / `std::shared_ptr<std::recursive_mutex>`) directly. The `*m_connMutex` dereference is required because the mutex is a `shared_ptr`:

```cpp
// Thread-safe variants
#if DB_DRIVER_THREAD_SAFE

// For nothrow DBConnection methods — returns unexpected if connection is closed or handle is null
#define MY_CONNECTION_LOCK_OR_RETURN(mark, msg)                                              \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                                                      \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)                                 \
    {                                                                                        \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",             \
                                               cpp_dbc::system_utils::captureCallStack()));  \
    }

// For throwing DBConnection methods — throws if connection is closed or handle is null
#define MY_CONNECTION_LOCK_OR_THROW(mark, msg)                             \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                                    \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)               \
    {                                                                      \
        throw DBException(mark, msg " (connection closed)",                \
                          cpp_dbc::system_utils::captureCallStack());       \
    }

// For close() method — returns success if already closed (idempotent)
#define MY_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()    \
    DB_DRIVER_LOCK_GUARD(*m_connMutex);                      \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn) \
    {                                                        \
        return {}; /* Already closed = success */            \
    }

#else
// Non-thread-safe: still check closed state and handle validity, no locking
#define MY_CONNECTION_LOCK_OR_RETURN(mark, msg)                                              \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)                                 \
    {                                                                                        \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",             \
                                               cpp_dbc::system_utils::captureCallStack()));  \
    }

#define MY_CONNECTION_LOCK_OR_THROW(mark, msg)                             \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn)               \
    {                                                                      \
        throw DBException(mark, msg " (connection closed)",                \
                          cpp_dbc::system_utils::captureCallStack());       \
    }

#define MY_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()    \
    if (m_closed.load(std::memory_order_seq_cst) || !m_conn) \
    {                                                        \
        return {}; /* Already closed = success */            \
    }
#endif
```

**Statement-level macros** — used inside `*DBPreparedStatement` and `*DBResultSet` methods. Use the `ConnectionLock` RAII helper to acquire the mutex through `weak_ptr<Connection>`. The RAII helper class **must** be defined in the driver's `*_internal.hpp` (see `ConnectionLock` section above). The lock variable name must follow the pattern `<driver>_conn_lock_` (e.g., `mysql_conn_lock_`, `postgresql_conn_lock_`). Never use reserved identifiers like `__lock`:

```cpp
#if DB_DRIVER_THREAD_SAFE

// For nothrow child methods — returns unexpected if lock fails
#define MY_STMT_LOCK_OR_RETURN(mark, msg)                                                    \
    cpp_dbc::MyNS::MyConnectionLock my_conn_lock_(this, m_closed);                           \
    if (!my_conn_lock_)                                                                      \
    {                                                                                        \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",             \
                                               cpp_dbc::system_utils::captureCallStack()));  \
    }

// For throwing child methods — throws if lock fails
#define MY_STMT_LOCK_OR_THROW(mark, msg)                                                                 \
    cpp_dbc::MyNS::MyConnectionLock my_conn_lock_(this, m_closed);                                       \
    if (!my_conn_lock_)                                                                                  \
    {                                                                                                    \
        throw DBException(mark, msg " (connection closed)", cpp_dbc::system_utils::captureCallStack());  \
    }

// For close() methods — returns success if already closed (idempotent)
#define MY_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()                          \
    cpp_dbc::MyNS::MyConnectionLock my_conn_lock_(this, m_closed);          \
    if (!my_conn_lock_)                                                     \
    {                                                                       \
        return {}; /* Already closed or connection lost = success */        \
    }

#else
// Non-thread-safe: just check closed flag, no locking
#define MY_STMT_LOCK_OR_RETURN(mark, msg)                                                    \
    if (m_closed.load(std::memory_order_seq_cst))                                            \
    {                                                                                        \
        return cpp_dbc::unexpected(DBException(mark, msg " (connection closed)",             \
                                               cpp_dbc::system_utils::captureCallStack()));  \
    }

#define MY_STMT_LOCK_OR_THROW(mark, msg)                                                                 \
    if (m_closed.load(std::memory_order_seq_cst))                                                        \
    {                                                                                                    \
        throw DBException(mark, msg " (connection closed)", cpp_dbc::system_utils::captureCallStack());  \
    }

#define MY_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED()        \
    if (m_closed.load(std::memory_order_seq_cst))          \
    {                                                      \
        return {}; /* Already closed = success */          \
    }
#endif
```

**Usage examples:**

```cpp
// In Connection nothrow method:
cpp_dbc::expected<void, DBException>
MyDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
{
    MY_CONNECTION_LOCK_OR_RETURN("XXXXXXXXXXXX", "executeUpdate failed");
    // ... safe to use C API under lock ...
}

// In PreparedStatement nothrow method:
cpp_dbc::expected<void, DBException>
MyDBPreparedStatement::setInt(std::nothrow_t, int index, int value) noexcept
{
    MY_STMT_LOCK_OR_RETURN("XXXXXXXXXXXX", "Connection lost in setInt");
    // ... safe to use C API under lock ...
}

// In close() method (idempotent):
cpp_dbc::expected<void, DBException>
MyDBPreparedStatement::close(std::nothrow_t) noexcept
{
    MY_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();
    // ... cleanup logic ...
}
```

**Naming convention**: Replace `MY_` with the driver prefix: `MYSQL_`, `FIREBIRD_`, `SQLITE_`, `SCYLLADB_`, `REDIS_`, `MONGODB_`. Replace `MyNS` with the driver namespace and `MyConnectionLock` with the driver-specific lock class name.

#### Applying to Non-Relational Families

The same patterns apply to all driver families — only the child class types differ:

| Family | Connection | Children (share mutex or own) |
|--------|-----------|-------------------------------|
| **Relational** | `*DBConnection` | `*DBPreparedStatement`, `*DBResultSet` |
| **Document** | `MongoDBConnection` | `MongoDBCollection`, `MongoDBCursor`, `MongoDBDocument` |
| **Columnar** | `ScyllaDBConnection` | `ScyllaDBPreparedStatement`, `ScyllaDBResultSet` |
| **KV** | `RedisDBConnection` | (typically no long-lived children) |

The invariants remain the same:
- Children hold `weak_ptr` back to Connection
- Connection tracks children in `weak_ptr` registries
- Connection close cascades to all children
- Mutex strictness depends on the C API's threading model

#### Reference Implementations

| Pattern | Reference Files |
|---------|----------------|
| **PrivateCtorTag** (all classes) | `drivers/relational/firebird/connection.hpp` (lines 57-60, 140-224), `drivers/document/mongodb/cursor.hpp` (lines 39-43, 151-161) |
| **Construction state** (`m_initFailed`/`m_initError` as `unique_ptr`) | `drivers/relational/mysql/connection.hpp` (lines 130-134), `drivers/relational/firebird/result_set.hpp` (lines 78-91) |
| **Strict mutex** (one per connection) | `drivers/relational/firebird/connection.hpp` (lines 76-85) |
| **Extra-strict mutex** (file-level) | `drivers/relational/sqlite/connection.hpp`, `common/file_mutex_registry.hpp` |
| **Lax mutex** (independent ResultSet) | `drivers/relational/mysql/result_set.hpp` (lines 145-172) |
| **Child registry** (weak_ptr set) | `drivers/relational/firebird/connection.hpp` (lines 72-74) |
| **Close cascade** (two-phase) | `src/drivers/relational/firebird/connection_01.cpp` (lines 343-425) |
| **ConnectionLock** (RAII helper) | `src/drivers/relational/firebird/firebird_internal.hpp` (lines 100-160), `src/drivers/relational/mysql/mysql_internal.hpp` (lines 95-143) |
| **Lock macros** (6-macro pattern) | `src/drivers/relational/firebird/firebird_internal.hpp` (conn: lines 40-69, stmt: lines 165-192), `src/drivers/relational/mysql/mysql_internal.hpp` (conn: lines 41-72, stmt: lines 148-191) |
| **Debug macro** | `src/drivers/relational/firebird/firebird_internal.hpp` (lines 196-212), `src/drivers/relational/mysql/mysql_internal.hpp` (lines 195-211) |
| **Complete `*_internal.hpp`** | `src/drivers/relational/firebird/firebird_internal.hpp` (full reference), `src/drivers/relational/mysql/mysql_internal.hpp` (full reference) |

---

## See Also

For detailed information on specific topics, see these companion documents:

| Document | When to Use |
|----------|-------------|
| [shell_script_dependencies.md](shell_script_dependencies.md) | Understanding script relationships during Phase 2 |
| [error_handling_patterns.md](error_handling_patterns.md) | Implementing error handling during Phase 1 |

---

## Contact and Support

For questions about adding new drivers, consult existing implementations:
- Relational: `libs/cpp_dbc/include/cpp_dbc/drivers/relational/mysql/`
- Document: `libs/cpp_dbc/include/cpp_dbc/drivers/document/mongodb/`
- Key-Value: `libs/cpp_dbc/include/cpp_dbc/drivers/kv/redis/`
- Columnar: `libs/cpp_dbc/include/cpp_dbc/drivers/columnar/scylladb/`
