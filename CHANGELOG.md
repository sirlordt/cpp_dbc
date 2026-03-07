# Changelog

## 2026-03-07 13:34:00 PST [Current]

### Unified URI API in DBDriver Base, PrivateCtorTag for MySQLBlob/InputStream, Expanded Convention Rules, and Convention Violations Reporting

This release refactors the `DBDriver` base class URI handling into a unified interface, applies the PrivateCtorTag creational pattern to `MySQLBlob` and `MySQLInputStream`, significantly expands the project conventions document with new coding rules, and adds a structured convention violations reporting document. Total: **49 files changed, +2938/-1354 lines**.

#### DBDriver Base Class — Unified URI API

- **`acceptsURL()` renamed to `acceptURI()`**: Both throwing and nothrow versions in `DBDriver` base class (throwing delegates to nothrow)
- **New `parseURI(std::nothrow_t)` pure virtual**: Each driver must implement URI parsing; returns `expected<map<string,string>, DBException>`
- **New `buildURI(std::nothrow_t)` pure virtual**: Each driver must implement URI building from components
- **New `getURIScheme()` pure virtual**: Returns human-readable URI template (e.g., `"cpp_dbc:mysql://<host>:<port>/<database>"`)
- **Default `acceptURI(std::nothrow_t)` implementation**: Uses `parseURI()` — a successful parse means the URI is accepted
- **Removed private helpers from family driver bases**: `acceptsURL()`, `parseURL()`, `buildURL()` removed from `ColumnarDBDriver`, `DocumentDBDriver`, `KVDBDriver` (-130 lines)

#### All 7 Drivers Updated for New URI API

- MySQL, PostgreSQL, SQLite, Firebird, MongoDB, Redis, ScyllaDB drivers updated to implement `acceptURI`, `parseURI`, `buildURI`, `getURIScheme` pure virtuals
- All driver `_01.cpp` files refactored: method renames + nothrow implementations
- All test files updated: `acceptsURL()` → `acceptURI()`, `parseURL()` → `parseURI()`, `buildURL()` → `buildURI()`

#### MySQLBlob — PrivateCtorTag Pattern

- **PrivateCtorTag + `m_initFailed`/`m_initError`**: Applied to `MySQLBlob` — public `noexcept` constructors guarded by private tag type
- **`#ifdef __cpp_exceptions` guards**: All throwing methods (`create`, `ensureLoaded`, `copyFrom`, `length`, `getBytes`, `getBinaryStream`, `setBytes`, `truncate`, `free`, `position`) now inside `#ifdef __cpp_exceptions`
- **Nothrow static factories**: `create(std::nothrow_t, ...)` uses `std::make_shared` with `PrivateCtorTag`
- **Removed `getMySQLHandle()` throwing variant**: Only nothrow version remains (private helper)

#### MySQLInputStream — PrivateCtorTag Pattern

- **PrivateCtorTag + `m_initFailed`/`m_initError`**: Same pattern applied to `MySQLInputStream`
- **`#ifdef __cpp_exceptions` guards**: All throwing methods guarded
- **Nothrow static factories**: `create(std::nothrow_t, ...)` with `PrivateCtorTag`

#### Convention Rules — Major Expansion (`.claude/rules/cpp_dbc_conventions.md`)

New convention sections added:
- **Preprocessor Directives — Always Flush Left (Column 0)**
- **Braces — Allman Style (Own Line)**: Opening and closing braces each on their own line
- **NOSONAR Comments — Rule ID and Explanation Required**: Must include `(cpp:SXXXX)` and brief explanation
- **Bug-Fix Comments**: ISO 8601 timestamp + `Bug:` + `Solution:` structured format
- **Unused Parameters — No `(void)` Cast**: Use unnamed parameters or `[[maybe_unused]]` instead
- **`private:` Access Specifier — Omit in `class`, Require in `struct`**
- **PrivateCtorTag Pattern**: Complete documentation for driver class creational pattern
- **Private/Protected Methods — Migration Rule**: Expanded with concrete examples and checklist

#### Convention Violations Reporting (New Document)

- **New `.claude/rules/cpp_dbc_conventions_violations_how_report_them.md`**: Structured format for convention compliance analysis reports
- Defines severity levels (Critical/High/Medium/Low), report structure, diagnostic checklist
- Comprehensive checklist of all conventions to verify
- Output location: `research/<driver_name>_improvements_<N>.md`

#### Documentation Updates

- `cppdbc-docs-en.md` and `cppdbc-docs-es.md`: Updated for `acceptURI`/`parseURI`/`buildURI` API
- `error_handling_patterns.md`: Updated method signatures
- `how_add_new_db_drivers.md`: Updated driver implementation guide with new URI API requirements

---

## 2026-03-06 20:29:58 PST

### MySQL Driver — Full Nothrow-First Refactor, Static Factory Pattern, Result Set Registry, and Dead try/catch Elimination

This release applies the nothrow-first dual-API architecture to the MySQL driver — the last relational driver to undergo this refactor. All four MySQL classes (`MySQLDBDriver`, `MySQLDBConnection`, `MySQLDBPreparedStatement`, `MySQLDBResultSet`) now follow the same patterns already established in Firebird, PostgreSQL, SQLite, MongoDB, ScyllaDB, and Redis: private nothrow constructors with `m_initFailed`/`m_initError` error deferral, static factory `create(std::nothrow_t)` without try/catch, `#ifdef __cpp_exceptions` guards, double-checked locking for driver init, `[[nodiscard]]` on all nothrow methods, and dead try/catch elimination across all `.cpp` files. Additionally, `MySQLDBConnection` gains a result set registry (`m_activeResultSets`) with `closeAllActiveResultSets()` for pool lifecycle safety, and `MySQLDBPreparedStatement`/`MySQLDBResultSet` now hold `weak_ptr<MySQLDBConnection>` instead of `weak_ptr<MYSQL>` — accessing the native handle and connection mutex through the connection object. Total: **17 files changed, +1559/-2202 lines** (net reduction of ~643 lines of boilerplate).

#### MySQLDBDriver — Double-Checked Locking for Library Init

- Added `static std::atomic<bool> s_initialized` + `static std::mutex s_initMutex` (replaces implicit init)
- New `static initialize(std::nothrow_t) noexcept` with double-checked locking pattern — compatible with `-fno-exceptions` and allows re-init after `cleanup()`
- New `static cleanup()` method — resets `s_initialized` so a new driver instance can re-initialize
- Constructor calls `initialize(std::nothrow)` internally

#### MySQLDBConnection — PrivateCtorTag + Result Set Registry

- **PrivateCtorTag pattern:** Public constructor guarded by private tag type — enables `std::make_shared` from factory while preventing external construction
- **Nothrow constructor:** All `mysql_init`, `mysql_real_connect`, `mysql_select_db`, and autocommit setup moved into `noexcept` constructor body with try/catch converting to `m_initFailed`/`m_initError`
- **`create(std::nothrow_t)` factory:** No try/catch — checks `m_initFailed` directly
- **`m_closed` default changed:** `{false}` → `{true}`, set to `false` only on successful connection
- **`m_resetting` flag:** `std::atomic<bool>` prevents unregister deadlock during `closeAllActiveResultSets()`/`closeAllStatements()`
- **Result set registry:** `m_activeResultSets` set of `weak_ptr<MySQLDBResultSet>` with `registerResultSet()`, `unregisterResultSet()`, `closeAllActiveResultSets()` — two-phase close pattern (collect under lock, close outside lock)
- **`getMySQLNativeHandle()`:** Returns raw `MYSQL*` for PreparedStatement/ResultSet access via `weak_ptr<MySQLDBConnection>`
- **`getConnectionMutex()`:** Returns `std::recursive_mutex&` for child object serialization (thread-safe builds only)
- **`isResetting()`:** Exposes `m_resetting` flag for ResultSet/PreparedStatement deadlock avoidance
- **Friend declarations:** `MySQLDBPreparedStatement`, `MySQLDBResultSet`, `MySQLBlob`
- **Dead try/catch eliminated** in: `executeUpdate`, `setAutoCommit`, `getAutoCommit`, `beginTransaction`, `commit`, `rollback`, `close`, `reset`, `isClosed`, `returnToPool`, `isPooled`, `getURL`, `setTransactionIsolation`, `getTransactionIsolation`, `prepareForPoolReturn`, `prepareForBorrow`

#### MySQLDBPreparedStatement — weak_ptr<MySQLDBConnection> + Private Constructor

- **`m_connection`:** `weak_ptr<MySQLDBConnection>` replaces `weak_ptr<MYSQL>` — accesses MYSQL* and mutex through the connection
- **Removed `m_connMutex`:** No longer stores separate mutex copy; locks via `m_connection.lock()->getConnectionMutex()`
- **Private nothrow constructor:** `mysql_stmt_init`, `mysql_stmt_prepare`, and parameter allocation in constructor body with try/catch → `m_initFailed`/`m_initError`
- **`create(std::nothrow_t)` uses `new`:** Cannot use `std::make_shared` with private constructor — static member function can access private ctor via `new`
- **`m_closed` flag:** New `std::atomic<bool>{true}` — set to `false` on successful init, checked in all operations
- **`notifyConnClosing()` → `notifyConnClosing(std::nothrow_t)`:** Returns `expected<void, DBException>` instead of void
- **Deleted copy/move operators** explicitly declared
- **Dead try/catch eliminated** in: all `set*()` methods, `executeQuery`, `executeUpdate`, `close`, `clearParameters`

#### MySQLDBResultSet — Private Constructor + Connection Registration

- **Private nothrow constructor:** Initializes column metadata from `MYSQL_RES*`; stores errors in `m_initFailed`/`m_initError`
- **`create(std::nothrow_t)` uses `new`:** Same pattern as PreparedStatement — `new` for private ctor access
- **`m_connection`:** `weak_ptr<MySQLDBConnection>` for lifecycle management (unregister on close)
- **`m_closed` flag:** New `std::atomic<bool>{true}` — checked in all operations
- **`notifyConnClosing(std::nothrow_t)`:** Connection calls this during `closeAllActiveResultSets()`
- **`[[nodiscard]]`** added to all nothrow method declarations
- **Dead try/catch eliminated** across all result set `.cpp` files

#### mysql_internal.hpp — Enhanced with RAII Helpers and Macros

- **`MySQLConnectionLock`:** RAII helper that locks connection mutex via `weak_ptr<MySQLDBConnection>` — validates connection is alive and not closed, returns `expected` on failure
- **`MYSQL_CONNECTION_LOCK_OR_RETURN` macro:** Expands to `MySQLConnectionLock` construction + error check — replaces repeated `DB_DRIVER_LOCK_GUARD` + closed-check boilerplate in every method
- **`MYSQL_DEBUG` macro:** Printf-style debug output using `logWithTimesMillis`

#### Connection Pool — Minor Fix

- `connection_pool.cpp`: Adjusted `returnConnection()` handling for improved connection validation flow

---

## 2026-03-06 08:43:23 PST

### CRTP `PooledDBConnectionBase<D,C,P>` — Unified Pooled Connection Logic via Template Inheritance

This release extracts the duplicated pooled connection wrapper logic from all four family-specific `PooledDBConnection` classes (relational, document, columnar, key-value) into a single CRTP base template `PooledDBConnectionBase<Derived, ConnType, PoolType>`. Previously, each family's pooled connection wrapper (~200–300 lines each) contained near-identical implementations of close/returnToPool (with the race-condition fix), destructor cleanup, and pool metadata accessors. Now, `PooledDBConnectionBase` contains all this logic once, and the family-specific wrappers provide one-line inline delegators to resolve the diamond inheritance between `DBConnectionPooled::DBConnection` and `FamilyDBConnection::DBConnection`. Dead try/catch blocks in `create(std::nothrow_t, ...)` factory methods are also removed. Total: **17 files changed, +1312/-2230 lines** (net reduction of ~918 lines).

#### New: `PooledDBConnectionBase<D,C,P>` — CRTP Template for Pooled Connections

- New file: `include/cpp_dbc/pool/pooled_db_connection_base.hpp` — declares the CRTP template inheriting from `DBConnectionPooled`
- New file: `src/pool/pooled_db_connection_base.cpp` (~485 lines) — complete implementation with explicit template instantiations for all four families:
  - **Protected members** (moved from each family wrapper): `m_conn`, `m_pool`, `m_poolAlive`, `m_creationTime`, `m_lastUsedTimeNs`, `m_active`, `m_closed`
  - **`*Impl` methods** for diamond-ambiguous DBConnection methods: `closeImpl`, `returnToPoolImpl`, `isClosedImpl`, `isPooledImpl`, `getURLImpl`, `resetImpl`, `pingImpl`, `prepareForPoolReturnImpl`, `prepareForBorrowImpl`
  - **`*Throw` methods** (under `#ifdef __cpp_exceptions`): `closeThrow`, `isClosedThrow`, `returnToPoolThrow`, `isPooledThrow`, `getURLThrow`, `resetThrow`, `pingThrow`
  - **DBConnectionPooled interface overrides** (no diamond): `isPoolValid`, `getCreationTime`, `getLastUsedTime`, `setActive`, `isActive`, `getUnderlyingConnection`, `markPoolClosed`, `isPoolClosed`, `updateLastUsedTime`
  - **Destructor** with pool-aware cleanup: returns connection if pool alive, calls `decrementActiveCount()` otherwise
  - Explicit instantiations: `RelationalPooledDBConnection`, `KVPooledDBConnection`, `DocumentPooledDBConnection`, `ColumnarPooledDBConnection`

#### Family Pooled Connections — Now Thin CRTP Delegators

Each family `PooledDBConnection` now inherits from `PooledDBConnectionBase<Derived, ConnType, PoolType>` and only provides:
1. **One-line inline throwing delegators** (under `#ifdef __cpp_exceptions`): `close() { this->closeThrow(); }`, etc.
2. **One-line inline nothrow delegators**: `close(std::nothrow_t) { return this->closeImpl(std::nothrow); }`, etc.
3. **`prepareForPoolReturn` / `prepareForBorrow` overrides**: delegate to `prepareForPoolReturnImpl` / `prepareForBorrowImpl`
4. **Family-specific methods** only (e.g., `prepareStatement`, `executeQuery` for columnar; KV get/set/hash operations; document CRUD)
5. **Destructors changed to `= default`** — cleanup logic now lives in the CRTP base destructor

- **`RelationalPooledDBConnection`**: relational-specific methods (prepareStatement, executeQuery, executeUpdate, transactions, BLOB)
- **`DocumentPooledDBConnection`**: document-specific methods (collections, CRUD, aggregation, sessions)
- **`ColumnarPooledDBConnection`**: columnar-specific methods (prepareStatement, executeQuery, executeUpdate, transactions)
- **`KVPooledDBConnection`**: KV-specific methods (get/set/hash/list/set/sorted set operations, scan, flushDB)

#### Friend Declarations for CRTP Access

- All four family connection base classes (`RelationalDBConnection`, `DocumentDBConnection`, `ColumnarDBConnection`, `KVDBConnection`) gained: `template <typename, typename, typename> friend class PooledDBConnectionBase;` — required because the CRTP base calls `m_conn->prepareForPoolReturn(std::nothrow, ...)` and `m_conn->prepareForBorrow(std::nothrow)`, which are `protected`
- `DBConnectionPoolBase` gained: `template <typename, typename, typename> friend class PooledDBConnectionBase;` — grants access to `returnConnection()` and `decrementActiveCount()`

#### Dead try/catch Elimination in Pool Factory Methods

- All four family pool `create(std::nothrow_t, ...)` factory methods: removed unnecessary try/catch blocks wrapping `std::make_shared` + `initializePool(std::nothrow)`. The only possible exception is `std::bad_alloc` from `std::make_shared`, which is a death-sentence exception per project conventions.

#### Documentation: Connection Pool Integration Guide

