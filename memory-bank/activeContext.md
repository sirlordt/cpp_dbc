# CPP_DBC Active Context

## Important Memory Bank Files

When working with this project, always review these key memory bank files:

1. **projectbrief.md**: Core project overview and goals
2. **techContext.md**: Technical details and environment setup
3. **systemPatterns.md**: Architecture and design patterns
4. **git_commands.md**: Git workflow and common commands
5. **asan_issues.md**: Known issues with AddressSanitizer and workarounds

These files contain critical information for understanding the project structure, development workflow, and known issues.

## Current Work Focus

The current focus appears to be on maintaining and potentially extending the CPP_DBC library. The library provides a C++ database connectivity framework inspired by Java's JDBC, with support for:

1. MySQL, PostgreSQL, SQLite, and Firebird SQL relational databases
2. MongoDB document database
3. ScyllaDB columnar database
4. Redis key-value database
5. Connection pooling for all supported databases (relational, document, columnar, and key-value)
6. Transaction management with isolation levels
7. Prepared statements and result sets
8. BLOB (Binary Large Object) support for all relational database drivers
9. Document database operations (CRUD) for MongoDB
10. Columnar database operations for ScyllaDB
11. YAML configuration for database connections and pools
12. Comprehensive testing for JOIN operations and BLOB handling
13. Debug output options for troubleshooting
14. Benchmark system for database operations performance testing

The code is organized in a modular fashion with clear separation between interfaces and implementations, following object-oriented design principles.

## Recent Changes

Recent changes to the codebase include:

1. **DBException Hybrid Storage, Build System TSAN/Debug Flags, Test Runner Sanitizer Label** (2026-03-13 10:40 PDT):
   - **DBException hybrid storage:** Fixed buffer reduced from 271 to 79 bytes (12 mark + 2 ": " + 64 msg + 1 null); new `shared_ptr<char[]> m_overflow` for messages > 64 chars allocated via `new(std::nothrow)`; graceful degradation to truncated buffer on alloc failure; class made `final`; `what_s()` no longer virtual; object size ~120 bytes (down from ~287)
   - **`build_cpp_dbc.sh`:** New `--tsan` flag for ThreadSanitizer; unified `SANITIZER_FLAGS`/`SANITIZER_LINKER_FLAGS` variables; `CMAKE_EXE_LINKER_FLAGS` now passed to CMake; new `--debug-mysql`/`--debug-postgresql` flags; CMake variable ordering cleaned up
   - **`build_test_cpp_dbc.sh`:** `PROJECT_ROOT` resolved to absolute path for consistent `CMAKE_INSTALL_PREFIX`; new `INSTALL_DIR` variable; `CPP_DBC_BUILD_EXAMPLES=OFF`/`CPP_DBC_BUILD_BENCHMARKS=OFF` explicitly passed; `DEBUG_ALL` now forwarded
   - **`run_test_parallel.sh`:** New `SANITIZER_LABEL` variable detected from pass-through args; TUI status bar shows `" | Tool: <label>"` when sanitizer/tool active
   - 4 files changed, +151/-33 lines

2. **Build System — CMake Cache Invalidation Fix, Lock Macro Unification Across Drivers** (2026-03-12 21:09 PDT):
   - **CMake cache invalidation fixed:** Redundant `build_test_cpp_dbc.sh` call removed from `build_cpp_dbc.sh`; `BACKWARD_HAS_DW` default aligned to `OFF`; `CMAKE_CXX_FLAGS` built dynamically to avoid trailing spaces; `EXAMPLES`/`BENCHMARKS` no longer hardcoded `OFF` in test build script
   - **`--mc-combo-01/02` now include benchmarks**
   - **MySQL/PostgreSQL/SQLite lock macro unification:** Statement/result-set registry methods now use driver-specific `*_LOCK_OR_RETURN` macros + `stmtLock` naming for inner lock
   - **SQLite new macros:** `SQLITE_CONNECTION_LOCK_OR_RETURN`, `SQLITE_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED`, `SQLITE_STMT_LOCK_OR_RETURN` in `sqlite_internal.hpp`
   - **PostgreSQL `connection.hpp`:** `m_initFailed`/`m_initError` moved to dedicated "Construction state" section
   - 11 files changed, +280/-140 lines

2. **PostgreSQL Driver — Shared-Mutex Refactoring, PrivateCtorTag Pattern, Convention Compliance** (2026-03-12 12:24 PDT):
   - **Connection mutex ownership:** `SharedConnMutex` (shared_ptr<recursive_mutex>) replaced with direct `std::recursive_mutex m_connMutex` in `PostgreSQLDBConnection`; new `getConnectionMutex()` method for child access
   - **`PostgreSQLConnectionLock` RAII helper:** New class in `postgresql_internal.hpp` acquires connection mutex through `weak_ptr<PostgreSQLDBConnection>` with double-checked locking; marks object closed if connection expired
   - **`m_closed` → `std::atomic<bool>`:** Connection, PreparedStatement, and ResultSet all use atomic closed flag with proper memory ordering
   - **PreparedStatement refactored:** `weak_ptr<PGconn>` → `weak_ptr<PostgreSQLDBConnection>`; PrivateCtorTag + `m_initFailed`/`m_initError`; `enable_shared_from_this`; `PG_STMT_LOCK_OR_RETURN` macro replaces `DB_DRIVER_LOCK_GUARD(*m_connMutex)`
   - **ResultSet refactored:** PrivateCtorTag pattern; `weak_ptr<PostgreSQLDBConnection>`; result set registry in connection; `notifyConnClosing()` lifecycle management; independent mutex removed (shares connection mutex)
   - **InputStream refactored:** PrivateCtorTag pattern; throwing `validateAndEnd()` removed; noexcept constructor
   - **Convention fixes:** NOSONAR annotations on all `catch(...)`, `std::from_chars` replacing `std::stoi`/`std::stoll`/`std::stod`/`strtol`, dead try/catch removed from 6+ methods, `.has_value()` enforcement
   - **`PG_DEBUG` macro rewritten:** `iostream`-based → `snprintf`-based with timestamp via `logWithTimesMillis()`
   - **`DB_DRIVER_LOCK_GUARD` → `std::scoped_lock`**
   - 18 files changed, +733/-655 lines

3. **Unified version/info API across all drivers, MySQL code hardening, BlobStream connection validation** (2026-03-08 17:25 PST):
   - **Unified version/info API:** `getDriverVersion()` moved from family driver bases to `DBDriver` base; `getServerVersion()` + `getServerInfo()` (throwing + nothrow) added to `DBConnection` base; all 7 drivers implement with driver-specific metadata
   - **API naming:** MongoDB `getServerInfo()` → `getServerInfoAsDocument()` to avoid base class collision; `getServerInfo()` removed from `KVDBConnection` (now inherited from `DBConnection`)
   - **BlobStream:** `isConnectionValid()` pure virtual added; all blob classes override with `const noexcept`
   - **MySQL hardening:** ~60 error codes regenerated, `std::stoi` → `std::from_chars`, `memset` → aggregate init, index loop → `std::span`, constructor try/catch removed, SQL injection prevention via `validateIdentifier()` in blob, internal helpers renamed with `std::nothrow_t`
   - **Pool:** version/info API delegated through all 4 family pooled connection wrappers via CRTP helpers
   - **Tests:** New version/info test cases for all 7 drivers
   - 66 files changed, +1760/-235 lines

2. **Convention refinements: PrivateCtorTag unification, violations checklist reorganization, MySQL code cleanup** (2026-03-08 00:21 PST):
   - **PrivateCtorTag naming unified:** `ConstructorTag` → `PrivateCtorTag` throughout convention docs; private constructors explicitly forbidden with migration rule
   - **Violations checklist reorganized:** Flat list → 6 categorized sections (Error Codes, Memory Safety, Atomics, Error Handling, Class Layout, Naming); ~20 new checklist items added
   - **MySQL code cleanup:** Added explicit destructors to `MySQLBlob`/`MySQLInputStream`; Allman brace formatting for `isResetting()`, lambdas in `connection_01.cpp`, and `mysql_internal.hpp` methods; fixed `connectRelational()` stub to delegate to nothrow; translated Spanish comments to English
   - 9 files changed, +137/-69 lines

3. **Unified URI API in DBDriver Base, PrivateCtorTag for MySQLBlob/InputStream, Expanded Conventions** (2026-03-07 13:34 PST):
   - **DBDriver base class refactored:** `acceptsURL()` → `acceptURI()` (throwing + nothrow); new `parseURI()`, `buildURI()`, `getURIScheme()` pure virtuals — all with default `acceptURI` using `parseURI`
   - **Family driver bases cleaned:** Removed `acceptsURL()`, `parseURL()`, `buildURL()` private helpers from `ColumnarDBDriver`, `DocumentDBDriver`, `KVDBDriver` (-130 lines)
   - **All 7 drivers updated:** Implement new URI pure virtuals; all tests updated for `acceptURI`/`parseURI`/`buildURI`
   - **MySQLBlob + MySQLInputStream:** PrivateCtorTag pattern with `m_initFailed`/`m_initError`, `#ifdef __cpp_exceptions` guards, nothrow static factories
   - **Convention rules expanded:** 8 new sections in `cpp_dbc_conventions.md` (preprocessor directives flush left, Allman braces, NOSONAR with rule ID, bug-fix ISO 8601 format, no `(void)` cast, `private:` omit in class, PrivateCtorTag pattern, migration rule for private methods)
   - **New convention violations report document:** `.claude/rules/cpp_dbc_conventions_violations_how_report_them.md` with structured format, severity levels, and comprehensive checklist
   - Docs updated: cppdbc-docs-en/es, error_handling_patterns, how_add_new_db_drivers
   - 49 files changed, +2938/-1354 lines

2. **MySQL Driver — Full Nothrow-First Refactor, Static Factory Pattern, Result Set Registry** (2026-03-06 20:29 PST):
   - **MySQLDBDriver:** Double-checked locking with `atomic<bool>` + `mutex` for library init; new `cleanup()` static method
   - **MySQLDBConnection:** PrivateCtorTag pattern, nothrow constructor with `m_initFailed`/`m_initError`, result set registry (`m_activeResultSets`) with two-phase close, `getMySQLNativeHandle()`/`getConnectionMutex()` for child access, `m_resetting` anti-deadlock flag, friend declarations for PreparedStatement/ResultSet/Blob
   - **MySQLDBPreparedStatement:** `weak_ptr<MySQLDBConnection>` replaces `weak_ptr<MYSQL>`, private nothrow constructor, `create(std::nothrow_t)` via `new`, `m_closed` atomic flag, `notifyConnClosing` returns `expected`
   - **MySQLDBResultSet:** Private nothrow constructor, `weak_ptr<MySQLDBConnection>` for lifecycle, `notifyConnClosing(std::nothrow_t)`, `[[nodiscard]]` on all nothrow methods
   - **mysql_internal.hpp:** `MySQLConnectionLock` RAII helper, `MYSQL_CONNECTION_LOCK_OR_RETURN` macro, `MYSQL_DEBUG` macro
   - Dead try/catch eliminated across all MySQL `.cpp` files
   - 17 files changed, +1559/-2202 lines (net reduction of ~643 lines)

3. **CRTP `PooledDBConnectionBase<D,C,P>` — Unified Pooled Connection Logic** (2026-03-06 08:43 PST):
   - **New CRTP template:** `PooledDBConnectionBase<Derived, ConnType, PoolType>` in `pool/pooled_db_connection_base.hpp` + `.cpp` (~485 lines) — extracts close/returnToPool (race-condition fix), destructor cleanup, and pool metadata from all 4 family pooled connection wrappers
   - **Family wrappers now thin CRTP delegators:** One-line inline delegators resolve diamond inheritance; destructors changed to `= default`
   - **Dead try/catch removed** from all `create(std::nothrow_t)` factory methods (death-sentence exception rule)
   - **Friend declarations** added to all 4 family connection classes and `DBConnectionPoolBase` for CRTP access
   - **`how_add_new_db_drivers.md`** updated with comprehensive connection pool integration guide (Scenario A: existing family, Scenario B: new family)
   - 17 files changed, +1312/-2230 lines (net reduction of ~918 lines)

4. **Unified Connection Pool Base Class (`DBConnectionPoolBase`) — Extracted Common Pool Logic into `pool/` Directory** (2026-03-06 01:59 PST):
   - **New `DBConnectionPoolBase`:** Single base class in `pool/connection_pool.hpp` + `pool/connection_pool.cpp` (955 lines) containing all pool infrastructure: connection acquisition with direct handoff, HikariCP validation skip, phase-based lock protocol, maintenance thread, `returnConnection()` with orphan detection
   - **`DBConnectionPooled` extended:** Added pool-internal lifecycle methods (`updateLastUsedTime`, `isPoolClosed`, `getClosedFlag`)
   - **Directory restructure:** All pool headers/sources moved from `core/` to `pool/` directory; new placeholder dirs for `pool/graph/`, `pool/timeseries/`
   - **Family pools now thin derived classes:** `RelationalDBConnectionPool`, `DocumentDBConnectionPool`, `ColumnarDBConnectionPool`, `KVDBConnectionPool` inherit from `DBConnectionPoolBase`, only override `createPooledDBConnection()` + typed getter
   - **Include path updates:** All examples (16), tests (30+), internal sources updated from `core/` to `pool/` paths
   - 77 files changed, +6020/-8459 lines (net reduction of ~2400 lines of duplicated pool logic)

