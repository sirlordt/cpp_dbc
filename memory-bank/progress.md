# CPP_DBC Project Progress

## Current Status

The CPP_DBC library appears to be in a functional state with the following components implemented:

1. **Core Interfaces**: All core interfaces (`Connection`, `PreparedStatement`, `ResultSet`, `Driver`) are defined
2. **MySQL Implementation**: Complete implementation of MySQL driver with optional thread-safety
3. **PostgreSQL Implementation**: Complete implementation of PostgreSQL driver with optional thread-safety
4. **SQLite Implementation**: Complete implementation of SQLite driver with optional thread-safety
5. **Firebird Implementation**: Complete implementation of Firebird SQL driver with optional thread-safety
6. **MongoDB Implementation**: Complete implementation of MongoDB document database driver with optional thread-safety
7. **ScyllaDB Implementation**: Complete implementation of ScyllaDB columnar database driver with optional thread-safety and connection pool
8. **Connection Pool**: Fully implemented with configuration options for MySQL, PostgreSQL, SQLite, Firebird, MongoDB, ScyllaDB, and Redis
9. **Transaction Manager**: Fully implemented with transaction tracking, timeout, and improved resource management
10. **Connection Options**: Support for database-specific connection options in all drivers
11. **BLOB Support**: Complete implementation of Binary Large Object (BLOB) support for all relational database drivers
12. **Logging System**: Structured logging system with dedicated log directories and automatic rotation
13. **VSCode Integration**: Complete VSCode configuration with build tasks and extension management
14. **JSON Support**: Complete implementation of JSON data type support for MySQL and PostgreSQL
15. **Code Quality**: Comprehensive warning flags and compile-time checks with improved variable naming
16. **Benchmark System**: Comprehensive benchmark system for database operations with different data sizes
17. **Thread-Safe Drivers**: Optional thread-safety support for all database drivers with mutex protection
18. **Exception-Free API**: Implementation of exception-free error handling using std::expected pattern for Redis, PostgreSQL, and ScyllaDB drivers
19. **Columnar DB Connection Pool**: Complete implementation of connection pool for ScyllaDB columnar database

The project includes example code demonstrating:
- Basic database operations
- Connection pooling
- Transaction management across threads
- YAML configuration loading and management
- Database configuration integration with connection classes

## What Works

### Core Functionality
- Database connection establishment
- SQL query execution
- Prepared statement creation and execution
- Result set processing
- Error handling through exceptions
- Binary Large Object (BLOB) handling
- Structured logging with automatic log rotation
- Comprehensive warning flags and compile-time checks

### MySQL Support
- Connection to MySQL databases
- Prepared statements with parameter binding
- Result set processing
- Transaction management
- Connection options for database-specific settings
- BLOB data handling with MySQLBlob implementation
- JSON data type support with comprehensive query capabilities

### PostgreSQL Support
- Connection to PostgreSQL databases
- Prepared statements with parameter binding
- Result set processing
- Transaction management
- Configurable connection options (gssencmode, client_encoding, etc.)
- BLOB data handling with PostgreSQLBlob implementation
- JSON and JSONB data type support with comprehensive query capabilities

### Firebird SQL Support
- Connection to Firebird SQL databases (version 2.5+, 3.0+, 4.0+)
- Prepared statements with parameter binding
- Result set processing with all data types
- Transaction management with isolation levels
- BLOB support with lazy loading and streaming capabilities
- Connection pooling with FirebirdConnectionPool
- Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
- Comprehensive test coverage:
  - JOIN operations (INNER, LEFT, RIGHT, FULL OUTER)
  - JSON operations tests
  - Thread-safety stress tests
  - Transaction isolation tests
  - Transaction manager tests
- Benchmark suite for performance testing:
  - SELECT, INSERT, UPDATE, DELETE operations
  - Small, medium, large, and extra-large datasets
- Connection URL format: `cpp_dbc:firebird://host:port/path/to/database.fdb`

### Connection Pooling
- Dynamic connection creation and management
- Connection validation
- Idle connection cleanup
- Connection timeout handling
- Thread-safe connection borrowing and returning
- Database-specific connection pool implementations:
  - MySQL: MySQLConnectionPool
  - PostgreSQL: PostgreSQLConnectionPool
  - SQLite: SQLiteConnectionPool
  - Firebird: FirebirdConnectionPool
  - MongoDB: MongoDBConnectionPool
  - ScyllaDB: ScyllaConnectionPool (ColumnarDBConnectionPool)
  - Redis: RedisConnectionPool

### SQLite Support
- Connection to SQLite databases
- Prepared statements with parameter binding
- Result set processing
- Transaction management
- In-memory database support
- File-based database support
- Connection pooling with SQLiteConnectionPool
- Improved resource management with enable_shared_from_this
- Better statement cleanup with weak_ptr-based active connections tracking
- Enhanced connection closing with sqlite3_close_v2
- Configurable SQLite pragmas (journal_mode, synchronous, foreign_keys)
- Safeguards to ensure connections are created with make_shared
- Comprehensive test cases for JOIN operations:
  - INNER JOIN with multiple tables
  - LEFT JOIN with NULL checks
  - Tests for invalid columns and type mismatches
- Debug output option for troubleshooting SQLite-specific issues
- BLOB data handling with SQLiteBlob implementation
- Comprehensive test cases for BLOB operations

### MongoDB Support
- Connection to MongoDB databases (version 4.0+)
- Document database operations (CRUD)
- Collection management (create, drop, list)
- Document insertion (single and batch)
- Document querying with filters
- Document updates with operators
- Document deletion
- Cursor-based result iteration
- BSON data type support
- Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
- Connection URL format: `mongodb://host:port/database` or `mongodb://username:password@host:port/database?authSource=admin`
- Build system integration with `USE_MONGODB` option
- Debug output option with `--debug-mongodb` flag
- Comprehensive benchmark suite:
  - `benchmark_mongodb_select.cpp` with SELECT operations benchmarks
    - Document query benchmarks with different filter complexities
    - Performance testing for simple queries, complex queries, and aggregations
    - Benchmarks for different result set sizes (small, medium, large datasets)
  - `benchmark_mongodb_insert.cpp` with INSERT operations benchmarks
    - Single document insertion performance
    - Batch document insertion for various batch sizes
    - Performance testing with different document complexities
  - `benchmark_mongodb_update.cpp` with UPDATE operations benchmarks
    - Performance testing for different update operators ($set, $inc, $push, etc.)
    - Single document update vs. multi-document update performance
    - Update performance with different document complexities
  - `benchmark_mongodb_delete.cpp` with DELETE operations benchmarks
    - Single document deletion performance
    - Bulk document deletion performance
    - Performance comparison for different deletion strategies
  - Memory usage tracking in benchmark baselines
    - Collection of heap memory usage metrics during operations
    - Peak memory usage analysis for different operation types
    - Comparison of memory efficiency across operations
  - Performance comparison with relational databases
    - Side-by-side performance metrics for equivalent operations
    - Analysis of strengths and weaknesses compared to relational databases
  - Baseline creation and comparison functionality
    - Storing benchmark results for different system configurations
    - Comparing results against established baseline measurements
    - Tracking performance changes over time