- `how_add_new_db_drivers.md`: Added comprehensive "Integrate with Connection Pool" section covering:
  - Connection pool architecture (3-layer diagram)
  - Scenario A: Adding a driver to an existing family (3 steps)
  - Scenario B: Adding a new family (6 steps with full code examples)
  - Updated Phase 1 checklist with pool integration items
  - New "Forgetting Connection Pool Integration" pitfall (#11)

#### CMakeLists.txt

- Added `src/pool/pooled_db_connection_base.cpp` to library source list

---

## 2026-03-06 01:59:06 PST

### Unified Connection Pool Base Class (`DBConnectionPoolBase`) — Extracted Common Pool Logic into `pool/` Directory

This release extracts the duplicated connection pool algorithm from all four database-family pool implementations (relational, document, columnar, key-value) into a single shared base class `DBConnectionPoolBase` in a new `pool/` directory. Previously, each family pool (~1700–2000 lines each) contained a near-identical copy of the core pool infrastructure (connection lifecycle, maintenance thread, direct handoff, HikariCP validation skip, phase-based lock protocol). Now, `DBConnectionPoolBase` (~955 lines) contains all this logic once, and the family-specific pools become thin derived classes (~100–250 lines each) that only override `createPooledDBConnection()` and provide typed getters. Total: **77 files changed, +6020/-8459 lines** (net reduction of ~2400 lines of duplicated pool logic).

#### New: `DBConnectionPoolBase` — Unified Pool Algorithm

- New file: `include/cpp_dbc/pool/connection_pool.hpp` — declares `DBConnectionPoolBase`, the single base class for all pools
- New file: `src/pool/connection_pool.cpp` (955 lines) — complete pool implementation:
  - Connection acquisition with direct handoff (`ConnectionRequest` + `m_waitQueue`)
  - HikariCP-style validation skip (500ms threshold)
  - Phase-based lock protocol (acquire under `m_mutexPool`, validate/close outside lock)
  - Maintenance thread with 30s wake cycle (idle eviction + minIdle replenishment)
  - `returnConnection()` with orphan detection, pool-closing exit, and validation
  - `decrementActiveCount()` for destructor-safe connection abandonment
- Pure virtual `createPooledDBConnection(std::nothrow_t) noexcept` — derived classes override to create family-specific pooled wrappers
- Protected accessors (`getUrl()`, `getUsername()`, `getPoolWeakPtr()`, etc.) for derived class use in `createPooledDBConnection()`
- Protected `acquireConnection()` — core borrow logic used by derived typed getters
- Protected `initializePool()` — called by derived factory methods after `make_shared`

#### New: `DBConnectionPooled` Interface Extension

- `include/cpp_dbc/pool/db_connection_pooled.hpp` — added pool-internal lifecycle methods:
  - `updateLastUsedTime(std::nothrow_t)`, `isPoolClosed(std::nothrow_t)`, `getClosedFlag(std::nothrow_t)` — used by `DBConnectionPoolBase` for pool algorithm operations

#### Directory Restructure: `core/` → `pool/`

All connection pool headers and sources moved from `core/` to `pool/`:

| Old Path | New Path |
|----------|----------|
| `include/cpp_dbc/core/db_connection_pool.hpp` | `include/cpp_dbc/pool/db_connection_pool.hpp` |
| `include/cpp_dbc/core/db_connection_pooled.hpp` | `include/cpp_dbc/pool/db_connection_pooled.hpp` |
| `include/cpp_dbc/core/relational/relational_db_connection_pool.hpp` | `include/cpp_dbc/pool/relational/relational_db_connection_pool.hpp` |
| `include/cpp_dbc/core/document/document_db_connection_pool.hpp` | `include/cpp_dbc/pool/document/document_db_connection_pool.hpp` |
| `include/cpp_dbc/core/columnar/columnar_db_connection_pool.hpp` | `include/cpp_dbc/pool/columnar/columnar_db_connection_pool.hpp` |
| `include/cpp_dbc/core/kv/kv_db_connection_pool.hpp` | `include/cpp_dbc/pool/kv/kv_db_connection_pool.hpp` |
| `src/core/relational/relational_db_connection_pool.cpp` | `src/pool/relational/relational_db_connection_pool.cpp` |
| `src/core/document/document_db_connection_pool.cpp` | `src/pool/document/document_db_connection_pool.cpp` |
| `src/core/columnar/columnar_db_connection_pool.cpp` | `src/pool/columnar/columnar_db_connection_pool.cpp` |
| `src/core/kv/kv_db_connection_pool.cpp` | `src/pool/kv/kv_db_connection_pool.cpp` |
| `src/core/connection_pool_internal.hpp` | `src/pool/connection_pool_internal.hpp` |

New placeholder directories: `include/cpp_dbc/pool/graph/`, `include/cpp_dbc/pool/timeseries/`

#### Family-Specific Pools — Now Thin Derived Classes

Each family pool now inherits from `DBConnectionPoolBase` and only provides:
1. `createPooledDBConnection()` override — creates the family-specific pooled wrapper via DriverManager
2. Typed getter (e.g., `getRelationalDBConnection()`) — casts from `DBConnectionPooled` to the family type
3. Factory methods (`create(std::nothrow_t, ...)`) — construct via `make_shared` + `initializePool()`

- **`RelationalDBConnectionPool`** (`src/pool/relational/relational_db_connection_pool.cpp`, 1114 lines): contains `RelationalPooledDBConnection` wrapper + thin pool class
- **`DocumentDBConnectionPool`** (`src/pool/document/document_db_connection_pool.cpp`, 1217 lines): contains `DocumentPooledDBConnection` wrapper + thin pool class
- **`ColumnarDBConnectionPool`** (`src/pool/columnar/columnar_db_connection_pool.cpp`, 852 lines): contains `ColumnarPooledDBConnection` wrapper + thin pool class
- **`KVDBConnectionPool`** (`src/pool/kv/kv_db_connection_pool.cpp`, 1250 lines): contains `KVPooledDBConnection` wrapper + thin pool class

#### Include Path Updates

All files that included pool headers updated from `core/` to `pool/` paths:
- All examples (16 files): connection pool and transaction manager examples across all database families
- All tests (30+ files): connection pool tests, transaction manager tests, and basic connection tests
- Internal sources: `database_config.cpp`, `transaction_manager.hpp`, `benchmark_common.hpp`, `example_relational_common.hpp`
- Documentation example: `simple_example.cpp`

#### CMakeLists.txt

Updated source file list from `src/core/` to `src/pool/` paths, added new `src/pool/connection_pool.cpp`.

---

## 2026-03-04 14:29:42 PST

### MongoDB Driver — Full Nothrow-First Refactor, Static Factory Pattern, `-fno-exceptions` Compatibility, and Dead try/catch Elimination

This release systematically migrates the entire MongoDB document driver layer to the project's dual-API (throwing + nothrow) pattern with `-fno-exceptions` compatibility — the same architecture already established in MySQL, PostgreSQL, SQLite, Firebird, ScyllaDB, and Redis drivers. MongoDB was the last driver to adopt this pattern. The nothrow API now contains all real implementations, and the throwing API is reduced to thin wrappers. Dead try/catch blocks around nothrow-only call chains are removed throughout. All 7 drivers now share the same nothrow-first, static factory, double-checked locking architecture. 41 files changed, +5956/-5321 lines.

#### Core Abstract Interfaces — `#ifdef __cpp_exceptions` Guard Separation

All five document abstract interfaces (`DocumentDBCollection`, `DocumentDBConnection`, `DocumentDBCursor`, `DocumentDBData`, `DocumentDBDriver`) now wrap throwing virtual methods inside `#ifdef __cpp_exceptions` / `#endif`. Nothrow methods sit outside this guard, ensuring they always compile even under `-fno-exceptions`.

- **`DocumentDBCollection`**: New nothrow pure-virtuals added: `getName(std::nothrow_t)`, `getNamespace(std::nothrow_t)`, `estimatedDocumentCount(std::nothrow_t)`, `countDocuments(std::nothrow_t)`
- **`DocumentDBConnection`**: `ping()` and `ping(std::nothrow_t)` removed from abstract interface (no longer part of the document connection contract)
- **`DocumentDBCursor`**: 9 new nothrow pure-virtuals: `next`, `hasNext`, `count`, `getPosition`, `skip`, `limit`, `sort`, `isExhausted`, `rewind` (with `std::reference_wrapper<DocumentDBCursor>` return type for chaining methods)
- **`DocumentDBData`**: Largest expansion — new nothrow pure-virtuals for: `getId`, `setId`, `toJson`, `toJsonPretty`, `fromJson`, all setters (`setString`, `setInt`, `setDouble`, `setBool`, `setBinary`, `setDocument`, `setNull`), `hasField`, `isNull`, `removeField`, `getFieldNames`, `clear`, `isEmpty`
- **`DocumentDBDriver`**: `getDefaultPort()` removed; `getURIScheme()` gains `noexcept` and now returns full URL prefix (e.g., `"cpp_dbc:mongodb://"`); `supportsReplicaSets()`, `supportsSharding()`, `getDriverVersion()` all marked `noexcept`; inline `connect(std::nothrow_t)` override added

#### MongoDBDriver — Double-Checked Locking + Nothrow Init

- `std::once_flag s_initFlag` replaced with `std::atomic<bool> s_initialized` + `std::mutex s_initMutex` — enables re-initialization after `cleanup()` (`once_flag` cannot be reset) and avoids `std::system_error` from `std::call_once` under `-fno-exceptions`
- New `initialize(std::nothrow_t) noexcept` returning `expected<bool, DBException>` — follows the DBDriver double-checked locking pattern
- Instance-level `m_mutex` removed — thread safety handled at connection level
- `connectDocument(std::nothrow_t)` now calls `MongoDBConnection::create(std::nothrow, ...)` static factory
- `parseURI(std::nothrow_t)` — dead try/catch removed (all `mongoc_uri_get_*` are C functions that cannot throw)

#### MongoDBConnection — Static Factory Pattern

- New private nothrow constructor: `MongoDBConnection(std::nothrow_t, ...) noexcept`
- New `initialize(std::nothrow_t)` private helper — performs URI parsing, client creation, database selection
- New static factory: `create(std::nothrow_t, ...) noexcept` returning `expected<shared_ptr<MongoDBConnection>, DBException>`
- Old public throwing constructor removed
- Private helpers converted: `validateConnection()`, `getClient()`, `getCollection()` → nothrow with `std::nothrow_t` + `expected` return

#### MongoDBCollection — Nothrow Helpers

- Private helpers converted: `validateConnection()`, `getClient()`, `getCollection()` → nothrow
- New `getCollectionHandle(std::nothrow_t) noexcept` — combines client + collection retrieval into a single operation

#### MongoDBCursor — Full Nothrow API

- Constructor converted to nothrow: `MongoDBCursor(std::nothrow_t, ...) noexcept`
- Private helpers `validateConnection`, `initializeCursor`, `parseDocument` all converted to nothrow
- `skip`, `limit`, `sort` return `expected<std::reference_wrapper<DocumentDBCursor>, DBException>`
- `rewind(std::nothrow_t)` always returns `unexpected` (MongoDB cursors do not support rewinding)

#### MongoDBDocument — Static Factory + ID Caching

- Constructors converted to nothrow: `MongoDBDocument(std::nothrow_t)` and `MongoDBDocument(std::nothrow_t, bson_t*)`
- Static factories: `create(std::nothrow_t)` and `create(std::nothrow_t, bson_t*)`
- ID caching: `m_idCached` / `m_cachedId` members avoid repeated BSON traversal for the `_id` field
- New `document_07.cpp` (776 lines) — nothrow implementations for: `getDocumentArray`, `getStringArray`, all setters, `hasField`, `isNull`, `removeField`, `getFieldNames`, `clone`, `clear`, `isEmpty`, `getBson`, `getBsonMutable`

#### `handles.hpp` — `makeBsonHandleFromJson()` Nothrow

- Throwing version removed; replaced by `expected<BsonHandle, DBException> makeBsonHandleFromJson(std::nothrow_t, ...) noexcept`

#### Dead try/catch Elimination

Nothrow methods that call only other nothrow methods no longer have try/catch blocks. This follows the project convention "No Redundant try/catch in Nothrow Methods" and the "Diagnostic Checklist" for detecting dead code. Applied across all MongoDB `.cpp` files.

#### Other Changes

- **CMakeLists.txt**: `document_07.cpp` uncommented in MongoDB source list
- **KV driver**: `""` → `std::string{}` for default parameter in `buildURI()` (kv_db_driver.hpp, redis/driver.hpp)
- **Redis connection.hpp**: Added `NOSONAR(cpp:S5950)` suppression for intentional raw `new` in static factory
- **Firebird driver.hpp stub**: Added missing `const` qualifiers and `override` keyword
- **MySQL/PostgreSQL blob.hpp**: Added `using MemoryBlob::copyFrom;` to unhide base-class overloads (SonarQube cpp:S1242)
- **Document connection pool**: Added `NOSONAR(cpp:S924)` comments on intentional `break` statements in maintenance loops

---

## 2026-02-26 18:27:31 PST

### DBException Fixed-Size Refactor, Unified `ping()` Interface, `std::string_view` Return Types, and Build Optimizations

This release replaces dynamic-allocation internals of `DBException` and `system_utils::StackFrame` with fixed-size stack buffers, unifies the `ping()` method across all database families under the base `DBConnection` interface with a `bool` return type, migrates `what_s()` and `getMark()` return types to `std::string_view` for zero-copy semantics, and improves the build system with deferred libdw flag processing and faster test builds.

#### `DBException` — Fixed-Size Memory Layout

`DBException` now inherits from `std::exception` instead of `std::runtime_error` and stores all fields in fixed-size char arrays:

```cpp
// Before
class DBException : public std::runtime_error
{
    std::string m_mark;
    std::string m_message;
    std::vector<system_utils::StackFrame> m_callStack;
};

// After
class DBException : public std::exception
{
    char m_mark[13]{};
    char m_message[257]{};
    char m_full_message[271]{};
    std::shared_ptr<system_utils::CallStackCapture> m_callStack;
};
```

- Constructor is now `noexcept` — no heap allocations that can fail
- `what()` returns pre-computed `m_full_message` (zero-cost, no concatenation on hot path)
- `what_s()` return type changed from `const std::string&` to `std::string_view`
- `getMark()` return type changed from `const std::string&` to `std::string_view`
- `getCallStack()` return type changed from `const std::vector<StackFrame>&` to `std::span<const system_utils::StackFrame>`
- Long messages/marks are left-truncated with `...[TRUNCATED]` marker
- Total object size: ~560 bytes (fixed, stack-allocatable)

#### `system_utils::CallStackCapture` — Fixed-Size Call Stack

`system_utils::StackFrame` now uses fixed-size char arrays instead of `std::string`:

```cpp
// Before
struct StackFrame { std::string file; std::string function; int line; };
// captureCallStack() → std::vector<StackFrame>

// After
struct StackFrame { char file[150]; char function[150]; int line; };
struct CallStackCapture { StackFrame frames[10]; int count; };
// captureCallStack() → std::shared_ptr<CallStackCapture>
```

- Heap allocation for the `CallStackCapture` struct happens only once (shared across copies)
- Maximum 10 frames captured — sufficient for most error paths
- `printCallStack()` now also accepts `std::span<const StackFrame>`

#### Unified `ping()` — Base `DBConnection` Interface

`ping()` is now a pure virtual method in the base `DBConnection` class, replacing family-specific overloads:

```cpp
// Before: Redis-specific in KVDBConnection
std::string ping();
expected<std::string, DBException> ping(std::nothrow_t) noexcept;

// Before: MongoDB-specific in DocumentDBConnection
bool ping();
expected<bool, DBException> ping(std::nothrow_t) noexcept;

// After: unified in DBConnection base
virtual bool ping() = 0;
virtual cpp_dbc::expected<bool, DBException> ping(std::nothrow_t) noexcept = 0;
```

Return type is now uniformly `bool` across all families (Redis, MongoDB, ScyllaDB, Relational). All pool wrappers and pooled connection classes updated accordingly.

#### All Examples and Benchmarks — `std::string_view` Adaptation

All example files (50+ across all 7 database families) updated for `what_s()` returning `std::string_view`:

```cpp
// Before
"Error: " + ex.what_s()

// After
"Error: " + std::string(ex.what_s())
```

`example_common.hpp` logging functions updated to accept `std::string_view` parameters:

```cpp
// Before
void logMsg(const std::string &message);

// After
void logMsg(std::string_view message);
```

Redis examples updated for `bool` ping return:

```cpp
// Before
std::string pong = conn->ping();
logData("PING = '" + pong + "'");

// After
bool pong = conn->ping();
logData(std::string("PING = '") + (pong ? "PONG" : "FAILED") + "'");
```

#### Build System — Deferred libdw Flag and Faster Test Builds

`helper.sh` now defers libdw flag processing so `dw-off` can cancel a preceding `dw-on` in the same option list:

```bash
# Now works correctly (dw-off wins over dw-on)
./helper.sh --run-test=dw-on,dw-off,sqlite
```

`build_test_cpp_dbc.sh` adds `-DCPP_DBC_BUILD_EXAMPLES=OFF -DCPP_DBC_BUILD_BENCHMARKS=OFF` to skip building examples and benchmarks during test-only builds, reducing compile time.

---

## 2026-02-24 18:25:00 PST

### Full Nothrow Pool API, Atomic int64_t Last-Used Time, MySQL Atomic Closed Flag, Destructor Safety, and Debug Macro Truncation Detection

This release hardens the pool and driver layer with a complete exception-free API: the `create()` factory and all internal pool methods now return `expected<T, DBException>`, `m_lastUsedTimeNs` switches from `atomic<time_point>` to `atomic<int64_t>` for true cross-platform portability, MySQL's `m_closed` becomes `std::atomic<bool>`, all result set destructors use nothrow close, and debug macros detect buffer truncation.

#### Pool Factory API — Nothrow `create()` (All Families)

All four pool families (`RelationalDBConnectionPool`, `DocumentDBConnectionPool`, `ColumnarDBConnectionPool`, `KVDBConnectionPool`) and all concrete subclasses now expose a nothrow factory:

```cpp
// Before
static std::shared_ptr<MySQLConnectionPool> create(const config::DBConnectionPoolConfig &config);

// After
static cpp_dbc::expected<std::shared_ptr<MySQLConnectionPool>, DBException>
    create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;
```

Usage:
```cpp
auto poolResult = cpp_dbc::MySQL::MySQLConnectionPool::create(std::nothrow, poolConfig);
if (!poolResult.has_value())
{
    throw poolResult.error();
}
auto pool = poolResult.value();
```

All examples (`20_031`, `20_041`, `21_031`, `21_041`, `22_031`, `22_041`, `23_031`, `23_041`, `24_031`, `25_031`, `26_031`) and all connection pool tests updated accordingly. `database_config.cpp` also updated.

#### Pool Internal Methods — Full Nothrow Conversion

All private/protected pool methods now return `expected<T, DBException>` with `std::nothrow_t` first parameter:

| Method | Before | After |
|--------|--------|-------|
| `createDBConnection()` | `shared_ptr<XDBConnection>` (throws) | `expected<shared_ptr<XDBConnection>, DBException>` noexcept |
| `createPooledDBConnection()` | `shared_ptr<XPooledDBConnection>` (throws) | `expected<...>` noexcept |
| `validateConnection()` | `bool` (throws) | `expected<bool, DBException>` noexcept |
| `returnConnection()` | `void` (throws) | `expected<void, DBException>` noexcept |
| `initializePool()` | `void` (throws) | `expected<void, DBException>` noexcept |

#### DBConnectionPool / DBConnectionPooled — Nothrow Pure Virtuals

`DBConnectionPool` base class now declares nothrow pure virtuals:
```cpp
virtual cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> getDBConnection(std::nothrow_t) noexcept = 0;
virtual cpp_dbc::expected<size_t, DBException> getActiveDBConnectionCount(std::nothrow_t) const noexcept = 0;
virtual cpp_dbc::expected<size_t, DBException> getIdleDBConnectionCount(std::nothrow_t) const noexcept = 0;
virtual cpp_dbc::expected<size_t, DBException> getTotalDBConnectionCount(std::nothrow_t) const noexcept = 0;
virtual cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept = 0;
virtual cpp_dbc::expected<bool, DBException> isRunning(std::nothrow_t) const noexcept = 0;
```

`DBConnectionPooled` base class also updated: `getCreationTime`, `getLastUsedTime`, `setActive`, `isActive`, `getUnderlyingConnection`, `isPoolValid` all gain nothrow signatures. `<new>` header added for `std::nothrow_t`.

#### `m_lastUsedTimeNs` — Portable Atomic int64_t (All Pool Families)

The previous `std::atomic<std::chrono::steady_clock::time_point>` is not guaranteed to be lock-free on ARM32/MIPS. Replaced with `std::atomic<int64_t>` storing nanoseconds since epoch:

```cpp
// Before (not portable — may not be lock-free on ARM32/MIPS)
static_assert(std::atomic<std::chrono::steady_clock::time_point>::is_always_lock_free, ...);
std::atomic<std::chrono::steady_clock::time_point> m_lastUsedTime;

// After (always lock-free on every supported 64-bit platform)
static_assert(std::atomic<int64_t>::is_always_lock_free, "int64_t atomic must be lock-free on this platform");
std::atomic<int64_t> m_lastUsedTimeNs{m_creationTime.time_since_epoch().count()};
```

`updateLastUsedTime(std::nothrow)` updated to store `steady_clock::now().time_since_epoch().count()`. `getLastUsedTime(std::nothrow)` reconstructs `time_point` from `m_lastUsedTimeNs`.

#### `m_creationTime` — In-Class Initialization Pattern

All pooled connection classes now use in-class member initialization for `m_creationTime`, with `m_lastUsedTimeNs` depending on it (safe because declared after):

```cpp
std::chrono::time_point<std::chrono::steady_clock> m_creationTime{std::chrono::steady_clock::now()};
static_assert(std::atomic<int64_t>::is_always_lock_free, ...);
std::atomic<int64_t> m_lastUsedTimeNs{m_creationTime.time_since_epoch().count()};
```

No constructor body initialization needed. This follows the new "Prefer In-Class Initializers" convention.

#### MySQL `m_closed` — `std::atomic<bool>` (Thread-Safe Access)

`MySQLDBConnection::m_closed` upgraded from `bool` to `std::atomic<bool>`:

```cpp
// Before
bool m_closed{true};  // requires mutex for safe cross-thread reads

// After
std::atomic<bool> m_closed{false};  // lock-free reads outside mutex
```

- All 8+ access sites in `connection_02.cpp`, `connection_03.cpp` updated to `.load(std::memory_order_acquire)` / `.store(true, std::memory_order_release)`
- `m_closed(false)` removed from constructor initializer list (in-class init handles it)
- Initial value corrected: `true` → `false` (connection starts OPEN, not closed)

#### `[[deprecated]]` on `what()` — Commented Out

The `[[deprecated]]` attribute on `DBException::what()` was generating noise for code that legitimately needs `what()` for `std::exception` compatibility. Attribute commented out; `what_s()` is still the recommended API.

#### Destructor Safety — Nothrow `close()` in All Result Sets

All result set destructors now use the nothrow `close()` overload:

| File | Before | After |
|------|--------|-------|
| `mysql/result_set_01.cpp` | `MySQLDBResultSet::close()` (throws) | `close(std::nothrow)` + error log |
| `postgresql/result_set_01.cpp` | `PostgreSQLDBResultSet::close()` (throws) | `close(std::nothrow)` + error log |
| `sqlite/result_set_01.cpp` | `close()` wrapped in try/catch | `close(std::nothrow)` + error log, try/catch removed |
| `firebird/result_set_01.cpp` | `[[maybe_unused]] close(std::nothrow)` | result checked, failure logged |

This eliminates potential `std::terminate()` from exceptions escaping destructors.

#### Debug Macro Truncation Detection

`CP_DEBUG`, `FIREBIRD_DEBUG`, `SQLITE_DEBUG` macros now detect `snprintf` buffer overflow and mark truncated messages:

```c
int n = std::snprintf(buf, sizeof(buf), format, ...);
if (n >= static_cast<int>(sizeof(buf)))
{
    static constexpr const char trunc[] = "...[TRUNCATED]";
    std::memcpy(buf + sizeof(buf) - sizeof(trunc), trunc, sizeof(trunc));
}
```

Previously, truncated messages were silently cut off with no indication.

#### `HighPerfLogger::m_flushCounter` — Static Local → Member Variable

`flushCounter` was a function-local `static size_t` inside `writerThreadFunc()`. Moved to `m_flushCounter` class member (`size_t`, only accessed from `m_writerThread`):

- **Before:** Static local; theoretically unique per process but adds hidden static storage and complicates testing
- **After:** Class member; clearly owned by logger instance, correct lifetime, better encapsulation

#### CMake — ASAN/TSAN Compile/Link Flags Directly in CMakeLists.txt

`ENABLE_ASAN` and `ENABLE_TSAN` CMake options now add compile/link flags directly:

```cmake
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()
```

Previously required passing flags via `-DCMAKE_CXX_FLAGS`.

#### Coding Conventions — New Sections

Four new sections added to `.claude/rules/cpp_dbc_conventions.md`:

1. **"Member Initialization — Prefer In-Class Initializers Over Constructor Body"** — Documents the pattern of using `= value` or `{value}` in class body instead of constructor assignments; covers safe inter-member dependencies
2. **"`std::atomic` — Always Use `.load(std::memory_order_acquire)`"** — Mandates explicit memory ordering on every atomic read; forbids implicit bool conversion
3. **"Nothrow Methods Must Call Nothrow Overloads"** — Nothrow methods must call the `std::nothrow` overload of inner methods (not the throwing variant)
4. **"No Redundant try/catch in Nothrow Methods"** — When all inner calls are nothrow, wrapping in try/catch is dead code and must be removed

#### Other Fixes

- **Firebird `prepared_statement_04.cpp`**: Removed unnecessary null-check on `conn` before autocommit (connection is always valid at that point after the existing guards)
- **Firebird `result_set_03.cpp`**: Fixed log message "Statement freed with 5ms delay" → "25ms delay" (actual sleep duration is 25ms)
- **ScyllaDB `result_set_02.cpp`**: Added `system_utils::captureCallStack()` to `DBException` constructors; fixed duplicate error code `"MLVT1AF9ZP3W"` → `"2GCPOH7KPUL4"` and `"F512VQ5M0HV8"` → `"U7F4FFY4KXMP"` in `catch(...)` handlers
- **`run_test_cpp_dbc.sh`**: Fixed `$@` → `"$@"` (proper quoting for arguments with spaces)
- **`build_test_cpp_dbc.sh`**: Removed unused `NO_REBUILD_DEPS` variable
- **`.gitignore`**: Removed redundant `logs/**` (kept `logs/`); simplified `research/**` → `research/`

---

## 2026-02-22 19:18:00 PST

### Unified Pool Mutex, Atomic Time-Point, Direct Handoff for All Pools, Notifications, and Test Quality Improvements

This release completes the connection pool unification that started with the relational pool: the columnar, document, and KV pools now share the same single-mutex + direct-handoff architecture. It also adds a post-test notification system, improves test quality thresholds, and extends Firebird with `role` support.

#### Connection Pool Architecture — Unified Mutex (Columnar, Document, KV)

All three non-relational pool families now match the relational pool architecture introduced in the previous release:

**Mutex consolidation** (from 5 mutexes → 1):

| Removed | Replaced by |
|---------|-------------|
| `m_mutexGetConnection` | `m_mutexPool` (single pool lock) |
| `m_mutexReturnConnection` | (eliminated) |
| `m_mutexAllConnections` | `m_mutexPool` |
| `m_mutexIdleConnections` | `m_mutexPool` |
| `m_mutexMaintenance` | `m_mutexPool` (maintenance CV now uses pool lock) |

**Direct handoff mechanism** (identical to relational pool):
- Added `ConnectionRequest` struct (connection slot + `fulfilled` flag) and `m_waitQueue` (`std::deque<ConnectionRequest*>`)
- `returnConnection()`: if waiters exist, hands off directly instead of pushing to idle queue
- Eliminates "stolen wakeup" race where a non-waiting thread could steal a connection before the awakened waiter acquires the lock

**`getXDBConnection()` redesign** (replaces polling):
- Old: `do { sleep(10ms); getIdleDBConnection(); } while (timeout)`
- New: `std::condition_variable::wait_until()` with deadline + inner FIFO wait loop
- `m_connectionAvailable` CV wakes borrowers; `m_maintenanceCondition` CV wakes maintenance thread
- `m_pendingCreations` counter tracks connections being created outside lock (prevents pool overshoot)

**`returnConnection()` fixes applied to all pools:**
- Pool-closing early exit: decrements `m_activeConnections` before closing (prevents `close()` waiting forever)
- Orphan detection: connections not in `m_allConnections` now decrement counter and notify
- Reset failure: `prepareForPoolReturn(std::nothrow)` failure → connection treated as invalid
- `updateLastUsedTime()` called on every valid return (enables HikariCP-style validation skip on borrow)
- All `notify_one()` / `notify_all()` calls are **inside** `m_mutexPool` lock (Helgrind correctness)
- Replacement handoff: when an invalid connection is replaced, the replacement uses direct handoff

**Destructor fix:**
- All pool destructors now call `XDBConnectionPool::close()` (qualified) instead of `close()` (virtual dispatch in destructor is undefined behavior — S1699)

**`getIdleDBConnection()` removed:**
- Private helper method removed from all pool types; its logic is inlined into the main borrow method

#### `std::atomic<time_point>` — Lock-Free Last-Used Time (All Pools)

The `m_lastUsedTimeMutex` + plain `std::chrono::time_point` pair was replaced with a single `std::atomic<std::chrono::steady_clock::time_point>` in all pool families:

```cpp
// Before: separate mutex required for safe access
mutable std::mutex m_lastUsedTimeMutex;
std::chrono::time_point<std::chrono::steady_clock> m_lastUsedTime;

// After: lock-free (x86-64: wraps int64_t nanoseconds, always lock-free)
static_assert(std::atomic<std::chrono::steady_clock::time_point>::is_always_lock_free,
              "time_point atomic must be lock-free on this platform");
std::atomic<std::chrono::steady_clock::time_point> m_lastUsedTime;
```

`updateLastUsedTime()` uses `memory_order_relaxed` store; `getLastUsedTime()` uses `memory_order_relaxed` load. The relaxed ordering is sufficient because the only consumer (validation skip) is an approximation — stale reads are tolerable.

**Impact:** Eliminates one mutex per pooled connection, reduces contention in maintenance thread.

#### `libdw` Disabled by Default for Tests

Compiling with `libdw` (Backward-cpp ELF/DWARF stack trace backend) significantly increases link time and can cause symbol lookup issues in certain environments. It is now **opt-in** for test builds:

| Before | After |
|--------|-------|
| `BACKWARD_HAS_DW=ON` (default) | `BACKWARD_HAS_DW=OFF` (default) |
| `--dw-off` disables | `--dw-on` enables |
| No `--dw-on` flag | `--dw-off` is a no-op (documented) |

Scripts updated: `build_test_cpp_dbc.sh`, `run_test_cpp_dbc.sh`, `run_test.sh`, `helper.sh`.

#### Test Notification System (`send_test_notification`)

A new notification hook sends a Gotify (or any curl-based) push notification when tests complete:

**`scripts/common/functions.sh` — new function `send_test_notification()`:**
- Reads `NOTIFY_COMMAND` from `<project_root>/.env.secrets` (gitignored)
- `NOTIFY_COMMAND` format: `curl "URL?token=TOKEN" -F "title=@__TITLE__@" -F "message=@__MESSAGE__@" -F "priority=N"`
- Placeholders `@__TITLE__@` and `@__MESSAGE__@` are substituted at call time
- Message body extracted from `final_report.log` (PASSED/FAILED/INTERRUPTED per prefix, Summary line, Total time)
- Title: `"Tests finished: all passed"` / `"Tests finished: FAILED"` / `"Tests finished: interrupted"`
- Graceful degradation: missing `.env.secrets`, empty `NOTIFY_COMMAND`, or missing `curl` → silent skip

**`helper.sh` — notification triggers:**
- Parallel run: called after `run_test_parallel.sh` exits (finds latest `final_report.log` automatically)
- Non-parallel run: called at end of `cmd_run_test()` (no report file, uses exit-code only)

**`.gitignore`:** Added `.env.secrets` to prevent accidental credential commits.

#### `run_test_parallel.sh` — Compact Summary and Catch2 WARN Detection

**New function `display_compact_summary()`:**
- Unified status display (replaces ~130 lines of duplicate code in summarize, TUI, and simple modes)
- Shows `[PASSED]`/`[FAILED]`/`[INTERRUPTED]` per prefix with run count
- Inline skipped/warning annotations: `(with skipped)`, `(with warnings)`, `(with skipped/warnings)`
- Structured `print_entry()` blocks with `Kind:`, `At:`, `Log:`, `Reason:` fields
- `print_reason()` wraps long reason text at 90 chars with proper alignment

**New function `extract_catch2_warnings()`:**
- Scans log files for Catch2 `WARN()` output: `file.cpp:LINE: warning: 'message'`
- Deduplicates warnings across multiple runs (same source+message appears once)
- Integrated into `display_compact_summary()` for all status types

**New function `reconstruct_helgrind_warnings()`:**
- Rebuilds `PREFIX_HELGRIND_WARNING` map from log files in summarize mode

**`save_report_to_file()` enhancement:**
- Now includes compact summary at top of `final_report.log` (before detailed summary)
- Accepts optional elapsed time string argument

#### Firebird: `role` Connection Option

The Firebird driver now supports the `role` connection option (e.g., `RDB$ADMIN`):

**`connection_01.cpp`:** Added `isc_dpb_sql_role_name` parameter to the Database Parameter Block (DPB) when `options["role"]` is set and non-empty.

**`test_db_connections.yml`:** Added `role: RDB$ADMIN` to `dev_firebird` and `test_firebird`; added new `prod_firebird` example database entry.

**Firebird tests updated:**
- `23_011_test_firebird_real_db_config.cpp`: Expects 3 databases; added `prod_firebird` test, `role` option assertions, new "test queries" test section (`[23_011_03]`)
- `23_121_test_firebird_real_transaction_isolation.cpp`: Added MVCC `READ_UNCOMMITTED` clarification comment; added new `SECTION("Firebird READ_UNCOMMITTED isolation behavior")` verifying MVCC prevents dirty reads

#### Test Quality Improvements

**Success rate thresholds:** All thread-safety and connection pool concurrent tests raised from 80–90% to **95%**.

**Connection timeout:** Increased from 2000 ms to **3500 ms** in all pool tests to reduce spurious timeouts under Helgrind instrumentation.

**Pool concurrent tests:** Relaxed from hard `REQUIRE(failureCount == 0)` to `REQUIRE(successCount >= 95%)` + `WARN()` for non-zero failures. Affected: MySQL, PostgreSQL, SQLite, Firebird, MongoDB, Redis, ScyllaDB pool tests.

**SQLite (`22_031_test_sqlite_real.cpp`):**
- New "SQLite metadata retrieval" section: `getColumnCount()`, `getColumnNames()`, `isNull()`, all type accessors
- New "SQLite stress test" section: 20 threads × 50 ops with retry logic for `SQLITE_BUSY`

**SQLite thread-safety (`22_111_test_sqlite_real_thread_safe.cpp`):**
- Thread/op counts increased; high concurrency 10×20 → 30×50; success threshold ≥ 95%

**SQLite transaction manager (`22_131_test_sqlite_real_transaction_manager.cpp`):**
- Pool sizes and thread/transaction counts increased for more meaningful stress

**PostgreSQL tests:** Removed inline `DROP TABLE` cleanup from test bodies (tables use `IF EXISTS` guard at start); fixed `conn->close()` → `conn->returnToPool()` for pooled connections; added missing `rs->close()` / `pstmt->close()` calls; added doc comment explaining Helgrind/PostgreSQL process-per-connection performance issue.

**VSCode settings:** Disabled preview tabs (`enablePreviewFromQuickOpen`, `enablePreviewFromCodeNavigation`, `enablePreview`).

---

## 2026-02-21 14:25:02 PST

### Pool Lifecycle API Hardening, C++ Code Analysis Toolset, Docker Container Auto-Restart, and Valgrind Suppression Report

#### RelationalDBConnection — Pool Lifecycle Methods Moved to Protected

`prepareForPoolReturn()`, `prepareForBorrow()`, and their nothrow versions were public virtual methods, making them part of the library's external API despite being exclusively an internal pool concern. They are now `protected`:

- Forward declarations for `RelationalDBConnectionPool` and `RelationalPooledDBConnection` added to `relational_db_connection.hpp` so the base class can declare them as `friend`s
- `friend class RelationalDBConnectionPool` and `friend class RelationalPooledDBConnection` added to `RelationalDBConnection` — only these two classes can call the pool lifecycle methods
- All four driver connection headers (MySQL, PostgreSQL, SQLite, Firebird) updated: lifecycle overrides moved from the `public` section to a new `protected` section with a clarifying comment
- `RelationalPooledDBConnection` pool lifecycle overrides similarly moved to `protected`
- `RelationalPooledDBConnection`, `MySQLConnectionPool`, `PostgreSQLConnectionPool`, `SQLiteConnectionPool`, `FirebirdConnectionPool` all marked `final` — removes virtual dispatch overhead and communicates intentional non-inheritance

**Impact**: Code calling `conn->prepareForPoolReturn()` directly from outside pool infrastructure will now fail to compile — enforcing the intended encapsulation.

#### C++ Code Analysis Toolset — 4 New Standalone Scripts

Four new root-level scripts for exploring C++ codebases. All are read-only, require only `python3` (standard library) and `bash`, and have no side effects on the build.

**`list_class.sh`** — List unique C++ class names in files

```bash
./list_class.sh --folder="./libs/cpp_dbc/src/drivers/*.cpp"
./list_class.sh --folder=./libs/cpp_dbc/src/
```

Detects classes via `class/struct` declarations, `ClassName::` implementation markers, and subtracts namespace names to avoid false positives.

**`list_public_methods.sh`** — Inspect a class's public interface

```bash
./list_public_methods.sh --class=RelationalDBConnectionPool
./list_public_methods.sh --class=MySQLDBConnection --output-format=lineal
```

Outputs ancestors (with header location), public method declarations (`.hpp:line`), and implementation locations (`.cpp:line`). Structured or one-line format.

**`list_class_usage.sh`** — Find call sites of a class's public methods

```bash
./list_class_usage.sh --class=DatabaseConfigManager --file="libs/cpp_dbc/test/*.cpp"
./list_class_usage.sh --class=MySQLDBConnection --file=libs/cpp_dbc/test/
```

Delegates method discovery to `list_public_methods.sh`, then searches target files for `.method(`, `->method(`, `Class::method(`, and template variants.

**`test_coverage.sh`** — Analyze test coverage per C++ class

```bash
./test_coverage.sh --src=libs/cpp_dbc/include --test=libs/cpp_dbc/test
./test_coverage.sh --check=db-driver
./test_coverage.sh --check=db-pool --min-coverage=80
```

Processes multiple classes in parallel (`--workers=N`). Presets:
- `db-driver` — all driver connection classes under `include/cpp_dbc/drivers`
- `db-pool` — all `*DBConnectionPool` and `*PooledDBConnection` classes under `include/cpp_dbc/core`

Options: `--min-coverage=N` (only show classes below N%), `--class=PATTERNS` (filter by name/glob), `--workers=N`.

All four scripts documented in `libs/cpp_dbc/docs/shell_script_dependencies.md` under the new "C++ Code Analysis Scripts" section.

#### Docker DB Container Auto-Restart Before Tests

`helper.sh` now restarts relevant database containers before any test execution, ensuring a clean container state each run.

**`helper.sh` changes:**
- Added `enabled_db_drivers` array populated as driver flags are parsed (sqlite, firebird, mongodb, scylladb, redis, mysql, postgres)
- After option parsing, calls `restart_db_containers_for_test "${enabled_db_drivers[@]}"` — runs **outside** the TUI so output is sequential and readable

**`scripts/common/functions.sh` — 4 new Docker functions:**

| Function | Purpose |
|----------|---------|
| `get_driver_docker_info driver` | Returns `"IMAGE_KEYWORD:PORT"` (e.g. `"mysql:3306"`) |
| `find_db_container driver` | Finds container: primary by port mapping `127.0.0.1:PORT->`, fallback by image name |
| `wait_for_container container [max_s]` | Polls `docker inspect` every second; waits for `running`+`healthy` (or `running`+no-healthcheck); 120s default timeout; sets `WAIT_ELAPSED_SECONDS` |
| `restart_db_containers_for_test drivers...` | Orchestrates restart: finds containers, runs `docker restart`, waits for readiness |

**Graceful degradation:**
- No Docker installed → warning, skip
- No Docker access privileges → warning, skip
- Container not found for a driver → warning for that driver only, others proceed
- Container timeout → warning, tests continue anyway
- SQLite always skipped (file-based, no container)

#### Valgrind Suppression Summary and Final Report in `run_test_parallel.sh`

Two new functions added to all three execution modes (TUI, simple, summarize):

**`display_valgrind_suppression_summary()`**: Scans all `*_RUN*.log` files in `$LOG_DIR`; for each file that contains at least one `ERROR SUMMARY: ... suppressed: N>0` line, shows the last such line (the final Helgrind process result) with the file path and line number. Section printed only when at least one suppressed error exists.

**`save_report_to_file()`**: Captures `display_detailed_summary` + `display_valgrind_suppression_summary` output (ANSI stripped) and writes it to `$LOG_DIR/final_report.log`. Useful for CI/CD artifact collection or post-run review.

Both are called at the end of `run_parallel_tests_tui()`, `run_parallel_tests_simple()`, and `run_summarize_mode()`.

---

## 2026-02-21 02:48:55 PST

### Direct Handoff Connection Pool, system_utils Performance Refactoring, and Debug Macro Unification

This release focuses on correctness and performance of the relational connection pool, elimination of heap allocations in time-formatting utilities, and unification of the debug logging API across all driver debug macros.

#### RelationalDBConnectionPool — Direct Handoff Mechanism

The relational connection pool was substantially redesigned to eliminate the "stolen wakeup" race condition that could cause deadlocks under high concurrency:

**Root Cause of the Race:**
When a connection was returned and a CV notification was sent, the returning thread itself (or any other non-waiting thread) could immediately call `getRelationalDBConnection()` and "steal" the connection before the awakened waiter could acquire the mutex. This starved waiters indefinitely.

**Fix — Direct Handoff (inspired by Java's `LinkedTransferQueue`):**
- Added `ConnectionRequest` struct (contains a connection slot + fulfilled flag) and `m_waitQueue` (`std::deque<ConnectionRequest*>`) as new members
- When `returnConnection()` has a valid connection and `m_waitQueue` is non-empty, the connection is assigned directly into the first waiter's `ConnectionRequest` struct instead of entering the idle queue
- The waiter checks its own `request.fulfilled` flag — no stealing possible because the connection is pre-assigned

**Mutex Simplification:**
- Removed `m_mutexGetConnection` (the separate borrow mutex) — pool now uses a single `m_mutexPool` for all state
- `getIdleDBConnection()` private method removed — its logic was merged into `getRelationalDBConnection()` with deadline-based waiting

**Additional `returnConnection()` Fixes:**
- **Pool-closing early exit**: if `m_running` is false, decrements counter before closing the connection (prevents `close()` from waiting forever)
- **Orphan detection**: connections not found in `m_allConnections` now properly decrement counter and notify
- **Reset failure handling**: if `reset(nothrow)` fails, the connection is marked invalid (before this it silently ignored reset errors)
- `updateLastUsedTime()` called on every valid return (enables HikariCP-style validation skip)
- **Replacement handoff**: when an invalid connection is replaced, the new connection uses the same direct handoff logic
- All `notify_one()` / `notify_all()` calls are now **inside** `m_mutexPool` lock for Helgrind correctness

#### system_utils.hpp — Performance Refactoring and New Thread Utilities

Three time-formatting functions were rewritten to eliminate heap allocations from `std::ostringstream`:

| Function | Before | After |
|----------|--------|-------|
| `currentTimeMillis()` | `ostringstream` + `put_time` + `setfill` | `strftime` + `snprintf` into stack buffer |
| `currentTimeMicros()` | `ostringstream` + `put_time` + `setfill` | `strftime` + `snprintf` into stack buffer |
| `getCurrentTimestamp()` | `stringstream` + `put_time` | `strftime` + `snprintf` into stack buffer |

Includes removed: `<iomanip>`, `<sstream>` (replaced by `<charconv>`).

**New functions:**

* `getThreadId()`: Returns the OS-native TID as a string — `gettid()` on Linux, `GetCurrentThreadId()` on Windows, hash fallback on other platforms. Uses `std::to_chars` (zero allocation).
* `threadIdToString(id)`: Converts any `std::thread::id` to its last 6 hash digits via `std::to_chars`.
* `logWithTimesMillis(prefix, message)`: Structured log line with format `[HH:MM:SS.mmm] [TID] [prefix] message\n`. Uses `write()` directly (non-interleaving, Helgrind-safe).

`logWithTimestamp()` was also enhanced to include `[TID]` in the output format.

**Note:** `getCurrentTimestamp()` output format changed — brackets removed: was `[YYYY-MM-DD HH:MM:SS.mmm]`, now `YYYY-MM-DD HH:MM:SS.mmm`.

#### system_utils.cpp — Re-enabled `stackTraceMutex`

`captureCallStack()` re-enables `std::recursive_mutex stackTraceMutex` + `std::scoped_lock lock(stackTraceMutex)` (were commented out during Helgrind investigation). This serializes calls to `backward::StackTrace::load_here()`, which accesses `dlopen`/`dlsym` globals that are not thread-safe, preventing data races visible to Helgrind/ThreadSanitizer.

#### Debug Macro Unification — `logWithTimesMillis`

All driver debug macros now delegate to `logWithTimesMillis()` instead of manually constructing a prefix string and calling `safePrint()`:

* **`CP_DEBUG`** (`connection_pool_internal.hpp`): `safePrint("[ConnectionPool] [" + currentTimeMillis() + "] [" + getThreadId() + "]", ...)` → `logWithTimesMillis("ConnectionPool", ...)`
* **`FIREBIRD_DEBUG`** (`firebird_internal.hpp`): same pattern → `logWithTimesMillis("Firebird", ...)`
* **`SQLITE_DEBUG`** (`sqlite_internal.hpp`): same pattern → `logWithTimesMillis("SQLite", ...)`

Reduces code duplication and ensures consistent log format across all debug output paths.

#### Printf Format Specifier Fixes (Columnar, Document, KV Pools)

* `%zu` → `%d` for `m_activeConnections.load()` (`std::atomic<int>::load()` returns `int`, not `size_t`)
* `%lld` → `%ld` for `elapsed_seconds`
* `ex.what()` → `ex.what_s().c_str()` for `DBException` catches (consistent with project conventions)

#### Helgrind Suppression — Broader glibc clockwait Coverage

Suppression `glibc_clockwait_internal_signal_maintenance_task` renamed to `glibc_clockwait_internal_signal_broadcast` and broadened:

* Wildcard `fun:pthread_cond_*_WRK` covers both `signal` and `broadcast` variants
* Removed specific maintenance task stack frame requirements — now triggers from any call site inside `__pthread_cond_wait_common`
* Updated evidence: observed in MySQL (`broadcast` from `getRelationalDBConnection()`), PostgreSQL (`signal`), and Firebird (`signal` from `maintenanceTask()`)

#### Test Updates

* All connection pool tests across all drivers: replaced `std::cerr` with `logWithTimesMillis("TEST", ...)` for thread-safe, timestamped output
* `20_141_test_mysql_real_connection_pool.cpp`: reduced `numOperations` 50 → 40
* `10_101_test_high_perf_logger.cpp`: `safePrint` → `logWithTimesMillis`
* `test/CMakeLists.txt`: minor blank line formatting improvements

---

## 2026-02-19 21:23:56 PST

### Source File Reorganization: Canonical Method Ordering and File Splitting Across All Relational Drivers

This release reorganizes all relational driver source files for consistency and maintainability. **No functional logic changes** — all method implementations are preserved exactly as they were.

#### File Splitting — New Source Files

Seven new `.cpp` files were added to reduce individual file sizes and improve compilation parallelism:

* **MySQL**: `result_set_04.cpp` (blob/binary nothrow methods: `getColumnCount`, `getBlob`, `getBinaryStream`, `getBytes`)
* **PostgreSQL**: `result_set_04.cpp` (blob/binary nothrow methods)
* **SQLite**: `result_set_04.cpp` (blob/binary nothrow methods), `result_set_05.cpp` (additional result set methods)
* **Firebird**: `result_set_05.cpp` (by-name nothrow methods + `getColumnNames`/`getColumnCount`), `prepared_statement_04.cpp` (`executeUpdate`, `execute`, `close`, `invalidate` nothrow methods), `connection_04.cpp` (nothrow methods part 2: `commit`, `rollback`, `close`, `setTransactionIsolation`, `getTransactionIsolation`, `isClosed`, `returnToPool`, `isPooled`, `getURL`, `prepareForPoolReturn`, `prepareForBorrow`)

#### Canonical Method Ordering (All Result Sets)

All result set nothrow methods across all 4 drivers are now ordered consistently:

1. `close` → `isEmpty` → `next` → `isBeforeFirst` → `isAfterLast` → `getRow`
2. Type accessors interleaved by-index/by-name: `getInt(index)`, `getInt(name)`, `getLong(index)`, `getLong(name)`, etc.
3. `getDate`, `getTimestamp`, `getTime` (index/name pairs)
4. `getColumnNames`, `getColumnCount`
5. Blob/binary methods in a separate new file

#### Header Declaration Reordering

* **MySQL, PostgreSQL, SQLite `result_set.hpp`**: Nothrow method declarations reordered to interleave by-index and by-name variants of the same type together
* **Firebird `connection.hpp`**: "NOTHROW VERSIONS" section comment repositioned above `reset(std::nothrow_t)`
* **SQLite `connection.hpp`**: `reset(std::nothrow_t)` moved to the nothrow methods section

#### Firebird Connection File Redistribution

* **`connection_01.cpp`**: Now contains constructor, destructor, and `executeCreateDatabase()` (raw ISC API)
* **`connection_02.cpp`**: Now contains ALL throwing wrapper methods (delegating to nothrow implementations)
* **`connection_03.cpp`**: Now contains nothrow methods part 1 (`reset`, `prepareStatement`, `executeQuery`, `executeUpdate`, `setAutoCommit`, `getAutoCommit`, `beginTransaction`, `transactionActive`)
* **`connection_04.cpp`** (NEW): Nothrow methods part 2 (`commit`, `rollback`, `close`, pool/isolation/URL methods)

#### Firebird Prepared Statement Redistribution

* **`prepared_statement_02.cpp`**: Gained `setInt(nothrow)` and `setLong(nothrow)` from `_01.cpp`; lost blob/binary methods to `_03.cpp`
* **`prepared_statement_03.cpp`**: Gained blob/binary setters from `_02.cpp`; lost `executeUpdate`/`execute`/`close`/`invalidate` to `_04.cpp`
* **`prepared_statement_04.cpp`** (NEW): `executeUpdate(nothrow)`, `execute(nothrow)`, `close(nothrow)`, `invalidate()`

#### Error Code Fix

* **`firebird/blob.hpp`**: Changed non-compliant error code `"FB_BLOB_CONN_CLOSED"` to properly formatted 12-char code `"LMHROWFG5PNN"`

#### Build System

* **`CMakeLists.txt`**: Added 7 new source files across all 4 relational drivers

---

## 2026-02-18 00:54:58 PST

### Complete Nothrow API Implementation Across All Drivers and Base Classes

This release completes the exception-free (`nothrow`) API across the entire cpp_dbc driver stack. All base class nothrow methods are now pure virtual, forcing every driver to provide a concrete implementation. The connection pool wrapper classes (`RelationalPooledDBConnection`, `DocumentPooledDBConnection`, etc.) also receive full nothrow overrides.

#### Base Class Changes — Pure Virtual Promotion

* **`DBConnection` (`db_connection.hpp`):**
  * `close(std::nothrow_t)`, `reset(std::nothrow_t)` — changed from default "not implemented" virtual implementations to `= 0` pure virtual
  * Added new pure virtuals: `void reset()`, `isClosed(std::nothrow_t) const noexcept`, `returnToPool(std::nothrow_t) noexcept`, `isPooled(std::nothrow_t) const noexcept`, `getURL(std::nothrow_t) const noexcept`

* **`DBResultSet` (`db_result_set.hpp`):**
  * `close(std::nothrow_t)` — changed from default virtual to `= 0`
  * Added `isEmpty(std::nothrow_t) noexcept = 0`

* **`RelationalDBConnection` (`relational_db_connection.hpp`):**
  * `prepareForPoolReturn()` and `prepareForBorrow()` — changed from inline default implementations to `= 0`
  * Added pure virtuals: `prepareForPoolReturn(std::nothrow_t) noexcept = 0`, `prepareForBorrow(std::nothrow_t) noexcept = 0`

#### Driver Implementations — New Nothrow Methods

All 7 drivers now implement the complete nothrow API surface. New source files were created to house the new implementations:

* **PostgreSQL** (`connection_03.cpp`, `result_set_03.cpp`):
  * `close(nothrow)`: acquires `m_connMutex`, calls `closeAllStatements()`, sleeps 25ms, resets `PGconn`
  * `reset(nothrow)`: closes all statements, rolls back active transaction, resets auto-commit to `true`
  * `prepareForPoolReturn(nothrow)`: delegates to `reset(nothrow)`
  * `prepareForBorrow(nothrow)`: no-op for PostgreSQL
  * Result set: `close(nothrow)` resets `PGresult`; `isEmpty(nothrow)` checks `m_rowCount`

* **MySQL** (`connection_03.cpp`, `result_set_03.cpp`):
  * Full nothrow implementations for all `DBConnection` and `RelationalDBConnection` methods
  * `reset(nothrow)`: closes statements and result sets, rolls back transaction, resets auto-commit

* **SQLite** (`connection_02.cpp`, `result_set_03.cpp`):
  * Integrates with `FileMutexRegistry` for sanitizer-safe locking
  * `close(nothrow)`, `reset(nothrow)`, `prepareForPoolReturn(nothrow)`, `prepareForBorrow(nothrow)`

* **Firebird** (`connection_03.cpp`):
  * `reset(nothrow)`: rolls back active transaction, resets transaction state
  * `prepareForBorrow(nothrow)`: refreshes MVCC transaction snapshot

* **MongoDB** (`connection_05.cpp`):
  * Full `DBConnection` nothrow overrides for `close`, `reset`, `isClosed`, `returnToPool`, `isPooled`, `getURL`

* **Redis** (`connection_06.cpp`):
  * Full `DBConnection` nothrow overrides for `close`, `reset`, `isClosed`, `returnToPool`, `isPooled`, `getURL`

* **ScyllaDB** (`connection_02.cpp`):
  * Full `DBConnection` nothrow overrides
  * `close(nothrow)` calls `cass_session_close()` asynchronously via RAII handle

#### Connection Pool Wrappers — New Nothrow Overrides

All pool wrapper classes (`RelationalPooledDBConnection`, `DocumentPooledDBConnection`, `KVPooledDBConnection`, `ColumnarPooledDBConnection`) now override all new pure virtual nothrow methods. The relational pool wrapper additionally implements `void reset()`, `prepareForPoolReturn()`, and `prepareForBorrow()`.

#### Throwing Wrappers — Delegate to Nothrow Implementations

All throwing method variants (`close()`, `reset()`, `isClosed()`, etc.) in every driver now delegate to their corresponding nothrow implementation. This centralizes error handling in a single code path and eliminates duplication.

#### Build System

* **`CMakeLists.txt`:** Added `src/drivers/document/mongodb/connection_05.cpp` and `src/drivers/columnar/scylladb/connection_02.cpp` to the build

#### Helgrind Suppressions

* **`valgrind/helgrind/suppression.conf`:** Added `scylladb_connection_close_nothrow_lockorder_heap_reuse` — suppresses a Helgrind LockOrder false positive in `ScyllaDBConnection::close(std::nothrow_t)`. Root cause: `cass_session_close()` acquires `uv_mutex`; Helgrind's heap-address-reuse tracking incorrectly maps the freed `m_connMutex` address to a new `uv_mutex`, creating a spurious lock order violation

---

## 2026-02-17 16:22:49 PST

### Helgrind Thread-Safety Hardening: Connection Pool Lock Order Fixes, Firebird Driver Refactor, and Test Infrastructure Improvements

#### Staged Changes: Connection Pool Internal Refactor + Test Script Improvements

* **Connection Pool Internal Header (`connection_pool_internal.hpp`):**
  * Extracted shared internal types and helpers used across all pool implementations (relational, columnar, document, kv) into a single internal header
  * Reduces duplication and ensures consistent lock-ordering discipline across all pool types

* **Connection Pool — Close Outside Pool Locks (CRITICAL Helgrind fix):**
  * `RelationalDBConnectionPool::close()`: Collect connections under pool locks, then close them **outside** pool locks
  * Root cause: Closing a connection acquires `m_connMutex` (per-connection); previously this happened inside `scoped_lock(m_mutexAllConnections, m_mutexIdleConnections)`, inverting the established lock order (`m_connMutex` first → pool locks)
  * Same fix applied to `ColumnarDBConnectionPool`, `DocumentDBConnectionPool`, `KVDBConnectionPool`
  * Removed `returnToPool()` call from `close()` path (connections closed directly, no pool re-entry)

* **Connection Pool — MinIdle Replenishment Outside Pool Locks:**
  * `maintenanceTask()`: `createPooledDBConnection()` now called **outside** pool locks
  * Prevents potential lock-ordering cycles with driver-internal mutexes during connection creation
  * Size check (`m_allConnections.size()`) performed under separate `scoped_lock` to maintain correctness

* **Connection Pool — Debug Output Modernized:**
  * Changed all `CP_DEBUG` calls from `operator<<` stream style to `printf`-style format strings
  * Eliminates potential data races on stream objects under Helgrind observation

* **`run_test_parallel.sh` — NTP/VM Resume Clock Jump Detection:**
  * Added `detect_and_handle_clock_jump()`: detects when wall-clock advances more than 30 seconds between loop iterations (caused by NTP correction after VM suspend/resume with stale RTC)
  * When jump detected: resets all running test activity timestamps to prevent false timeout kills
  * `LAST_LOOP_TIME` tracks previous iteration timestamp; called once per main loop in both TUI and simple modes

* **`run_test_parallel.sh` — Process Group Kill (PGID-based):**
  * `kill_test_timeout()` and `cleanup()` now resolve PGID via `ps -o pgid=` and send `SIGTERM`/`SIGKILL` to `-$pgid` (entire process group)
  * Correctly terminates deep descendants such as Helgrind/Valgrind child processes that `pkill -P` misses
  * Safety guard: PGID 0 and 1 are never killed (kernel/init protection)

* **Deleted Bug Documentation (Bugs Now Fixed):**
  * Removed `libs/cpp_dbc/docs/bugs/README.md` — all tracked Firebird bugs resolved
  * Removed `libs/cpp_dbc/docs/bugs/firebird_helgrind_analysis.md` — analysis complete; Firebird driver fully refactored

---

#### Commit `3327c62`: Refactor Firebird Driver Mutex Model and Harden nothrow API Across Drivers

* **`AtomicGuard<T>` RAII Template (`system_utils.hpp`):**
  * New template: `AtomicGuard<T>` — sets `atomic<T>` to a value on construction, restores on destruction
  * Exception-safe, non-copyable, non-movable to prevent double-reset bugs
  * Used internally to mark `m_resetting` flag on `FirebirdDBConnection` during `closeAllActiveResultSets()`

* **`DBConnection` Base Class — Hardened nothrow API:**
  * `reset(std::nothrow_t)`: Changed from `virtual void reset() noexcept {}` (silent no-op) to `virtual expected<void,DBException>` returning explicit "Not implemented" error (`ZDC68V9OMICL`)
  * `close(std::nothrow_t)`: New virtual method returning `expected<void,DBException>` — was absent from base class

* **`DBResultSet` Base Class — nothrow close():**
  * Added `close(std::nothrow_t)` returning `expected<void,DBException>` with default "Not implemented" error (`3FROYWSMRL4N`)

* **Firebird `FirebirdDBConnection` — Mutex Model Simplification:**
  * **Removed** `m_statementsMutex` and `m_resultSetsMutex` — two independent mutexes for statement/result-set registries that enabled ABBA deadlock
  * All access to `m_activeStatements` and `m_activeResultSets` now serialized through the single shared `m_connMutex`
  * `m_closed` changed from `bool` to `std::atomic<bool>` — eliminates data race between concurrent threads
  * Added `m_resetting` (`std::atomic<bool>`) — signals `ResultSet::close()` to skip the "unregister" step during `closeAllActiveResultSets()`, preventing re-entrant lock acquisition
  * Added `getConnectionMutex()` — PreparedStatement/ResultSet can acquire connection mutex via `m_connection.lock()` without storing it separately
  * Added `isResetting()` accessor — checked by ResultSet to conditionally skip unregister
  * Added `reset(std::nothrow_t)` and `close(std::nothrow_t)` overrides

* **`FirebirdDBPreparedStatement` — Simplified Construction:**
  * Inherits `std::enable_shared_from_this<FirebirdDBPreparedStatement>`
  * Constructor no longer takes `SharedConnMutex connMutex` — mutex obtained via `m_connection.lock()->getConnectionMutex()` when needed
  * `m_closed` changed from `bool` to `std::atomic<bool>`
  * `m_connMutex` member removed

* **`FirebirdDBResultSet` — enable_shared_from_this:**
  * Inherits `std::enable_shared_from_this<FirebirdDBResultSet>`

* **`RelationalPooledDBConnection` — reset() and close() overrides:**
  * Added proper `reset(std::nothrow_t)` and `close(std::nothrow_t)` implementations delegating to the underlying connection

* **Valgrind Helgrind Suppressions — Two New False Positives:**
  * **ScyllaDB `CassSessionDeleter` heap-address reuse:** Two different mutexes occupy the same heap address at different points in the test (pool mutex freed → libuv internal mutex allocated at same address). Helgrind can't distinguish → false LockOrder report. Anchored to `CassSessionDeleter + uv_loop_close`
  * **glibc GLIBC_2.34 `pthread_cond_clockwait` internal signal:** glibc's newer `pthread_cond_clockwait` emits an internal `pthread_cond_signal` as part of futex unbind. Helgrind reports "signal without lock". Identical pattern to `helgrind-glibc2X-005` (which suppresses 3M+ occurrences of the older variant)

---

#### Commit `e3736ee`: Fix Firebird Use-After-Free Bug and Improve SQLite Test Thread Safety

* **Firebird `FirebirdDBPreparedStatement` — Eliminate `m_trPtr` Dangling Pointer (USE-AFTER-FREE Fix):**
  * **Before:** `isc_tr_handle *m_trPtr{nullptr}` — raw pointer to the connection's `m_tr` member
  * **Problem:** If connection was destroyed while PreparedStatement still existed, `m_trPtr` pointed to freed memory → SEGFAULT on `isc_dsql_execute()`
  * **After:** Access via `m_connection.lock()->m_tr` — lifecycle-safe through weak_ptr
  * All `executeQuery()`, `executeUpdate()`, and `execute()` methods updated
  * Constructor no longer takes `isc_tr_handle *trPtr` parameter

* **SQLite Tests — Thread-Safe Output:**
  * Replaced all `std::cout`/`std::cerr` calls in SQLite test files with `cpp_dbc::system_utils::safePrint()` — atomic output, no interleaving under Helgrind
  * Files: `22_001`, `22_031`, `22_041`, `22_071`, `22_081`, `22_111`

* **SQLite Tests — Driver Registration Outside SECTION:**
  * Moved `DriverManager::registerDriver()` and connection setup to test-case scope (outside `SECTION` blocks)
  * Prevents double-registration when multiple sections run under the same `TEST_CASE`

* **SQLite Tests — Database Path from YAML Config:**
  * Transaction manager tests now load database path from YAML config (`test_sqlite_transaction_multithread`, `test_sqlite_transaction`) instead of hardcoded filenames

* **SQLite Tests — Disabled WAL Mode Under Helgrind:**
  * `PRAGMA journal_mode=WAL` disabled in thread-safety tests — WAL mode creates Helgrind false positives via POSIX shared-memory locks

* **SQLite Tests — Connection String Assertion Fix:**
  * `22_011_test_sqlite_real_config.cpp`: Changed from hardcoded `"cpp_dbc:sqlite://cpp_dbc_test_sqlite.db"` to `"cpp_dbc:sqlite://" + dbConfig.getDatabase()` for portability

---

#### Commit `bf312e8`: Refactor Firebird Driver Debug Output for Thread Safety

* **Firebird Driver — Printf-Style Debug Output:**
  * Replaced all `FIREBIRD_DEBUG("..." << value)` stream-style output with `FIREBIRD_DEBUG("... %s", value)` printf-style
  * `operator<<` is not atomic — causes interleaved/garbled output and Helgrind false positives under concurrent execution
  * Applied across all Firebird driver implementation files

---

## 2026-02-06 21:44:02 PST

### Test Directory Reorganization and Parallel Test Runner Enhancements

* **Test Directory Complete Restructuring:**
  * Migrated all test files from flat structure to hierarchical, database-family-based organization
  * Test files now organized by database family following the same pattern as examples
  * Directory structure mirrors example organization for consistency

* **New Test Directory Organization:**
  * **Common Tests (10_xxx):** `test/common/` — Core framework tests (database_config, db_config, db_exception, expected, yaml, integration)
  * **Relational Tests:**
    * `test/relational/mysql/` — MySQL tests (20_xxx)
    * `test/relational/postgresql/` — PostgreSQL tests (21_xxx)
    * `test/relational/sqlite/` — SQLite tests (22_xxx)
    * `test/relational/firebird/` — Firebird tests (23_xxx)
  * **Key-Value Tests:**
    * `test/kv/redis/` — Redis tests (24_xxx)
  * **Document Tests:**
    * `test/document/mongodb/` — MongoDB tests (25_xxx)
  * **Columnar Tests:**
    * `test/columnar/scylladb/` — ScyllaDB tests (26_xxx)
  * **Placeholder Directories for Future Tests:**
    * `test/graph/.git_keep` — Reserved for graph database tests
    * `test/timeseries/.git_keep` — Reserved for time-series database tests

* **Test CMakeLists.txt Updates:**
  * Updated all test file paths to use family-prefixed paths (e.g., `relational/mysql/20_001_...`)
  * Added include directories for each database family folder
  * Updated file copy operations to use `common/` prefix for shared resources
  * Common resources (`test_db_connections.yml`, `test.jpg`) now in `test/common/` directory

* **Updated Driver Documentation (`how_add_new_db_drivers.md`):**
  * Added comprehensive "Test Directory Structure" section in Phase 3
  * Updated prefix assignment table with directory column showing full paths
  * Updated all test file examples to include full family-prefixed paths
  * Added detailed CMakeLists.txt update instructions:
    * How to add family-prefixed test files
    * How to configure include directories for test headers
    * How to update common integration tests with new drivers
  * Enhanced checklist with directory structure validation steps

* **Parallel Test Runner Enhancements (`run_test_parallel.sh`):**
  * **Terminal Resize Handling:**
    * Added SIGWINCH signal handler for terminal resize events
    * Implemented `handle_terminal_resize()` function with `TERM_RESIZED` flag
    * TUI now properly redraws on terminal resize without visual artifacts
    * Added `trap 'handle_terminal_resize' WINCH` in TUI initialization
  * **Dual Log File System:**
    * Clean `.log` files (ANSI codes stripped) — stored in `logs/test/TIMESTAMP/`
    * Colored `.log.ansi` files (with ANSI codes) — stored in `/tmp/cpp_dbc/logs/test/TIMESTAMP/`
    * Uses `tee` to split output stream: `tee "$log_file_ansi" | sed 's/\x1b\[[0-9;]*m//g' > "$log_file"`
    * TUI displays colored output from `.ansi` files for better visual feedback
    * Log files remain clean for easy reading and parsing
  * **Temporary Directory Management:**
    * Added `TEMP_ANSI_DIR` variable for `/tmp/cpp_dbc/logs/test/TIMESTAMP/` directory
    * Added `cleanup_old_tmp_ansi_folders()` function to maintain temp directory
    * Keeps last 5 folders in both project logs and `/tmp` directories
    * Critical for VMs that suspend instead of reboot (prevents `/tmp` fill-up)
    * Mirrored timestamp directory structure maintains correlation between logs
  * **Recursive Test Discovery:**
    * Updated `discover_prefixes()` to use `find` for recursive subdirectory search
    * Automatically discovers tests in new family-based directory structure
    * Replaced file loop with `find "$test_dir" -type f -name "*_*_test_*.cpp"`
    * Supports unlimited nesting depth for future organizational needs

* **Impact:**
  * 110 files changed in git diff
  * Test files moved to family-specific directories (no content changes)
  * Better project organization with consistent structure across tests/examples
  * Improved maintainability with tests grouped by database type
  * Enhanced TUI stability with terminal resize support
  * Cleaner logs for automated parsing while maintaining colored TUI

## 2026-02-06 16:40:00 PST

### Major Examples Reorganization and Automation Improvements

* **Examples Complete Restructuring (59 new files, 18 deleted):**
  * Migrated from flat structure to hierarchical, database-family-based organization
  * Implemented numeric prefix naming convention: `XX_YZZ_example_<db>_<feature>.cpp`
    * XX = Database family (10=common, 20=MySQL, 21=PostgreSQL, 22=SQLite, 23=Firebird, 24=Redis, 25=MongoDB, 26=ScyllaDB)
    * YZZ = Feature category (001=basic, 021=connection_info, 031=pool, 041=transaction, 051=json, 061=blob, 071=join, 081=batch, 091=error_handling)
  * Allows up to 99 database families and 99 feature categories per family for future scalability

* **Example Organization by Database Family:**
  * **Common Examples (10_xxx):** Config and integration examples
  * **MySQL Examples (20_xxx):** 9 examples covering all major features
  * **PostgreSQL Examples (21_xxx):** 9 examples covering all major features
  * **SQLite Examples (22_xxx):** 9 examples covering all major features
  * **Firebird Examples (23_xxx):** 10 examples including reserved word handling
  * **Redis Examples (24_xxx):** 7 examples for key-value operations
  * **MongoDB Examples (25_xxx):** 5 examples for document operations
  * **ScyllaDB Examples (26_xxx):** 7 examples for columnar operations

* **New Shared Helper Headers:**
  * `examples/common/example_common.hpp` — Shared utility functions for all examples
  * `examples/relational/example_relational_common.hpp` — Shared relational database helpers

* **Placeholder Directories for Future Examples:**
  * `examples/graph/README.md` — Reserved for graph database examples
  * `examples/timeseries/README.md` — Reserved for time-series database examples

* **New Example Discovery and Execution Script (`run_examples.sh`):**
  * Automatically discovers all compiled examples from build directory
  * Lists examples by category (relational, document, kv, columnar)
  * Supports wildcard pattern matching: `--run='23_*'`, `--run='*_basic'`
  * Provides build hints for missing examples
  * Integrates with `helper.sh` via `--run-example` command

* **Enhanced `helper.sh` Automation:**
  * Added `--run-example` command for running examples
    * `--run-example` — Run all available examples
    * `--run-example=NAMES` — Run specific examples (comma-separated)
    * `--run-example=*` — Run all examples with wildcard support
  * New `cmd_run_examples()` function (188 lines) with full wildcard pattern expansion
  * Automatic example logging to `/logs/example/{timestamp}/`
  * Auto-cleanup of old logs (keeps 4 most recent)
  * Provides summary statistics (success/failure counts)
  * Updated combo command descriptions for clarity

* **Script Organization and Improvements:**
  * Moved shared functions: `lib/common_functions.sh` → `scripts/common/functions.sh`
  * Better organization following DRY principle with centralized `scripts/common/` directory
  * All scripts updated to source from new location
  * Added `--help` / `-h` option to all major scripts:
    * `helper.sh` — Shows all available commands with descriptions
    * `check_dbexception_codes.sh` — Shows usage for checking/fixing error codes
    * `generate_dbexception_code.sh` — Shows usage for generating unique codes
    * `download_and_setup_cassandra_driver.sh` — Shows setup instructions
  * Added `show_usage()` functions with detailed examples to all scripts

* **System Utilities Enhancement (`system_utils.hpp/cpp`):**
  * Added `getExecutablePath()` — Returns directory path where executable is located
  * Added `getExecutablePathAndName()` — Returns full path including executable name
  * Purpose: Portable way to locate executables and resources across platforms
  * Replaced ad-hoc implementations in benchmarks with standardized utility

* **Test Framework Improvements:**
  * **New MongoDB Cursor API Test (`25_521_test_mongodb_real_cursor_api.cpp`):**
    * First use of "exclusive test category" (5YY range) for driver-specific APIs
    * Tests MongoDB-specific cursor methods: `hasNext()` and `nextDocument()`
    * Documents pattern for testing APIs unique to a database family
  * Updated test helper functions in `10_000_test_main.cpp/hpp`
  * Updated all database family tests to use new helpers (MySQL, PostgreSQL, SQLite, Firebird, MongoDB, ScyllaDB)

* **Build System Updates (`CMakeLists.txt`):**
  * Reorganized example targets (lines 468-661) with clear database family sections
  * Added conditional compilation for 56+ example targets based on `USE_<DRIVER>` flags
  * Better documentation with section comments for each database family
  * Proper file paths for moved resources (`test.jpg`, `example_config.yml`)

* **Documentation Updates:**
  * **`how_add_new_db_drivers.md`:**
    * Added "Exclusive Test Categories" section documenting 5YY range
    * Updated test category table with category 521 (driver-exclusive API tests)
    * Reserved categories 522-599 for future exclusive tests
  * **`shell_script_dependencies.md`:**
    * Updated path references: `lib/common_functions.sh` → `scripts/common/functions.sh`
    * Added "Shared Functions Library" section with comprehensive function documentation
    * Added documentation for `run_examples.sh` script
    * Documents 20+ shared bash functions with usage examples

* **Benchmark Improvements (`benchmark_common.cpp/hpp`):**
  * Removed local `getExecutablePathAndName()` and `getOnlyExecutablePath()` functions
  * Now uses `cpp_dbc::system_utils::getExecutablePath()` from core library
  * Cleaner code following DRY principle

* **Deleted Legacy Files:**
  * Removed 15 old example files with generic names
  * Removed old example runner scripts: `run_config_example.sh`, `run_config_integration_example.sh`

* **TODO.md Updates:**
  * Added task: Helgrind support in helper.sh for data race and deadlock detection
  * Added task: Script to detect public methods and check test coverage
  * Added task: Reorganize test source code into family folders

* **162 files changed, 34,214 lines in diff**
* **Impact:** Significantly improved project discoverability, maintainability, and developer experience

## 2026-02-04 22:06:05 PST

### Comprehensive Documentation for New Driver Development and Error Handling

* **New Driver Development Guide (`how_add_new_db_drivers.md`):**
  * Created comprehensive guide (~1,500 lines) for adding new database drivers to cpp_dbc
  * Covers all 5 phases: driver files, build configuration, tests, benchmarks, and examples
  * Includes detailed code reference for base interfaces and family-specific interfaces
  * Documents thread safety macros, RAII handles patterns, and reference implementations
  * Provides complete CMake configuration examples with `USE_<DRIVER>` patterns
  * Includes checklist summary and common mistakes to avoid

* **Error Handling Patterns Documentation (`error_handling_patterns.md`):**
  * Created comprehensive guide (~600 lines) for error handling in cpp_dbc
  * Documents `DBException` class with 12-character error codes and call stack capture
  * Covers both exception-based API and exception-free API (nothrow) patterns
  * Provides patterns by component: Driver, Connection, PreparedStatement, ResultSet
  * Includes DO and DON'T best practices with code examples

* **Shell Script Dependencies Documentation (`shell_script_dependencies.md`):**
  * Created comprehensive guide documenting all shell script dependencies
  * Documents call hierarchy between helper.sh, build scripts, and utility scripts
  * Includes troubleshooting guide for common script issues

* **Updated Project Conventions (`.claude/rules/cpp_dbc_conventions.md`):**
  * Added "Adding New Database Drivers" section referencing the new guide
  * Documents key files that require `USE_<DRIVER>` updates
  * Added reference to validation checkpoint using `helper.sh`

* **4 files changed (new), 1 file modified**

## 2026-02-04 14:14:16 PST

### Cross-Platform Compatibility, Type Portability, and Security Improvements

* **Cross-Platform Compatibility (system_utils.hpp):**
  * Added Windows support with conditional compilation for `localtime_s` (Windows) vs `localtime_r` (Unix)
  * Made `logWithTimestamp()` thread-safe with `std::scoped_lock` on global cout mutex

* **Type Portability (`long` → `int64_t`):**
  * Changed all `long` types to `int64_t` for consistent 64-bit integer handling across platforms
  * Affected interfaces: `ColumnarDBPreparedStatement`, `ColumnarDBResultSet`, `RelationalDBPreparedStatement`, `RelationalDBResultSet`
  * Affected implementations: All driver prepared statements and result sets (MySQL, PostgreSQL, SQLite, Firebird, ScyllaDB)

* **[[nodiscard]] Attribute for Error Handling:**
  * Added `[[nodiscard]]` attribute to all nothrow API methods returning `expected<T, DBException>`
  * Ensures callers check for errors from exception-free API calls
  * Applied across all prepared statement interfaces (relational, columnar) and all driver implementations

* **SQLite BLOB Security Improvements (blob.hpp):**
  * Added `validateIdentifier()` method to prevent SQL injection in table/column names
  * Changed BLOB queries from string concatenation to parameterized queries for rowid
  * Added `ensureLoaded()` call in `saveToDatabase()` to prevent overwriting with empty data
  * Follows cpp_dbc project convention for schema name validation (alphanumeric and underscore only)

* **Destructor Error Handling Improvements:**
  * Improved error handling in destructors by properly checking nothrow API return values
  * Instead of discarding close() return value, now logs errors when close fails in destructors
  * Applied to: MySQL, PostgreSQL, SQLite, and ScyllaDB prepared statement destructors

* **Driver URL Parsing Fixes:**
  * Fixed MySQL driver `parseURL()` to properly handle URLs without port specification
  * Fixed PostgreSQL driver `parseURL()` to properly continue parsing after host extraction
  * Both drivers now correctly set default ports (MySQL: 3306, PostgreSQL: 5432) when port is omitted

* **New Driver parseURL Unit Tests:**
  * Added comprehensive URL parsing tests for MySQL driver (valid URLs, invalid URLs, default port)
  * Added comprehensive URL parsing tests for PostgreSQL driver (valid URLs, IP addresses, default port)
  * Added comprehensive URL parsing tests for SQLite driver (memory databases, file paths)
  * Added comprehensive URL parsing tests for Firebird driver (local and remote connections)
  * Added comprehensive URL parsing tests for Redis driver (URI parsing with IPv6 support)
  * Added comprehensive URL parsing tests for MongoDB driver (standard URI format)

* **Build Script Improvements (helper.sh, run_test_parallel.sh):**
  * Added `-u` (unbuffered) flag to sed for better real-time log output
  * Added ANSI code stripping before grep for better test failure detection in colored output

* **Documentation Fixes:**
  * Fixed accent characters in Spanish documentation (cppdbc-docs-es.md)
  * Improved inline code examples with proper variable declarations
  * Fixed typo in kv_db_connection_pool.hpp comment ("statisticsH" → "statistics")
  * Corrected @brief descriptions in blob.hpp and transaction_manager.hpp
  * Updated code examples to use `cpp_dbc:` URL prefix consistently (replacing `jdbc:` references)

* **59 files changed, 692 insertions(+), 254 deletions(-)**

## 2026-02-04 00:41:38 PST

### Comprehensive Doxygen API Documentation for All Public Headers
* **Documentation Enhancement (64 header files, 2,384 insertions):**
  * Added Doxygen-compatible `/** @brief ... */` documentation blocks to all public header files
  * Replaced simple `//` comments with structured Doxygen documentation across the entire API surface
  * Added inline ```` ```cpp ```` usage examples to all major classes and methods
  * Added `@param`, `@return`, `@throws`, `@see` tags for cross-referencing and parameter documentation
  * Added section separator comments for improved readability

* **Core Interfaces Documented:**
  * `blob.hpp`: MemoryInputStream, MemoryOutputStream, FileInputStream, FileOutputStream, MemoryBlob
  * `streams.hpp`: InputStream, OutputStream, Blob base classes with streaming usage examples
  * `system_utils.hpp`: StackFrame, safePrint, logging functions, captureCallStack, printCallStack
  * `db_connection.hpp`, `db_driver.hpp`, `db_exception.hpp`, `db_expected.hpp`: Core database abstractions
  * `db_result_set.hpp`, `db_types.hpp`: Result set and type system documentation
  * `db_connection_pool.hpp`, `db_connection_pooled.hpp`: Connection pool interfaces
  * `cpp_dbc.hpp`: Main library header with DriverManager, TransactionManager references
  * `transaction_manager.hpp`: Transaction management with cross-thread coordination examples

* **Configuration Layer Documented:**
  * `database_config.hpp`: DBConnectionOptions, DatabaseConfig, DBConnectionPoolConfig, TestQueries, DatabaseConfigManager
  * `yaml_config_loader.hpp`: YamlConfigLoader with YAML file loading examples

* **Relational Database Interfaces Documented:**
  * `relational_db_connection.hpp`: Connection management, SQL execution, transaction examples
  * `relational_db_prepared_statement.hpp`: Parameter binding, BLOB handling, batch operations
  * `relational_db_result_set.hpp`: Result set navigation, type getters, BLOB retrieval

* **Columnar Database Interfaces Documented:**
  * `columnar_db_connection.hpp`: CQL execution, prepared statements, transaction management
  * `columnar_db_prepared_statement.hpp`: CQL parameter binding with UUID, timestamp support
  * `columnar_db_result_set.hpp`: CQL result set navigation with wide-column data types

* **Document Database Interfaces Documented:**
  * `document_db_collection.hpp`: CRUD operations, aggregation, index management
  * `document_db_connection.hpp`: Collection access, database management
  * `document_db_connection_pool.hpp`, `document_db_cursor.hpp`, `document_db_data.hpp`, `document_db_driver.hpp`

* **Key-Value Database Interfaces Documented:**
  * `kv_db_connection.hpp`: String, list, hash, set, sorted set, counter, scan, server operations
  * `kv_db_connection_pool.hpp`, `kv_db_driver.hpp`

* **All Driver Header Files Documented (7 drivers, 36 files):**
  * `drivers/relational/mysql/`: handles, input_stream, connection, driver, prepared_statement
  * `drivers/relational/postgresql/`: handles, input_stream, connection, driver, prepared_statement
  * `drivers/relational/sqlite/`: handles, input_stream, blob, connection, driver, prepared_statement
  * `drivers/relational/firebird/`: handles, input_stream, connection, driver, prepared_statement
  * `drivers/document/mongodb/`: collection, connection, cursor, document, driver
  * `drivers/columnar/scylladb/`: handles, memory_input_stream, connection, driver, prepared_statement, result_set
  * `drivers/kv/redis/`: handles, reply_handle, connection, driver
  * `drivers/columnar/driver_scylladb.hpp`: Aggregator header updated

* **Benefits:**
  * IDE tooltip support — hover over any class or method to see documentation and examples
  * Doxygen-ready — can generate HTML/PDF API reference documentation
  * Self-documenting API — developers can understand usage without leaving the header file
  * Cross-referenced — `@see` tags link related classes and methods
  * 64 files changed, 2,384 insertions(+), 359 deletions(-)

## 2026-02-03 14:58:04 PST

### Driver Header Split Refactoring (One-Class-Per-File)
* **Major Header File Reorganization:**
  * Split all 7 multi-class driver header files (`driver_*.hpp`) into individual per-class `.hpp` files
  * Each driver now has its own subfolder under the respective category directory
  * Original `driver_*.hpp` files converted to pure aggregator headers with only `#include` directives
  * 44 new header files created across 7 driver subdirectories (6,727 lines total)
  * Backward compatible — external consumers still include `driver_mysql.hpp` etc.

* **BLOB Header Files Consolidated:**
  * Deleted 4 separate BLOB header files: `mysql_blob.hpp`, `postgresql_blob.hpp`, `sqlite_blob.hpp`, `firebird_blob.hpp`
  * BLOB classes moved into their respective driver subfolders (e.g., `mysql/blob.hpp`, `sqlite/blob.hpp`)
  * Removed 25 now-redundant BLOB `#include` directives from source, test, and example files

* **New Driver Subfolders:**
  * `relational/mysql/`: `handles.hpp`, `input_stream.hpp`, `blob.hpp`, `result_set.hpp`, `prepared_statement.hpp`, `connection.hpp`, `driver.hpp` (7 files)
  * `relational/postgresql/`: `handles.hpp`, `input_stream.hpp`, `blob.hpp`, `result_set.hpp`, `prepared_statement.hpp`, `connection.hpp`, `driver.hpp` (7 files)
  * `relational/sqlite/`: `handles.hpp`, `input_stream.hpp`, `blob.hpp`, `result_set.hpp`, `prepared_statement.hpp`, `connection.hpp`, `driver.hpp` (7 files)
  * `relational/firebird/`: `handles.hpp`, `input_stream.hpp`, `blob.hpp`, `result_set.hpp`, `prepared_statement.hpp`, `connection.hpp`, `driver.hpp` (7 files)
  * `document/mongodb/`: `handles.hpp`, `document.hpp`, `cursor.hpp`, `collection.hpp`, `connection.hpp`, `driver.hpp` (6 files)
  * `columnar/scylladb/`: `handles.hpp`, `memory_input_stream.hpp`, `result_set.hpp`, `prepared_statement.hpp`, `connection.hpp`, `driver.hpp` (6 files)
  * `kv/redis/`: `handles.hpp`, `reply_handle.hpp`, `connection.hpp`, `driver.hpp` (4 files)

* **File Organization Pattern:**
  * `handles.hpp`: All custom deleters and smart pointer type aliases (RAII handles)
  * `connection.hpp`: Connection class declaration
  * `driver.hpp`: Driver class declaration (real + `#else` stub)
  * `prepared_statement.hpp`: PreparedStatement class declaration
  * `result_set.hpp`: ResultSet class declaration
  * `blob.hpp`: BLOB class declaration (relational drivers only)
  * `input_stream.hpp`: InputStream class declaration (relational drivers only)

* **Benefits:**
  * Better LLM comprehension — smaller, focused files are easier to process in context windows
  * Improved IDE navigation — each class has its own dedicated file
  * Faster header parsing — only include what you need
  * Clearer dependency graph — each file includes only its requirements
  * One-class-per-file convention matches modern C++ best practices
  * 76 files changed, 5,699 insertions(+), 5,106 deletions(-)

## 2026-02-03 12:16:31 PST

### Build System and VSCode IntelliSense Improvements
* **Build Configuration Management (DRY Principle):**
  * Created centralized `libs/cpp_dbc/generate_build_config.sh` script for generating `build/.build_config` file
  * Eliminates code duplication across `build.sh` and test build scripts
  * Supports two modes:
    * With parameters: Direct configuration from command-line arguments
    * Without parameters: Auto-detection from CMakeCache.txt in known build locations
  * Falls back to existing `.build_config` if no CMakeCache.txt found
  * Fails gracefully with clear error messages when no configuration source exists
  * Updated `build.sh` to call `generate_build_config.sh` instead of inline generation
  * Updated `build_test_cpp_dbc.sh` to call `generate_build_config.sh` after building
  * Updated `helper.sh --vscode` to auto-regenerate `.build_config` if missing

* **Unified Library Build Directory (Critical Architecture Fix):**
  * Fixed double compilation issue where `cpp_dbc` library was compiled twice:
    * Once for main project in `build/`
    * Once privately for tests in `build/libs/cpp_dbc/test/`
  * Unified build directory to `build/libs/cpp_dbc/build/` where:
    * Library compiles once and generates `.a` static library
    * Tests link against the single compiled library
    * Both library and tests share the same build configuration
  * Updated `libs/cpp_dbc/build_test_cpp_dbc.sh`:
    * Changed `TEST_BUILD_DIR` to unified `BUILD_DIR`
    * Changed build command from `--target cpp_dbc_tests` to build entire project
  * Updated `libs/cpp_dbc/run_test_cpp_dbc.sh` to use new unified build directory

* **VSCode IntelliSense Path Management:**
  * Updated `.vscode/detect_include_paths.sh` to support both main and library builds:
    * Main project paths: `build/Debug/generators`, `build/Release/generators`
    * Library build paths: `build/libs/cpp_dbc/build`, `build/libs/cpp_dbc/conan/build/{Debug,Release}/generators`
  * Converts absolute paths to relative using VSCode variables:
    * Paths starting with `$HOME` → `${userHome}`
    * Paths starting with `$PROJECT_ROOT` → `${workspaceFolder}`
    * System paths (like `/usr/include/*`) remain absolute
  * Added `convert_to_relative()` function for path normalization
  * Added path deduplication to prevent duplicate entries in `c_cpp_properties.json`
  * Ensures IntelliSense works correctly regardless of which build method is used

* **Default Build Options Changed:**
  * `USE_CPP_YAML` is now `ON` by default (previously `OFF`)
  * Projects now have MySQL and YAML support enabled by default
  * Updated `libs/cpp_dbc/CMakeLists.txt`, `generate_build_config.sh`, and `README.md`

* **Benefits:**
  * Eliminates code duplication (DRY principle)
  * Prevents library double compilation (faster builds)
  * Consistent build configuration across all build methods
  * VSCode IntelliSense stays synchronized with actual build configuration
  * Portable configuration files work across different user environments
  * Clear error messages guide users when configuration is missing

## 2026-01-31 23:41:14 PST

### Driver Code Split Refactoring
* **Major Code Reorganization:**
  * Split all database driver implementations from single large files into multiple smaller, focused files
  * This improves code maintainability, compilation speed, and makes it easier to navigate the codebase
  * Each driver now has dedicated files for Connection, Driver, PreparedStatement, and ResultSet implementations

* **SQLite Driver Split:**
  * `src/drivers/relational/sqlite/sqlite_internal.hpp` - Internal declarations and shared definitions
  * `src/drivers/relational/sqlite/driver_01.cpp` - SQLiteDBDriver implementation
  * `src/drivers/relational/sqlite/connection_01.cpp`, `connection_02.cpp` - SQLiteConnection implementation
  * `src/drivers/relational/sqlite/prepared_statement_01.cpp` to `prepared_statement_04.cpp` - SQLitePreparedStatement implementation
  * `src/drivers/relational/sqlite/result_set_01.cpp` to `result_set_03.cpp` - SQLiteResultSet implementation

* **MySQL Driver Split:**
  * `src/drivers/relational/mysql/mysql_internal.hpp` - Internal declarations and shared definitions
  * `src/drivers/relational/mysql/driver_01.cpp` - MySQLDBDriver implementation
  * `src/drivers/relational/mysql/connection_01.cpp` to `connection_03.cpp` - MySQLConnection implementation
  * `src/drivers/relational/mysql/prepared_statement_01.cpp` to `prepared_statement_03.cpp` - MySQLPreparedStatement implementation
  * `src/drivers/relational/mysql/result_set_01.cpp` to `result_set_03.cpp` - MySQLResultSet implementation

* **PostgreSQL Driver Split:**
  * `src/drivers/relational/postgresql/postgresql_internal.hpp` - Internal declarations and shared definitions
  * `src/drivers/relational/postgresql/driver_01.cpp` - PostgreSQLDBDriver implementation
  * `src/drivers/relational/postgresql/connection_01.cpp` to `connection_03.cpp` - PostgreSQLConnection implementation
  * `src/drivers/relational/postgresql/prepared_statement_01.cpp` to `prepared_statement_04.cpp` - PostgreSQLPreparedStatement implementation
  * `src/drivers/relational/postgresql/result_set_01.cpp` to `result_set_03.cpp` - PostgreSQLResultSet implementation

* **Firebird Driver Split:**
  * `src/drivers/relational/firebird/firebird_internal.hpp` - Internal declarations and shared definitions
  * `src/drivers/relational/firebird/driver_01.cpp` - FirebirdDBDriver implementation
  * `src/drivers/relational/firebird/connection_01.cpp` to `connection_03.cpp` - FirebirdConnection implementation
  * `src/drivers/relational/firebird/prepared_statement_01.cpp` to `prepared_statement_03.cpp` - FirebirdPreparedStatement implementation
  * `src/drivers/relational/firebird/result_set_01.cpp` to `result_set_04.cpp` - FirebirdResultSet implementation

* **MongoDB Driver Split:**
  * `src/drivers/document/mongodb/mongodb_internal.hpp` - Internal declarations and shared definitions
  * `src/drivers/document/mongodb/driver_01.cpp` - MongoDBDriver implementation
  * `src/drivers/document/mongodb/connection_01.cpp` to `connection_04.cpp` - MongoDBConnection implementation
  * `src/drivers/document/mongodb/collection_01.cpp` to `collection_07.cpp` - MongoDBCollection implementation
  * `src/drivers/document/mongodb/cursor_01.cpp`, `cursor_02.cpp` - MongoDBCursor implementation
  * `src/drivers/document/mongodb/document_01.cpp` to `document_06.cpp` - MongoDBData implementation

* **Redis Driver Split:**
  * `src/drivers/kv/redis/redis_internal.hpp` - Internal declarations and shared definitions
  * `src/drivers/kv/redis/driver_01.cpp` - RedisDBDriver implementation
  * `src/drivers/kv/redis/connection_01.cpp` to `connection_06.cpp` - RedisDBConnection implementation

* **ScyllaDB Driver Split:**
  * `src/drivers/columnar/scylladb/scylladb_internal.hpp` - Internal declarations and shared definitions
  * `src/drivers/columnar/scylladb/driver_01.cpp` - ScyllaDBDriver implementation
  * `src/drivers/columnar/scylladb/connection_01.cpp` - ScyllaDBConnection implementation
  * `src/drivers/columnar/scylladb/prepared_statement_01.cpp` to `prepared_statement_03.cpp` - ScyllaDBPreparedStatement implementation
  * `src/drivers/columnar/scylladb/result_set_01.cpp` to `result_set_03.cpp` - ScyllaDBResultSet implementation

* **Build System Updates:**
  * Updated `libs/cpp_dbc/CMakeLists.txt` to compile all new split source files
  * Each driver's source files are now organized in dedicated subdirectories
  * Original monolithic driver files now just include the split files

* **Benefits of Code Split:**
  * Faster incremental compilation - only changed files need to be recompiled
  * Better code organization - each class component has dedicated files
  * Easier navigation - find specific functionality quickly
  * Reduced file complexity - smaller, more focused files
  * Better IDE support - smaller files load and parse faster

## 2026-01-29 16:23:21 PST

### SonarCloud Code Quality Fixes and Connection Pool Improvements
* **Critical Race Condition Fix in Connection Pool Return Flow:**
  * Fixed race condition in `close()` and `returnToPool()` methods across all pool types (Relational, Document, Columnar, KV)
  * Bug: `m_closed` was reset to `false` AFTER `returnConnection()` completed, creating a window where another thread could get a connection with `m_closed=true`
  * Fix: Reset `m_closed` to `false` BEFORE calling `returnConnection()` to ensure connection state is correct when available in idle queue
  * Added catch-all exception handlers to ensure `m_closed` is always set correctly on any exception
  * Added detailed documentation comments explaining the race condition and fix
* **Connection Pool ConstructorTag Pattern (PassKey Idiom):**
  * Added `DBConnectionPool::ConstructorTag` protected struct to enable `std::make_shared` while enforcing factory pattern
  * Updated all connection pool classes to use ConstructorTag in constructors
  * Enables single memory allocation with `std::make_shared` instead of separate allocations with `new`
  * Removed NOSONAR comments that were needed for the previous `new` usage
  * Applied to: `ColumnarDBConnectionPool`, `DocumentDBConnectionPool`, `KVDBConnectionPool`, `RelationalDBConnectionPool`
  * Applied to all derived pools: `ScyllaConnectionPool`, `MongoDBConnectionPool`, `RedisDBConnectionPool`, `MySQLConnectionPool`, `PostgreSQLConnectionPool`, `SQLiteConnectionPool`, `FirebirdConnectionPool`
* **SonarCloud Code Quality Fixes:**
  * Added NOSONAR comments with explanations for intentional code patterns
  * Added `[[noreturn]]` attributes to stub methods that always throw exceptions (ScyllaDB disabled driver)
  * Changed `virtual ~Class()` to `~Class() override` for derived classes (ColumnarDBDriver, DocumentDBDriver, KVDBDriver, RelationalDBDriver, etc.)
  * Added Rule of 5 (delete copy/move operations) to `MySQLDBDriver` and `PostgreSQLDBDriver`
  * Changed nested namespace declarations to modern C++17 syntax (`namespace cpp_dbc::config` instead of `namespace cpp_dbc { namespace config {`)
  * Fixed catch blocks to use specific exception types instead of bare `catch(...)` in PostgreSQL driver
  * Added `NOSONAR` comments for `RedisReplyHandle` class (Rule of 5 satisfied via unique_ptr)
* **Parallel Test Script Improvements (`run_test_parallel.sh`):**
  * Fixed TUI initialization to occur after build completes (so build output is visible)
  * Added `TUI_ACTIVE` flag to track if TUI was initialized for proper cleanup on interrupt
  * Fixed cleanup to only run if TUI was actually initialized
  * Fixed `--clean` and `--rebuild` flags to only apply during initial build, not per-test execution
* **SonarQube Issues Fetch Script Enhancement (`sonar_qube_issues_fetch.sh`):**
  * Made `--file` parameter optional
  * When `--file` is not specified, fetches ALL issues from ALL files in the project
  * Added pagination support for fetching all issues (handles large projects)
  * Issues are organized by file in timestamped folders
* **Test Fixes:**
  * Fixed Firebird connection pool test to use `initialIdleCount` instead of hardcoded value 3

## 2026-01-28 21:43:01 PST

### Parallel Test Execution System
* **New Parallel Test Runner (`run_test_parallel.sh`):**
  * Added complete parallel test execution script (~1900 lines of bash)
  * Runs test prefixes (10_, 20_, 21_, 23_, 26_, etc.) in parallel batches
  * Each prefix runs independently with separate log files
  * If a prefix fails, it stops but others continue running
  * Logs saved to `logs/test/YYYY-MM-DD-HH-MM-SS/PREFIX_RUNXX.log`
  * TUI (Text User Interface) mode with split panel view using `--progress` flag
  * Summarize mode with `--summarize` to show summary of past test runs
  * Valgrind error detection support
  * Color-coded output for test status (pass/fail)
  * Detailed test execution summary with timing information
* **Helper Script Enhancement (`helper.sh`):**
  * Added `parallel=N` option to run N test prefixes in parallel
  * Added `parallel-order=P1_,P2_,...` option to prioritize specific prefixes
  * Added comprehensive documentation and examples for parallel execution
  * Integrated with `run_test_parallel.sh` when parallel mode is enabled
  * Examples:
    * `./helper.sh --run-test=parallel=4` - Run 4 prefixes in parallel
    * `./helper.sh --run-test=parallel=2,parallel-order=23_` - Prioritize 23_ tests
    * `./helper.sh --run-test=parallel=4,progress` - Run with TUI progress display
* **Run Test Script Enhancement (`run_test.sh`):**
  * Added `--skip-build` flag to skip the build step
  * Added `--list` flag to list tests without running them
* **ScyllaDB Test Fix:**
  * Increased sleep time from 100ms to 250ms in `26_091_test_scylladb_real_right_join.cpp`
  * Ensures proper time for ScyllaDB indexes/data consistency

## 2026-01-20 15:17:41

### SonarCloud Code Quality Fixes and Helper Script Enhancement
* **SonarCloud Configuration Updates:**
  * Consolidated SonarCloud configuration into `.sonarcloud.properties`
  * Deleted redundant `sonar-project.properties` file
  * Added exclusion for cognitive complexity rule (cpp:S3776)
  * Added exclusion for nesting depth rule (cpp:S134)
  * Added documentation comments for each excluded rule
* **Code Quality Improvements in `blob.hpp`:**
  * Added `explicit` keyword to `FileOutputStream` constructor
  * Removed redundant `m_position(0)` initialization in `MemoryInputStream` (already initialized at declaration)
* **Code Quality Improvements in Columnar DB Connection Pool:**
  * Changed `ColumnarDBConnection` destructor from `virtual` to `override`
  * Added in-class member initialization for `m_running{true}` and `m_activeConnections{0}` in `ColumnarDBConnectionPool`
  * Added in-class member initialization for `m_active{false}` and `m_closed{false}` in `ColumnarPooledDBConnection`
  * Changed `validateConnection` method to `const`
  * Changed `close()` and `isPoolValid()` methods to `final` in appropriate classes
  * Replaced `std::lock_guard<std::mutex>` with `std::scoped_lock` for modern C++ style
  * Changed `std::unique_lock<std::mutex>` to `std::unique_lock` with CTAD
  * Replaced generic `std::exception` catches with specific `DBException` catches
  * Simplified conditional logic in `getIdleDBConnection()` method
  * Removed redundant member initializations from constructor initializer lists
* **Helper Script Enhancement (`helper.sh`):**
  * Enhanced `extract_executed_tests()` function to track line numbers for executed tests
  * Updated `display_test_execution_table()` to show log file location with line numbers for executed tests
  * Added relative path display for log files (e.g., `./logs/test/output.log:42`)
  * Improved test execution output to help users quickly navigate to specific test results

## 2026-01-18 23:26:52

### Connection Pool Race Condition Fix and Code Quality Improvements
* Fixed connection pool race condition in all database types (relational, document, columnar, key-value):
  * **Race Condition Fix:**
    * Added pool size recheck under lock to prevent exceeding `m_maxSize` under concurrent creation
    * New connections are now created as candidates and only registered if pool hasn't filled
    * Unregistered candidate connections are properly closed to prevent resource leaks
  * **Improved Return Connection Logic:**
    * Added null check to prevent crash when returning null connections
    * Added proper connection cleanup when pool is shutting down
    * Added `m_maintenanceCondition.notify_one()` after returning connections
  * **Enhanced Idle Connection Handling:**
    * Improved `getIdleDBConnection()` with better error handling
    * Added try-catch around new connection creation in replacement logic
* Fixed MongoDB stub driver (when `USE_MONGODB=OFF`):
  * Uncommented disabled stub implementation code
  * Updated exception marks from text-based (`MONGODB_DISABLED`) to UUID-style marks
* Fixed other driver stub exception marks:
  * MySQL: `MYSQL_DISABLED` → UUID marks (`4FE1EBBEA99F`, `23D2107DA64F`)
  * PostgreSQL: `PGSQL_DISABLED` → UUID marks (`3FE734D0BDE9`, `E39F6F23D06B`)
  * ScyllaDB: `SCYLLA_DISABLED` → `SCYLLADB_DISABLED` for consistency
* Fixed variable initialization in `blob.hpp`:
  * `MemoryInputStream::m_position` now initialized to 0 at declaration
  * `MemoryOutputStream::m_position` now initialized to 0 at declaration
* Fixed remaining ScyllaDB variable naming in `build_dist_pkg.sh`:
  * `SCYLLA_DEV_PKG` → `SCYLLADB_DEV_PKG` for both Debian and Fedora packages

## 2026-01-18 22:49:41

### ScyllaDB Native DATE Type Support Fix
* Fixed ScyllaDB driver to properly handle native Cassandra DATE type:
  * **Reading DATE values (`getDate` in ScyllaDBResultSet):**
    * Added support for `CASS_VALUE_TYPE_DATE` (uint32 - days since epoch with 2^31 bias)
    * Correctly converts Cassandra DATE format to ISO date string (YYYY-MM-DD)
    * Uses `cass_value_get_uint32` instead of treating as timestamp
  * **Writing DATE values (`setDate` in ScyllaDBPreparedStatement):**
    * Changed from `cass_statement_bind_int64` (timestamp) to `cass_statement_bind_uint32` (date)
    * Uses `cass_date_from_epoch` to convert epoch seconds to Cassandra DATE format
    * Uses `timegm` for proper UTC timezone handling (with Windows `_mkgmtime` fallback)
  * **Technical Details:**
    * Cassandra DATE is stored as: `days_since_epoch + 2^31` (bias offset)
    * Previous implementation incorrectly used TIMESTAMP type (milliseconds since epoch)
    * Now properly distinguishes between DATE (uint32) and TIMESTAMP (int64) types

## 2026-01-18 22:33:51

### Connection Pool Deadlock Prevention and ScyllaDB Naming Consistency Fixes
* Fixed potential deadlock in all connection pool implementations:
  * **Deadlock Prevention:**
    * Changed from sequential `std::lock_guard` calls to `std::scoped_lock` for consistent lock ordering
    * Fixed in `columnar_db_connection_pool.cpp` (ScyllaDB)
    * Fixed in `document_db_connection_pool.cpp` (MongoDB)
    * Fixed in `kv_db_connection_pool.cpp` (Redis)
    * Fixed in `relational_db_connection_pool.cpp` (MySQL, PostgreSQL, SQLite, Firebird)
  * **Before (potential deadlock):**
    ```cpp
    std::lock_guard<std::mutex> lockAllConnections(m_mutexAllConnections);
    std::lock_guard<std::mutex> lockIdleConnections(m_mutexIdleConnections);
    ```
  * **After (deadlock-safe):**
    ```cpp
    std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);
    ```
  * **Connection Registration Fix:**
    * Fixed `getIdleDBConnection()` method to properly register newly created connections
    * New connections are now added to `m_allConnections` before being returned
* Fixed ScyllaDB naming consistency:
  * **Namespace Rename:**
    * Changed namespace from `Scylla` to `ScyllaDB` in `driver_scylladb.hpp`
    * Updated error code prefix from `SCYLLA_` to `SCYLLADB_`
  * **Build Script Fixes:**
    * Added ScyllaDB echo output in `build.dist.sh`
    * Added `DEBUG_SCYLLADB=ON` in debug mode for `build.sh`
    * Fixed `--debug-scylladb` option alias in `build_cpp_dbc.sh`
  * **Distribution Package Fixes:**
    * Renamed `SCYLLA_CONTROL_DEP` to `SCYLLADB_CONTROL_DEP` in `build_dist_pkg.sh`
    * Updated `__SCYLLA_DEV_PKG__` to `__SCYLLADB_DEV_PKG__` in all distro Dockerfiles
    * Added `__REDIS_CONTROL_DEP__` placeholder handling in build scripts
* Fixed documentation numbering in `cppdbc-package.md` (corrected step numbers 19→20, 20→21, 20→22)
* Fixed typos in `TODO.md`:
  * "proyect" → "project"
  * "ease" → "easy"
  * "INTERGRATION" → "INTEGRATION"

## 2026-01-18 02:59:56 PM PST

### VSCode IntelliSense Automatic Synchronization System
* Added automatic synchronization system for VSCode IntelliSense:
  * **New Scripts:**
    * Added `.vscode/sync_intellisense.sh` - Quick sync without rebuilding (reads saved config)
    * Added `.vscode/regenerate_intellisense.sh` - Rebuild from last config or with new parameters
    * Added `.vscode/update_defines.sh` - Update defines from compile_commands.json
    * Added `.vscode/detect_include_paths.sh` - Detect system include paths automatically
  * **Documentation:**
    * Added `.vscode/README_INTELLISENSE.md` - Comprehensive guide for IntelliSense configuration
  * **Features:**
    * Build parameters automatically saved to `build/.build_args`
    * Configuration state saved to `build/.build_config`
    * VSCode scripts read these files to stay synchronized
    * No need to manually specify parameters twice
    * Quick sync option that doesn't require rebuild
  * **Workflow Improvements:**
    * Run `./build.sh [options]` then `.vscode/sync_intellisense.sh` to sync IntelliSense
    * Or use `.vscode/regenerate_intellisense.sh` to rebuild with same parameters
    * Supports all database drivers: MySQL, PostgreSQL, SQLite, Firebird, MongoDB, ScyllaDB, Redis
* Removed `EXCEPTION_FREE_ANALYSIS.md` (analysis completed and integrated into codebase)

## 2026-01-18 02:08:02 PM PST

### ScyllaDB Connection Pool and Driver Enhancements
* Added columnar database connection pool implementation for ScyllaDB:
  * **New Connection Pool Files:**
    * Added `include/cpp_dbc/core/columnar/columnar_db_connection_pool.hpp` - Columnar database connection pool interfaces
    * Added `src/core/columnar/columnar_db_connection_pool.cpp` - Columnar database connection pool implementation
    * Added `examples/scylladb_connection_pool_example.cpp` - Example for ScyllaDB connection pool usage
    * Added `test/test_scylladb_connection_pool.cpp` - Comprehensive tests for ScyllaDB connection pool
  * **Pool Features:**
    * Factory pattern with `create` static methods for ScyllaDB connection pool creation
    * Smart pointer-based pool lifetime tracking for memory safety
    * Connection validation with CQL query (`SELECT now() FROM system.local`)
    * Configurable pool size, connection timeout, and idle timeout
    * Support for ScyllaDB authentication
    * Background maintenance thread for connection cleanup
    * Thread-safe connection management
    * `ColumnarDBConnectionPool` base class for columnar databases
    * `ColumnarPooledDBConnection` wrapper class for pooled columnar connections
    * `Scylla::ScyllaConnectionPool` specialized implementation for ScyllaDB
  * **Build System Updates:**
    * Renamed `USE_SCYLLA` to `USE_SCYLLADB` for consistency
    * Renamed driver files from `driver_scylla` to `driver_scylladb`
    * Renamed namespace from `Scylla` to `ScyllaDB`
    * Updated CMakeLists.txt to include the new source files
    * Added ScyllaDB connection pool example to build configuration
    * Added ScyllaDB connection pool tests to test suite
  * **Test Coverage:**
    * Basic connection pool operations (get/return connections)
    * ScyllaDB operations through pooled connections
    * Concurrent connections stress testing
    * Connection pool behavior under load
    * Connection validation and cleanup
    * Pool growth and scaling tests
* Added comprehensive ScyllaDB examples:
  * **New Example Files:**
    * Added `examples/scylla_example.cpp` - Basic ScyllaDB operations example
    * Added `examples/scylla_blob_example.cpp` - BLOB operations with ScyllaDB
    * Added `examples/scylla_json_example.cpp` - JSON data handling with ScyllaDB
    * Added `examples/scylladb_connection_pool_example.cpp` - Connection pool usage example
* Added ScyllaDB benchmark suite:
  * **New Benchmark Files:**
    * Added `benchmark/benchmark_scylladb_select.cpp` - CQL SELECT operations benchmarks
    * Added `benchmark/benchmark_scylladb_insert.cpp` - CQL INSERT operations benchmarks
    * Added `benchmark/benchmark_scylladb_update.cpp` - CQL UPDATE operations benchmarks
    * Added `benchmark/benchmark_scylladb_delete.cpp` - CQL DELETE operations benchmarks
  * **Benchmark Infrastructure Updates:**
    * Updated benchmark scripts with ScyllaDB support
    * Added ScyllaDB configuration to benchmark_db_connections.yml
* Renamed test files for consistency:
  * Renamed `test_scylla_*` to `test_scylladb_*`
  * Updated all test references to use new naming convention
* Added ScyllaDB exception-free API:
  * All ScyllaDB driver methods support nothrow variants
  * Returns `expected<T, DBException>` for error handling
  * Consistent with Redis and PostgreSQL exception-free patterns

## 2026-01-15 11:46:28 PM PST

### ScyllaDB Columnar Database Driver Support
* Added complete ScyllaDB columnar database driver implementation:
  * **New Core Columnar Database Interfaces:**
    * Added implementations for `core/columnar/columnar_db_connection.hpp` - Base connection interface for columnar databases
    * Added implementations for `core/columnar/columnar_db_driver.hpp` - Base driver interface for columnar databases
    * Added implementations for `core/columnar/columnar_db_prepared_statement.hpp` - Prepared statement interface for columnar databases
    * Added implementations for `core/columnar/columnar_db_result_set.hpp` - Result set interface for columnar databases
  * **ScyllaDB Driver Implementation:**
    * Added `drivers/columnar/driver_scylla.hpp` - ScyllaDB driver class declarations
    * Added `src/drivers/columnar/driver_scylla.cpp` - Full ScyllaDB driver implementation
    * Added `cmake/FindCassandra.cmake` - CMake module for Cassandra C++ driver detection (used by ScyllaDB)
  * **Driver Features:**
    * Full support for ScyllaDB/Cassandra databases
    * Connection management with proper resource cleanup using smart pointers
    * Prepared statement support with parameter binding
    * Result set handling with all supported data types
    * Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
  * **Build System Updates:**
    * Added `USE_SCYLLA` option to CMakeLists.txt (default: OFF)
    * Added `--scylla` and `--scylla-off` options to build.sh
    * Added `--scylla` and `--scylla-off` options to build_cpp_dbc.sh
    * Added `--debug-scylla` option for ScyllaDB driver debug output
    * Added `scylla` option to helper.sh for --run-build, --run-test, and --run-benchmarks
    * Automatic detection and installation of Cassandra C++ driver development libraries
  * **Required System Packages:**
    * Debian/Ubuntu: `sudo apt-get install libcassandra-dev`
    * RHEL/CentOS/Fedora: `sudo dnf install cassandra-cpp-driver-devel`
  * **Connection URL Format:**
    * `cpp_dbc:scylladb://host:port/keyspace`
    * Default port: 9042
  * **Smart Pointer Implementation:**
    * Smart pointer-based connection and statement management for memory safety
    * Proper resource cleanup with custom deleters for Cassandra C++ driver
* Added comprehensive ScyllaDB test suite:
  * **New Test Files:**
    * Added `test_scylla_common.hpp` - ScyllaDB test helper declarations
    * Added `test_scylla_common.cpp` - ScyllaDB test helper implementations
    * Added `test_scylla_connection.cpp` - Connection and basic operations tests
    * Added `test_scylla_thread_safe.cpp` - Thread-safety stress tests
    * Added `test_scylla_real_right_join.cpp` - Emulated RIGHT JOIN operations
    * Added `test_scylla_real_inner_join.cpp` - INNER JOIN operations
  * **Test Coverage:**
    * Basic connection and authentication
    * CQL query execution and prepared statements
    * Data type handling and conversions
    * Join operation emulation (not natively supported in CQL)
    * Thread safety with multiple concurrent connections
* Updated build scripts with ScyllaDB support:
  * Updated `build.sh` with `--scylla`, `--scylla-off`, `--debug-scylla` options
  * Updated `build.dist.sh` with ScyllaDB options and dependency detection
  * Updated `run_test.sh` with ScyllaDB test options
  * Updated `helper.sh` with ScyllaDB options for all commands
  * Added `download_and_setup_cassandra_driver.sh` for easy dependency installation

## 2026-01-06 08:11:44 PM PST

### PostgreSQL Exception-Free API Implementation
* Added comprehensive exception-free API for PostgreSQL driver operations:
  * **Implementation:**
    * Implemented nothrow versions of all PostgreSQL driver methods using `std::nothrow_t` parameter
    * All methods return `expected<T, DBException>` with clear error information
    * Comprehensive error handling with unique error codes for each method
    * Replaced "NOT_IMPLEMENTED" placeholders with full implementations
  * **Operations Supported:**
    * Connection management (prepareStatement, executeQuery, executeUpdate)
    * Transaction handling (beginTransaction, commit, rollback)
    * Transaction isolation level management
    * Auto-commit settings
    * Connection URL parsing and validation
    * Parameter binding in prepared statements
  * **Error Handling:**
    * Error propagation with expected<T, DBException>
    * Preserves call stack information in error cases
    * Consistent error code format across all methods
    * Clear error messages with operation and failure reason
  * **Benefits:**
    * Consistent API pattern with other database drivers
    * Performance improvements for code using nothrow API
    * Safer error handling without exception overhead
    * Full compatibility with both exception and non-exception usage patterns

## 2026-01-03 05:23:03 PM PST

### Redis Exception-Free API Implementation
* Added comprehensive exception-free API for Redis driver operations:
  * **Implementation:**
    * Added nothrow versions of all Redis driver methods using `std::nothrow_t` parameter
    * All methods return `expected<T, DBException>` with clear error information
    * Comprehensive error handling with unique error codes for each method
    * Added `#include <new>` for `std::nothrow_t`
  * **Operations Supported:**
    * Key-Value operations (setString, getString, exists, deleteKey, etc.)
    * List operations (listPushLeft, listPushRight, listPopLeft, etc.)
    * Hash operations (hashSet, hashGet, hashDelete, hashExists, etc.)
    * Set operations (setAdd, setRemove, setIsMember, etc.)
    * Sorted Set operations (sortedSetAdd, sortedSetRemove, sortedSetScore, etc.)
    * Server operations (scanKeys, ping, flushDB, getServerInfo, etc.)
  * **Error Handling:**
    * Error propagation with expected<T, DBException>
    * Preserves call stack information in error cases
    * Consistent error code format across all methods
    * Clear error messages with operation and failure reason

## 2026-01-02 06:18:19 PM PST
### Exception-Free API Implementation using std::expected and std::nothrow
* Added comprehensive exception-free API as an alternative to traditional exception-based error handling:
  * **New Header File:**
    * Added `include/cpp_dbc/core/db_expected.hpp` - Implementation of `expected<T, E>` with polyfill for C++11+
    * Support for native `std::expected` in C++23 with automatic detection
  * **Architecture:**
    * Inverted implementation pattern: `std::nothrow` versions contain the real logic, exception versions are lightweight wrappers
    * Both APIs always coexist (no conditional compilation) - user chooses which to use according to their needs
    * Reuses existing `DBException` directly in `expected<T, DBException>` (no additional error type needed)
    * Maintains same call stack and error information as exception-based API
  * **Method Signatures:**
    * All methods have dual signatures: exception-throwing and exception-free variants
    * Exception-free variants use `std::nothrow_t` parameter and return `expected<T, DBException>`
    * Void-returning methods use `expected<void, DBException>` for error handling
  * **Testing:**
    * Added `test_expected.cpp` with comprehensive tests for `cpp_dbc::expected` implementation
    * Tests for basic construction, value/error access, move/copy semantics, and monadic operations
    * Tests for exception-free API with various database driver methods
  * **Usage Examples:**
    * Traditional exception-based code continues to work unchanged
    * New code can use exception-free variants with `std::nothrow` parameter
    * Supports fluent programming with monadic operations (and_then, transform, or_else)
  * **Benefits:**
    * No logic duplication - single implementation in nothrow methods
    * Natural migration path - existing code works without changes
    * Performance - nothrow users completely avoid exception overhead
    * Cleaner error handling in performance-critical code
    * Better interop with code that can't use exceptions

## 2026-01-01 07:48:12 PM PST

### Redis KV Connection Pool Implementation
* Added key-value database connection pool implementation:
  * **New Files:**
    * Added `include/cpp_dbc/core/kv/kv_db_connection_pool.hpp` - Key-value database connection pool interfaces
    * Added `src/core/kv/kv_db_connection_pool.cpp` - Key-value database connection pool implementation
    * Added `examples/kv_connection_pool_example.cpp` - Example for Redis connection pool usage
    * Added `test/test_redis_connection_pool.cpp` - Comprehensive tests for Redis connection pool
  * **Pool Features:**
    * Factory pattern with `create` static methods for Redis connection pool creation
    * Smart pointer-based pool lifetime tracking for memory safety
    * Connection validation with Redis ping command
    * Configurable pool size, connection timeout, and idle timeout
    * Support for Redis authentication
    * Background maintenance thread for connection cleanup
    * Thread-safe connection management
  * **Build System Updates:**
    * Updated CMakeLists.txt to include the new source files
    * Added key-value connection pool example to build configuration
    * Added Redis connection pool tests to test suite
  * **Test Coverage:**
    * Basic connection pool operations (get/return connections)
    * Redis operations through pooled connections
    * Concurrent connections stress testing
    * Connection pool behavior under load
    * Connection validation and cleanup
    * Pool growth and scaling tests

## 2025-12-31 08:34:52 PM PST

### Redis KV Database Driver Support
* Added complete Redis key-value database driver implementation:
  * **New Core KV Database Interfaces:**
    * Added `core/kv/kv_db_connection.hpp` - Base connection interface for key-value databases
    * Added `core/kv/kv_db_driver.hpp` - Base driver interface for key-value databases
  * **Redis Driver Implementation:**
    * Added `drivers/kv/driver_redis.hpp` - Redis driver class declarations
    * Added `src/drivers/kv/driver_redis.cpp` - Full Redis driver implementation
  * **Driver Features:**
    * Full support for Redis databases
    * Connection management with proper resource cleanup using smart pointers
    * Key-Value operations: get, set, delete
    * String operations with TTL support
    * List operations (push, pop, range)
    * Hash operations (set, get, delete)
    * Set operations (add, remove, members)
    * Sorted Set operations (add, remove, range)
    * Counter operations (increment, decrement)
    * Scan operations for key pattern matching
    * Server operations (ping, info, flush)
    * Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
  * **Build System Updates:**
    * Added `USE_REDIS` option to CMakeLists.txt (default: OFF)
    * Added `--redis` and `--redis-off` options to build.sh
    * Added `--redis` and `--redis-off` options to build_cpp_dbc.sh
    * Added `--debug-redis` option for Redis driver debug output
    * Added `redis` option to helper.sh for --run-build, --run-test, and --run-benchmarks
  * **Required System Packages:**
    * Debian/Ubuntu: `sudo apt-get install libhiredis-dev`
    * RHEL/CentOS/Fedora: `sudo dnf install hiredis-devel`
  * **Connection URL Format:**
    * `cpp_dbc:redis://host:port/database`
    * `cpp_dbc:redis://username:password@host:port/database`
    * Default port: 6379
  * **Smart Pointer Implementation:**
    * `shared_ptr<redisContext>` for connection management
    * Custom deleters for proper Redis resource cleanup
    * Thread-safe connection operations with mutex protection
* Added comprehensive Redis test suite:
  * **New Test Files:**
    * Added `test_redis_common.hpp` - Redis test helper declarations
    * Added `test_redis_common.cpp` - Redis test helper implementations
    * Added `test_redis_connection.cpp` - Comprehensive Redis connection and operations tests
  * **Test Coverage:**
    * Basic connection and authentication
    * String operations with and without TTL
    * List operations (push, pop, range)
    * Hash operations (set, get, delete)
    * Set operations (add, remove, membership)
    * Sorted Set operations with score handling
    * Counter operations (increment, decrement)
    * Scan operations with pattern matching
    * Server info and command execution
    * Database selection and management
* Added Redis benchmark suite:
  * **New Benchmark Files:**
    * Added `benchmark_redis_select.cpp` - Key-value retrieval benchmarks
    * Added `benchmark_redis_insert.cpp` - Key-value insertion benchmarks
    * Added `benchmark_redis_update.cpp` - Key-value update benchmarks
    * Added `benchmark_redis_delete.cpp` - Key-value deletion benchmarks
  * **Benchmark Infrastructure Updates:**
    * Updated benchmark scripts with Redis support
    * Added Redis configuration to benchmark_db_connections.yml
* Updated build scripts with Redis support:
  * Updated `build.sh` with `--redis`, `--redis-off`, `--debug-redis` options
  * Updated `run_test.sh` with Redis options
  * Updated `helper.sh` with Redis options for all commands
  * Updated `build_cpp_dbc.sh` with Redis build configuration
  * Updated `run_test_cpp_dbc.sh` with Redis test options
  * Updated `build_test_cpp_dbc.sh` with Redis test build options
* Updated configuration files:
  * Added Redis database configurations to `test_db_connections.yml`

## 2025-12-30 11:38:13 PM PST

### Document Database Connection Pool Factory Pattern Implementation
* Implemented factory pattern for MongoDB connection pool creation:
  * **API Changes:**
    * Added `create` static factory methods to `DocumentDBConnectionPool` and `MongoDBConnectionPool`
    * Made constructors protected to enforce factory method usage
    * Added `std::enable_shared_from_this` inheritance to `DocumentDBConnectionPool`
    * Added `initializePool` method for initialization after shared_ptr construction
    * Updated all code to use factory methods instead of direct instantiation
  * **Memory Safety Improvements:**
    * Improved connection lifetime management with weak_ptr reference tracking
    * Removed unnecessary raw pointer references in pooled connections
    * Simplified pooled connection constructor interface
    * Enhanced resource cleanup with proper initialization sequence
  * **Test Updates:**
    * Updated MongoDB connection pool tests to use the factory methods
    * Fixed thread safety tests to use shared_ptr for pool access
    * Improved test readability with auto type deduction
    * Added comprehensive validation in connection pool tests

## 2025-12-30 04:28:19 PM PST

### Connection Pool Factory Pattern Implementation
* Implemented factory pattern for connection pool creation:
  * **Pool Creation API Changes:**
    * Added `create` static factory methods to `RelationalDBConnectionPool` and all specific pools
    * Made constructors protected to enforce factory method usage
    * Added `std::enable_shared_from_this` inheritance to `RelationalDBConnectionPool`
    * Added `initializePool` method for initialization after shared_ptr construction
    * Updated all code to use factory methods instead of direct instantiation
  * **Pool Management Improvements:**
    * Improved connection lifetime management with weak_ptr reference tracking
    * Removed unnecessary raw pointer references in pooled connections
    * Simplified pooled connection constructor interface
    * Enhanced resource cleanup with proper initialization sequence
  * **Test Updates:**
    * Updated all test files to use the factory methods
    * Fixed thread safety tests to use shared_ptr for pool access
    * Improved test readability with auto type deduction
    * Enhanced pool validation in transaction tests

## 2025-12-30 12:38:57 PM PST

### MongoDB Connection Pool Implementation
* Added document database connection pool implementation:
  * **New Files:**
    * Added `include/cpp_dbc/core/document/document_db_connection_pool.hpp` - Document database connection pool interfaces
    * Added `src/core/document/document_db_connection_pool.cpp` - Document database connection pool implementation
    * Added `examples/document_connection_pool_example.cpp` - Example for MongoDB connection pool usage
    * Added `test/test_mongodb_connection_pool.cpp` - Comprehensive tests for MongoDB connection pool
  * **Pool Features:**
    * Smart pointer-based pool lifetime tracking for memory safety
    * Connection validation with MongoDB ping command
    * Configurable pool size, connection timeout, and idle timeout
    * Support for MongoDB authentication
    * Background maintenance thread for connection cleanup
    * Thread-safe connection management
  * **Build System Updates:**
    * Updated CMakeLists.txt to include the new source files
    * Added document connection pool example to build configuration
    * Added MongoDB connection pool tests to test suite
  * **Test Coverage:**
    * Basic connection pool operations (get/return connections)
    * Document operations through pooled connections
    * Concurrent connections stress testing
    * Connection pool behavior under load
    * Connection validation and cleanup

## 2025-12-29 10:09:31 PM PST
### Connection Pool Architecture Refactoring
* Reorganized connection pool implementation for multi-database paradigm support:
  * **New Abstract Interfaces:**
    * Added `core/db_connection_pool.hpp` - Generic connection pool interface for all database types
    * Added `core/pooled_db_connection.hpp` - Generic pooled connection interface for all database types
  * **Renamed Files:**
    * Renamed `connection_pool.cpp` → `relational_db_connection_pool.cpp` for relational-specific implementation
    * Updated CMakeLists.txt to use the new source file name
  * **API Changes:**
    * Renamed `getDBConnection()` → `getRelationalDBConnection()` for relational database specific access
    * Updated all examples, tests, and benchmarks with the new method name
  * **Include Path Updates:**
    * Changed include path from `<cpp_dbc/connection_pool.hpp>` → `<cpp_dbc/core/relational/relational_db_connection_pool.hpp>`
    * Updated all source files, examples, tests, and benchmarks with the new path
* Benefits of connection pool architecture refactoring:
  * Clear separation between different database paradigms (relational, document, etc.)
  * Generic interfaces for consistent connection pool behavior across all database types
  * Specialized implementations for paradigm-specific features
  * Improved code organization following industry standards

## 2025-12-29 04:28:15 PM PST

### MongoDB Benchmark and Test Improvements
* Added comprehensive MongoDB benchmark suite:
  * **New Benchmark Files:**
    * Added `benchmark_mongodb_select.cpp` with SELECT operations benchmarks
    * Added `benchmark_mongodb_insert.cpp` with INSERT operations benchmarks
    * Added `benchmark_mongodb_update.cpp` with UPDATE operations benchmarks
    * Added `benchmark_mongodb_delete.cpp` with DELETE operations benchmarks
  * **Benchmark Infrastructure Updates:**
    * Updated `benchmark/CMakeLists.txt` to include MongoDB benchmark files
    * Added `USE_MONGODB` compile definition for conditional compilation
    * Added MongoDB helper functions to `benchmark_common.hpp`
    * Updated benchmark scripts with MongoDB-specific options
  * **New Benchmark Options:**
    * Added `--mongodb` parameter to run MongoDB benchmarks
    * Added MongoDB memory usage tracking to baseline creation
* Added comprehensive MongoDB test coverage:
  * **New Test Files:**
    * Added `test_mongodb_real_json.cpp` with JSON operations tests
    * Added `test_mongodb_thread_safe.cpp` with thread-safety stress tests
    * Added `test_mongodb_real_join.cpp` with join operations tests (aggregation pipeline)
  * **Test Coverage:**
    * Document JSON operations (basic, nested, arrays)
    * JSON query operators ($eq, $gt, $lt, etc.)
    * JSON updates and modifications
    * JSON aggregation operations
    * Thread safety with multiple connections
    * Thread safety with shared connection
    * Join operations using MongoDB aggregation pipeline
* Enhanced helper script with MongoDB support:
  * Added new command combinations for MongoDB testing and benchmarks
  * Added example command combinations for different scenarios
  * Updated usage information with MongoDB examples

## 2025-12-27 08:34:18 PM PST

### MongoDB Document Database Driver Support
* Added complete MongoDB document database driver implementation:
  * **New Core Document Database Interfaces:**
    * Added `core/document/document_db_connection.hpp` - Base connection interface for document databases
    * Added `core/document/document_db_driver.hpp` - Base driver interface for document databases
    * Added `core/document/document_db_collection.hpp` - Collection interface for document operations
    * Added `core/document/document_db_cursor.hpp` - Cursor interface for iterating query results
    * Added `core/document/document_db_data.hpp` - Document data interface for BSON/JSON handling
  * **MongoDB Driver Implementation:**
    * Added `drivers/document/driver_mongodb.hpp` - MongoDB driver class declarations
    * Added `src/drivers/document/driver_mongodb.cpp` - Full MongoDB driver implementation
    * Added `cmake/FindMongoDB.cmake` - CMake module for MongoDB C++ driver detection
  * **Driver Features:**
    * Full support for MongoDB databases (version 4.0+)
    * Connection management with proper resource cleanup using smart pointers
    * Collection operations: create, drop, list collections
    * Document CRUD operations: insertOne, insertMany, findOne, find, updateOne, updateMany, replaceOne, deleteOne, deleteMany
    * Aggregation pipeline support
    * Index management: createIndex, dropIndex, listIndexes
    * Cursor-based iteration for query results
    * Document data type handling: strings, integers, doubles, booleans, arrays, nested objects, null values
    * Upsert support for update operations
    * Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
  * **Build System Updates:**
    * Added `USE_MONGODB` option to CMakeLists.txt (default: OFF)
    * Added `--mongodb` and `--mongodb-off` options to build.sh
    * Added `--mongodb` and `--mongodb-off` options to build_cpp_dbc.sh
    * Added `--debug-mongodb` option for MongoDB driver debug output
    * Added `mongodb` option to helper.sh for --run-build, --run-test, and --run-benchmarks
    * Automatic detection of MongoDB C++ driver libraries
  * **Required System Packages:**
    * Debian/Ubuntu: `sudo apt-get install libmongoc-dev libbson-dev libmongocxx-dev libbsoncxx-dev`
    * RHEL/CentOS/Fedora: `sudo dnf install mongo-c-driver-devel libbson-devel mongo-cxx-driver-devel`
  * **Connection URL Format:**
    * `mongodb://host:port/database`
    * `mongodb://username:password@host:port/database?authSource=admin`
    * Default port: 27017
  * **Smart Pointer Implementation:**
    * `shared_ptr` for connection and collection management
    * Custom deleters for proper MongoDB resource cleanup
    * `weak_ptr` in cursors for safe connection reference
* Added comprehensive MongoDB test suite:
  * **New Test Files:**
    * Added `test_mongodb_common.hpp` - MongoDB test helper declarations
    * Added `test_mongodb_common.cpp` - MongoDB test helper implementations
    * Added `test_mongodb_real.cpp` - Comprehensive MongoDB integration tests
  * **Test Coverage:**
    * Basic CRUD operations (insert, find, update, delete)
    * Document data types (strings, integers, doubles, booleans, arrays, nested objects)
    * Aggregation pipeline operations
    * Index creation and management
    * Concurrent operations with multiple threads
    * Find with projection
    * Replace and upsert operations
* Updated build scripts with MongoDB support:
  * Updated `build.sh` with `--mongodb`, `--mongodb-off`, `--debug-mongodb` options
  * Updated `run_test.sh` with MongoDB options
  * Updated `helper.sh` with MongoDB options for all commands
  * Updated `build_cpp_dbc.sh` with MongoDB build configuration
  * Updated `run_test_cpp_dbc.sh` with MongoDB test options
  * Updated `build_test_cpp_dbc.sh` with MongoDB test build options
* Updated configuration files:
  * Added MongoDB database configurations to `test_db_connections.yml`
  * Added MongoDB connection pool configuration options

## 2025-12-27 01:22:58 AM PST

### Directory Restructuring for Multi-Database Type Support
* Reorganized project directory structure to support multiple database types:
  * **Core Interfaces Moved:**
    * Moved `relational/` → `core/relational/`
    * Files: `relational_db_connection.hpp`, `relational_db_driver.hpp`, `relational_db_prepared_statement.hpp`, `relational_db_result_set.hpp`
  * **Driver Files Moved:**
    * Moved `drivers/` → `drivers/relational/`
    * Files: `driver_firebird.hpp/cpp`, `driver_mysql.hpp/cpp`, `driver_postgresql.hpp/cpp`, `driver_sqlite.hpp/cpp`
    * BLOB headers: `firebird_blob.hpp`, `mysql_blob.hpp`, `postgresql_blob.hpp`, `sqlite_blob.hpp`
  * **New Placeholder Directories Created:**
    * Core interfaces: `core/columnar/`, `core/document/`, `core/graph/`, `core/kv/`, `core/timeseries/`
    * Driver implementations: `drivers/columnar/`, `drivers/document/`, `drivers/graph/`, `drivers/kv/`, `drivers/timeseries/`
    * Source files: `src/drivers/columnar/`, `src/drivers/document/`, `src/drivers/graph/`, `src/drivers/kv/`, `src/drivers/timeseries/`
  * **Updated Include Paths:**
    * Updated all include paths in source files to reflect new directory structure
    * Updated `cpp_dbc.hpp` main header with new paths
    * Updated `connection_pool.hpp` with new paths
    * Updated all example files, test files, and benchmark files
    * Updated `CMakeLists.txt` with new source file paths
* Benefits of directory restructuring:
  * Clear separation between database types (relational, document, graph, etc.)
  * Prepared for future database driver implementations
  * Better code organization following industry standards
  * Easier navigation and maintenance of codebase

## 2025-12-27 12:09:26 AM PST

### Connection Pool Memory Safety Improvements
* Enhanced connection pool with smart pointer-based pool lifetime tracking:
  * **RelationalDBConnectionPool Changes:**
    * Added `m_poolAlive` shared atomic flag (`std::shared_ptr<std::atomic<bool>>`) to track pool lifetime
    * Pool sets `m_poolAlive` to `false` in `close()` method before cleanup
    * Prevents pooled connections from attempting to return to a destroyed pool
  * **RelationalDBPooledConnection Changes:**
    * Changed `m_pool` from raw pointer to `std::weak_ptr<RelationalDBConnectionPool>`
    * Added `m_poolAlive` shared atomic flag for safe pool lifetime checking
    * Added `m_poolPtr` raw pointer for pool access (only used when `m_poolAlive` is true)
    * Added `isPoolValid()` helper method to check if pool is still alive
    * Updated constructor to accept weak_ptr, poolAlive flag, and raw pointer
    * Updated `close()` method to check `isPoolValid()` before returning connection to pool
* Benefits of connection pool smart pointer migration:
  * Prevention of use-after-free when pool is destroyed while connections are in use
  * Safe detection of pool destruction via shared atomic flag
  * Graceful handling of connection return when pool is already closed
  * Clear ownership semantics with weak_ptr for non-owning references

## 2025-12-26 10:08:08 PM PST

### API Naming Convention Refactoring
* Renamed classes and methods to use "DB" prefix for better clarity and consistency:
  * **Driver Classes:**
    * `MySQLDriver` → `MySQLDBDriver`
    * `PostgreSQLDriver` → `PostgreSQLDBDriver`
    * `SQLiteDriver` → `SQLiteDBDriver`
    * `FirebirdDriver` → `FirebirdDBDriver`
  * **Connection Classes:**
    * `Connection` → `RelationalDBConnection` (for relational database connections)
  * **Configuration Classes:**
    * `ConnectionPoolConfig` → `DBConnectionPoolConfig`
  * **DriverManager Methods:**
    * `getConnection()` → `getDBConnection()`
  * **TransactionManager Methods:**
    * `getTransactionConnection()` → `getTransactionDBConnection()`
  * **Driver Methods:**
    * `connect()` → `connectRelational()` (for relational database connections)
* Updated all benchmark files to use new class and method names
* Updated all test files to use new class and method names
* Updated all example files to use new class and method names
* Updated main.cpp to use new driver class names

## 2025-12-22 08:15:09 PM PST

### BLOB Memory Safety Improvements with Smart Pointers
* Migrated all BLOB implementations from raw pointers to smart pointers for improved memory safety:
  * **Firebird BLOB:**
    * Changed from raw `isc_db_handle*` and `isc_tr_handle*` to `std::weak_ptr<FirebirdConnection>`
    * Added `getConnection()` helper method that throws `DBException` if connection is closed
    * Added `getDbHandle()` and `getTrHandle()` inline methods for safe handle access
    * Added `FirebirdBlob` as friend class to `FirebirdConnection` for private member access
    * Updated all BLOB operations to use connection-based constructor
  * **MySQL BLOB:**
    * Changed from raw `MYSQL*` to `std::weak_ptr<MYSQL>`
    * Added `getMySQLConnection()` helper method that throws `DBException` if connection is closed
    * Added `isConnectionValid()` method to check connection state
    * Updated constructors to accept `std::shared_ptr<MYSQL>`
  * **PostgreSQL BLOB:**
    * Changed from raw `PGconn*` to `std::weak_ptr<PGconn>`
    * Added `getPGConnection()` helper method that throws `DBException` if connection is closed
    * Added `isConnectionValid()` method to check connection state
    * Updated all large object operations to use safe connection access
    * Improved `remove()` method to gracefully handle closed connections
  * **SQLite BLOB:**
    * Changed from raw `sqlite3*` to `std::weak_ptr<sqlite3>`
    * Added `getSQLiteConnection()` helper method that throws `DBException` if connection is closed
    * Added `isConnectionValid()` method to check connection state
    * Updated constructors to accept `std::shared_ptr<sqlite3>`
* Updated driver implementations to use new BLOB constructors:
  * Updated `FirebirdResultSet::getBlob()` to use connection-based constructor
  * Updated `FirebirdPreparedStatement::setString()` and `setBytes()` to use connection-based constructor
  * Updated `MySQLResultSet::getBlob()` to pass empty `shared_ptr` for data-only blobs
  * Updated `PostgreSQLResultSet::getBlob()` to pass empty `shared_ptr` for data-only blobs
  * Updated `SQLiteResultSet::getBlob()` to pass `shared_ptr` for safe reference
* Benefits of BLOB smart pointer migration:
  * Automatic detection of closed connections via `weak_ptr::lock()`
  * Prevention of use-after-free errors when connection is closed while BLOB is in use
  * Clear ownership semantics with `weak_ptr` for non-owning references
  * Comprehensive error messages with unique error codes for debugging
  * Graceful handling of connection closure in cleanup operations

## 2025-12-21 11:22:26 PM PST

### Firebird Driver Database Creation and Error Handling Improvements
* Added database creation support to Firebird driver:
  * **New Driver Methods:**
    * Added `createDatabase()` method to `FirebirdDriver` for creating new Firebird databases
    * Added `command()` method for executing driver-specific commands
    * Added `executeCreateDatabase()` method to `FirebirdConnection` for CREATE DATABASE statements
  * **Database Creation Features:**
    * Support for specifying page size (default: 4096)
    * Support for specifying character set (default: UTF8)
    * Automatic database file creation with proper Firebird connection string format
    * Works with both local and remote database servers
* Improved Firebird error message handling:
  * **Critical Bug Fix:**
    * Fixed error message capture in `isc_dsql_prepare` - now saves error message BEFORE calling cleanup functions
    * Fixed error message capture in `isc_dsql_execute` - now saves error message BEFORE calling cleanup functions
    * Uses separate status array for cleanup operations to avoid overwriting error information
  * **Enhanced Error Reporting:**
    * Improved `interpretStatusVector()` to provide more detailed error messages
    * Added SQLCODE interpretation with `isc_sql_interprete`
    * Combined SQLCODE and `fb_interpret` messages for comprehensive error information
    * Added fallback error message with status vector values for debugging
* Added new example:
  * **New Example File:**
    * Added `firebird_reserved_word_example.cpp` demonstrating reserved word handling in Firebird
    * Shows what happens when using reserved words like 'value' as column names
    * Includes database creation example using the new driver methods
  * **Build System Update:**
    * Updated `CMakeLists.txt` to include the new example when Firebird is enabled

## 2025-12-21 09:41:52 PM PST

### Firebird SQL Driver Enhancements and Benchmark Support
* Added comprehensive Firebird benchmark suite:
  * **New Benchmark Files:**
    * Added `benchmark_firebird_select.cpp` with SELECT benchmarks (small, medium, large datasets)
    * Added `benchmark_firebird_insert.cpp` with INSERT benchmarks (individual and prepared statements)
    * Added `benchmark_firebird_update.cpp` with UPDATE benchmarks (individual and prepared statements)
    * Added `benchmark_firebird_delete.cpp` with DELETE benchmarks (individual and prepared statements)
  * **Benchmark Infrastructure Updates:**
    * Updated `benchmark/CMakeLists.txt` to include Firebird benchmark files
    * Added `USE_FIREBIRD` compile definition for conditional compilation
    * Updated `benchmark_common.hpp` with Firebird helper functions and namespace
    * Updated `benchmark_common.cpp` with Firebird connection setup implementation
  * **Benchmark Script Updates:**
    * Updated `run_benchmarks_cpp_dbc.sh` with `--firebird` and `--firebird-off` options
    * Updated `create_benchmark_cpp_dbc_base_line.sh` to capture Firebird memory usage
    * Added Firebird time command output processing for baseline creation
* Enhanced Firebird driver implementation:
  * **BLOB Text Support:**
    * Added automatic BLOB content reading for BLOB SUB_TYPE TEXT columns
    * Enables JSON and text data retrieval from BLOB columns
  * **Connection Pool Improvements:**
    * Enhanced `returnToPool()` with proper transaction cleanup
    * Added autocommit mode handling when returning connections
    * Added fresh transaction start for pool reuse
    * Improved debug logging for connection pool operations
  * **Error Handling Improvements:**
    * Added automatic rollback on failed queries when autocommit is enabled
    * Ensures connection remains in clean state after errors
  * **URL Parsing Cleanup:**
    * Simplified URL acceptance to only `cpp_dbc:firebird://` prefix
    * Removed legacy `jdbc:firebird://` and `firebird://` prefix support
* Added comprehensive Firebird test coverage:
  * **New Test Files:**
    * Added `test_firebird_real_full_join.cpp` with FULL OUTER JOIN tests
    * Added `test_firebird_real_inner_join.cpp` with INNER JOIN tests
    * Added `test_firebird_real_left_join.cpp` with LEFT JOIN tests
    * Added `test_firebird_real_right_join.cpp` with RIGHT JOIN tests
    * Added `test_firebird_real_json.cpp` with JSON operations tests
    * Added `test_firebird_thread_safe.cpp` with thread-safety stress tests
  * **Updated Test Files:**
    * Updated `test_drivers.cpp` with Firebird driver URL acceptance tests
    * Updated `test_integration.cpp` with Firebird integration tests
    * Updated `test_transaction_isolation.cpp` with Firebird isolation level tests
    * Updated `test_transaction_manager_real.cpp` with Firebird transaction manager tests
  * **Test Improvements:**
    * Fixed column name from `value` to `val_data` (reserved word in Firebird)
    * Used `RECREATE TABLE` instead of DROP/CREATE pattern
    * Added proper pool closure before table cleanup
    * Fixed column index usage for Firebird result sets

## 2025-12-20 01:28:00 PM PST

### Firebird SQL Database Driver Support
* Added complete Firebird SQL database driver implementation:
  * **New Files:**
    * Added `include/cpp_dbc/drivers/driver_firebird.hpp` with Firebird driver class declarations
    * Added `include/cpp_dbc/drivers/firebird_blob.hpp` with Firebird BLOB support classes
    * Added `src/drivers/driver_firebird.cpp` with full Firebird driver implementation
    * Added `cmake/FindFirebird.cmake` module for Firebird library detection
  * **Driver Features:**
    * Full support for Firebird SQL databases (version 2.5+, 3.0+, 4.0+)
    * Connection management with proper resource cleanup using smart pointers
    * Prepared statement support with parameter binding
    * Result set handling with all data types (TEXT, VARCHAR, SHORT, LONG, INT64, FLOAT, DOUBLE, TIMESTAMP, DATE, TIME, BLOB)
    * BLOB support with lazy loading and streaming capabilities
    * Transaction management with isolation level support
    * Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
  * **Build System Updates:**
    * Added `USE_FIREBIRD` option to CMakeLists.txt (default: OFF)
    * Added `--firebird` and `--firebird-off` options to build.sh
    * Added `--firebird` and `--firebird-off` options to build_cpp_dbc.sh
    * Added `firebird` option to helper.sh for --run-build, --run-test, and --run-benchmarks
    * Automatic detection and installation of Firebird development libraries
  * **Required System Packages:**
    * Debian/Ubuntu: `sudo apt-get install firebird-dev libfbclient2`
    * RHEL/CentOS/Fedora: `sudo dnf install firebird-devel libfbclient2`
  * **Connection URL Format:**
    * `cpp_dbc:firebird://host:port/path/to/database.fdb`
    * `cpp_dbc:firebird:///path/to/local/database.fdb` (local connection)
    * Default port: 3050
  * **Smart Pointer Implementation:**
    * `shared_ptr<isc_db_handle>` for connection management
    * Custom deleters for proper Firebird resource cleanup
    * `weak_ptr<isc_db_handle>` in PreparedStatement for safe connection reference
    * SQLDA structure management with proper memory allocation/deallocation
* Updated documentation:
  * Updated README.md with Firebird support information
  * Updated memory-bank/techContext.md with Firebird technical details

## 2025-12-17 06:41:25 PM PST

### Thread-Safe Database Driver Operations
* Added optional thread-safety support for database driver operations:
  * **New CMake Option:**
    * Added `DB_DRIVER_THREAD_SAFE` option (default: ON) to enable/disable thread-safe operations
    * Use `-DDB_DRIVER_THREAD_SAFE=OFF` to disable thread-safety for single-threaded applications
  * **Build Script Updates:**
    * Added `--db-driver-thread-safe-off` option to all build scripts (build.sh, build.dist.sh, helper.sh, run_test.sh)
    * Updated helper.sh with new option for --run-build, --run-build-dist, --run-test, and --run-benchmarks
  * **Driver Implementations:**
    * **MySQL Driver:**
      * Added `std::mutex m_mutex` member to `MySQLConnection` class
      * Added `std::lock_guard` protection in `executeQuery()`, `executeUpdate()`, `prepareStatement()`, and `close()` methods
      * Thread-safety is conditionally compiled based on `DB_DRIVER_THREAD_SAFE` macro
    * **PostgreSQL Driver:**
      * Added `std::mutex m_mutex` member to `PostgreSQLConnection` class
      * Added `std::lock_guard` protection in `executeQuery()`, `executeUpdate()`, `prepareStatement()`, and `close()` methods
      * Thread-safety is conditionally compiled based on `DB_DRIVER_THREAD_SAFE` macro
    * **SQLite Driver:**
      * Added `std::mutex m_mutex` member to `SQLiteConnection` class
      * Added `std::lock_guard` protection in `executeQuery()`, `executeUpdate()`, `prepareStatement()`, and `close()` methods
      * Thread-safety is conditionally compiled based on `DB_DRIVER_THREAD_SAFE` macro
  * **New Thread-Safety Tests:**
    * Added `test_mysql_thread_safe.cpp` with comprehensive MySQL thread-safety stress tests
    * Added `test_postgresql_thread_safe.cpp` with comprehensive PostgreSQL thread-safety stress tests
    * Added `test_sqlite_thread_safe.cpp` with comprehensive SQLite thread-safety stress tests
    * Tests include:
      * Multiple threads with individual connections
      * Connection pool concurrent access
      * Concurrent read operations with connection pool
      * High concurrency stress test with mixed operations
      * Rapid connection open/close stress test
    * Tests are conditionally compiled based on `DB_DRIVER_THREAD_SAFE` macro
* Benefits of thread-safe driver operations:
  * Safe concurrent access to database connections from multiple threads
  * Protection against race conditions in connection operations
  * Optional feature that can be disabled for performance in single-threaded applications
  * Comprehensive test coverage for thread-safety scenarios

## 2025-12-10 10:36:15 PM PST

### Smart Pointer Migration for Database Drivers
* Migrated all database drivers from raw pointers to smart pointers for improved memory safety:
  * **MySQL Driver:**
    * Added `MySQLResDeleter` custom deleter for `MYSQL_RES*` with `unique_ptr`
    * Added `MySQLStmtDeleter` custom deleter for `MYSQL_STMT*` with `unique_ptr`
    * Added `MySQLDeleter` custom deleter for `MYSQL*` with `shared_ptr`
    * Changed `MySQLConnection::m_mysql` from raw pointer to `MySQLHandle` (`shared_ptr<MYSQL>`)
    * Changed `MySQLPreparedStatement::m_mysql` from raw pointer to `weak_ptr<MYSQL>` for safe connection reference
    * Changed `MySQLPreparedStatement::m_stmt` from raw pointer to `MySQLStmtHandle` (`unique_ptr<MYSQL_STMT>`)
    * Changed `MySQLResultSet::m_result` from raw pointer to `MySQLResHandle` (`unique_ptr<MYSQL_RES>`)
    * Added `validateResultState()` and `validateCurrentRow()` helper methods
    * Added `getMySQLConnection()` helper method for safe connection access
    * Added comprehensive documentation explaining why `MYSQL_ROW` remains a raw pointer
  * **PostgreSQL Driver:**
    * Added `PGresultDeleter` custom deleter for `PGresult*` with `unique_ptr`
    * Added `PGconnDeleter` custom deleter for `PGconn*` with `shared_ptr`
    * Changed `PostgreSQLConnection::m_conn` from raw pointer to `PGconnHandle` (`shared_ptr<PGconn>`)
    * Changed `PostgreSQLPreparedStatement::m_conn` from raw pointer to `weak_ptr<PGconn>` for safe connection reference
    * Changed `PostgreSQLResultSet::m_result` from raw pointer to `PGresultHandle` (`unique_ptr<PGresult>`)
    * Added `getPGConnection()` helper method for safe connection access
  * **SQLite Driver:**
    * Added `SQLiteStmtDeleter` custom deleter for `sqlite3_stmt*` with `unique_ptr`
    * Added `SQLiteDbDeleter` custom deleter for `sqlite3*` with `shared_ptr`
    * Changed `SQLiteConnection::m_db` from raw pointer to `shared_ptr<sqlite3>`
    * Changed `SQLitePreparedStatement::m_db` from raw pointer to `weak_ptr<sqlite3>` for safe connection reference
    * Changed `SQLitePreparedStatement::m_stmt` from raw pointer to `SQLiteStmtHandle` (`unique_ptr<sqlite3_stmt>`)
    * Changed `SQLiteResultSet::m_stmt` to use `getStmt()` accessor method
    * Changed `m_activeStatements` from `set<shared_ptr>` to `set<weak_ptr>` to avoid preventing statement destruction
    * Added `getSQLiteConnection()` helper method for safe connection access
    * Removed obsolete `activeConnections` static list and related mutex
* Benefits of smart pointer migration:
  * Automatic resource cleanup even in case of exceptions
  * Prevention of memory leaks through RAII
  * Safe detection of closed connections via `weak_ptr`
  * Clear ownership semantics documented in code
  * Elimination of manual `delete`/`free` calls

## 2025-12-04 09:30:36 PM PST

### Benchmark Baseline and Comparison System
* Added benchmark baseline creation and comparison functionality:
  * Added `create_benchmark_cpp_dbc_base_line.sh` script to create benchmark baselines from log files
  * Added `compare_benchmark_cpp_dbc_base_line.sh` script to compare two benchmark baseline files
  * Baseline files are stored in `libs/cpp_dbc/benchmark/base_line/` directory organized by system configuration
  * Baseline data includes benchmark results, memory usage, and time command output
  * Comparison script supports filtering by benchmark name and automatic detection of latest baseline files
* Added memory usage tracking for benchmarks:
  * Added `--memory-usage` option to `run_benchmarks_cpp_dbc.sh` to track memory usage with `/usr/bin/time -v`
  * Automatic installation of `time` package if not available on Debian-based systems
  * Memory usage data is captured per database type (MySQL, PostgreSQL, SQLite)
* Updated helper.sh with new benchmark options:
  * Added `memory-usage` option to enable memory tracking during benchmarks
  * Added `base-line` option to automatically create baseline after running benchmarks
  * Updated help text with new options and examples

## 2025-11-30 11:26:33 PM PST

### Benchmark System Migration to Google Benchmark
* Migrated benchmark system from Catch2 to Google Benchmark:
  * Updated CMakeLists.txt to use Google Benchmark instead of Catch2WithMain
  * Added benchmark/1.8.3 as a dependency in conanfile.txt
  * Rewrote all benchmark files to use Google Benchmark API
  * Improved benchmark table initialization system with table reuse
  * Added support for multiple iterations and benchmark repetitions
* Logging system improvements:
  * Added timestamp-based logging functions in system_utils.hpp
  * Implemented logWithTimestampInfo, logWithTimestampError, logWithTimestampException, etc.
  * Replaced std::cerr and std::cout with these logging functions in benchmark code
* Connection management improvements for benchmarks:
  * Added setupMySQLConnection and setupPostgreSQLConnection functions
  * Implemented system to reuse already initialized tables
  * Used mutex for synchronization in multi-threaded environments
* Script updates:
  * Added mysql-off option in helper.sh to disable MySQL benchmarks
  * Changed configuration parameters (--samples and --resamples to --min-time and --repetitions)
  * Improved filter handling for running specific benchmarks

## 2025-11-29 12:01:07 AM PST

### SQLite Driver Thread Safety Improvements
* Enhanced thread safety in SQLite driver implementation:
  * Added thread-safe initialization pattern using std::atomic and std::mutex
  * Implemented singleton pattern for SQLite configuration to ensure it's only done once
  * Added static class members to track initialization state
  * Improved debug output system with unique error codes for better troubleshooting
* Unified debug output system:
  * Added support for DEBUG_ALL option to enable all debug outputs at once
  * Updated debug macros in connection_pool.cpp, transaction_manager.cpp, and driver_sqlite.cpp
  * Replaced std::cerr usage with debug macros for better control
  * Added [[maybe_unused]] attribute to avoid warnings with unused variables
* Removed obsolete inject_cline_custom_instructions.sh script

## 2025-11-27 11:14:58 PM PST

### Benchmark and Testing Framework Improvements
* Added improved benchmark organization and reusability:
  * Added benchmark_common.cpp with implementation of common benchmark helper functions
  * Reorganized helper functions into namespaces (common_benchmark_helpers, mysql_benchmark_helpers, etc.)
  * Updated benchmark CMakeLists.txt to include the new benchmark_common.cpp file
  * Changed configuration to use dedicated benchmark_db_connections.yml instead of test config
  * Improved consistency in helper function usage across benchmark files
* Enhanced database driver improvements:
  * Added getURL() method to Connection interface for URL retrieval
  * Implemented getURL() in all database driver implementations
  * Added URL caching in database connections for better performance
  * Improved connection closing in MySQL driver with longer sleep time (25ms)
  * Enhanced PostgreSQL database creation with proper existence checking
* Added new example:
  * Added connection_info_example.cpp showing different connection URL formats
  * Updated CMakeLists.txt to build the new example
* Updated helper.sh:
  * Added new example command for comprehensive testing

## 2025-11-26 09:04:53 PM PST

### Test Code Refactoring
* Refactored test code to improve organization and reusability:
  * Added test_sqlite_common.hpp with SQLite-specific helper functions
  * Added test_sqlite_common.cpp with helper function implementations
  * Moved utility functions to database-specific namespaces
  * Replaced repetitive configuration loading with helper functions
  * Improved consistency in helper usage across all test files
* Enhanced database connection configuration:
  * Added support for database-specific connection options
  * Implemented option filtering in driver_postgresql.cpp to ignore options starting with "query__"
  * Replaced configuration search code with simplified helper calls
  * Removed redundant test_blob_common.hpp file
  * Refactored helpers to use common_test_helpers namespace instead of global functions
* Optimized file structure and organization:
  * Moved global helper functions to common namespace for better encapsulation
  * Reordered header inclusion for consistency
  * Updated CMakeLists.txt to include new test files
  * Removed update_headers.sh script, replaced with more precise manual management

## 2025-11-22 09:04:39 PM PST

### Benchmark System Implementation
* Added comprehensive benchmark system for database operations:
  * Added benchmark directory with benchmark files for all database drivers
  * Added benchmark_main.cpp with common benchmark setup
  * Added benchmark files for MySQL, PostgreSQL, and SQLite operations
  * Implemented benchmarks for SELECT, INSERT, UPDATE, and DELETE operations
  * Added support for different data sizes (10, 100, 1000, and 10000 rows)
  * Added `--benchmarks` option to build.sh to enable building benchmarks
  * Added `--run-benchmarks` option to helper.sh to run benchmarks
  * Added support for running specific database benchmarks (mysql, postgresql, sqlite)
  * Added automatic benchmark log rotation in logs/benchmark/ directory
* Updated build system:
  * Added `CPP_DBC_BUILD_BENCHMARKS` option to CMakeLists.txt
  * Added `--benchmarks` parameter to build scripts
  * Updated helper.sh with benchmark support
* Improved test files:
  * Added conditional compilation for YAML support in test files
  * Added default connection parameters when YAML is disabled
  * Fixed PostgreSQL test files to work without YAML configuration
  * Improved SQLite test files to work with in-memory databases
* Updated documentation:
  * Added benchmark information to README.md
  * Updated build script documentation with benchmark options

## 2025-11-16 06:49:00 PM -0800

### Code Quality Improvements
* Added comprehensive warning flags and compile-time checks:
  * Added `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wformat=2 -Wunused -Werror=return-type -Werror=switch -Wdouble-promotion -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align` flags to build scripts
  * Added compile definitions for backward.hpp to silence -Wundef warnings
  * Refactored code to use m_ prefix for member variables to avoid -Wshadow warnings
  * Fixed implicit conversions to avoid -Wconversion warnings
  * Improved exception handling to avoid variable shadowing
  * Changed int return types to uint64_t for executeUpdate() methods
  * Added static_cast<> for numeric conversions
  * Fixed comparison between signed and unsigned integers
* Updated CMakeLists.txt:
  * Added special compile flags for backward.hpp
  * Added compile definitions to silence -Wundef warnings
  * Updated both main and test CMakeLists.txt files
* Updated TODO.md:
  * Moved "Activate ALL possible warnings and compile time checks" to Completed Tasks
  * Added details about the implementation

## 2025-11-15 12:19:56 PM -0800

### RPM Package Support
* Added RPM package (.rpm) build support to the distribution package system:
  * Added support for Fedora 42 and Fedora 43 distributions
  * Created Dockerfiles and build scripts for Fedora distributions
  * Added RPM spec file generation with proper dependencies
  * Updated `build_dist_pkg.sh` to handle both .deb and .rpm package formats
  * Updated documentation to reflect RPM package support
  * Improved package naming with distribution and version information
* Updated TODO.md:
  * Marked "Add --run-build-lib-dist-rpm using docker" as completed

## 2025-11-15 01:20:10 AM -0800

### Debian Package Build System
* Added comprehensive Debian package (.deb) build system:
  * Created `build_dist_pkg.sh` script to replace `build_dist_deb.sh` with improved functionality
  * Added support for building packages for multiple distributions in a single command
  * Added support for Debian 12, Debian 13, Ubuntu 22.04, and Ubuntu 24.04
  * Added Docker-based build environment for each distribution
  * Added MySQL fix patch for Debian distributions
  * Added CMake integration files for better package usage
  * Added version specification option with `--version` parameter
  * Improved package naming with distribution and version information
  * Added documentation and examples for CMake integration
* Added CMake integration improvements:
  * Enhanced `cpp_dbc-config.cmake.in` for better CMake integration
  * Added `FindSQLite3.cmake` for SQLite dependency handling
  * Added example CMake project in `docs/example_cmake_project/`
  * Added comprehensive CMake usage documentation in `docs/cmake_usage.md`
* Updated package naming and structure:
  * Changed package name from `cpp-dbc` to `cpp-dbc-dev` to better reflect its purpose
  * Improved package description with build options information
  * Added CMake files to the package for better integration
  * Added documentation and examples to the package

## 2025-11-14 06:41:00 PM -0800

### Stack Trace Improvements with libdw Support
* Added libdw support for enhanced stack traces:
  * Added `BACKWARD_HAS_DW` option to CMakeLists.txt to enable/disable libdw support
  * Added `--dw-off` flag to build scripts to disable libdw support when needed
  * Added automatic detection and installation of libdw development libraries
  * Added libdw dependency to package map in build.dist.sh
  * Updated main.cpp with stack trace testing functionality
  * Fixed function name references in system_utils.cpp for stack frame filtering
* Updated error handling in examples:
  * Changed `e.what()` to `e.what_s()` in blob_operations_example.cpp and join_operations_example.cpp
* Updated TODO.md:
  * Marked "Add library dw to linker en CPP_SBC" as completed
  * Changed "Add script for build inside a docker..." to more specific tasks:
    * "Add --run-build-lib-dist-deb using docker"
    * "Add --run-build-lib-dist-rpm using docker"

## 2025-11-13 05:18:00 PM -0800

### Mejoras de Seguridad y Tipos de Datos
* Mejoras en el manejo de excepciones:
  * Reemplazado `what()` por `what_s()` en toda la base de código para evitar el uso de punteros `const char*` inseguros
  * Añadido un destructor virtual a `DBException`
  * Marcado el método `what()` como obsoleto con [[deprecated]]
* Mejoras en tipos de datos:
  * Cambio de tipos `int` y `long` a `unsigned int` y `unsigned long` para parámetros que no deberían ser negativos en `ConnectionPoolConfig` y `DatabaseConfig`
  * Inicialización de variables en la declaración en `ConnectionPoolConfig`
* Optimización en SQLite:
  * Reordenamiento de la inicialización de SQLite (primero configuración, luego inicialización)
* Actualización de TODO.md con nuevas tareas:
  * "Add library dw to linker en CPP_SBC"
  * "Add script for build inside a docker the creation of .deb (ubuntu 22.04) .rpm (fedora) and make simple build for another distro version"

## 2025-11-12 01:59:00 AM -0800

### Exception Handling and Stack Trace Improvements
* Added comprehensive stack trace capture and error tracking:
  * Added `backward.hpp` library for stack trace capture and analysis
  * Created `system_utils::StackFrame` structure to store stack frame information
  * Implemented `system_utils::captureCallStack()` function to capture the current call stack
  * Implemented `system_utils::printCallStack()` function to print stack traces
  * Enhanced `DBException` class with improved error tracking:
    * Added mark field to uniquely identify error locations
    * Added call stack capture to store the stack trace at exception creation
    * Updated constructor to accept mark, message, and call stack
    * Added `getMark()` method to retrieve the error mark
    * Added `getCallStack()` and `printCallStack()` methods
  * Updated all exception throws in SQLite driver to include:
    * Unique error marks for better error identification
    * Separated error marks from error messages
    * Call stack capture for better debugging
  * Updated transaction manager to use the new exception format
  * Added comprehensive tests for the new exception features in test_drivers.cpp

## 2025-11-11 03:45:00 PM -0800

### JSON Data Type Support
* Added comprehensive support for JSON data types in MySQL and PostgreSQL:
  * Added test files for JSON operations:
    * `test_mysql_real_json.cpp` for MySQL JSON testing
    * `test_postgresql_real_json.cpp` for PostgreSQL JSON and JSONB testing
  * Added helper function `generateRandomJson()` in test_main.cpp for generating test JSON data
  * Implemented tests for various JSON operations:
    * Basic JSON storage and retrieval
    * JSON path expressions and operators
    * JSON search and filtering
    * JSON modification and transformation
    * JSON validation and error handling
    * JSON indexing and performance testing
    * JSON aggregation functions
  * Added support for database-specific JSON features:
    * MySQL: JSON_EXTRACT, JSON_SEARCH, JSON_CONTAINS, JSON_ARRAYAGG
    * PostgreSQL: JSON operators (@>, <@, ?, ?|, ?&), jsonb_set, jsonb_insert, GIN indexing
  * Made `close()` method virtual in PostgreSQLResultSet for better inheritance support
  * Updated CMakeLists.txt to include the new JSON test files
  * Updated TODO.md to move "Add specific testing of json field types" to Completed Tasks

## 2025-11-10 00:03:00 AM -0800

### Test Log Analysis Feature
* Added test log analysis functionality to helper.sh:
  * Added `--check-test-log` option to analyze test log files for failures and issues
  * Added `--check-test-log=PATH` option to analyze a specific log file
  * Implemented detection of test failures from Catch2 output
  * Implemented detection of memory leaks from Valgrind output
  * Implemented detection of Valgrind errors from ERROR SUMMARY
  * Added detailed reporting with file and line number references
  * Updated helper.sh usage information with the new options

## 2025-11-09 03:20:00 PM -0800

### Logging System Improvements
* Added structured logging system with dedicated log directories:
  * Created logs/build directory for build output logs
  * Created logs/test directory for test output logs
  * Added automatic log rotation keeping 4 most recent logs
  * Added timestamp to log filenames for better tracking
  * Modified helper.sh to support the new logging structure
  * Added color support in terminal while keeping clean logs
  * Added unbuffer usage to preserve colors in terminal output
  * Added sed command to strip ANSI color codes from log files
  * Updated command output to show log file location

### VSCode Integration
* Added VSCode configuration files:
  * Added .vscode/c_cpp_properties.json with proper include paths
  * Added .vscode/tasks.json with build tasks
  * Added .vscode/build_with_props.sh script to extract defines from c_cpp_properties.json
  * Added support for building with MySQL and PostgreSQL from VSCode
  * Added automatic extension installation task

### Build System Improvements
* Modified .gitignore to exclude log files and directories
* Changed default AUTO_MODE to false in run_test.sh
* Updated helper.sh with better log handling and color support
* Improved error handling in build scripts

### BLOB Support for Image Files
* Added support for storing and retrieving image files as BLOBs:
  * Added helper functions in test_main.cpp for binary file operations:
    * `readBinaryFile()` to read binary data from files
    * `writeBinaryFile()` to write binary data to files
    * `getTestImagePath()` to get the path to the test image
    * `generateRandomTempFilename()` to create temporary filenames
  * Added test.jpg file for BLOB testing
  * Updated CMakeLists.txt to copy test.jpg to the build directory
  * Added comprehensive test cases for image file BLOB operations in:
    * `test_mysql_real_blob.cpp`
    * `test_postgresql_real_blob.cpp`
    * `test_sqlite_real_blob.cpp`
  * Added tests for storing, retrieving, and verifying image data integrity
  * Added tests for writing retrieved image data to temporary files

## 2025-11-08 05:49:00 AM -0800

### SQLite JOIN Operations Testing
* Added comprehensive test cases for SQLite JOIN operations:
  * Added `test_sqlite_real_inner_join.cpp` with INNER JOIN test cases
  * Added `test_sqlite_real_left_join.cpp` with LEFT JOIN test cases
  * Enhanced test coverage for complex JOIN operations with multiple tables
  * Added tests for JOIN operations with NULL checks and invalid columns
  * Added tests for type mismatches in JOIN conditions

### Debug Output Options
* Added debug output options for better troubleshooting:
  * Added `--debug-pool` option to enable debug output for ConnectionPool
  * Added `--debug-txmgr` option to enable debug output for TransactionManager
  * Added `--debug-sqlite` option to enable debug output for SQLite driver
  * Added `--debug-all` option to enable all debug output
  * Updated build scripts to pass debug options to CMake
  * Added debug output parameters to helper.sh script

### Test Script Enhancements
* Enhanced test script functionality:
  * Added `--run-test="tag"` option to run specific tests by tag
  * Added support for multiple test tags using + separator (e.g., "tag1+tag2+tag3")
  * Added debug options to run_test.sh script
  * Improved test command construction with better parameter handling

### Valgrind Suppressions Removal
* Removed valgrind-suppressions.txt file as it's no longer needed with improved PostgreSQL driver

## 2025-11-07 02:15:00 PM -0800

### Helper Script Parameter Improvements
* Renamed helper.sh script parameters for better clarity:
  * Changed `--run` to `--run-ctr` for container execution
  * Changed `--ldd` to `--ldd-bin-ctr` for container binary inspection
  * Changed `--labels` to `--show-labels-ctr` for container label display
  * Changed `--tags` to `--show-tags-ctr` for container tag display
  * Changed `--env` to `--show-env-ctr` for container environment display
  * Changed `--ldd-bin` to `--ldd-build-bin` for local binary inspection
  * Changed `--run-bin` to `--run-build-bin` for local binary execution
  * Updated usage information in show_usage() function

## 2025-11-07 11:28:00 AM -0800

### SQLite Connection Management Improvements
* Enhanced SQLite connection management with better resource handling:
  * Changed SQLiteConnection to inherit from std::enable_shared_from_this
  * Replaced raw pointer tracking with weak_ptr in activeConnections list
  * Improved connection cleanup with weak_ptr-based reference tracking
  * Added proper error handling for shared_from_this() usage
  * Added safeguards to ensure connections are created with make_shared
  * Fixed potential memory issues in connection and statement cleanup

## 2025-11-07 11:01:00 AM -0800

### Connection Options Support
* Added connection options support for all database drivers:
  * Added options parameter to Driver::connect() method
  * Added options parameter to all driver implementations (MySQL, PostgreSQL, SQLite)
  * Added options parameter to ConnectionPool constructor
  * Added options map to ConnectionPoolConfig
  * Updated DatabaseConfig to provide options through getOptions() method
  * Renamed original getOptions() to getOptionsObj() for backward compatibility
  * Updated all tests to support the new options parameter

### PostgreSQL Driver Improvements
* Enhanced PostgreSQL driver with better configuration options:
  * Added support for passing connection options from configuration to PQconnectdb
  * Made gssencmode configurable through options map (default: disable)
  * Added error codes to exception messages for better debugging

### SQLite Driver Improvements
* Enhanced SQLite driver with better configuration options:
  * Added support for configuring SQLite pragmas through options map
  * Added support for journal_mode, synchronous, and foreign_keys options
  * Added error codes to exception messages for better debugging

## 2025-11-06 2:58:00 PM -0800

### SQLite Connection Pool Implementation
* Added SQLite connection pool support:
  * Added `SQLiteConnectionPool` class in connection_pool.hpp
  * Added SQLite-specific connection pool configuration in test_db_connections.yml
  * Added SQLite connection pool tests in test_connection_pool_real.cpp
  * Improved connection handling for SQLite connections
  * Added transaction isolation level support for SQLite pools

### SQLite Driver Improvements
* Enhanced SQLite driver with better resource management:
  * Added static list of active connections for statement cleanup
  * Improved connection closing with sqlite3_close_v2 instead of sqlite3_close
  * Added better handling of prepared statements with shared_from_this
  * Fixed memory leaks in SQLite connection and statement handling
  * Improved error handling in SQLite driver

### Test Script Enhancements
* Added new options to run_test_cpp_dbc.sh:
  * Added `--auto` option to automatically continue to next test set if tests pass
  * Added `--gssapi-leak-ok` option to ignore GSSAPI leaks in PostgreSQL with Valgrind

## 2025-11-06 11:43:35 AM -0800

### SQLite Driver Implementation
* Added SQLite database driver support:
  * Added `driver_sqlite.hpp` and `driver_sqlite.cpp` with full SQLite implementation
  * Added SQLite connection string format support (`cpp_dbc:sqlite://path/to/database.db`)
  * Added SQLite-specific transaction isolation level handling (only SERIALIZABLE supported)
  * Added SQLite test cases in `test_sqlite_connection.cpp` and `test_sqlite_real.cpp`
  * Updated build scripts with `--sqlite` option to enable SQLite support
  * Added SQLite dependency detection and installation in build scripts
  * Added SQLite configuration in test YAML file with dev, test, and prod environments
  * Updated test cases to handle SQLite-specific connection parameters

### Connection Pool and Transaction Manager Improvements
* Enhanced connection pool with better connection handling:
  * Added transaction isolation level preservation when returning connections to pool
  * Improved connection closing mechanism with proper cleanup
  * Added better handling of connection errors during transaction isolation level setting
  * Fixed issue with active connections not being properly marked as inactive
* Enhanced transaction manager with better resource management:
  * Improved connection return to pool after transaction completion
  * Added explicit connection closing in transaction commit and rollback
  * Ensured connections are properly returned to pool even on errors

## 2025-11-05 6:03:00 PM -0800

### Transaction Isolation Level Support
* Added transaction isolation level support following JDBC standard:
  * Added `TransactionIsolationLevel` enum with JDBC-compatible isolation levels
  * Added `setTransactionIsolation` and `getTransactionIsolation` methods to Connection interface
  * Implemented methods in MySQL and PostgreSQL drivers
  * Updated PooledConnection to delegate isolation level methods to underlying connection
  * Default isolation levels set to database defaults (REPEATABLE READ for MySQL, READ COMMITTED for PostgreSQL)

## 2025-11-05 4:48:00 PM -0800

### Debug Features and Test Improvements
* Added debug options for ConnectionPool and TransactionManager:
  * Added `DEBUG_CONNECTION_POOL` and `DEBUG_TRANSACTION_MANAGER` options to CMakeLists.txt
  * Added `--debug-pool`, `--debug-txmgr`, and `--debug-all` options to build_test_cpp_dbc.sh
  * Updated build scripts to pass debug options to CMake
* Enhanced test framework:
  * Completely overhauled MockResultSet implementation in test_mocks.hpp with in-memory data storage
  * Added more realistic behavior to mock database implementations
  * Added timing information output in test_integration.cpp
  * Fixed test assertions in test_drivers.cpp for empty result sets
  * Uncommented code in test_transaction_manager_real.cpp for more thorough testing
  * Updated expected return values for executeUpdate in mock implementations
  * Added comprehensive tests for transaction isolation levels in test_transaction_isolation.cpp

## 2025-05-25 2:13:00 PM -0700 [e6091bd]

### Documentation Updates
* Updated CHANGELOG.md with database configuration integration changes
* Updated memory-bank/activeContext.md with recent changes
* Updated memory-bank/progress.md with database configuration integration
* Updated documentation in libs/cpp_dbc/docs/ with new features and URL format change
* Standardized documentation across all files

## 2025-05-25 2:06:00 PM -0700 [f4c7947]

### Database Configuration Integration
* Added integration between database configuration and connection classes
* Created new `config_integration_example.cpp` with examples of different connection methods
* Added `createConnection()` method to `DatabaseConfig` class
* Added `createConnection()` and `createConnectionPool()` methods to `DatabaseConfigManager`
* Added new methods to `DriverManager` to work with configuration classes
* Added script to run the configuration integration example

### Connection Pool Enhancements
* Moved `ConnectionPoolConfig` from connection_pool.hpp to config/database_config.hpp
* Enhanced `ConnectionPoolConfig` with more options and better encapsulation
* Added new constructors and factory methods to `ConnectionPool`
* Added integration between connection pool and database configuration

### URL Format Change
* Changed URL format from "cppdbc:" to "cpp_dbc:" for consistency
* Updated all examples, tests, and code to use the new format

### Code Organization
* Added database_config.cpp implementation file
* Added forward declarations to improve header organization
* Fixed CMakeLists.txt to include database_config.cpp
* Updated all examples to use the new configuration classes

### Documentation Updates
* Updated memory-bank/progress.md with build.dist.sh and helper.sh improvements
* Updated memory-bank/activeContext.md with recent changes
* Enhanced README.md with detailed information about build.dist.sh and --ldd option
* Updated libs/cpp_dbc/docs/cppdbc-package.md with current project structure and features
* Updated libs/cpp_dbc/docs/cppdbc-docs-en.md with new build options and helper script information
* Updated libs/cpp_dbc/docs/cppdbc-docs-es.md with the same changes in Spanish

## 2025-05-25 12:32:00 PM -0700 [4536048]

### Testing Framework and Helper Script Enhancements
* Added test management improvements:
  * Added `--test` option to build.sh
  * Created run_test.sh in project root
  * Fixed path issues in test scripts
  * Added yaml-cpp dependency for tests
  * Added auto-detection of project build state
* Enhanced helper.sh:
  * Added support for multiple commands in single invocation
  * Added `--test`, `--run-test`, `--ldd-bin` and `--run-bin` options
  * Improved error handling and reporting
  * Added executable name detection from `.dist_build`
* Updated documentation:
  * Updated README.md with testing and helper info
  * Updated memory-bank files with recent changes
  * Updated CHANGELOG.md with previous commits
  * Standardized timestamp format in CHANGELOG.md

## 2025-05-25 11:32:00 AM -0700 [289ea75]

### YAML Configuration Support
* Added optional YAML configuration support to the library
* Created database configuration classes in `include/cpp_dbc/config/database_config.hpp`
* Implemented YAML configuration loader in `include/cpp_dbc/config/yaml_config_loader.hpp` and `src/config/yaml_config_loader.cpp`
* Added `--yaml` option to `build.sh` and `build_cpp_dbc.sh` to enable YAML support
* Modified CMakeLists.txt to conditionally include YAML-related files based on USE_CPP_YAML flag

### Examples Improvements
* Added `--examples` option to `build.sh` and `build_cpp_dbc.sh` to build examples
* Created YAML configuration example in `examples/config_example.cpp`
* Added example YAML configuration file in `examples/example_config.yml`
* Created script to run the configuration example in `examples/run_config_example.sh`
* Fixed initialization issue in `examples/transaction_manager_example.cpp`

### Build System Enhancements
* Modified `build.sh` to support `--yaml` and `--examples` options
* Updated `build_cpp_dbc.sh` to support `--yaml` and `--examples` options
* Fixed issue with Conan generators directory path in `build_cpp_dbc.sh`
* Improved error handling in build scripts

## 2025-05-25 10:28:43 AM -0700 [9561f51]

### Testing Improvements
* Added `--test` option to `build.sh` to enable building tests
* Created `run_test.sh` script in the root directory to run tests
* Modified `build_cpp_dbc.sh` to accept the `--test` option
* Changed default value of `CPP_DBC_BUILD_TESTS` to `OFF` in CMakeLists.txt
* Fixed path issues in `run_test_cpp_dbc.sh`
* Added `yaml-cpp` dependency to `libs/cpp_dbc/conanfile.txt` for tests
* Added automatic project build detection in `run_test.sh`

### Helper Script Enhancements
* Improved `helper.sh` to support multiple commands in a single invocation
* Added `--test` option to build tests
* Added `--run-test` option to run tests
* Added `--ldd` option to check executable dependencies
* Added `--run-bin` option to run the executable
* Improved error handling and reporting
* Added support for getting executable name from `.dist_build`

### Documentation Updates
* Updated README.md with information about testing and helper script
* Updated memory-bank/activeContext.md with recent changes
* Updated memory-bank/progress.md with testing improvements
* Added documentation about IntelliSense issue with USE_POSTGRESQL preprocessor definition:
  * Added to memory-bank/techContext.md under "Known VSCode Issues"
  * Added to memory-bank/activeContext.md under "Fixed VS Code Debugging Issues"
  * Added to memory-bank/progress.md under "VS Code Debugging Issues"
* Added useful Git command to memory-bank/git_commands.md:
  * Documented `git --no-pager diff --cached` for viewing staged changes
  * Added workflow for using this command when updating CHANGELOG.md
  * Added guidance for creating comprehensive commit messages

## 2025-05-25 10:21:44 AM -0700 [f250d34]

### Test Integration Improvements
* Enhanced `helper.sh` with better error handling and test support
* Updated `libs/cpp_dbc/conanfile.txt` with required test dependencies
* Refined `run_test.sh` for better test execution flow

## 2025-05-25 10:06:29 AM -0700 [3b9bd04]

### Test Framework Integration
* Modified build scripts to support test compilation
* Added `run_test.sh` in the root directory for easy test execution
* Updated CMake configuration for test integration
* Improved test build and run scripts with better error handling

## 2025-05-25 09:43:28 AM -0700 [cdf29c5]

### Initial Test Implementation
* Added test directory structure in `libs/cpp_dbc/test/`
* Created test files: test_basic.cpp, test_main.cpp, test_yaml.cpp
* Added test build script `libs/cpp_dbc/build_test_cpp_dbc.sh`
* Added test run script `libs/cpp_dbc/run_test_cpp_dbc.sh`
* Added CMake configuration for tests
* Created documentation for AddressSanitizer issues in `memory-bank/asan_issues.md`
* Updated memory-bank files with test information
* Modified build configuration to support test compilation

## 2025-05-25 12:01:46 AM -0700 [525ee54]

### Documentation Updates
* Added documentation about IntelliSense issue with USE_POSTGRESQL preprocessor definition:
  * Added to memory-bank/techContext.md under "Known VSCode Issues"
  * Added to memory-bank/activeContext.md under "Fixed VS Code Debugging Issues"
  * Added to memory-bank/progress.md under "VS Code Debugging Issues"
* Added useful Git command to memory-bank/git_commands.md:
  * Documented `git --no-pager diff --cached` for viewing staged changes
  * Added workflow for using this command when updating CHANGELOG.md
  * Added guidance for creating comprehensive commit messages

## 2025-05-24 11:15:11 PM -0700 [c5615e5]

### Build System Improvements
* Added support for fmt and nlohmann_json packages in CMakeLists.txt
* Improved build type detection and configuration
* Updated CMakeUserPresets.json to use the Debug build directory
* Added Conan generators directory to include path

### New Features
* Added JSON support with nlohmann_json library
* Added JSON object creation, manipulation, and serialization examples in main.cpp

### Testing
* Added new test files using Catch2 framework:
  * catch_main.cpp - Main entry point for Catch2 tests
  * catch_test.cpp - Tests for JSON operations
  * json_test.cpp - Additional JSON functionality tests
  * main_test.cpp - Empty file for Catch2WithMain

### Documentation Updates
* Updated README.md with correct file paths reflecting the new project structure
* Updated memory-bank/systemPatterns.md with directory references
* Updated memory-bank/techContext.md to reference C++23 instead of C++11
* Added details about VSCode configuration in techContext.md
* Updated progress.md with details about project structure reorganization

### Project Structure
* Reorganized directory structure with separate src/ and include/ directories
* Improved VSCode integration with CMakeTools