4. **MongoDB Driver — Full Nothrow-First Refactor, Static Factory Pattern, `-fno-exceptions` Compatibility** (2026-03-04 14:29 PST):
   - **Core Abstract Interfaces — `#ifdef __cpp_exceptions` Guard Separation:**
     - All 5 document interfaces guard throwing methods with `#ifdef __cpp_exceptions`; nothrow methods always compile under `-fno-exceptions`
     - `DocumentDBCursor`: +9 new nothrow pure-virtuals (`next`, `hasNext`, `count`, `getPosition`, `skip`, `limit`, `sort`, `isExhausted`, `rewind`)
     - `DocumentDBData`: +17 nothrow pure-virtuals (all getters, setters, field ops)
     - `DocumentDBCollection`: +4 nothrow pure-virtuals (`getName`, `getNamespace`, `estimatedDocumentCount`, `countDocuments`)
     - `DocumentDBConnection`: `ping()` removed from abstract interface
     - `DocumentDBDriver`: `getDefaultPort()` removed; `getURIScheme()` now `noexcept` returning full URL prefix; `supportsReplicaSets()`/`supportsSharding()`/`getDriverVersion()` all `noexcept`
   - **MongoDBDriver — Double-Checked Locking:**
     - `std::once_flag` → `std::atomic<bool>` + `std::mutex` (compatible with `-fno-exceptions`; allows re-init after `cleanup()`)
     - Instance-level `m_mutex` removed; `connectDocument(std::nothrow_t)` calls `MongoDBConnection::create(std::nothrow)`
   - **MongoDBConnection — Static Factory:**
     - `create(std::nothrow_t, ...)` + private nothrow constructor + `initialize(std::nothrow_t)` helper
     - Old public throwing constructor removed; all helpers converted to nothrow
   - **MongoDBDocument — Static Factory + ID Caching:**
     - `create(std::nothrow_t)` and `create(std::nothrow_t, bson_t*)` static factories
     - `m_idCached`/`m_cachedId` for BSON `_id` field caching
     - New `document_07.cpp` (776 lines): nothrow setters, field ops, clone/clear/isEmpty, getBson/getBsonMutable
   - **MongoDBCursor/Collection — Full Nothrow:**
     - Cursor constructor and helpers converted to nothrow; `skip`/`limit`/`sort` use `std::reference_wrapper<DocumentDBCursor>`
     - Collection: new `getCollectionHandle(std::nothrow_t)` helper
   - **Dead try/catch elimination** across all MongoDB `.cpp` files
   - **Note:** MongoDB was the last driver to adopt these patterns — MySQL, PostgreSQL, SQLite, Firebird, ScyllaDB, and Redis already implement them. All 7 drivers now share the same nothrow-first, static factory, double-checked locking, and `-fno-exceptions` compatible architecture.
   - **Other:** KV `""` → `std::string{}`; Firebird stub fixes; MySQL/PostgreSQL blob `using MemoryBlob::copyFrom`
   - 41 files changed, +5956/-5321 lines

5. **DBException Fixed-Size Refactor, Unified `ping()` Interface, `std::string_view` Return Types, and Build Optimizations** (2026-02-26 18:27 PST):
   - **`DBException` — Hybrid Fixed/Dynamic Storage (updated 2026-03-13):**
     - `class DBException final : public std::exception`; constructor is `noexcept`
     - `m_full_message[79]` fixed buffer (12 mark + 2 ": " + 64 msg + 1 null); `shared_ptr<char[]> m_overflow` for messages > 64 chars via `new(std::nothrow)` — graceful degradation (~120 bytes object)
     - `m_callstack` stored as `std::shared_ptr<CallStackCapture>` — one allocation shared across copies
     - `what_s()` returns `std::string_view`; `getMark()` same; `getCallStack()` returns `std::span<const StackFrame>`
     - `what()` returns overflow buffer when available, fixed buffer otherwise
   - **`system_utils::CallStackCapture` — Fixed-Size Stack Capture:**
     - `StackFrame` now uses `char file[150]`, `char function[150]` (was `std::string`)
     - New `CallStackCapture` struct: `StackFrame frames[10]`, `int count` — max 10 frames
     - `captureCallStack()` returns `std::shared_ptr<CallStackCapture>` (was `std::vector<StackFrame>`)
     - `printCallStack()` now also accepts `std::span<const StackFrame>`
   - **Unified `ping()` in Base `DBConnection`:**
     - `ping()` promoted to pure virtual in `DBConnection`: `virtual bool ping() = 0`
     - Nothrow variant: `virtual cpp_dbc::expected<bool, DBException> ping(std::nothrow_t) noexcept = 0`
     - Removed Redis-specific `std::string ping()` from `KVDBConnection`; return type was `"PONG"` string
     - Removed MongoDB-specific `bool ping()` from `DocumentDBConnection`
     - Pool wrappers (relational, KV, columnar) updated with `bool ping()` override
   - **All Examples/Benchmarks Updated for `std::string_view`:**
     - 50+ example files: `+ ex.what_s()` → `+ std::string(ex.what_s())`
     - `example_common.hpp`: all logging function params changed from `const std::string&` to `std::string_view`
     - Redis examples: `std::string pong = conn->ping()` → `bool pong = conn->ping()` with `"PONG"/"FAILED"` output
   - **Build System — Deferred libdw + Faster Test Builds:**
     - `helper.sh`: `dw-on`/`dw-off` now deferred — later flags override earlier ones in same option list
     - `build_test_cpp_dbc.sh`: adds `-DCPP_DBC_BUILD_EXAMPLES=OFF -DCPP_DBC_BUILD_BENCHMARKS=OFF` to skip non-test targets

6. **Full Nothrow Pool API, Atomic int64_t Last-Used Time, MySQL Atomic Closed Flag, Destructor Safety, and Debug Macro Truncation Detection** (2026-02-24 18:25 PST):
   - **Pool Factory — Nothrow `create()` (All Families):**
     - All `create()` factories return `expected<shared_ptr<Pool>, DBException>` with `std::nothrow_t` (breaking change)
     - All internal pool methods (createDBConnection, createPooledDBConnection, validateConnection, returnConnection, initializePool) return `expected<...>` noexcept
     - All examples, tests, and `database_config.cpp` updated to use `create(std::nothrow, ...)` + expected pattern
   - **Nothrow Base Interfaces:**
     - `DBConnectionPool` base: added nothrow pure virtuals for 6 pool methods
     - `DBConnectionPooled` base: all 6 interface methods get nothrow signatures; `<new>` header added
   - **`m_lastUsedTimeNs` (atomic<int64_t>):** `atomic<time_point>` → `atomic<int64_t>` (nanoseconds since epoch) — truly portable (ARM32/MIPS); `static_assert` updated; `m_creationTime` uses in-class init; `m_lastUsedTimeNs` initialized from it
   - **MySQL `m_closed` → `std::atomic<bool>{false}`:** All 8+ access sites use `.load(std::memory_order_acquire)` / `.store(..., std::memory_order_release)`; constructor init removed
   - **Destructor Nothrow Close:** MySQL/PostgreSQL/SQLite/Firebird result set destructors all use `close(std::nothrow)` + error log; SQLite try/catch removed
   - **Debug Macro Truncation:** `CP_DEBUG`, `FIREBIRD_DEBUG`, `SQLITE_DEBUG` detect `snprintf` overflow → append `...[TRUNCATED]`
   - **`HighPerfLogger::m_flushCounter`:** Static local → class member
   - **CMake:** `ENABLE_ASAN`/`ENABLE_TSAN` add compile/link flags directly; `[[deprecated]]` on `what()` commented out
   - **Conventions:** 4 new sections — in-class member init, `atomic.load(memory_order_acquire)`, nothrow calls nothrow, no redundant try/catch
   - **Minor fixes:** Shell `"$@"` quoting, `NO_REBUILD_DEPS` removed, Firebird null-check removed, "5ms" → "25ms" log, ScyllaDB captureCallStack + error code dedup

7. **Unified Pool Mutex, Atomic Time-Point, Direct Handoff for All Pools, Notifications, and Test Quality Improvements** (2026-02-22 19:18 PST):
   - **Columnar/Document/KV Pools — Unified Mutex + Direct Handoff:**
     - 5 separate mutexes → single `m_mutexPool`; `getIdleDBConnection()` private method removed
     - `getXDBConnection()`: replaced 10ms-polling loop with `condition_variable::wait_until()` + FIFO wait loop
     - Added `ConnectionRequest` struct + `m_waitQueue` for direct handoff (no "stolen wakeup" race)
     - Added `m_connectionAvailable` CV (borrowers) and `m_pendingCreations` counter (overshoot prevention)
     - `returnConnection()` fixes: pool-closing exit, orphan handling, reset-failure → invalid, `updateLastUsedTime()`, all notifies inside lock
     - Destructors call `XDBConnectionPool::close()` (qualified) to avoid S1699 virtual dispatch in destructor
   - **`std::atomic<time_point>` for Last-Used Time:**
     - `m_lastUsedTimeMutex` removed; `m_lastUsedTime` → `std::atomic<steady_clock::time_point>` with `memory_order_relaxed`
     - `static_assert(::is_always_lock_free)` verifies no hidden mutex at compile time
   - **`libdw` Disabled by Default:** All test scripts default `BACKWARD_HAS_DW=OFF`; `--dw-on` opt-in
   - **Test Notification System:** `send_test_notification()` in `scripts/common/functions.sh`; reads `NOTIFY_COMMAND` from `.env.secrets` (gitignored); sends Gotify push after parallel and non-parallel runs
   - **`run_test_parallel.sh` — Compact Summary Refactor:** `display_compact_summary()` + `extract_catch2_warnings()` + `reconstruct_helgrind_warnings()` + `print_entry()`/`print_reason()`; `save_report_to_file()` includes compact summary + elapsed time
   - **Firebird `role` Option:** `isc_dpb_sql_role_name` in DPB; YAML updated; new `prod_firebird` example; `READ_UNCOMMITTED` MVCC behavior test
   - **Test Quality:** All success thresholds → 95%; timeout 2000 ms → 3500 ms; pool tests use `WARN()` instead of hard `REQUIRE(failureCount == 0)`; PostgreSQL tests fixed (`returnToPool()`, removed inline `DROP TABLE`, added `rs->close()`); SQLite new test sections

8. **Pool Lifecycle API Hardening, C++ Code Analysis Toolset, Docker Container Auto-Restart, and Valgrind Suppression Report** (2026-02-21 14:25 PST):
   - **Pool Lifecycle Methods — Protected API:**
     - `prepareForPoolReturn()`, `prepareForBorrow()` (and nothrow versions) moved from `public` to `protected` in `RelationalDBConnection`
     - Forward declarations + `friend class RelationalDBConnectionPool` / `friend class RelationalPooledDBConnection` added — only pool infrastructure can call lifecycle methods
     - All four driver headers (MySQL, PostgreSQL, SQLite, Firebird) updated: lifecycle overrides moved to protected section
     - `RelationalPooledDBConnection`, `MySQLConnectionPool`, `PostgreSQLConnectionPool`, `SQLiteConnectionPool`, `FirebirdConnectionPool` marked `final`
   - **C++ Code Analysis Toolset (4 new standalone scripts):**
     - `list_class.sh`: Lists unique C++ class names in files (Python3 + bash, no deps)
     - `list_public_methods.sh`: Inspects a class's public interface (structured or lineal output)
     - `list_class_usage.sh`: Finds all call sites of a class's public methods in target files
     - `test_coverage.sh`: Analyzes test coverage per class; parallel workers; presets `--check=db-driver` / `--check=db-pool`
   - **Docker DB Container Auto-Restart:**
     - `helper.sh`: Tracks `enabled_db_drivers` array; calls `restart_db_containers_for_test()` before TUI/test execution
     - `scripts/common/functions.sh`: 4 new functions (`get_driver_docker_info`, `find_db_container`, `wait_for_container`, `restart_db_containers_for_test`)
     - SQLite always skipped; graceful degradation on missing Docker / missing container / timeout
   - **Valgrind Suppression Summary:**
     - `display_valgrind_suppression_summary()` + `save_report_to_file()` added to `run_test_parallel.sh`
     - Called at end of TUI, simple, and summarize modes; report saved to `$LOG_DIR/final_report.log`
   - **Documentation:** `shell_script_dependencies.md` updated with C++ analysis tools and Docker management sections