- Extended test coverage:
  - `test_mongodb_real_json.cpp` with JSON operations tests
    - Testing of basic JSON document structures
    - Nested JSON object handling
    - Array operations in JSON documents
    - JSON query operators ($eq, $gt, $lt, $in, $nin, etc.)
    - JSON update operations and modifications
    - Complex JSON aggregation operations
  - `test_mongodb_thread_safe.cpp` with thread safety stress tests
    - Concurrent operations from multiple threads
    - Connection sharing across threads
    - Thread safety with high contention scenarios
    - Resource management under concurrent load
  - `test_mongodb_real_join.cpp` with join operations (aggregation pipeline)
    - $lookup operator for joining collections
    - Complex multi-stage aggregation pipelines
    - Testing of different join types (equivalent to SQL inner, left, etc.)
  - JSON operations (basic, nested, arrays, query operators, updates, aggregation)
  - Thread safety with multiple connections and shared connections
  - Join operations using $lookup and other MongoDB aggregation pipeline operations

### ScyllaDB Support
- Connection to ScyllaDB/Cassandra databases
- Prepared statements with parameter binding
- Result set processing with all supported data types
- Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
- Connection URL format: `cpp_dbc:scylladb://host:port/keyspace` or `cpp_dbc:scylladb://username:password@host:port/keyspace`
- Default port: 9042
- Build system integration with `USE_SCYLLADB` option
- Debug output option with `--debug-scylladb` flag
- Connection pooling with `ScyllaConnectionPool`:
  - Factory pattern with `create` static methods for pool creation
  - `ColumnarDBConnectionPool` base class for columnar databases
  - `ColumnarPooledDBConnection` wrapper class for pooled connections
  - Smart pointer-based pool lifetime tracking for memory safety
  - Connection validation with CQL query (`SELECT now() FROM system.local`)
  - Configurable pool parameters (initial size, max size, min idle, etc.)
  - Background maintenance thread for connection cleanup
- Exception-free API with nothrow variants:
  - All methods return `expected<T, DBException>` for error handling
  - Consistent with Redis and PostgreSQL exception-free patterns
- Comprehensive test coverage:
  - Basic connection and authentication
  - CQL query execution and prepared statements
  - Data type handling and conversions
  - JOIN operation emulation (not natively supported in CQL)
  - Thread safety with multiple concurrent connections
  - Connection pool operations and stress testing
- Benchmark suite for performance testing:
  - SELECT, INSERT, UPDATE, DELETE operations
  - Memory usage tracking and baseline comparison
- Examples for ScyllaDB operations:
  - Basic operations (`scylla_example.cpp`)
  - BLOB handling (`scylla_blob_example.cpp`)
  - JSON data (`scylla_json_example.cpp`)
  - Connection pool (`scylladb_connection_pool_example.cpp`)
- JOIN emulation for ScyllaDB:
  - Implementation for INNER JOIN and RIGHT JOIN operations
  - Manual implementation using multiple queries
  - Result aggregation and sorting to emulate JOIN behavior
  - Comprehensive test cases in test_scylladb_real_inner_join.cpp and test_scylladb_real_right_join.cpp
- Support for tables, keyspaces, and indexes
- Support for filtering operations (with ALLOW FILTERING)
- Thread safety testing with high concurrency stress tests

### Transaction Management
- Transaction creation and tracking
- Cross-thread transaction coordination
- Transaction timeout handling
- Automatic cleanup of abandoned transactions
- Transaction isolation levels (JDBC-compatible)
  - TRANSACTION_NONE
  - TRANSACTION_READ_UNCOMMITTED
  - TRANSACTION_READ_COMMITTED
  - TRANSACTION_REPEATABLE_READ
  - TRANSACTION_SERIALIZABLE
- Database-specific isolation level implementations:
  - MySQL: All levels supported (default: REPEATABLE_READ)
  - PostgreSQL: All levels supported (default: READ_COMMITTED)
  - SQLite: Only SERIALIZABLE supported
  - Firebird: All levels supported (default: READ_COMMITTED)

### YAML Configuration
- Database configuration loading from YAML files
- Connection pool configuration from YAML files
- Test query configuration from YAML files
- Conditional compilation with USE_CPP_YAML flag
- Example application demonstrating YAML configuration usage
- Integration between database configuration and connection classes
- Support for BLOB configuration in YAML files

### Debug and Testing Support
- Debug output options for ConnectionPool, TransactionManager, and SQLite driver
- Ability to run specific tests by tag
- Support for multiple test tags using + separator
- Enhanced test script functionality with better parameter handling
- Improved test command construction

## What's Left to Build

Based on the current state of the project, potential areas for enhancement include:

1. **Additional Database Support**:
   - Oracle Database driver
   - Microsoft SQL Server driver
   - ODBC driver for generic database support

2. **Feature Enhancements**:
   - Batch statement execution
   - Stored procedure support
   - Metadata retrieval (table information, etc.)
   - Connection event listeners
   - Statement caching
   - Enhanced BLOB functionality (streaming, compression, etc.)

3. **Performance Optimizations**:
   - Statement caching
   - Result set streaming for large datasets
   - Connection validation optimization

4. **Documentation and Examples**:
   - More comprehensive documentation
   - Additional example applications
   - More configuration examples with different database systems
   - Examples of using configuration with connection pooling and transaction management

5. **Testing**:
   - Expand the basic unit tests implemented with Catch2
   - Add more comprehensive test cases for all components
   - Implement integration tests with actual databases
   - Develop performance tests
   - Add test cases for other database operations (e.g., RIGHT JOIN, FULL JOIN)
   - Enhance debug output options for other components

## Known Issues
### Recent Improvements

#### This PR (driver_code_split)

