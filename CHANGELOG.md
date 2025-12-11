# Changelog

## 2025-12-10 10:36:15 PM PST [Current]

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