2. **Direct Handoff Connection Pool, system_utils Performance Refactoring, and Debug Macro Unification** (2026-02-21 02:48 PST):
   - **RelationalDBConnectionPool — Direct Handoff:**
     - Removed `m_mutexGetConnection` (2 mutexes → 1 unified `m_mutexPool`)
     - Added `ConnectionRequest` struct + `m_waitQueue` (`std::deque`) for direct handoff: returned connections are assigned directly to waiting borrowers → eliminates "stolen wakeup" race
     - Removed `getIdleDBConnection()` (merged into `getRelationalDBConnection()` with deadline-based wait)
     - `returnConnection()` fixes: pool-closing early exit, orphan detection, reset-failure marks connection invalid, `updateLastUsedTime()` on return, all notifies inside lock
   - **system_utils.hpp — Performance + New Thread Utilities:**
     - `currentTimeMillis()`, `currentTimeMicros()`, `getCurrentTimestamp()`: replaced `ostringstream`/`put_time`/`setfill` with stack buffers + `strftime` + `snprintf` (zero heap allocation)
     - Removed `<iomanip>`, `<sstream>`; added `<charconv>`
     - New `getThreadId()`: OS-native TID via `gettid()` (Linux) / `GetCurrentThreadId()` (Windows), using `std::to_chars`
     - New `threadIdToString(id)`: last 6 digits of thread ID hash via `std::to_chars`
     - New `logWithTimesMillis(prefix, msg)`: structured `[HH:MM:SS.mmm] [TID] [prefix] message` log line
     - `getCurrentTimestamp()` format changed: brackets removed (was `[YYYY-MM-DD ...]`, now `YYYY-MM-DD ...`)
   - **system_utils.cpp:** Re-enabled `stackTraceMutex` in `captureCallStack()` — serializes `backward::StackTrace::load_here()` calls (thread-safe)
   - **Debug Macro Unification:** `CP_DEBUG`, `FIREBIRD_DEBUG`, `SQLITE_DEBUG` all simplified to use `logWithTimesMillis()` instead of manual prefix construction
   - **Printf fixes:** `%zu` → `%d` for `atomic<int>::load()`, `%lld` → `%ld` for elapsed, `ex.what()` → `ex.what_s().c_str()`
   - **Helgrind suppression broadened:** `glibc_clockwait_internal_signal_broadcast` now covers both `signal` and `broadcast` variants via wildcard
   - **Test updates:** all pool tests use `logWithTimesMillis("TEST", ...)` instead of `std::cerr`

2. **Source File Reorganization: Canonical Method Ordering and File Splitting** (2026-02-19 PST):
   - **File Splitting:** 7 new `.cpp` files added across all 4 relational drivers to reduce file sizes and improve compilation parallelism:
     - MySQL: `result_set_04.cpp` (blob/binary nothrow methods)
     - PostgreSQL: `result_set_04.cpp` (blob/binary nothrow methods)
     - SQLite: `result_set_04.cpp`, `result_set_05.cpp`
     - Firebird: `result_set_05.cpp`, `prepared_statement_04.cpp`, `connection_04.cpp`
   - **Canonical Method Ordering:** All result set nothrow methods reordered consistently across all drivers: `close` → `isEmpty` → `next` → navigation → type-by-index/by-name interleaved → metadata → blob/binary (separate file)
   - **Header Declaration Reordering:** MySQL, PostgreSQL, SQLite result_set.hpp declarations reordered to interleave by-index/by-name variants
   - **Firebird Connection Redistribution:** connection_01 (constructor/destructor/executeCreateDatabase), connection_02 (all throwing wrappers), connection_03 (nothrow part 1), connection_04 NEW (nothrow part 2)
   - **Firebird PreparedStatement Redistribution:** Rebalanced across 4 files with new `prepared_statement_04.cpp`
   - **Error Code Fix:** `firebird/blob.hpp` — `"FB_BLOB_CONN_CLOSED"` → `"LMHROWFG5PNN"` (12-char alphanumeric format)
   - **No functional logic changes** — all method implementations preserved exactly as they were
   - **CMakeLists.txt:** Added 7 new source files to build

2. **Complete Nothrow API Implementation Across All Drivers** (2026-02-18 PST):
   - Base class nothrow methods promoted to pure virtual `= 0` in `DBConnection`, `DBResultSet`, `RelationalDBConnection`
   - All 7 drivers implement full nothrow API surface
   - Throwing wrappers delegate to nothrow implementations (single code path)
   - Connection pool wrappers updated for new pure virtuals

3. **Helgrind Thread-Safety Hardening — Firebird Driver Refactor + Connection Pool Lock Order Fixes** (2026-02-17 PST):
   - **Firebird USE-AFTER-FREE Fix:** Eliminated raw `m_trPtr` from `FirebirdDBPreparedStatement`. All methods now access `m_connection.lock()->m_tr` via weak_ptr — lifecycle-safe
   - **Firebird Mutex Model:** Removed `m_statementsMutex` / `m_resultSetsMutex`. All registry access uses single `m_connMutex` — eliminates ABBA deadlock
   - **Firebird `m_closed`/`m_resetting` as atomics:** Prevents data races on connection state flags
   - **Printf-style debug output:** Firebird/pool debug output converted from `operator<<` to `printf`-style for non-interleaved Helgrind-safe output
   - **Connection Pool Close Outside Locks:** All pool types now collect connections under locks, then close OUTSIDE locks — fixes Helgrind LockOrder violations
   - **Connection Pool MinIdle Replenishment Outside Locks:** `maintenanceTask()` calls `createPooledDBConnection()` outside pool locks
   - **`AtomicGuard<T>` RAII template:** New `system_utils::AtomicGuard<T>` for exception-safe atomic flag management
   - **DBConnection/DBResultSet nothrow API:** `reset(nothrow)` and `close(nothrow)` now return `expected<void,DBException>` with explicit errors (no more silent no-ops)
   - **Valgrind suppressions:** 2 new false-positive suppressions (ScyllaDB heap-address reuse, glibc GLIBC_2.34 clockwait)
   - **`run_test_parallel.sh`:** Clock jump detection (NTP/VM resume), PGID-based process group kill
   - **`connection_pool_internal.hpp`:** New shared internal header for all pool implementations
   - **Bug docs deleted:** `docs/bugs/README.md` and `docs/bugs/firebird_helgrind_analysis.md` — all tracked bugs resolved

2. **Test Directory Reorganization and Parallel Test Runner Enhancements** (2026-02-06 21:44:02 PST):
   - **Test Directory Complete Restructuring:**
     - Migrated all test files to hierarchical, database-family-based organization
     - Test structure mirrors example organization for consistency
     - Organization: `test/common/`, `test/relational/mysql/`, `test/kv/redis/`, `test/document/mongodb/`, `test/columnar/scylladb/`
     - Added placeholder directories: `test/graph/.git_keep`, `test/timeseries/.git_keep`
   - **Parallel Test Runner Enhancements (`run_test_parallel.sh`):**
     - Terminal resize handling with SIGWINCH signal handler
     - Dual log file system: clean `.log` files + colored `.log.ansi` files in `/tmp`
     - Automatic cleanup of temporary directories (keeps last 5)
     - Recursive test discovery for family-based directory structure
   - **Driver Documentation Updates:**
     - Enhanced `how_add_new_db_drivers.md` with test directory structure section
     - Updated with family-prefixed path examples and CMakeLists.txt instructions
   - **Impact:** 110 files changed, better organization, improved maintainability

1a. **Major Examples Reorganization and Automation Improvements** (2026-02-06 16:40:00 PST):
   - **Examples Complete Restructuring (59 new files, 18 deleted):**
     - Migrated from flat structure to hierarchical, database-family-based organization
     - Implemented numeric prefix naming convention: `XX_YZZ_example_<db>_<feature>.cpp`
     - Organization by family: common (10_xxx), MySQL (20_xxx), PostgreSQL (21_xxx), SQLite (22_xxx), Firebird (23_xxx), Redis (24_xxx), MongoDB (25_xxx), ScyllaDB (26_xxx)
     - Examples in family folders: `examples/relational/`, `examples/kv/`, `examples/document/`, `examples/columnar/`
   - **New Example Discovery and Execution System:**
     - Created `run_examples.sh` script with automatic example discovery
     - Supports wildcard pattern matching: `--run='23_*'`, `--run='*_basic'`
     - Lists examples by category (relational, document, kv, columnar)
   - **Enhanced Helper Script (`helper.sh`):**
     - Added `--run-example` command with full wildcard support
     - Automatic logging to `/logs/example/{timestamp}/` with auto-rotation
     - Added `--help` / `-h` option to all major scripts
   - **Script Reorganization:**
     - Moved `lib/common_functions.sh` → `scripts/common/functions.sh`
     - Centralized shared bash functions following DRY principle
   - **System Utilities:**
     - Added `getExecutablePath()` and `getExecutablePathAndName()` functions
     - Portable way to locate executables and resources
   - **Test Framework:**
     - New `25_521_test_mongodb_real_cursor_api.cpp` — first exclusive test (5YY range)
     - Tests MongoDB-specific cursor APIs: `hasNext()` and `nextDocument()`
   - **Impact:** 162 files changed, 34,214 lines in diff

2. **Comprehensive Documentation for New Driver Development and Error Handling** (2026-02-04 22:06:05 PST):
   - **New Driver Development Guide (`how_add_new_db_drivers.md`):**
     - Created comprehensive guide (~1,500 lines) for adding new database drivers to cpp_dbc
     - Covers all 5 phases: driver files, build configuration, tests, benchmarks, and examples
     - Includes detailed code reference for base interfaces and family-specific interfaces
     - Documents thread safety macros, RAII handles patterns, and reference implementations
     - Provides complete CMake configuration examples with `USE_<DRIVER>` patterns
     - Includes checklist summary and common mistakes to avoid
   - **Error Handling Patterns Documentation (`error_handling_patterns.md`):**
     - Created comprehensive guide (~600 lines) for error handling in cpp_dbc
     - Documents `DBException` class with 12-character error codes and call stack capture
     - Covers both exception-based API and exception-free API (nothrow) patterns
     - Provides patterns by component: Driver, Connection, PreparedStatement, ResultSet
     - Includes DO and DON'T best practices with code examples
   - **Shell Script Dependencies Documentation (`shell_script_dependencies.md`):**
     - Created comprehensive guide documenting all shell script dependencies
     - Documents call hierarchy between helper.sh, build scripts, and utility scripts
     - Includes troubleshooting guide for common script issues
   - **Updated Project Conventions (`.claude/rules/cpp_dbc_conventions.md`):**
     - Added "Adding New Database Drivers" section referencing the new guide
     - Documents key files that require `USE_<DRIVER>` updates

2. **Cross-Platform Compatibility and Type Portability Improvements** (2026-02-04 14:14:16 PST):
   - **Cross-Platform Time Functions:**
     - Replaced `localtime_r` (Unix) with `localtime_s` (Windows) with proper cross-platform detection
     - Added `#ifdef _WIN32` preprocessor guards for platform-specific code in `system_utils.hpp`
   - **Type Portability (long → int64_t):**
     - Changed `long` to `int64_t` for consistent 64-bit integer handling across all platforms
     - Applied to `RelationalDBResultSet::getLong()` and `ColumnarDBResultSet::getLong()` interfaces
     - Updated all driver implementations: MySQL, PostgreSQL, SQLite, Firebird, ScyllaDB
   - **[[nodiscard]] Attribute for Exception-Free API:**
     - Added `[[nodiscard]]` to all nothrow methods returning `expected<T, DBException>`
     - Compiler now warns if error-returning functions are called without checking result
   - **SQLite BLOB Security Improvements:**
     - Added `validateIdentifier()` function to prevent SQL injection in BLOB operations
     - Enhanced parameterized query usage in BLOB read/write
   - **Destructor Error Handling:**
     - Thread-safe logging in destructors using `std::scoped_lock`
     - Exceptions caught and logged instead of potentially throwing from destructors
   - **Driver URL Parsing Fixes:**
     - MySQL: Fixed `parseURL()` to handle URLs without database name
     - PostgreSQL: Fixed `parseURL()` validation
     - Added comprehensive unit tests for URL parsing

1a. **Comprehensive Doxygen API Documentation for All Public Headers** (2026-02-04 00:41:38 PST):
   - **Documentation Enhancement (64 header files, 2,384 insertions):**
     - Added Doxygen-compatible `/** @brief ... */` documentation blocks to all public header files
     - Replaced simple `//` comments with structured Doxygen documentation across the entire API surface
     - Added inline ```` ```cpp ```` usage examples to all major classes and methods
     - Added `@param`, `@return`, `@throws`, `@see` tags for cross-referencing
     - Added section separator comments for improved readability
   - **Scope of Documentation:**
     - Core interfaces: blob.hpp, streams.hpp, system_utils.hpp, db_connection.hpp, db_driver.hpp, db_exception.hpp, db_expected.hpp, db_result_set.hpp, db_types.hpp, cpp_dbc.hpp, transaction_manager.hpp
     - Configuration: database_config.hpp, yaml_config_loader.hpp
     - Relational interfaces: relational_db_connection.hpp, relational_db_prepared_statement.hpp, relational_db_result_set.hpp
     - Columnar interfaces: columnar_db_connection.hpp, columnar_db_prepared_statement.hpp, columnar_db_result_set.hpp
     - Document interfaces: all document_db_*.hpp files
     - KV interfaces: all kv_db_*.hpp files
     - All 7 driver header subfolders (mysql, postgresql, sqlite, firebird, mongodb, scylladb, redis)
   - **Benefits:**
     - IDE tooltip support — hover over any class or method to see documentation
     - Doxygen-ready — can generate HTML/PDF API reference
     - Self-documenting API with inline code examples

2. **Driver Header Split Refactoring — One-Class-Per-File** (2026-02-03 14:58:04 PST):
   - **Header File Reorganization:**
     - Split all 7 multi-class `driver_*.hpp` files into individual per-class `.hpp` files in driver subfolders
     - Original `driver_*.hpp` files now serve as pure aggregator headers with only `#include` directives
     - 44 new header files created across 7 driver subdirectories (6,727 lines total)
     - Backward compatible — external consumers still include `driver_mysql.hpp` etc.
   - **BLOB Header Consolidation:**
     - Deleted 4 separate BLOB headers: `mysql_blob.hpp`, `postgresql_blob.hpp`, `sqlite_blob.hpp`, `firebird_blob.hpp`
     - BLOB classes moved into their respective driver subfolders (e.g., `mysql/blob.hpp`)
     - Removed 25 now-redundant BLOB `#include` directives from source, test, and example files
   - **New Driver Subfolders:**
     - `relational/mysql/`, `postgresql/`, `sqlite/`, `firebird/` (7 files each: handles, input_stream, blob, result_set, prepared_statement, connection, driver)
     - `document/mongodb/` (6 files: handles, document, cursor, collection, connection, driver)
     - `columnar/scylladb/` (6 files: handles, memory_input_stream, result_set, prepared_statement, connection, driver)
     - `kv/redis/` (4 files: handles, reply_handle, connection, driver)
   - **Benefits:**
     - Better LLM comprehension — smaller, focused files are easier to process in context windows
     - Improved IDE navigation — each class has its own dedicated file
     - One-class-per-file convention matches modern C++ best practices