1. **Driver Code Split Refactoring** (2026-01-31 23:41:14 PST):
   - **Major Code Reorganization:**
     - Split all database driver implementations from single large files into multiple smaller, focused files
     - Each driver now has its own subdirectory with internal header and split implementation files
   - **Split Drivers:**
     - SQLite: `src/drivers/relational/sqlite/` (sqlite_internal.hpp, driver_01.cpp, connection_*.cpp, prepared_statement_*.cpp, result_set_*.cpp)
     - MySQL: `src/drivers/relational/mysql/` (mysql_internal.hpp, driver_01.cpp, connection_*.cpp, prepared_statement_*.cpp, result_set_*.cpp)
     - PostgreSQL: `src/drivers/relational/postgresql/` (postgresql_internal.hpp, driver_01.cpp, connection_*.cpp, prepared_statement_*.cpp, result_set_*.cpp)
     - Firebird: `src/drivers/relational/firebird/` (firebird_internal.hpp, driver_01.cpp, connection_*.cpp, prepared_statement_*.cpp, result_set_*.cpp)
     - MongoDB: `src/drivers/document/mongodb/` (mongodb_internal.hpp, driver_01.cpp, connection_*.cpp, collection_*.cpp, cursor_*.cpp, document_*.cpp)
     - Redis: `src/drivers/kv/redis/` (redis_internal.hpp, driver_01.cpp, connection_*.cpp)
     - ScyllaDB: `src/drivers/columnar/scylladb/` (scylladb_internal.hpp, driver_01.cpp, connection_*.cpp, prepared_statement_*.cpp, result_set_*.cpp)
   - **Build System Updates:**
     - Updated `libs/cpp_dbc/CMakeLists.txt` to compile all new split source files
   - **Benefits:**
     - Faster incremental compilation - only changed files need to be recompiled
     - Better code organization - each class component has dedicated files
     - Easier navigation - find specific functionality quickly
     - Reduced file complexity - smaller, more focused files
     - Better IDE support - smaller files load and parse faster

#### Previous PR (test_parallel)

2. **SonarCloud Code Quality Fixes and Connection Pool Improvements** (2026-01-29 16:23:21 PST):
   - **Critical Race Condition Fix in Connection Pool Return Flow:**
     - Fixed race condition in `close()` and `returnToPool()` methods across all pool types
     - Bug: `m_closed` was reset to `false` AFTER `returnConnection()` completed
     - Fix: Reset `m_closed` to `false` BEFORE calling `returnConnection()`
     - Added catch-all exception handlers to ensure correct state on any exception
   - **Connection Pool ConstructorTag Pattern (PassKey Idiom):**
     - Added `DBConnectionPool::ConstructorTag` struct to enable `std::make_shared` while enforcing factory pattern
     - Updated all connection pool classes to use ConstructorTag
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
     - Fixed `--clean` and `--rebuild` flags to only apply during initial build
   - **SonarQube Issues Fetch Script Enhancement:**
     - Made `--file` parameter optional (fetches ALL issues when not specified)
     - Added pagination support for fetching all issues
   - **Test Fixes:**
     - Fixed Firebird connection pool test to use `initialIdleCount` instead of hardcoded value

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

#### Previous PR (sonnar_cloud_issues)

2. **SonarCloud Code Quality Fixes and Helper Script Enhancement** (2026-01-20 15:17:41):
   - **SonarCloud Configuration Updates:**
     - Consolidated SonarCloud configuration into `.sonarcloud.properties`
     - Deleted redundant `sonar-project.properties` file
     - Added exclusion for cognitive complexity rule (cpp:S3776) - some functions inherently require complex logic
     - Added exclusion for nesting depth rule (cpp:S134) - control flow > 3 levels allowed
     - Added documentation comments for each excluded rule
   - **Code Quality Improvements in `blob.hpp`:**
     - Added `explicit` keyword to `FileOutputStream` constructor
     - Removed redundant `m_position(0)` initialization in `MemoryInputStream` (already initialized at declaration)
   - **Code Quality Improvements in Columnar DB Connection Pool:**
     - Changed `ColumnarDBConnection` destructor from `virtual` to `override`
     - Added in-class member initialization for `m_running{true}` and `m_activeConnections{0}` in `ColumnarDBConnectionPool`
     - Added in-class member initialization for `m_active{false}` and `m_closed{false}` in `ColumnarPooledDBConnection`
     - Changed `validateConnection` method to `const`
     - Changed `close()` and `isPoolValid()` methods to `final` in appropriate classes
     - Replaced `std::lock_guard<std::mutex>` with `std::scoped_lock` for modern C++ style
     - Changed `std::unique_lock<std::mutex>` to `std::unique_lock` with CTAD
     - Replaced generic `std::exception` catches with specific `DBException` catches
     - Simplified conditional logic in `getIdleDBConnection()` method
     - Removed redundant member initializations from constructor initializer lists
   - **Helper Script Enhancement (`helper.sh`):**
     - Enhanced `extract_executed_tests()` function to track line numbers for executed tests
     - Updated `display_test_execution_table()` to show log file location with line numbers for executed tests
     - Added relative path display for log files (e.g., `./logs/test/output.log:42`)
     - Improved test execution output to help users quickly navigate to specific test results

#### Previous PR (scylladb_driver_fix1)

2. **ScyllaDB Native DATE Type Support Fix** (2026-01-18 22:49:41): *(Included in previous PR)*
   - Fixed ScyllaDB driver to properly handle native Cassandra DATE type:
     - **Reading DATE values:** Added `CASS_VALUE_TYPE_DATE` support with proper uint32 conversion
     - **Writing DATE values:** Changed from int64 (timestamp) to uint32 (date) binding
     - **Technical:** Uses `cass_date_from_epoch` and `timegm` for correct UTC handling

3. **Connection Pool Deadlock Prevention and ScyllaDB Naming Consistency Fixes** (2026-01-18 22:33:51): *(Included in previous PR)*
   - Fixed potential deadlock in all connection pool implementations:
     - **Deadlock Prevention:**
       - Changed from sequential `std::lock_guard` calls to `std::scoped_lock` for consistent lock ordering
       - Fixed in `columnar_db_connection_pool.cpp` (ScyllaDB)
       - Fixed in `document_db_connection_pool.cpp` (MongoDB)
       - Fixed in `kv_db_connection_pool.cpp` (Redis)
       - Fixed in `relational_db_connection_pool.cpp` (MySQL, PostgreSQL, SQLite, Firebird)
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
   - Fixed documentation numbering in `cppdbc-package.md`
   - Fixed typos in `TODO.md`