1b. **Driver Code Split Refactoring** (2026-01-31 23:41:14 PST):
   - Split all database driver .cpp implementations into multiple smaller, focused files
   - Each driver now has its own subdirectory with internal header and split implementation files
   - Updated `libs/cpp_dbc/CMakeLists.txt` to compile all new split source files

2. **SonarCloud Code Quality Fixes and Connection Pool Improvements** (2026-01-29 16:23:21 PST):
   - **Critical Race Condition Fix in Connection Pool Return Flow:**
     - Fixed race condition in `close()` and `returnToPool()` methods across all pool types
     - Bug: `m_closed` was reset to `false` AFTER `returnConnection()` completed
     - Fix: Reset `m_closed` to `false` BEFORE calling `returnConnection()`
     - Added catch-all exception handlers to ensure correct state on any exception
   - **Connection Pool PrivateCtorTag Pattern (PassKey Idiom):**
     - Added `DBConnectionPool::PrivateCtorTag` struct to enable `std::make_shared` while enforcing factory pattern
     - Updated all connection pool classes to use PrivateCtorTag
     - Enables single memory allocation with `std::make_shared`
   - **SonarCloud Code Quality Fixes:**
     - Added NOSONAR comments with explanations for intentional code patterns
     - Added `[[noreturn]]` attributes to stub methods that always throw exceptions
     - Changed `virtual ~Class()` to `~Class() override` for derived classes
     - Added Rule of 5 to `MySQLDBDriver` and `PostgreSQLDBDriver`
     - Changed nested namespace declarations to modern C++17 syntax
   - **Parallel Test Script Improvements:**
     - Fixed TUI initialization to occur after build completes
     - Added `TUI_ACTIVE` flag for proper cleanup on interrupt
   - **SonarQube Issues Fetch Script Enhancement:**
     - Made `--file` parameter optional (fetches ALL issues when not specified)

2. **Parallel Test Execution System** (2026-01-28 21:43:01 PST):
   - **New Parallel Test Runner (`run_test_parallel.sh`):**
     - Added complete parallel test execution script (~1900 lines of bash)
     - Runs test prefixes (10_, 20_, 21_, 23_, 26_, etc.) in parallel batches
     - Each prefix runs independently with separate log files
     - If a prefix fails, it stops but others continue running
     - Logs saved to `logs/test/YYYY-MM-DD-HH-MM-SS/PREFIX_RUNXX.log`
     - TUI (Text User Interface) mode with split panel view using `--progress` flag
     - Summarize mode with `--summarize` to show summary of past test runs
     - Valgrind error detection support
     - Color-coded output for test status (pass/fail)
   - **Helper Script Enhancement (`helper.sh`):**
     - Added `parallel=N` option to run N test prefixes in parallel
     - Added `parallel-order=P1_,P2_,...` option to prioritize specific prefixes
     - Added comprehensive documentation and examples for parallel execution
     - Integrated with `run_test_parallel.sh` when parallel mode is enabled
   - **Run Test Script Enhancement (`run_test.sh`):**
     - Added `--skip-build` flag to skip the build step
     - Added `--list` flag to list tests without running them
   - **ScyllaDB Test Fix:**
     - Increased sleep time from 100ms to 250ms in `26_091_test_scylladb_real_right_join.cpp`
     - Ensures proper time for ScyllaDB indexes/data consistency

2. **SonarCloud Code Quality Fixes and Helper Script Enhancement** (2026-01-20 15:17:41):
   - **SonarCloud Configuration Updates:**
     - Consolidated SonarCloud configuration into `.sonarcloud.properties`
     - Deleted redundant `sonar-project.properties` file
     - Added exclusion for cognitive complexity rule (cpp:S3776)
     - Added exclusion for nesting depth rule (cpp:S134)
     - Added documentation comments for each excluded rule
   - **Code Quality Improvements in `blob.hpp`:**
     - Added `explicit` keyword to `FileOutputStream` constructor
     - Removed redundant `m_position(0)` initialization in `MemoryInputStream`
   - **Code Quality Improvements in Columnar DB Connection Pool:**
     - Changed `ColumnarDBConnection` destructor from `virtual` to `override`
     - Added in-class member initialization for atomic variables
     - Changed `validateConnection` method to `const`
     - Changed `close()` and `isPoolValid()` methods to `final` in appropriate classes
     - Replaced `std::lock_guard<std::mutex>` with `std::scoped_lock`
     - Changed `std::unique_lock<std::mutex>` to `std::unique_lock` with CTAD
     - Replaced generic `std::exception` catches with specific `DBException` catches
     - Simplified conditional logic in `getIdleDBConnection()` method
   - **Helper Script Enhancement (`helper.sh`):**
     - Enhanced test execution table to show log file location with line numbers
     - Improved test output to help users quickly navigate to specific test results

2. **Connection Pool Race Condition Fix and Code Quality Improvements** (2026-01-18 23:26:52):
   - Fixed connection pool race condition in all database types:
     - Added pool size recheck under lock to prevent exceeding `m_maxSize` under concurrent creation
     - New connections are created as candidates and only registered if pool hasn't filled
     - Unregistered candidate connections are properly closed to prevent resource leaks
   - Improved return connection logic with null checks and proper cleanup on shutdown
   - Fixed MongoDB stub driver (uncommented disabled code, updated exception marks)
   - Fixed driver stub exception marks to use UUID-style marks for consistency
   - Fixed `blob.hpp` variable initialization (`m_position` now initialized at declaration)
   - Fixed remaining ScyllaDB variable naming (`SCYLLA_DEV_PKG` → `SCYLLADB_DEV_PKG`)

2. **ScyllaDB Native DATE Type Support Fix** (2026-01-18 22:49:41):
   - Fixed ScyllaDB driver to properly handle native Cassandra DATE type:
     - **Reading DATE values (`getString`):**
       - Added support for `CASS_VALUE_TYPE_DATE` (uint32 - days since epoch with 2^31 bias)
       - Correctly converts Cassandra DATE format to ISO date string (YYYY-MM-DD)
       - Uses `cass_value_get_uint32` instead of treating as timestamp
     - **Writing DATE values (`setDate`):**
       - Changed from `cass_statement_bind_int64` (timestamp) to `cass_statement_bind_uint32` (date)
       - Uses `cass_date_from_epoch` to convert epoch seconds to Cassandra DATE format
       - Uses `timegm` for proper UTC timezone handling (with Windows `_mkgmtime` fallback)

3. **Connection Pool Deadlock Prevention and ScyllaDB Naming Consistency Fixes** (2026-01-18 22:33:51):
   - Fixed potential deadlock in all connection pool implementations:
     - **Deadlock Prevention:**
       - Changed from sequential `std::lock_guard` calls to `std::scoped_lock` for consistent lock ordering
       - Fixed in `columnar_db_connection_pool.cpp` (ScyllaDB)
       - Fixed in `document_db_connection_pool.cpp` (MongoDB)
       - Fixed in `kv_db_connection_pool.cpp` (Redis)
       - Fixed in `relational_db_connection_pool.cpp` (MySQL, PostgreSQL, SQLite, Firebird)
     - **Before (potential deadlock):**
       ```cpp
       std::lock_guard<std::mutex> lockAllConnections(m_mutexAllConnections);
       std::lock_guard<std::mutex> lockIdleConnections(m_mutexIdleConnections);
       ```
     - **After (deadlock-safe):**
       ```cpp
       std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);
       ```
     - **Connection Registration Fix:**
       - Fixed `getIdleDBConnection()` method to properly register newly created connections
       - New connections are now added to `m_allConnections` before being returned
   - Fixed ScyllaDB naming consistency:
     - **Namespace Rename:**
       - Changed namespace from `Scylla` to `ScyllaDB` in `driver_scylladb.hpp`
       - Updated error code prefix from `SCYLLA_` to `SCYLLADB_`
     - **Build Script Fixes:**
       - Added ScyllaDB echo output in `build.dist.sh`
       - Added `DEBUG_SCYLLADB=ON` in debug mode for `build.sh`
       - Fixed `--debug-scylladb` option alias in `build_cpp_dbc.sh`
     - **Distribution Package Fixes:**
       - Renamed `SCYLLA_CONTROL_DEP` to `SCYLLADB_CONTROL_DEP` in `build_dist_pkg.sh`
       - Updated `__SCYLLA_DEV_PKG__` to `__SCYLLADB_DEV_PKG__` in all distro Dockerfiles
       - Added `__REDIS_CONTROL_DEP__` placeholder handling in build scripts
   - Fixed documentation numbering in `cppdbc-package.md` (corrected step numbers 19→20, 20→21, 20→22)
   - Fixed typos in `TODO.md`: "proyect" → "project", "ease" → "easy", "INTERGRATION" → "INTEGRATION"

4. **VSCode IntelliSense Automatic Synchronization System** (2026-01-18 02:59:56 PM PST):
   - Added automatic synchronization system for VSCode IntelliSense:
     - **New Scripts:**
       - Added `.vscode/sync_intellisense.sh` - Quick sync without rebuilding
       - Added `.vscode/regenerate_intellisense.sh` - Rebuild from last config
       - Added `.vscode/update_defines.sh` - Update defines from compile_commands.json
       - Added `.vscode/detect_include_paths.sh` - Detect system include paths
     - **Documentation:**
       - Added `.vscode/README_INTELLISENSE.md` - Comprehensive guide
     - **Features:**
       - Build parameters automatically saved to `build/.build_args`
       - Configuration state saved to `build/.build_config`
       - Quick sync option that doesn't require rebuild
   - Removed `EXCEPTION_FREE_ANALYSIS.md` (analysis completed)

5. **ScyllaDB Connection Pool and Driver Enhancements** (2026-01-18 02:08:02 PM PST):
   - Added columnar database connection pool implementation for ScyllaDB:
     - **New Connection Pool Files:**
       - Added `include/cpp_dbc/core/columnar/columnar_db_connection_pool.hpp` - Columnar database connection pool interfaces
       - Added `src/core/columnar/columnar_db_connection_pool.cpp` - Columnar database connection pool implementation
       - Added `examples/scylladb_connection_pool_example.cpp` - Example for ScyllaDB connection pool usage
       - Added `test/test_scylladb_connection_pool.cpp` - Comprehensive tests for ScyllaDB connection pool
     - **Pool Architecture:**
       - `ColumnarDBConnectionPool` base class for all columnar database pools
       - `ColumnarPooledDBConnection` wrapper class for pooled columnar connections
       - `ScyllaDB::ScyllaConnectionPool` specialized implementation for ScyllaDB
       - Smart pointer-based pool lifetime tracking for memory safety
       - Connection validation with CQL query (`SELECT now() FROM system.local`)
       - Configurable pool parameters (initial size, max size, min idle, etc.)
     - **Build System Updates:**
       - Renamed `USE_SCYLLA` to `USE_SCYLLADB` for consistency
       - Renamed driver files from `driver_scylla` to `driver_scylladb`
       - Renamed namespace from `Scylla` to `ScyllaDB`
     - **New Examples:**
       - Added `examples/scylla_example.cpp` - Basic ScyllaDB operations example
       - Added `examples/scylla_blob_example.cpp` - BLOB operations with ScyllaDB
       - Added `examples/scylla_json_example.cpp` - JSON data handling with ScyllaDB
       - Added `examples/scylladb_connection_pool_example.cpp` - Connection pool usage example
     - **New Benchmarks:**
       - Added `benchmark/benchmark_scylladb_select.cpp` - CQL SELECT operations benchmarks
       - Added `benchmark/benchmark_scylladb_insert.cpp` - CQL INSERT operations benchmarks
       - Added `benchmark/benchmark_scylladb_update.cpp` - CQL UPDATE operations benchmarks
       - Added `benchmark/benchmark_scylladb_delete.cpp` - CQL DELETE operations benchmarks
     - **Test Updates:**
       - Renamed test files from `test_scylla_*` to `test_scylladb_*`
       - Added `test/test_scylladb_connection_pool.cpp` for connection pool tests
     - **Exception-Free API:**
       - All ScyllaDB driver methods support nothrow variants
       - Returns `expected<T, DBException>` for error handling