#### Other commits on branch (not part of previous PR)

1. **Connection Pool Race Condition Fix and Code Quality Improvements** (2026-01-18 23:26:52): *(Not part of previous PR)*
   - Fixed connection pool race condition in all database types (relational, document, columnar, key-value)
   - Added pool size recheck under lock to prevent exceeding `m_maxSize` under concurrent creation
   - Improved return connection logic with null checks and proper cleanup
   - Fixed MongoDB stub driver and driver exception marks to use UUID-style marks
   - Fixed `blob.hpp` variable initialization and ScyllaDB variable naming

4. **VSCode IntelliSense Automatic Synchronization System** (2026-01-18 02:59:56 PM PST): *(Not part of previous PR)*
   - Added automatic synchronization system for VSCode IntelliSense:
     - **New Scripts:**
       - `.vscode/sync_intellisense.sh` - Quick sync without rebuilding
       - `.vscode/regenerate_intellisense.sh` - Rebuild from last config
       - `.vscode/update_defines.sh` - Update defines from compile_commands.json
       - `.vscode/detect_include_paths.sh` - Detect system include paths
     - **Documentation:**
       - `.vscode/README_INTELLISENSE.md` - Comprehensive guide for IntelliSense configuration
     - **Features:**
       - Build parameters automatically saved to `build/.build_args`
       - Configuration state saved to `build/.build_config`
       - Quick sync option that doesn't require rebuild
   - Removed `EXCEPTION_FREE_ANALYSIS.md` (analysis completed and integrated)

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
       - Factory pattern with `create` static methods for pool creation
       - Smart pointer-based pool lifetime tracking for memory safety
       - Connection validation with CQL query (`SELECT now() FROM system.local`)
       - Configurable pool parameters (initial size, max size, min idle, etc.)
       - Background maintenance thread for connection cleanup
       - Thread-safe connection management
     - **Build System Updates:**
       - Renamed `USE_SCYLLA` to `USE_SCYLLADB` for consistency
       - Renamed driver files from `driver_scylla` to `driver_scylladb`
       - Renamed namespace from `Scylla` to `ScyllaDB`
       - Updated CMakeLists.txt to include the new source files
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
       - Consistent with Redis and PostgreSQL exception-free patterns

6. **ScyllaDB Columnar Database Driver Support** (2026-01-15 11:46:28 PM PST):
   - Added complete ScyllaDB columnar database driver implementation:
     - **New Core Columnar Database Interfaces:**
       - Implemented `core/columnar/columnar_db_connection.hpp` - Base connection interface for columnar databases
       - Implemented `core/columnar/columnar_db_driver.hpp` - Base driver interface for columnar databases
       - Implemented `core/columnar/columnar_db_prepared_statement.hpp` - Prepared statement interface for columnar databases
       - Implemented `core/columnar/columnar_db_result_set.hpp` - Result set interface for columnar databases
     - **ScyllaDB Driver Implementation:**
       - Added `drivers/columnar/driver_scylladb.hpp` - ScyllaDB driver class declarations
       - Added `src/drivers/columnar/driver_scylladb.cpp` - Full ScyllaDB driver implementation
       - Added `cmake/FindCassandra.cmake` - CMake module for Cassandra C++ driver detection
     - **Driver Features:**
       - Full support for ScyllaDB/Cassandra databases
       - Connection management with proper resource cleanup using smart pointers
       - Prepared statement support with parameter binding
       - Result set handling with all supported data types
       - Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)
     - **Build System Updates:**
       - Added `USE_SCYLLA` option to CMakeLists.txt (default: OFF)
       - Added `--scylla` and `--scylla-off` options to build.sh
       - Added `--debug-scylla` option for ScyllaDB driver debug output
       - Added `scylla` option to helper.sh for --run-build, --run-test, and --run-benchmarks
     - **Test Coverage:**
       - Basic connection and authentication
       - CQL query execution and prepared statements
       - Data type handling and conversions
       - JOIN operation emulation (not natively supported in CQL)
       - Thread safety with multiple concurrent connections
     - **JOIN Emulation:**
       - Implementation for INNER JOIN and RIGHT JOIN operations
       - Manual implementation using multiple queries
       - Result aggregation and sorting to emulate JOIN behavior

7. **PostgreSQL Exception-Free API Implementation** (2026-01-06 08:11:44 PM PST):
   - Added comprehensive exception-free API for PostgreSQL driver operations:
     - **Implementation Details:**
       - Implemented nothrow versions of all PostgreSQL driver methods using `std::nothrow_t` parameter
       - All methods return `expected<T, DBException>` with clear error information
       - Replaced "NOT_IMPLEMENTED" placeholders with full implementations
       - Comprehensive error handling with unique error codes for each method
       - Follows the inverted implementation pattern where nothrow methods contain the real logic
     - **Operations Covered:**
       - Connection management (prepareStatement, executeQuery, executeUpdate)
       - Transaction handling (beginTransaction, commit, rollback)
       - Transaction isolation level management
       - Auto-commit settings and status checks
       - Connection URL parsing and validation
       - Parameter binding and execution in prepared statements
     - **Error Handling Approach:**
       - Error propagation with expected<T, DBException>
       - Preserves call stack information in error cases
       - Consistent error code format across all methods
       - Clear error messages with operation and failure reason
     - **Benefits:**
       - Consistent API pattern with other database drivers
       - Performance improvements for code using nothrow API
       - Safer error handling without exception overhead
       - Full compatibility with both exception and non-exception usage patterns

8. **Redis Exception-Free API Implementation** (2026-01-03 05:23:03 PM PST):
   - Added comprehensive exception-free API for Redis driver operations:
     - **Implementation Details:**
       - Added nothrow versions of all Redis driver methods using `std::nothrow_t` parameter
       - All methods return `expected<T, DBException>` with clear error information
       - Added `std::expected`-based error handling approach as alternative to exceptions
       - Custom `cpp_dbc::expected<T, E>` implementation for pre-C++23 compatibility
       - Automatic use of native `std::expected` when C++23 is available
       - Comprehensive error handling with unique error codes for each method
       - Added `#include <new>` for `std::nothrow_t`
     - **Operations Covered:**
       - Key-Value operations (setString, getString, exists, deleteKey, etc.)
       - List operations (listPushLeft, listPushRight, listPopLeft, etc.)
       - Hash operations (hashSet, hashGet, hashDelete, hashExists, etc.)
       - Set operations (setAdd, setRemove, setIsMember, etc.)
       - Sorted Set operations (sortedSetAdd, sortedSetRemove, sortedSetScore, etc.)
       - Server operations (scanKeys, ping, flushDB, getServerInfo, etc.)
     - **Error Handling Approach:**
       - Error propagation with expected<T, DBException>
       - Preserves call stack information in error cases
       - Consistent error code format across all methods
       - Clear error messages with operation and failure reason
       - Support for monadic operations: and_then(), transform(), or_else()
     - **Benefits:**
       - No exception overhead in performance-critical code
       - More explicit error handling with monadic operations
       - Better interoperability with code that can't use exceptions
       - Same comprehensive error information as exception-based API
       - Natural migration path - existing code works without changes

9. **MongoDB Document Database Driver Support** (2025-12-27):
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
       - Tests for connection, CRUD operations, and collection management
   - Connection URL format: `mongodb://host:port/database` or `mongodb://username:password@host:port/database?authSource=admin`
   - Required packages: libmongoc-dev, libbson-dev, libmongocxx-dev, libbsoncxx-dev (Debian/Ubuntu)

10. **Directory Restructuring for Multi-Database Type Support** (2025-12-27):
   - Reorganized project directory structure to support multiple database types:
     - **Core Interfaces Moved:**
       - Moved `relational/` → `core/relational/`
       - Files: `relational_db_connection.hpp`, `relational_db_driver.hpp`, `relational_db_prepared_statement.hpp`, `relational_db_result_set.hpp`
     - **Driver Files Moved:**
       - Moved `drivers/` → `drivers/relational/`
       - Files: `driver_firebird.hpp/cpp`, `driver_mysql.hpp/cpp`, `driver_postgresql.hpp/cpp`, `driver_sqlite.hpp/cpp`
       - BLOB headers: `firebird_blob.hpp`, `mysql_blob.hpp`, `postgresql_blob.hpp`, `sqlite_blob.hpp`
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

11. **Connection Pool Memory Safety Improvements** (2025-12-27):
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

12. **API Naming Convention Refactoring** (2025-12-26):
   - Renamed classes and methods to use "DB" prefix for better clarity and consistency:
     - **Driver Classes:**
       - `MySQLDriver` → `MySQLDBDriver`
       - `PostgreSQLDriver` → `PostgreSQLDBDriver`
       - `SQLiteDriver` → `SQLiteDBDriver`
       - `FirebirdDriver` → `FirebirdDBDriver`
     - **Connection Classes:**
       - `Connection` → `RelationalDBConnection` (for relational database connections)
     - **Configuration Classes:**
       - `ConnectionPoolConfig` → `DBConnectionPoolConfig`
     - **DriverManager Methods:**
       - `getConnection()` → `getDBConnection()`
     - **TransactionManager Methods:**
       - `getTransactionConnection()` → `getTransactionDBConnection()`
     - **Driver Methods:**
       - `connect()` → `connectRelational()` (for relational database connections)
   - Updated all benchmark files to use new class and method names
   - Updated all test files to use new class and method names
   - Updated all example files to use new class and method names
   - Updated main.cpp to use new driver class names

13. **BLOB Memory Safety Improvements with Smart Pointers** (2025-12-22):
   - Migrated all BLOB implementations from raw pointers to smart pointers for improved memory safety:
     - **Firebird BLOB:**
       - Changed from raw `isc_db_handle*` and `isc_tr_handle*` to `std::weak_ptr<FirebirdConnection>`
       - Added `getConnection()` helper method that throws `DBException` if connection is closed
       - Added `getDbHandle()` and `getTrHandle()` inline methods for safe handle access
       - Added `FirebirdBlob` as friend class to `FirebirdConnection` for private member access
     - **MySQL BLOB:**
       - Changed from raw `MYSQL*` to `std::weak_ptr<MYSQL>`
       - Added `getMySQLConnection()` helper method that throws `DBException` if connection is closed
       - Added `isConnectionValid()` method to check connection state
     - **PostgreSQL BLOB:**
       - Changed from raw `PGconn*` to `std::weak_ptr<PGconn>`
       - Added `getPGConnection()` helper method that throws `DBException` if connection is closed
       - Added `isConnectionValid()` method to check connection state
       - Improved `remove()` method to gracefully handle closed connections
     - **SQLite BLOB:**
       - Changed from raw `sqlite3*` to `std::weak_ptr<sqlite3>`
       - Added `getSQLiteConnection()` helper method that throws `DBException` if connection is closed
       - Added `isConnectionValid()` method to check connection state
   - Updated driver implementations to use new BLOB constructors
   - Benefits: Automatic detection of closed connections, prevention of use-after-free errors, clear ownership semantics

14. **Firebird Driver Database Creation and Error Handling Improvements** (2025-12-21):
   - Added database creation support to Firebird driver:
     - Added `createDatabase()` method to `FirebirdDriver` for creating new Firebird databases
     - Added `command()` method for executing driver-specific commands
     - Added `executeCreateDatabase()` method to `FirebirdConnection` for CREATE DATABASE statements
     - Support for specifying page size (default: 4096) and character set (default: UTF8)
   - Improved Firebird error message handling:
     - Fixed critical bug: error messages now captured BEFORE calling cleanup functions
     - Uses separate status array for cleanup operations to avoid overwriting error information
     - Improved `interpretStatusVector()` with SQLCODE interpretation using `isc_sql_interprete`
     - Combined SQLCODE and `fb_interpret` messages for comprehensive error information
     - Added fallback error message with status vector values for debugging
   - Added new example:
     - Added `firebird_reserved_word_example.cpp` demonstrating reserved word handling in Firebird
     - Updated `CMakeLists.txt` to include the new example when Firebird is enabled

15. **Firebird SQL Driver Enhancements and Benchmark Support** (2025-12-21):
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

16. **Firebird SQL Database Driver Support** (2025-12-20):
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

17. **Thread-Safe Database Driver Operations**:
   - Added optional thread-safety support for database driver operations:
     - **New CMake Option:**
       - Added `DB_DRIVER_THREAD_SAFE` option (default: ON) to enable/disable thread-safe operations
       - Use `-DDB_DRIVER_THREAD_SAFE=OFF` to disable thread-safety for single-threaded applications
     - **Build Script Updates:**
       - Added `--db-driver-thread-safe-off` option to all build scripts
       - Updated helper.sh with new option for --run-build, --run-build-dist, --run-test, and --run-benchmarks
     - **Driver Implementations:**
       - MySQL: Added mutex protection in executeQuery(), executeUpdate(), prepareStatement(), and close()
       - PostgreSQL: Added mutex protection in executeQuery(), executeUpdate(), prepareStatement(), and close()
       - SQLite: Added mutex protection in executeQuery(), executeUpdate(), prepareStatement(), and close()
     - **New Thread-Safety Tests:**
       - Added test_mysql_thread_safe.cpp, test_postgresql_thread_safe.cpp, test_sqlite_thread_safe.cpp
       - Tests include: multiple threads with individual connections, connection pool concurrent access,
         concurrent read operations, high concurrency stress test, rapid connection open/close stress test
   - Benefits: Safe concurrent access, protection against race conditions, optional feature for performance