6. **PostgreSQL Exception-Free API Implementation** (2026-01-06 08:11:44 PM PST):
   - Added comprehensive exception-free API for PostgreSQL driver operations:
     - **Implementation:**
       - Implemented nothrow versions of all PostgreSQL driver methods using `std::nothrow_t` parameter
       - All methods return `expected<T, DBException>` with clear error information
       - Comprehensive error handling with unique error codes for each method
       - Replaced "NOT_IMPLEMENTED" placeholders with full implementations
     - **Operations Supported:**
       - Connection management (prepareStatement, executeQuery, executeUpdate)
       - Transaction handling (beginTransaction, commit, rollback)
       - Transaction isolation level management
       - Auto-commit settings
       - Connection URL parsing and validation
       - Parameter binding in prepared statements
     - **Error Handling:**
       - Error propagation with expected<T, DBException>
       - Preserves call stack information in error cases
       - Consistent error code format across all methods
       - Clear error messages with operation and failure reason
     - **Benefits:**
       - Consistent API pattern with other database drivers
       - Performance improvements for code using nothrow API
       - Safer error handling without exception overhead
       - Full compatibility with both exception and non-exception usage patterns

7. **Redis Exception-Free API Implementation** (2026-01-03 05:23:03 PM PST):
   - Added comprehensive exception-free API for Redis driver operations:
     - **Implementation:**
       - Added nothrow versions of all Redis driver methods using `std::nothrow_t` parameter
       - All methods return `expected<T, DBException>` with clear error information
       - Comprehensive error handling with unique error codes for each method
       - Added `#include <new>` for `std::nothrow_t`
     - **Operations Supported:**
       - Key-Value operations (setString, getString, exists, deleteKey, etc.)
       - List operations (listPushLeft, listPushRight, listPopLeft, etc.)
       - Hash operations (hashSet, hashGet, hashDelete, hashExists, etc.)
       - Set operations (setAdd, setRemove, setIsMember, etc.)
       - Sorted Set operations (sortedSetAdd, sortedSetRemove, sortedSetScore, etc.)
       - Server operations (scanKeys, ping, flushDB, getServerInfo, etc.)
     - **Error Handling:**
       - Error propagation with expected<T, DBException>
       - Preserves call stack information in error cases
       - Consistent error code format across all methods
       - Clear error messages with operation and failure reason
     - **Benefits:**
       - No exception overhead in performance-critical code
       - More explicit error handling
       - Better interoperability with code that can't use exceptions
       - Same comprehensive error information as exception-based API

8. **Redis KV Connection Pool Implementation** (2026-01-01 07:48:12 PM PST):
   - Added key-value database connection pool implementation:
     - **New Files:**
       - Added `include/cpp_dbc/core/kv/kv_db_connection_pool.hpp` - Key-value database connection pool interfaces
       - Added `src/core/kv/kv_db_connection_pool.cpp` - Key-value database connection pool implementation
       - Added `examples/kv_connection_pool_example.cpp` - Example for Redis connection pool usage
       - Added `test/test_redis_connection_pool.cpp` - Comprehensive tests for Redis connection pool
     - **Pool Architecture:**
       - `KVDBConnectionPool` abstract base class for all key-value database pools
       - `KVPooledDBConnection` wrapper class for pooled key-value connections
       - Smart pointer-based pool lifetime tracking for memory safety
       - Connection validation with Redis ping command
       - Configurable pool parameters (initial size, max size, min idle, etc.)
     - **Redis Implementation:**
       - `RedisDBConnectionPool` concrete implementation for Redis
       - Factory pattern with `create` static methods for pool creation
       - Protected constructors to enforce factory method usage
       - Support for Redis authentication and custom connection options
       - Connection string format: `cpp_dbc:redis://host:port/database`
       - Background maintenance thread for connection cleanup and idle connection management
     - **Test Coverage:**
       - Basic connection pool operations (get/return connections)
       - Redis operations through pooled connections
       - Concurrent connections stress testing
       - Connection pool behavior under load
       - Connection validation and cleanup
       - Pool growth and scaling tests
     - **Benefits:**
       - Better resource management with clearer ownership semantics
       - Thread-safe connection pooling for Redis databases
       - Automatic connection validation and cleanup
       - Consistent API with other database type connection pools
       - Improved performance through connection reuse

9. **Redis KV Database Driver Support** (2025-12-31 08:34:52 PM PST):
   - Added complete Redis key-value database driver implementation:
     - **Core Key-Value Database Interfaces:**
       - `KVDBConnection`: Base interface for key-value database connections
       - `KVDBDriver`: Base interface for key-value database drivers
     - **Redis Driver Implementation:**
       - `RedisDBDriver`: Driver class for Redis connections
       - `RedisDBConnection`: Connection management with proper resource cleanup
     - **Features:**
       - String operations with TTL support
       - List operations (push, pop, range)
       - Hash operations (set, get, delete)
       - Set operations (add, remove, members)
       - Sorted Set operations (add, remove, range)
       - Counter operations (increment, decrement)
       - Scan operations for key pattern matching
       - Server operations (ping, info, flush)
       - Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
     - **Build System Updates:**
       - Added `USE_REDIS` CMake option
       - Added `--redis` and `--redis-off` build script options
       - Added `--debug-redis` option for debug output
       - Added `FindHiredis.cmake` for Hiredis library detection
     - **Test Coverage:**
       - Basic connection and authentication
       - Key-value operations (set, get, delete)
       - List, hash, set, and sorted set operations
       - Expiration and TTL handling
       - Server commands and information

10. **Document Database Connection Pool Factory Pattern Implementation** (2025-12-30 11:38:13 PM PST):
   - Implemented factory pattern for MongoDB connection pool creation:
     - **API Changes:**
       - Added `create` static factory methods to `DocumentDBConnectionPool` and `MongoDBConnectionPool`
       - Made constructors protected to enforce factory method usage
       - Added `std::enable_shared_from_this` inheritance to `DocumentDBConnectionPool`
       - Added `initializePool` method for initialization after shared_ptr construction
       - Updated all code to use factory methods instead of direct instantiation
     - **Memory Safety Improvements:**
       - Improved connection lifetime management with weak_ptr reference tracking
       - Removed unnecessary raw pointer references in pooled connections
       - Simplified pooled connection constructor interface
       - Enhanced resource cleanup with proper initialization sequence
     - **Test Updates:**
       - Updated MongoDB connection pool tests to use the factory methods
       - Fixed thread safety tests to use shared_ptr for pool access
       - Improved test readability with auto type deduction
       - Added comprehensive validation in connection pool tests
     - **Benefits:**
       - Better resource management with clearer ownership semantics
       - Avoids potential use-after-free issues with pool and connection lifetimes
       - Safer handling of self-referencing in document database connection pools
       - More consistent API across different pool implementations
       - Improved thread safety in concurrent environments

11. **Connection Pool Factory Pattern Implementation** (2025-12-30 04:28:19 PM PST):
   - Implemented factory pattern for relational connection pool creation:
     - **API Changes:**
       - Added `create` static factory methods to `RelationalDBConnectionPool` and all specific pools
       - Made constructors protected to enforce factory method usage
       - Added `std::enable_shared_from_this` inheritance to `RelationalDBConnectionPool`
       - Added `initializePool` method for initialization after shared_ptr construction
       - Updated all code to use factory methods instead of direct instantiation
     - **Resource Management Improvements:**
       - Improved connection lifetime management with weak_ptr reference tracking
       - Removed unnecessary raw pointer references in pooled connections
       - Simplified pooled connection constructor interface
       - Enhanced resource cleanup with proper initialization sequence
     - **Test Updates:**
       - Updated all test files to use the factory methods
       - Fixed thread safety tests to use shared_ptr for pool access
       - Improved test readability with auto type deduction
       - Enhanced pool validation in transaction tests
     - **Benefits:**
       - Better resource management with clearer ownership semantics
       - Avoids potential use-after-free issues with pool and connection lifetimes
       - Safer handling of self-referencing in connection pools
       - More consistent API across different pool implementations

12. **MongoDB Connection Pool Implementation** (2025-12-30 12:38:57 PM PST):
   - Added complete document database connection pool implementation:
     - **New Core Files:**
       - Added `include/cpp_dbc/core/document/document_db_connection_pool.hpp` with document pool interface definitions
       - Added `src/core/document/document_db_connection_pool.cpp` with document pool implementation
       - Added `examples/document_connection_pool_example.cpp` showing MongoDB connection pool usage
       - Added `test/test_mongodb_connection_pool.cpp` with comprehensive pool tests
     - **Pool Architecture:**
       - `DocumentDBConnectionPool` abstract base class for all document database pools
       - `DocumentPooledDBConnection` wrapper class for pooled document connections
       - Smart pointer-based pool lifetime tracking for memory safety
       - Connection validation with MongoDB ping command
       - Configurable pool parameters (initial size, max size, min idle, etc.)
     - **MongoDB Implementation:**
       - `MongoDBConnectionPool` concrete implementation for MongoDB
       - Support for MongoDB authentication
       - Connection string format: `cpp_dbc:mongodb://host:port/database`
       - Comprehensive test coverage including concurrent operations
     - **Build System Updates:**
       - Updated CMakeLists.txt to include new source files
       - Added document connection pool example to build configuration
       - Added MongoDB connection pool tests to test suite

13. **MongoDB Benchmark and Test Improvements** (2025-12-29 04:28:15 PM PST):
   - Added comprehensive MongoDB benchmark suite:
     - **New Benchmark Files:**
       - `benchmark_mongodb_select.cpp` with SELECT operations benchmarks
       - `benchmark_mongodb_insert.cpp` with INSERT operations benchmarks
       - `benchmark_mongodb_update.cpp` with UPDATE operations benchmarks
       - `benchmark_mongodb_delete.cpp` with DELETE operations benchmarks
     - **Benchmark Infrastructure Updates:**
       - Updated `benchmark/CMakeLists.txt` to include MongoDB benchmark files
       - Added `USE_MONGODB` compile definition for conditional compilation
       - Added MongoDB helper functions to `benchmark_common.hpp`
       - Updated benchmark scripts with MongoDB-specific options
     - **New Benchmark Options:**
       - Added `--mongodb` parameter to run MongoDB benchmarks
       - Added MongoDB memory usage tracking to baseline creation
     - **Performance Metrics:**
       - Document insertion throughput (documents/sec)
       - Query response times with varying collection sizes
       - Update operation performance with different operators
       - Delete operation efficiency metrics
       - Memory consumption during operations
       - Comparison against baseline measurements
   - Added comprehensive MongoDB test coverage:
     - **New Test Files:**
       - `test_mongodb_real_json.cpp` with JSON operations tests
       - `test_mongodb_thread_safe.cpp` with thread-safety stress tests
       - `test_mongodb_real_join.cpp` with join operations tests (aggregation pipeline)
     - **Test Coverage:**
       - Document JSON operations (basic, nested, arrays)
       - JSON query operators ($eq, $gt, $lt, etc.)
       - JSON updates and modifications
       - JSON aggregation operations
       - Thread safety with multiple connections
       - Thread safety with shared connection
       - Join operations using MongoDB aggregation pipeline
       - Error handling and recovery scenarios
   - Enhanced helper script with MongoDB support:
     - Added new command combinations for MongoDB testing and benchmarks
     - Added example command combinations for different scenarios
     - Updated usage information with MongoDB examples
     - Added memory usage tracking support for MongoDB operations
     - Added baseline creation and comparison functionality

14. **MongoDB Document Database Driver Support** (2025-12-27):
   - Added complete MongoDB document database driver implementation:
     - **Core Document Database Interfaces:**
       - `DocumentDBConnection`: Base interface for document database connections
       - `DocumentDBDriver`: Base interface for document database drivers
       - `DocumentDBCollection`: Interface for collection operations
       - `DocumentDBCursor`: Interface for cursor-based result iteration
       - `DocumentDBData`: Interface for document data representation
     - **MongoDB Driver Implementation:**
       - `MongoDBDriver`: Driver class for MongoDB connections
       - `MongoDBConnection`: Connection management with proper resource cleanup
       - `MongoDBCollection`: Collection operations (CRUD, indexing)
       - `MongoDBCursor`: Cursor-based document iteration
       - `MongoDBData`: BSON document wrapper
     - **Features:**
       - Document insertion (single and batch)
       - Document querying with filters
       - Document updates with operators ($set, $inc, etc.)
       - Document deletion (single and multiple)
       - Collection management (create, drop, list)
       - Index management
       - Cursor-based result iteration
       - BSON data type support
       - Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
     - **Build System Updates:**
       - Added `USE_MONGODB` CMake option
       - Added `--mongodb` and `--mongodb-off` build script options
       - Added `--debug-mongodb` option for debug output
       - Added `FindMongoDB.cmake` for MongoDB C++ driver detection
       - Updated all distribution Dockerfiles with MongoDB support
     - **Test Coverage:**
       - Added `test_mongodb_common.cpp` with helper functions
       - Added `test_mongodb_real.cpp` with comprehensive tests
   - Connection URL format: `mongodb://host:port/database` or `mongodb://username:password@host:port/database?authSource=admin`