18. **Smart Pointer Migration for Database Drivers**:
   - Migrated all database drivers from raw pointers to smart pointers for improved memory safety:
     - **MySQL Driver:**
       - Added `MySQLResDeleter` custom deleter for `MYSQL_RES*` with `unique_ptr`
       - Added `MySQLStmtDeleter` custom deleter for `MYSQL_STMT*` with `unique_ptr`
       - Added `MySQLDeleter` custom deleter for `MYSQL*` with `shared_ptr`
       - Changed `MySQLConnection::m_mysql` from raw pointer to `MySQLHandle` (`shared_ptr<MYSQL>`)
       - Changed `MySQLPreparedStatement::m_mysql` from raw pointer to `weak_ptr<MYSQL>` for safe connection reference
       - Changed `MySQLPreparedStatement::m_stmt` from raw pointer to `MySQLStmtHandle` (`unique_ptr<MYSQL_STMT>`)
       - Changed `MySQLResultSet::m_result` from raw pointer to `MySQLResHandle` (`unique_ptr<MYSQL_RES>`)
       - Added `validateResultState()` and `validateCurrentRow()` helper methods
       - Added `getMySQLConnection()` helper method for safe connection access
     - **PostgreSQL Driver:**
       - Added `PGresultDeleter` custom deleter for `PGresult*` with `unique_ptr`
       - Added `PGconnDeleter` custom deleter for `PGconn*` with `shared_ptr`
       - Changed `PostgreSQLConnection::m_conn` from raw pointer to `PGconnHandle` (`shared_ptr<PGconn>`)
       - Changed `PostgreSQLPreparedStatement::m_conn` from raw pointer to `weak_ptr<PGconn>` for safe connection reference
       - Changed `PostgreSQLResultSet::m_result` from raw pointer to `PGresultHandle` (`unique_ptr<PGresult>`)
       - Added `getPGConnection()` helper method for safe connection access
     - **SQLite Driver:**
       - Added `SQLiteStmtDeleter` custom deleter for `sqlite3_stmt*` with `unique_ptr`
       - Added `SQLiteDbDeleter` custom deleter for `sqlite3*` with `shared_ptr`
       - Changed `SQLiteConnection::m_db` from raw pointer to `shared_ptr<sqlite3>`
       - Changed `SQLitePreparedStatement::m_db` from raw pointer to `weak_ptr<sqlite3>` for safe connection reference
       - Changed `SQLitePreparedStatement::m_stmt` from raw pointer to `SQLiteStmtHandle` (`unique_ptr<sqlite3_stmt>`)
       - Changed `SQLiteResultSet::m_stmt` to use `getStmt()` accessor method
       - Changed `m_activeStatements` from `set<shared_ptr>` to `set<weak_ptr>` to avoid preventing statement destruction
       - Added `getSQLiteConnection()` helper method for safe connection access
       - Removed obsolete `activeConnections` static list and related mutex
   - Benefits of smart pointer migration:
     - Automatic resource cleanup even in case of exceptions
     - Prevention of memory leaks through RAII
     - Safe detection of closed connections via `weak_ptr`
     - Clear ownership semantics documented in code
     - Elimination of manual `delete`/`free` calls

19. **Benchmark Baseline and Comparison System**:
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

20. **Benchmark System Migration to Google Benchmark**:
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

21. **SQLite Driver Thread Safety Improvements**:
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

22. **Benchmark and Testing Framework Improvements**:
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

23. **Test Code Refactoring**:
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

24. **Benchmark System Implementation**:
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
     - Updated TODO.md to mark benchmark task as completed

25. **Code Quality Improvements with Comprehensive Warning Flags**:
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

26. **Distribution Package Build System (DEB and RPM)**:
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

27. **Stack Trace Improvements with libdw Support**:
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

28. **Mejoras de Seguridad y Tipos de Datos**:
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

29. **Exception Handling and Stack Trace Improvements**:
   - Added comprehensive stack trace capture and error tracking:
     - Added `backward.hpp` library for stack trace capture and analysis
     - Created `system_utils::StackFrame` structure to store stack frame information
     - Implemented `system_utils::captureCallStack()` function to capture the current call stack
     - Implemented `system_utils::printCallStack()` function to print stack traces
     - Added libdw support for enhanced stack trace information with detailed function names and line numbers
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
   - Added stack trace testing functionality to main.cpp

30. **JSON Data Type Support**:
   - Added comprehensive support for JSON data types in MySQL and PostgreSQL
   - Added test files for JSON operations:
     - `test_mysql_real_json.cpp` for MySQL JSON testing
     - `test_postgresql_real_json.cpp` for PostgreSQL JSON and JSONB testing
   - Added helper function `generateRandomJson()` in test_main.cpp for generating test JSON data
   - Implemented tests for various JSON operations:
     - Basic JSON storage and retrieval
     - JSON path expressions and operators
     - JSON search and filtering
     - JSON modification and transformation
     - JSON validation and error handling
     - JSON indexing and performance testing
     - JSON aggregation functions
   - Added support for database-specific JSON features:
     - MySQL: JSON_EXTRACT, JSON_SEARCH, JSON_CONTAINS, JSON_ARRAYAGG
     - PostgreSQL: JSON operators (@>, <@, ?, ?|, ?&), jsonb_set, jsonb_insert, GIN indexing
   - Made `close()` method virtual in PostgreSQLResultSet for better inheritance support
   - Updated CMakeLists.txt to include the new JSON test files


31. **Logging System Improvements**:
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

32. **VSCode Integration**:
   - Added VSCode configuration files:
     - Added .vscode/c_cpp_properties.json with proper include paths
     - Added .vscode/tasks.json with build tasks
     - Added .vscode/build_with_props.sh script to extract defines from c_cpp_properties.json
     - Added support for building with MySQL and PostgreSQL from VSCode
     - Added automatic extension installation task