15. **Directory Restructuring for Multi-Database Type Support** (2025-12-27):
   - Reorganized project directory structure to support multiple database types:
     - **Core Interfaces Moved:**
       - Moved `relational/` → `core/relational/`
       - Files: `relational_db_connection.hpp`, `relational_db_driver.hpp`, `relational_db_prepared_statement.hpp`, `relational_db_result_set.hpp`
     - **Driver Files Moved:**
       - Moved `drivers/` → `drivers/relational/`
       - Files: `driver_firebird.hpp/cpp`, `driver_mysql.hpp/cpp`, `driver_postgresql.hpp/cpp`, `driver_sqlite.hpp/cpp`
       - BLOB headers: moved to driver subfolders (e.g., `mysql/blob.hpp`) — standalone `*_blob.hpp` files deleted in 2026-02-03 refactoring
     - **New Placeholder Directories Created:**
       - Core interfaces: `core/columnar/`, `core/document/`, `core/graph/`, `core/kv/`, `core/timeseries/`
       - Driver implementations: `drivers/columnar/`, `drivers/document/`, `drivers/graph/`, `drivers/kv/`, `drivers/timeseries/`
       - Source files: `src/drivers/columnar/`, `src/drivers/document/`, `src/drivers/graph/`, `src/drivers/kv/`, `src/drivers/timeseries/`
     - **Updated Include Paths:**
       - Updated all include paths in source files to reflect new directory structure
       - Updated `cpp_dbc.hpp` main header with new paths
       - Updated `connection_pool.hpp` with new paths
       - Updated all example files, test files, and benchmark files
       - Updated `CMakeLists.txt` with new source file paths
   - Benefits: Clear separation between database types, prepared for future database driver implementations, better code organization

16. **Connection Pool Memory Safety Improvements** (2025-12-27):
   - Enhanced connection pool with smart pointer-based pool lifetime tracking:
     - **RelationalDBConnectionPool Changes:**
       - Added `m_poolAlive` shared atomic flag (`std::shared_ptr<std::atomic<bool>>`) to track pool lifetime
       - Pool sets `m_poolAlive` to `false` in `close()` method before cleanup
       - Prevents pooled connections from attempting to return to a destroyed pool
     - **RelationalDBPooledConnection Changes:**
       - Changed `m_pool` from raw pointer to `std::weak_ptr<RelationalDBConnectionPool>`
       - Added `m_poolAlive` shared atomic flag for safe pool lifetime checking
       - Added `m_poolPtr` raw pointer for pool access (only used when `m_poolAlive` is true)
       - Added `isPoolValid()` helper method to check if pool is still alive
       - Updated constructor to accept weak_ptr, poolAlive flag, and raw pointer
       - Updated `close()` method to check `isPoolValid()` before returning connection to pool
   - Benefits: Prevention of use-after-free when pool is destroyed while connections are in use

17. **API Naming Convention Refactoring** (2025-12-26):
   - Renamed classes and methods to use "DB" prefix for better clarity and consistency:
     - **Driver Classes:** `MySQLDriver` → `MySQLDBDriver`, `PostgreSQLDriver` → `PostgreSQLDBDriver`, `SQLiteDriver` → `SQLiteDBDriver`, `FirebirdDriver` → `FirebirdDBDriver`
     - **Connection Classes:** `Connection` → `RelationalDBConnection`
     - **Configuration Classes:** `ConnectionPoolConfig` → `DBConnectionPoolConfig`
     - **DriverManager Methods:** `getConnection()` → `getDBConnection()`
     - **TransactionManager Methods:** `getTransactionConnection()` → `getTransactionDBConnection()`
     - **Driver Methods:** `connect()` → `connectRelational()`
   - Updated all benchmark, test, and example files to use new naming convention

18. **BLOB Memory Safety Improvements with Smart Pointers** (2025-12-22):
   - Migrated all BLOB implementations from raw pointers to smart pointers for improved memory safety:
     - **Firebird BLOB:** Changed from raw `isc_db_handle*` and `isc_tr_handle*` to `std::weak_ptr<FirebirdConnection>`
     - **MySQL BLOB:** Changed from raw `MYSQL*` to `std::weak_ptr<MYSQL>`
     - **PostgreSQL BLOB:** Changed from raw `PGconn*` to `std::weak_ptr<PGconn>`
     - **SQLite BLOB:** Changed from raw `sqlite3*` to `std::weak_ptr<sqlite3>`
   - Added helper methods for safe connection access that throw `DBException` if connection is closed
   - Added `isConnectionValid()` methods to check connection state
   - Updated driver implementations to use new BLOB constructors
   - Benefits: Automatic detection of closed connections, prevention of use-after-free errors, clear ownership semantics

19. **Firebird Driver Database Creation and Error Handling Improvements** (2025-12-21):
   - Added database creation support to Firebird driver:
     - Added `createDatabase()` method to `FirebirdDriver` for creating new Firebird databases
     - Added `command()` method for executing driver-specific commands
     - Added `executeCreateDatabase()` method to `FirebirdConnection` for CREATE DATABASE statements
     - Support for specifying page size and character set
   - Improved Firebird error message handling:
     - Fixed critical bug: error messages now captured BEFORE calling cleanup functions
     - Uses separate status array for cleanup operations to avoid overwriting error information
     - Improved `interpretStatusVector()` with SQLCODE interpretation
     - Combined SQLCODE and `fb_interpret` messages for comprehensive error information
   - Added new example:
     - Added `firebird_reserved_word_example.cpp` demonstrating reserved word handling
     - Updated `CMakeLists.txt` to include the new example when Firebird is enabled

20. **Firebird SQL Driver Enhancements and Benchmark Support** (2025-12-21):
   - Added comprehensive Firebird benchmark suite:
     - New benchmark files for SELECT, INSERT, UPDATE, DELETE operations
     - Updated benchmark infrastructure with Firebird support
     - Updated benchmark scripts with `--firebird` options
   - Enhanced Firebird driver implementation:
     - Added automatic BLOB content reading for BLOB SUB_TYPE TEXT columns
     - Enhanced `returnToPool()` with proper transaction cleanup
     - Added automatic rollback on failed queries when autocommit is enabled
     - Simplified URL acceptance to only `cpp_dbc:firebird://` prefix
   - Added comprehensive Firebird test coverage:
     - New test files for JOIN operations (FULL, INNER, LEFT, RIGHT)
     - Added JSON operations tests
     - Added thread-safety stress tests
     - Updated integration tests with Firebird support
     - Updated transaction isolation and transaction manager tests

21. **Firebird SQL Database Driver Support** (2025-12-20):
   - Added complete Firebird SQL database driver implementation
   - Full support for Firebird SQL databases (version 2.5+, 3.0+, 4.0+)
   - Connection management with proper resource cleanup using smart pointers
   - Prepared statement support with parameter binding
   - Result set handling with all data types
   - BLOB support with lazy loading and streaming capabilities
   - Transaction management with isolation level support
   - Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
   - Build system updates with `USE_FIREBIRD` option
   - Connection URL format: `cpp_dbc:firebird://host:port/path/to/database.fdb`

22. **Thread-Safe Database Driver Operations**:
   - Added optional thread-safety support for database driver operations:
     - **New CMake Option:**
       - Added `DB_DRIVER_THREAD_SAFE` option (default: ON) to enable/disable thread-safe operations
       - Use `-DDB_DRIVER_THREAD_SAFE=OFF` to disable thread-safety for single-threaded applications
     - **Build Script Updates:**
       - Added `--db-driver-thread-safe-off` option to all build scripts (build.sh, build.dist.sh, helper.sh, run_test.sh)
       - Updated helper.sh with new option for --run-build, --run-build-dist, --run-test, and --run-benchmarks
     - **Driver Implementations:**
       - MySQL: Added `std::mutex m_mutex` and `std::lock_guard` protection in key methods
       - PostgreSQL: Added `std::mutex m_mutex` and `std::lock_guard` protection in key methods
       - SQLite: Added `std::mutex m_mutex` and `std::lock_guard` protection in key methods
     - **New Thread-Safety Tests:**
       - Added test_mysql_thread_safe.cpp, test_postgresql_thread_safe.cpp, test_sqlite_thread_safe.cpp
       - Tests include: multiple threads with individual connections, connection pool concurrent access,
         concurrent read operations, high concurrency stress test, rapid connection open/close stress test
   - Benefits: Safe concurrent access, protection against race conditions, optional feature for performance

23. **Smart Pointer Migration for Database Drivers**:
   - Migrated all database drivers from raw pointers to smart pointers for improved memory safety:
     - **MySQL Driver:**
       - Added `MySQLResDeleter`, `MySQLStmtDeleter`, `MySQLDeleter` custom deleters
       - Changed `m_mysql` to `shared_ptr<MYSQL>`, `m_stmt` to `unique_ptr<MYSQL_STMT>`, `m_result` to `unique_ptr<MYSQL_RES>`
       - PreparedStatement uses `weak_ptr<MYSQL>` for safe connection reference
       - Added `validateResultState()`, `validateCurrentRow()`, and `getMySQLConnection()` helper methods
     - **PostgreSQL Driver:**
       - Added `PGresultDeleter`, `PGconnDeleter` custom deleters
       - Changed `m_conn` to `shared_ptr<PGconn>`, `m_result` to `unique_ptr<PGresult>`
       - PreparedStatement uses `weak_ptr<PGconn>` for safe connection reference
       - Added `getPGConnection()` helper method for safe connection access
     - **SQLite Driver:**
       - Added `SQLiteStmtDeleter`, `SQLiteDbDeleter` custom deleters
       - Changed `m_db` to `shared_ptr<sqlite3>`, `m_stmt` to `unique_ptr<sqlite3_stmt>`
       - PreparedStatement uses `weak_ptr<sqlite3>` for safe connection reference
       - Changed `m_activeStatements` from `set<shared_ptr>` to `set<weak_ptr>`
       - Added `getSQLiteConnection()` helper method for safe connection access
       - Removed obsolete `activeConnections` static list and related mutex
   - Benefits: RAII cleanup, exception safety, clear ownership semantics, elimination of manual delete/free calls

24. **Benchmark Baseline and Comparison System**:
   - Added benchmark baseline creation and comparison functionality:
     - Added `create_benchmark_cpp_dbc_base_line.sh` script to create benchmark baselines from log files
     - Added `compare_benchmark_cpp_dbc_base_line.sh` script to compare two benchmark baseline files
     - Baseline files are stored in `libs/cpp_dbc/benchmark/base_line/` directory organized by system configuration
     - Baseline data includes benchmark results, memory usage, and time command output
     - Comparison script supports filtering by benchmark name and automatic detection of latest baseline files
   - Added memory usage tracking for benchmarks:
     - Added `--memory-usage` option to `run_benchmarks_cpp_dbc.sh` to track memory usage with `/usr/bin/time -v`
     - Automatic installation of `time` package if not available on Debian-based systems
     - Memory usage data is captured per database type (MySQL, PostgreSQL, SQLite)
   - Updated helper.sh with new benchmark options:
     - Added `memory-usage` option to enable memory tracking during benchmarks
     - Added `base-line` option to automatically create baseline after running benchmarks
     - Updated help text with new options and examples

25. **Benchmark System Migration to Google Benchmark**:
   - Migrated benchmark system from Catch2 to Google Benchmark:
     - Updated CMakeLists.txt to use Google Benchmark instead of Catch2WithMain
     - Added benchmark/1.8.3 as a dependency in conanfile.txt
     - Rewrote all benchmark files to use Google Benchmark API
     - Improved benchmark table initialization system with table reuse
     - Added support for multiple iterations and benchmark repetitions
   - Logging system improvements:
     - Added timestamp-based logging functions in system_utils.hpp
     - Implemented logWithTimestampInfo, logWithTimestampError, logWithTimestampException, etc.
     - Replaced std::cerr and std::cout with these logging functions in benchmark code
   - Connection management improvements for benchmarks:
     - Added setupMySQLConnection and setupPostgreSQLConnection functions
     - Implemented system to reuse already initialized tables
     - Used mutex for synchronization in multi-threaded environments
   - Script updates:
     - Added mysql-off option in helper.sh to disable MySQL benchmarks
     - Changed configuration parameters (--samples and --resamples to --min-time and --repetitions)
     - Improved filter handling for running specific benchmarks

26. **SQLite Driver Thread Safety Improvements**:
   - Enhanced thread safety in SQLite driver implementation:
     - Added thread-safe initialization pattern using std::atomic and std::mutex
     - Implemented singleton pattern for SQLite configuration to ensure it's only done once
     - Added static class members to track initialization state
     - Improved debug output system with unique error codes for better troubleshooting
   - Unified debug output system:
     - Added support for DEBUG_ALL option to enable all debug outputs at once
     - Updated debug macros in connection_pool.cpp, transaction_manager.cpp, and driver_sqlite.cpp
     - Replaced std::cerr usage with debug macros for better control
     - Added [[maybe_unused]] attribute to avoid warnings with unused variables
   - Removed obsolete inject_cline_custom_instructions.sh script