33. **License Header Updates**:
   - Added standardized license headers to all .cpp and .hpp files
   - Created update_headers.sh script to automate header updates
   - Headers include copyright information, license terms, and file descriptions
   - All example files and test files now have proper headers

34. **BLOB Support for Image Files**:
   - Added support for storing and retrieving image files as BLOBs
   - Added helper functions in test_main.cpp for binary file operations:
     * `readBinaryFile()` to read binary data from files
     * `writeBinaryFile()` to write binary data to files
     * `getTestImagePath()` to get the path to the test image
     * `generateRandomTempFilename()` to create temporary filenames
   - Added test.jpg file for BLOB testing
   - Updated CMakeLists.txt to copy test.jpg to the build directory
   - Added comprehensive test cases for image file BLOB operations in:
     * `test_mysql_real_blob.cpp`
     * `test_postgresql_real_blob.cpp`
     * `test_sqlite_real_blob.cpp`
   - Added tests for storing, retrieving, and verifying image data integrity
   - Added tests for writing retrieved image data to temporary files

35. **BLOB Support Implementation**:
   - Added comprehensive BLOB (Binary Large Object) support for all database drivers
   - Implemented base classes: Blob, InputStream, OutputStream
   - Added memory-based implementations: MemoryBlob, MemoryInputStream, MemoryOutputStream
   - Added file-based implementations: FileInputStream, FileOutputStream
   - Implemented database-specific BLOB classes: MySQLBlob, PostgreSQLBlob, SQLiteBlob
   - Added BLOB support methods to ResultSet and PreparedStatement interfaces
   - Added test cases for BLOB operations in all database drivers
   - Added test.jpg file for BLOB testing
   - Updated Spanish documentation with BLOB support information

36. **SQLite JOIN Operations Testing**:
   - Added comprehensive test cases for SQLite JOIN operations
   - Added `test_sqlite_real_inner_join.cpp` with INNER JOIN test cases
   - Added `test_sqlite_real_left_join.cpp` with LEFT JOIN test cases
   - Enhanced test coverage for complex JOIN operations with multiple tables
   - Added tests for JOIN operations with NULL checks and invalid columns
   - Added tests for type mismatches in JOIN conditions

37. **Debug Output Options**:
   - Added debug output options for better troubleshooting
   - Added `--debug-pool` option to enable debug output for ConnectionPool
   - Added `--debug-txmgr` option to enable debug output for TransactionManager
   - Added `--debug-sqlite` option to enable debug output for SQLite driver
   - Added `--debug-all` option to enable all debug output
   - Updated build scripts to pass debug options to CMake
   - Added debug output parameters to helper.sh script
   - Added logging to files for build and test output

38. **Test Script Enhancements**:
   - Enhanced test script functionality
   - Added `--run-test="tag"` option to run specific tests by tag
   - Added support for multiple test tags using + separator (e.g., "tag1+tag2+tag3")
   - Added debug options to run_test.sh script
   - Improved test command construction with better parameter handling
   - Changed default AUTO_MODE to false in run_test.sh
   - Added logging to files for test output with automatic log rotation

39. **Valgrind Suppressions Removal**:
   - Removed valgrind-suppressions.txt file as it's no longer needed with improved PostgreSQL driver

### Fixed Issues
1. **SQLite Connection Management Improvements**:
   - Enhanced SQLiteConnection to inherit from std::enable_shared_from_this
   - Replaced raw pointer tracking with weak_ptr in activeConnections list
   - Improved connection cleanup with weak_ptr-based reference tracking
   - Added proper error handling for shared_from_this() usage
   - Added safeguards to ensure connections are created with make_shared
   - Fixed potential memory issues in connection and statement cleanup

2. **Connection Options Support**:
   - Added connection options support for all database drivers
   - Added options parameter to Driver::connect() method
   - Added options parameter to all driver implementations
   - Added options parameter to ConnectionPool constructor
   - Added options map to ConnectionPoolConfig
   - Updated DatabaseConfig to provide options through getOptions() method
   - Renamed original getOptions() to getOptionsObj() for backward compatibility
   - Updated all tests to support the new options parameter

3. **PostgreSQL Driver Improvements**:
   - Enhanced PostgreSQL driver with better configuration options
   - Added support for passing connection options from configuration to PQconnectdb
   - Made gssencmode configurable through options map (default: disable)
   - Added error codes to exception messages for better debugging

4. **SQLite Driver Improvements**:
   - Enhanced SQLite driver with better configuration options
   - Added support for configuring SQLite pragmas through options map
   - Added support for journal_mode, synchronous, and foreign_keys options
   - Added error codes to exception messages for better debugging

5. **SQLite Driver Implementation**:
   - Added SQLite database driver support with full implementation
   - Added SQLite connection string format support
   - Added SQLite-specific transaction isolation level handling
   - Added SQLite test cases and configuration
   - Updated build scripts with `--sqlite` option
   - Added SQLite dependency detection and installation in build scripts
   - Added SQLite configuration in test YAML file

6. **Connection Pool and Transaction Manager Improvements**:
   - Enhanced connection pool with better connection handling
   - Added transaction isolation level preservation when returning connections to pool
   - Improved connection closing mechanism with proper cleanup
   - Added better handling of connection errors
   - Enhanced transaction manager with better resource management
   - Improved connection return to pool after transaction completion

7. **Database Configuration Integration**:
   - Added integration between database configuration and connection classes
   - Created new `config_integration_example.cpp` with examples of different connection methods
   - Added `createConnection()` method to `DatabaseConfig` class
   - Added `createConnection()` and `createConnectionPool()` methods to `DatabaseConfigManager`
   - Added new methods to `DriverManager` to work with configuration classes
   - Added script to run the configuration integration example
   - Changed URL format from "cppdbc:" to "cpp_dbc:" for consistency
   - Updated all examples, tests, and code to use the new format

8. **Connection Pool Enhancements**:
   - Moved `ConnectionPoolConfig` from connection_pool.hpp to config/database_config.hpp
   - Enhanced `ConnectionPoolConfig` with more options and better encapsulation
   - Added new constructors and factory methods to `ConnectionPool`
   - Added integration between connection pool and database configuration
   - Improved code organization with forward declarations
   - Added database_config.cpp implementation file