27. **Benchmark and Testing Framework Improvements**:
   - Added improved benchmark organization and reusability:
     - Added benchmark_common.cpp with implementation of common benchmark helper functions
     - Reorganized helper functions into namespaces (common_benchmark_helpers, mysql_benchmark_helpers, etc.)
     - Updated benchmark CMakeLists.txt to include the new benchmark_common.cpp file
     - Changed configuration to use dedicated benchmark_db_connections.yml instead of test config
     - Improved consistency in helper function usage across benchmark files
   - Enhanced database driver improvements:
     - Added getURL() method to Connection interface for URL retrieval
     - Implemented getURL() in all database driver implementations
     - Added URL caching in database connections for better performance
     - Improved connection closing in MySQL driver with longer sleep time (25ms)
     - Enhanced PostgreSQL database creation with proper existence checking
   - Added new example:
     - Added connection_info_example.cpp showing different connection URL formats
     - Updated CMakeLists.txt to build the new example
   - Updated helper.sh with new example command for comprehensive testing

28. **Test Code Refactoring**:
   - Refactored test code to improve organization and reusability:
     - Added test_sqlite_common.hpp with SQLite-specific helper functions
     - Added test_sqlite_common.cpp with helper function implementations
     - Moved utility functions to database-specific namespaces
     - Replaced repetitive configuration loading with helper functions
     - Improved consistency in helper usage across all test files
   - Enhanced database connection configuration:
     - Added support for database-specific connection options
     - Implemented option filtering in driver_postgresql.cpp to ignore options starting with "query__"
     - Replaced configuration search code with simplified helper calls
     - Removed redundant test_blob_common.hpp file
     - Refactored helpers to use common_test_helpers namespace instead of global functions
   - Optimized file structure and organization:
     - Moved global helper functions to common namespace for better encapsulation
     - Reordered header inclusion for consistency
     - Updated CMakeLists.txt to include new test files
     - Removed update_headers.sh script, replaced with more precise manual management

29. **Benchmark System Implementation**:
   - Added comprehensive benchmark system for database operations:
     - Added benchmark directory with benchmark files for all database drivers
     - Added benchmark_main.cpp with common benchmark setup
     - Added benchmark files for MySQL, PostgreSQL, and SQLite operations
     - Implemented benchmarks for SELECT, INSERT, UPDATE, and DELETE operations
     - Added support for different data sizes (10, 100, 1000, and 10000 rows)
     - Added `--benchmarks` option to build.sh to enable building benchmarks
     - Added `--run-benchmarks` option to helper.sh to run benchmarks
     - Added support for running specific database benchmarks (mysql, postgresql, sqlite)
     - Added automatic benchmark log rotation in logs/benchmark/ directory
   - Updated build system:
     - Added `CPP_DBC_BUILD_BENCHMARKS` option to CMakeLists.txt
     - Added `--benchmarks` parameter to build scripts
     - Updated helper.sh with benchmark support
   - Improved test files:
     - Added conditional compilation for YAML support in test files
     - Added default connection parameters when YAML is disabled
     - Fixed PostgreSQL test files to work without YAML configuration
     - Improved SQLite test files to work with in-memory databases
   - Updated documentation:
     - Added benchmark information to README.md
     - Updated build script documentation with benchmark options

30. **Code Quality Improvements with Comprehensive Warning Flags**:
   - Added comprehensive warning flags and compile-time checks:
     - Added `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wformat=2 -Wunused -Werror=return-type -Werror=switch -Wdouble-promotion -Wfloat-equal -Wundef -Wpointer-arith -Wcast-align` to all build scripts
     - Added special handling for backward.hpp to silence -Wundef warnings
     - Added `-Werror=return-type` and `-Werror=switch` to treat these warnings as errors
   - Improved code quality with better variable naming:
     - Added `m_` prefix to member variables to avoid -Wshadow warnings
     - Used static_cast<> for numeric conversions to avoid -Wconversion warnings
     - Changed int return types to uint64_t for executeUpdate() methods
   - Improved exception handling to avoid variable shadowing
   - Updated TODO.md:
     - Marked "Activate ALL possible warnings and compile time checks" as completed


31. **Distribution Package Build System (DEB and RPM)**:
   - Added comprehensive distribution package build system:
     - Created `build_dist_pkg.sh` script to replace `build_dist_deb.sh` with improved functionality
     - Added support for building packages for multiple distributions in a single command
     - Added support for Debian 12, Debian 13, Ubuntu 22.04, and Ubuntu 24.04 (.deb packages)
     - Added support for Fedora 42 and Fedora 43 (.rpm packages)
     - Added Docker-based build environment for each distribution
     - Added MySQL fix patch for Debian distributions
     - Added CMake integration files for better package usage
     - Added version specification option with `--version` parameter
     - Improved package naming with distribution and version information
     - Added documentation and examples for CMake integration
   - Added CMake integration improvements:
     - Enhanced `cpp_dbc-config.cmake.in` for better CMake integration
     - Added `FindSQLite3.cmake` for SQLite dependency handling
     - Added example CMake project in `docs/example_cmake_project/`
     - Added comprehensive CMake usage documentation in `docs/cmake_usage.md`
   - Updated package naming and structure:
     - Changed package name from `cpp-dbc` to `cpp-dbc-dev` to better reflect its purpose
     - Improved package description with build options information
     - Added CMake files to the package for better integration
     - Added documentation and examples to the package
   - Updated TODO.md:
     - Marked "Add --run-build-lib-dist-deb using docker" as completed
     - Marked "Add --run-build-lib-dist-rpm using docker" as completed

32. **Stack Trace Improvements with libdw Support**:
   - Added libdw support for enhanced stack traces:
     - Added `BACKWARD_HAS_DW` option to CMakeLists.txt to enable/disable libdw support
     - Added `--dw-off` flag to build scripts to disable libdw support when needed
     - Added automatic detection and installation of libdw development libraries
     - Added libdw dependency to package map in build.dist.sh
     - Updated main.cpp with stack trace testing functionality
     - Fixed function name references in system_utils.cpp for stack frame filtering
   - Updated error handling in examples:
     - Changed `e.what()` to `e.what_s()` in blob_operations_example.cpp and join_operations_example.cpp
   - Updated TODO.md:
     - Marked "Add library dw to linker en CPP_SBC" as completed
     - Changed "Add script for build inside a docker..." to more specific tasks:
       - "Add --run-build-lib-dist-deb using docker"
       - "Add --run-build-lib-dist-rpm using docker"

33. **Mejoras de Seguridad y Tipos de Datos**:
   - Mejoras en el manejo de excepciones:
     - Reemplazado `what()` por `what_s()` en toda la base de código para evitar el uso de punteros `const char*` inseguros
     - Añadido un destructor virtual a `DBException`
     - Marcado el método `what()` como obsoleto con [[deprecated]]
   - Mejoras en tipos de datos:
     - Cambio de tipos `int` y `long` a `unsigned int` y `unsigned long` para parámetros que no deberían ser negativos en `ConnectionPoolConfig` y `DatabaseConfig`
     - Inicialización de variables en la declaración en `ConnectionPoolConfig`
   - Optimización en SQLite:
     - Reordenamiento de la inicialización de SQLite (primero configuración, luego inicialización)
   - Actualización de TODO.md con nuevas tareas:
     - "Add library dw to linker en CPP_SBC" (completada)
     - "Add script for build inside a docker the creation of .deb (ubuntu 22.04) .rpm (fedora) and make simple build for another distro version" (reemplazada por tareas más específicas)

34. **Exception Handling and Stack Trace Improvements**:
   - Added comprehensive stack trace capture and error tracking:
     - Added `backward.hpp` library for stack trace capture and analysis
     - Created `system_utils::StackFrame` structure to store stack frame information
     - Implemented `system_utils::captureCallStack()` function to capture the current call stack
     - Implemented `system_utils::printCallStack()` function to print stack traces
   - Enhanced `DBException` class with improved error tracking:
     - Added mark field to uniquely identify error locations
     - Added call stack capture to store the stack trace at exception creation
     - Updated constructor to accept mark, message, and call stack
     - Added `getMark()` method to retrieve the error mark
     - Added `getCallStack()` and `printCallStack()` methods
   - Updated all exception throws in SQLite driver to include:
     - Unique error marks for better error identification
     - Separated error marks from error messages
     - Call stack capture for better debugging
   - Updated transaction manager to use the new exception format
   - Added comprehensive tests for the new exception features in test_drivers.cpp

35. **Logging System Improvements**:
   - Added structured logging system with dedicated log directories:
     - Created logs/build directory for build output logs
     - Created logs/test directory for test output logs
     - Added automatic log rotation keeping 4 most recent logs
     - Added timestamp to log filenames for better tracking
   - Modified helper.sh to support the new logging structure:
     - Added color support in terminal while keeping clean logs
     - Added unbuffer usage to preserve colors in terminal output
     - Added sed command to strip ANSI color codes from log files
     - Updated command output to show log file location

36. **VSCode Integration**:
   - Added VSCode configuration files:
     - Added .vscode/c_cpp_properties.json with proper include paths
     - Added .vscode/tasks.json with build tasks
     - Added .vscode/build_with_props.sh script to extract defines from c_cpp_properties.json
     - Added support for building with MySQL and PostgreSQL from VSCode
     - Added automatic extension installation task

37. **License Header Updates**:
   - Added standardized license headers to all .cpp and .hpp files
   - Created update_headers.sh script to automate header updates
   - Headers include copyright information, license terms, and file descriptions
   - All example files and test files now have proper headers


38. **BLOB Support for Image Files**:
   - Added support for storing and retrieving image files as BLOBs
   - Added helper functions in test_main.cpp for binary file operations:
     * `readBinaryFile()` to read binary data from files
     * `writeBinaryFile()` to write binary data to files
     * `getTestImagePath()` to get the path to the test image
     * `generateRandomTempFilename()` to create temporary filenames
   - Updated CMakeLists.txt to copy test.jpg to the build directory
   - Added comprehensive test cases for image file BLOB operations in:
     * `test_mysql_real_blob.cpp`
     * `test_postgresql_real_blob.cpp`
     * `test_sqlite_real_blob.cpp`
   - Added tests for storing, retrieving, and verifying image data integrity
   - Added tests for writing retrieved image data to temporary files

39. **BLOB Support Implementation**:
   - Added comprehensive BLOB (Binary Large Object) support for all database drivers
   - Added `<cstdint>` include to several header files for fixed-width integer types
   - Implemented base classes: `Blob`, `InputStream`, `OutputStream`
   - Added memory-based implementations: `MemoryBlob`, `MemoryInputStream`, `MemoryOutputStream`
   - Added file-based implementations: `FileInputStream`, `FileOutputStream`
   - Implemented database-specific BLOB classes: `MySQLBlob`, `PostgreSQLBlob`, `SQLiteBlob`
   - Added BLOB support methods to `ResultSet` and `PreparedStatement` interfaces
   - Added test cases for BLOB operations in all database drivers:
     - `test_mysql_real_blob.cpp`
     - `test_postgresql_real_blob.cpp`
     - `test_sqlite_real_blob.cpp`
   - Added `test_blob_common.hpp` with helper functions for BLOB testing
   - Added `test.jpg` file for BLOB testing
   - Updated Spanish documentation with BLOB support information
   - Updated CMakeLists.txt to include the new BLOB test files

40. **SQLite JOIN Operations Testing**:
   - Added comprehensive test cases for SQLite JOIN operations
   - Added `test_sqlite_real_inner_join.cpp` with INNER JOIN test cases
   - Added `test_sqlite_real_left_join.cpp` with LEFT JOIN test cases
   - Enhanced test coverage for complex JOIN operations with multiple tables
   - Added tests for JOIN operations with NULL checks and invalid columns
   - Added tests for type mismatches in JOIN conditions

41. **Debug Output Options**:
   - Added debug output options for better troubleshooting
   - Added `--debug-pool` option to enable debug output for ConnectionPool
   - Added `--debug-txmgr` option to enable debug output for TransactionManager
   - Added `--debug-sqlite` option to enable debug output for SQLite driver
   - Added `--debug-all` option to enable all debug output
   - Updated build scripts to pass debug options to CMake
   - Added debug output parameters to helper.sh script
   - Added logging to files for build and test output

42. **Test Script Enhancements**:
   - Enhanced test script functionality
   - Added `--run-test="tag"` option to run specific tests by tag
   - Added support for multiple test tags using + separator (e.g., "tag1+tag2+tag3")
   - Added debug options to run_test.sh script
   - Improved test command construction with better parameter handling
   - Changed default AUTO_MODE to false in run_test.sh
   - Added logging to files for test output with automatic log rotation
   - Added test log analysis functionality with `--check-test-log` option
   - Implemented detection of test failures, memory leaks, and Valgrind errors

43. **Valgrind Suppressions Removal**:
   - Removed valgrind-suppressions.txt file as it's no longer needed with improved PostgreSQL driver

44. **SQLite Connection Management Improvements**:
   - Enhanced SQLiteConnection to inherit from std::enable_shared_from_this
   - Replaced raw pointer tracking with weak_ptr in activeConnections list
   - Improved connection cleanup with weak_ptr-based reference tracking
   - Added proper error handling for shared_from_this() usage
   - Added safeguards to ensure connections are created with make_shared
   - Fixed potential memory issues in connection and statement cleanup