9. **YAML Configuration Support**:
   - Added optional YAML configuration support to the library
   - Created database configuration classes in `include/cpp_dbc/config/database_config.hpp`
   - Implemented YAML configuration loader in `include/cpp_dbc/config/yaml_config_loader.hpp` and `src/config/yaml_config_loader.cpp`
   - Added `--yaml` option to `build.sh` and `build_cpp_dbc.sh` to enable YAML support
   - Modified CMakeLists.txt to conditionally include YAML-related files based on USE_CPP_YAML flag
   - Fixed issue with Conan generators directory path in `build_cpp_dbc.sh`

10. **Examples Improvements**:
   - Added `--examples` option to `build.sh` and `build_cpp_dbc.sh` to build examples
   - Created YAML configuration example in `examples/config_example.cpp`
   - Added example YAML configuration file in `examples/example_config.yml`
   - Created script to run the configuration example in `examples/run_config_example.sh`
   - Fixed initialization issue in `examples/transaction_manager_example.cpp`

11. **Project Structure Reorganization**:
   - Moved all content from `src/libs/cpp_dbc/` to `libs/cpp_dbc/` in the root of the project.
   - Reorganized the internal structure of the library with separate `src/` and `include/cpp_dbc/` directories.
   - Updated all CMake files and include paths to reflect the new directory structure.
   - This provides a cleaner, more standard library organization that follows C++ best practices.

12. **Build Configuration**:
   - Changed the default build type from Release to Debug to facilitate development and debugging.
   - Added support for `--release` argument to build in Release mode when needed.
   - Fixed issues with finding the correct `conan_toolchain.cmake` file based on build type.
   - Ensured both the library and the main project use the same build type.

13. **VS Code Debugging Issues**:
   - Added the correct include path for nlohmann_json library.
   - Updated the `c_cpp_properties.json` file to include the necessary paths.
   - Modified CMakeLists.txt to add explicit include directories.
   - Configured VSCode to use CMakeTools for better integration:
     - Updated settings.json to use direct configuration (without presets)
     - Added Debug and Release configurations
     - Configured proper include paths for IntelliSense
     - Added CMake-based debugging configuration with direct path to executable
     - Fixed tasks.json to support CMake builds
   - Added .vscode/build_with_props.sh script to extract defines from c_cpp_properties.json
   - Added support for building with MySQL and PostgreSQL from VSCode
   - Added automatic extension installation task
   - Identified and documented IntelliSense issue with preprocessor definitions:
     - IntelliSense may show `USE_POSTGRESQL` as 0 even after compilation has activated it
     - Solution: After compilation, use CTRL+SHIFT+P and select "Developer: Reload Window"
     - This forces IntelliSense to reload with the updated preprocessor definitions
   - These changes ensure that VS Code can find all header files and properly debug the application.

14. **Installation Directory**:
   - Modified the installation path to use `/home/dsystems/Desktop/projects/cpp/cpp_dbc/build/libs/cpp_dbc` for consistency.
   - Updated the main CMakeLists.txt to look for the library in the new location.

15. **Testing Infrastructure**:
   - Implemented basic unit tests using Catch2
   - Created test directory structure in `libs/cpp_dbc/test`
   - Added build scripts for tests with ASAN and Valgrind support
   - Documented ASAN issues in `memory-bank/asan_issues.md`
   - Added `--test` option to `build.sh` to enable building tests
   - Created `run_test.sh` script in the root directory to run tests
   - Modified `build_cpp_dbc.sh` to accept the `--test` option
   - Changed default value of `CPP_DBC_BUILD_TESTS` to `OFF` in CMakeLists.txt
   - Fixed path issues in `run_test_cpp_dbc.sh`
   - Added `yaml-cpp` dependency to `libs/cpp_dbc/conanfile.txt` for tests
   - Added automatic project build detection in `run_test.sh`

16. **Enhanced Helper Script**:
   - Improved `helper.sh` to support multiple commands in a single invocation
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

17. **Improved Docker Container Build**:
   - Enhanced `build.dist.sh` to accept the same parameters as `build.sh`
   - Implemented automatic detection of shared library dependencies
   - Added mapping of libraries to their corresponding Debian packages
   - Ensured correct package names for special cases (e.g., libsasl2-2)
   - Improved Docker container creation with only necessary dependencies
   - Fixed numbering of build steps for better readability

18. **Previous Fixed Issues**:
   - Fixed PostgreSQL header propagation issues.
   - Fixed naming conflicts with executable names.
   - Suppressed CMake warnings with `-Wno-dev` flag.
   - Fixed environment variable issues for database driver status.
   - Fixed MySQL-only dependencies by separating the DriverManager implementation.
   - Updated C++ standard to C++23.

### Potential Areas of Concern
Based on the code structure, potential areas of concern might include:

1. **Resource Management**:
   - Ensuring proper cleanup of database resources in all error scenarios
   - Handling connection failures gracefully
   - Maintaining proper memory management with warning flags like -Wconversion and -Wshadow

2. **Thread Safety**:
   - Potential race conditions in complex multi-threaded scenarios
   - Deadlock prevention in transaction management

3. **Error Handling**:
   - Consistent error reporting across different database implementations
   - Proper propagation of database-specific errors

4. **Performance**:
   - Connection pool tuning for different workloads
   - Optimization of prepared statement handling

5. **AddressSanitizer Issues**:
   - Random crashes during compilation with ASAN enabled
   - See `memory-bank/asan_issues.md` for detailed information and workarounds
   - Consider using Valgrind as an alternative for memory checking

## Evolution of Project Decisions

The project shows evidence of several key architectural decisions:

1. **JDBC-Inspired API**:
   - The decision to follow JDBC patterns provides familiarity for developers with Java experience
   - This approach enables a clean separation between interface and implementation

2. **Driver Architecture**:
   - The driver-based approach allows for easy extension to support additional databases
   - The `DriverManager` pattern simplifies client code

3. **Connection Pooling**:
   - The implementation of a dedicated connection pool addresses performance concerns
   - The configurable nature allows adaptation to different usage patterns

4. **Transaction Management**:
   - The separate transaction manager component addresses the complexity of distributed transactions
   - The design supports cross-thread transaction coordination

5. **Smart Pointer Usage**:
   - The use of `std::shared_ptr` simplifies resource management
   - This approach reduces the risk of resource leaks

6. **Configuration Management**:
   - The decision to implement a configuration system allows for centralized database settings
   - The YAML support is optional to avoid unnecessary dependencies
   - The configuration classes are designed to be extensible for other formats
   - This approach provides flexibility in how database connections are configured

These decisions reflect a focus on:
- Clean architecture
- Extensibility
- Performance
- Resource safety
- Developer ergonomics
- Code quality and robustness
- Type safety and compile-time checks