45. **Connection Options Support**:
   - Added connection options support for all database drivers
   - Added options parameter to Driver::connect() method
   - Added options parameter to all driver implementations (MySQL, PostgreSQL, SQLite)
   - Added options parameter to ConnectionPool constructor
   - Added options map to ConnectionPoolConfig
   - Updated DatabaseConfig to provide options through getOptions() method
   - Renamed original getOptions() to getOptionsObj() for backward compatibility
   - Updated all tests to support the new options parameter

46. **PostgreSQL Driver Improvements**:
   - Enhanced PostgreSQL driver with better configuration options
   - Added support for passing connection options from configuration to PQconnectdb
   - Made gssencmode configurable through options map (default: disable)
   - Added error codes to exception messages for better debugging

47. **SQLite Driver Improvements**:
   - Enhanced SQLite driver with better configuration options
   - Added support for configuring SQLite pragmas through options map
   - Added support for journal_mode, synchronous, and foreign_keys options
   - Added error codes to exception messages for better debugging

48. **SQLite Connection Pool Implementation**:
   - Added SQLite connection pool support with `SQLiteConnectionPool` class
   - Added SQLite-specific connection pool configuration in test_db_connections.yml
   - Added SQLite connection pool tests in test_connection_pool_real.cpp
   - Improved connection handling for SQLite connections
   - Added transaction isolation level support for SQLite pools

49. **SQLite Driver Improvements (Resource Management)**:
   - Enhanced SQLite driver with better resource management
   - Added static list of active connections for statement cleanup
   - Improved connection closing with sqlite3_close_v2 instead of sqlite3_close
   - Added better handling of prepared statements with shared_from_this
   - Fixed memory leaks in SQLite connection and statement handling

50. **Test Script Enhancements (Valgrind Options)**:
   - Added new options to run_test_cpp_dbc.sh
   - Added `--auto` option to automatically continue to next test set if tests pass
   - Added `--gssapi-leak-ok` option to ignore GSSAPI leaks in PostgreSQL with Valgrind

51. **Transaction Isolation Level Support**:
   - Added transaction isolation level support following JDBC standard
   - Implemented `TransactionIsolationLevel` enum with JDBC-compatible isolation levels
   - Added `setTransactionIsolation` and `getTransactionIsolation` methods to Connection interface
   - Implemented methods in MySQL and PostgreSQL drivers
   - Updated PooledConnection to delegate isolation level methods to underlying connection
   - Default isolation levels set to database defaults (REPEATABLE READ for MySQL, READ COMMITTED for PostgreSQL)
   - Added comprehensive tests for transaction isolation levels in test_transaction_isolation.cpp

52. **Database Configuration Integration**:
   - Added integration between database configuration and connection classes
   - Created new `config_integration_example.cpp` with examples of different connection methods
   - Added `createConnection()` method to `DatabaseConfig` class
   - Added `createConnection()` and `createConnectionPool()` methods to `DatabaseConfigManager`
   - Added new methods to `DriverManager` to work with configuration classes
   - Added script to run the configuration integration example
   - Changed URL format from "cppdbc:" to "cpp_dbc:" for consistency
   - Updated all examples, tests, and code to use the new format

53. **Connection Pool Enhancements**:
   - Moved `ConnectionPoolConfig` from connection_pool.hpp to config/database_config.hpp
   - Enhanced `ConnectionPoolConfig` with more options and better encapsulation
   - Added new constructors and factory methods to `ConnectionPool`
   - Added integration between connection pool and database configuration
   - Improved code organization with forward declarations

54. **YAML Configuration Support**:
   - Added optional YAML configuration support to the library
   - Created database configuration classes in `include/cpp_dbc/config/database_config.hpp`
   - Implemented YAML configuration loader in `include/cpp_dbc/config/yaml_config_loader.hpp` and `src/config/yaml_config_loader.cpp`
   - Added `--yaml` option to `build.sh` and `build_cpp_dbc.sh` to enable YAML support
   - Modified CMakeLists.txt to conditionally include YAML-related files based on USE_CPP_YAML flag

55. **Examples Improvements**:
   - Added `--examples` option to `build.sh` and `build_cpp_dbc.sh` to build examples
   - Created YAML configuration example in `examples/config_example.cpp`
   - Added example YAML configuration file in `examples/example_config.yml`
   - Created script to run the configuration example in `examples/run_config_example.sh`
   - Fixed initialization issue in `examples/transaction_manager_example.cpp`

56. **Build System Enhancements**:
   - Modified `build.sh` to support `--yaml` and `--examples` options
   - Updated `build_cpp_dbc.sh` to support `--yaml` and `--examples` options
   - Resolved issue with Conan generators directory path in `build_cpp_dbc.sh`
   - Improved error handling in build scripts

57. **Enhanced Testing Support**:
   - Added `--test` option to `build.sh` to enable building tests
   - Created `run_test.sh` script in the root directory to run tests
   - Modified `build_cpp_dbc.sh` to accept the `--test` option
   - Changed default value of `CPP_DBC_BUILD_TESTS` to `OFF` in CMakeLists.txt
   - Fixed path issues in `run_test_cpp_dbc.sh`
   - Added `yaml-cpp` dependency to `libs/cpp_dbc/conanfile.txt` for tests
   - Added automatic project build detection in `run_test.sh`

58. **Improved Helper Script**:
   - Enhanced `helper.sh` to support multiple commands in a single invocation
   - Added `--test` option to build tests
   - Added `--run-test` option to run tests
   - Added `--ldd-bin-ctr` option to check executable dependencies inside the container
   - Added `--ldd-build-bin` option to check executable dependencies locally
   - Added `--run-build-bin` option to run the executable
   - Added `--run-ctr` option to run the container
   - Added `--show-labels-ctr` option to show container labels
   - Added `--show-tags-ctr` option to show container tags
   - Added `--show-env-ctr` option to show container environment variables
   - Improved error handling and reporting
   - Added support for getting executable name from `.dist_build`
   - Added structured logging system with dedicated log directories
   - Added automatic log rotation keeping 4 most recent logs
   - Added color support in terminal while keeping clean logs
   - Added unbuffer usage to preserve colors in terminal output
   - Added sed command to strip ANSI color codes from log files
   - Added `--check-test-log` option to analyze test log files for failures and issues
   - Added `--check-test-log=PATH` option to analyze a specific log file
   - Implemented detection of test failures from Catch2 output
   - Implemented detection of memory leaks from Valgrind output
   - Implemented detection of Valgrind errors from ERROR SUMMARY

59. **Enhanced Docker Container Build**:
   - Modified `build.dist.sh` to accept the same parameters as `build.sh`
   - Implemented automatic detection of shared library dependencies
   - Added mapping of libraries to their corresponding Debian packages
   - Ensured correct package names for special cases (e.g., libsasl2-2)
   - Improved Docker container creation with only necessary dependencies
   - Fixed numbering of build steps for better readability

60. **Reorganized Project Structure**:
   - Moved all content from `src/libs/cpp_dbc/` to `libs/cpp_dbc/` in the root of the project
   - Reorganized the internal structure of the library:
     - Created `src/` directory for implementation files (.cpp)
     - Created `include/cpp_dbc/` directory for header files (.hpp)
     - Moved drivers to their respective folders: `src/drivers/` and `include/cpp_dbc/drivers/`
   - Updated all CMake files to reflect the new directory structure
   - Updated include paths in all source files

61. **Modified Build Configuration**:
   - Changed the default build type from Release to Debug
   - Added support for `--release` argument to build in Release mode when needed
   - Ensured both the library and the main project use the same build type
   - Resolved issues with finding the correct `conan_toolchain.cmake` file based on build type

62. **Fixed VS Code Debugging Issues**:
   - Added the correct include path for nlohmann_json library
   - Updated the `c_cpp_properties.json` file to include the necessary paths
   - Modified CMakeLists.txt to add explicit include directories
   - Configured VSCode to use CMakeTools for better integration:
     - Updated settings.json to use direct configuration (without presets)
     - Added Debug and Release configurations
     - Configured proper include paths for IntelliSense
     - Added CMake-based debugging configuration with direct path to executable
     - Fixed tasks.json to support CMake builds
   - Added .vscode/build_with_props.sh script to extract defines from c_cpp_properties.json
   - Added support for building with MySQL and PostgreSQL from VSCode
   - Added automatic extension installation task
   - Identified IntelliSense issue with preprocessor definitions:
     - IntelliSense may show `USE_POSTGRESQL` as 0 even after compilation has activated it
     - Solution: After compilation, use CTRL+SHIFT+P and select "Developer: Reload Window"
     - This forces IntelliSense to reload with the updated preprocessor definitions

63. **Previous changes**:
   - Fixed build issues with PostgreSQL support
   - Modified the installation directory to use `build/libs/cpp_dbc`
   - Suppressed CMake warnings with `-Wno-dev` flag
   - Fixed the empty database driver status
   - Fixed build issues when MySQL support is disabled
   - Updated C++ standard to C++23

The project structure suggests a mature library with complete implementations for:

- Core database connectivity interfaces
- MySQL and PostgreSQL drivers
- Connection pooling
- Transaction management

## Next Steps

Potential next steps for the project could include:

1. **Further Code Quality Improvements**:
   - Implementing static analysis tools integration (clang-tidy, cppcheck)
   - Adding runtime sanitizers (AddressSanitizer, UndefinedBehaviorSanitizer)
   - Implementing code coverage reporting
   - Adding CI/CD pipeline for automated quality checks

2. **Additional Database Support**:
   - Implementing drivers for additional database systems (Oracle, SQL Server)
   - Creating a common test suite for all database implementations

3. **Feature Enhancements**:
   - Adding support for batch operations
   - Implementing metadata retrieval functionality
   - Adding support for stored procedures and functions
   - Implementing connection event listeners
   - Enhancing BLOB functionality with compression and encryption

4. **Performance Optimizations**:
   - Statement caching
   - Connection validation improvements
   - Result set streaming for large datasets

5. **Documentation and Examples**:
   - Creating comprehensive API documentation
   - Developing more example applications
   - Writing performance benchmarks
   - Expanding YAML configuration support with more features
   - Adding more configuration formats (JSON, XML, etc.)

6. **Testing**:
   - Implementing comprehensive unit tests
   - Creating integration tests with actual databases
   - Developing performance tests

## Active Decisions and Considerations

Key architectural decisions that appear to be guiding the project:

1. **Interface-Based Design**:
   - All database operations are defined through abstract interfaces
   - Concrete implementations are provided for specific databases
   - This approach enables easy extension to new database systems

2. **Resource Management**:
   - Smart pointers are used for automatic resource management
   - RAII principles are followed for resource cleanup
   - Connection pooling is used for efficient connection management

3. **Thread Safety**:
   - Connection pool is thread-safe for concurrent access
   - Transaction manager supports cross-thread transactions
   - Individual connections are not thread-safe and should not be shared

4. **Error Handling**:
   - Custom SQLException class for consistent error reporting
   - Exceptions are used for error propagation
   - Each driver translates database-specific errors to common format

## Important Patterns and Preferences

The codebase demonstrates several important patterns and preferences:

1. **Design Patterns**:
   - Abstract Factory: DriverManager creates database-specific drivers
   - Factory Method: Connections create PreparedStatements and ResultSets
   - Proxy: PooledConnection wraps actual Connection objects
   - Object Pool: ConnectionPool manages connection resources
   - Command: PreparedStatement encapsulates database operations

2. **Coding Style**:
   - Clear class hierarchies with proper inheritance
   - Consistent method naming following JDBC conventions
   - Use of modern C++ features (smart pointers, lambdas, etc.)
   - Comprehensive error handling
   - Member variables prefixed with `m_` to avoid shadowing issues
   - Consistent use of static_cast<> for numeric conversions
   - Strict warning flags to enforce code quality standards

3. **API Design**:
   - Method names follow JDBC conventions (executeQuery, executeUpdate, etc.)
   - Parameter types are consistent across different implementations
   - Return types use smart pointers for automatic resource management

4. **Configuration Approach**:
   - Connection pool uses a configuration structure with sensible defaults
   - Transaction manager has configurable timeouts
   - URL-based connection strings follow JDBC format
   - Database configurations can be loaded from YAML files
   - Configuration classes follow a clean, object-oriented design
   - Configuration loading is abstracted to support multiple formats

## Learnings and Project Insights

Key insights from the project:

1. **Database Abstraction**:
   - The project demonstrates effective abstraction of database-specific details
   - Common interfaces enable code reuse across different database systems
   - Driver architecture allows for easy extension

2. **Resource Management**:
   - Connection pooling significantly improves performance for database operations
   - Smart pointers simplify resource management and prevent leaks
   - Background maintenance threads handle cleanup tasks

3. **Transaction Handling**:
   - Distributed transactions require careful coordination
   - Transaction IDs provide a way to track transactions across threads
   - Timeout handling is essential for preventing resource leaks
   - Transaction isolation levels control how concurrent transactions interact
   - Different isolation levels provide trade-offs between consistency and performance
   - JDBC-compatible isolation levels (READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE)

4. **Thread Safety**:
   - Proper synchronization is critical for connection pooling
   - Condition variables enable efficient waiting for resources
   - Atomic operations provide thread-safe counters without heavy locks
