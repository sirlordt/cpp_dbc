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

1. MySQL, PostgreSQL, SQLite, and Firebird SQL databases
2. Connection pooling for all supported databases
3. Transaction management with isolation levels
4. Prepared statements and result sets
5. BLOB (Binary Large Object) support for all database drivers
6. YAML configuration for database connections and pools
7. Comprehensive testing for JOIN operations and BLOB handling
8. Debug output options for troubleshooting
9. Benchmark system for database operations performance testing

The code is organized in a modular fashion with clear separation between interfaces and implementations, following object-oriented design principles.

## Recent Changes

Recent changes to the codebase include:

1. **Firebird Driver Database Creation and Error Handling Improvements** (2025-12-21):
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

2. **Firebird SQL Driver Enhancements and Benchmark Support** (2025-12-21):
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

2. **Firebird SQL Database Driver Support** (2025-12-20):
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

3. **Thread-Safe Database Driver Operations**:
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

2. **Smart Pointer Migration for Database Drivers**:
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

2. **Benchmark Baseline and Comparison System**:
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

2. **Benchmark System Migration to Google Benchmark**:
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

2. **SQLite Driver Thread Safety Improvements**:
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

2. **Benchmark and Testing Framework Improvements**:
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

2. **Test Code Refactoring**:
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

2. **Benchmark System Implementation**:
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

2. **Code Quality Improvements with Comprehensive Warning Flags**:
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


1. **Distribution Package Build System (DEB and RPM)**:
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

2. **Stack Trace Improvements with libdw Support**:
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

2. **Mejoras de Seguridad y Tipos de Datos**:
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

2. **Exception Handling and Stack Trace Improvements**:
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

1. **Logging System Improvements**:
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

2. **VSCode Integration**:
   - Added VSCode configuration files:
     - Added .vscode/c_cpp_properties.json with proper include paths
     - Added .vscode/tasks.json with build tasks
     - Added .vscode/build_with_props.sh script to extract defines from c_cpp_properties.json
     - Added support for building with MySQL and PostgreSQL from VSCode
     - Added automatic extension installation task

3. **License Header Updates**:
   - Added standardized license headers to all .cpp and .hpp files
   - Created update_headers.sh script to automate header updates
   - Headers include copyright information, license terms, and file descriptions
   - All example files and test files now have proper headers


1. **BLOB Support for Image Files**:
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

2. **BLOB Support Implementation**:
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

2. **SQLite JOIN Operations Testing**:
   - Added comprehensive test cases for SQLite JOIN operations
   - Added `test_sqlite_real_inner_join.cpp` with INNER JOIN test cases
   - Added `test_sqlite_real_left_join.cpp` with LEFT JOIN test cases
   - Enhanced test coverage for complex JOIN operations with multiple tables
   - Added tests for JOIN operations with NULL checks and invalid columns
   - Added tests for type mismatches in JOIN conditions

2. **Debug Output Options**:
   - Added debug output options for better troubleshooting
   - Added `--debug-pool` option to enable debug output for ConnectionPool
   - Added `--debug-txmgr` option to enable debug output for TransactionManager
   - Added `--debug-sqlite` option to enable debug output for SQLite driver
   - Added `--debug-all` option to enable all debug output
   - Updated build scripts to pass debug options to CMake
   - Added debug output parameters to helper.sh script
   - Added logging to files for build and test output

3. **Test Script Enhancements**:
   - Enhanced test script functionality
   - Added `--run-test="tag"` option to run specific tests by tag
   - Added support for multiple test tags using + separator (e.g., "tag1+tag2+tag3")
   - Added debug options to run_test.sh script
   - Improved test command construction with better parameter handling
   - Changed default AUTO_MODE to false in run_test.sh
   - Added logging to files for test output with automatic log rotation
   - Added test log analysis functionality with `--check-test-log` option
   - Implemented detection of test failures, memory leaks, and Valgrind errors

4. **Valgrind Suppressions Removal**:
   - Removed valgrind-suppressions.txt file as it's no longer needed with improved PostgreSQL driver

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
   - Added options parameter to all driver implementations (MySQL, PostgreSQL, SQLite)
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

4. **SQLite Connection Pool Implementation**:
   - Added SQLite connection pool support with `SQLiteConnectionPool` class
   - Added SQLite-specific connection pool configuration in test_db_connections.yml
   - Added SQLite connection pool tests in test_connection_pool_real.cpp
   - Improved connection handling for SQLite connections
   - Added transaction isolation level support for SQLite pools

5. **SQLite Driver Improvements**:
   - Enhanced SQLite driver with better resource management
   - Added static list of active connections for statement cleanup
   - Improved connection closing with sqlite3_close_v2 instead of sqlite3_close
   - Added better handling of prepared statements with shared_from_this
   - Fixed memory leaks in SQLite connection and statement handling

6. **Test Script Enhancements**:
   - Added new options to run_test_cpp_dbc.sh
   - Added `--auto` option to automatically continue to next test set if tests pass
   - Added `--gssapi-leak-ok` option to ignore GSSAPI leaks in PostgreSQL with Valgrind

4. **Transaction Isolation Level Support**:
   - Added transaction isolation level support following JDBC standard
   - Implemented `TransactionIsolationLevel` enum with JDBC-compatible isolation levels
   - Added `setTransactionIsolation` and `getTransactionIsolation` methods to Connection interface
   - Implemented methods in MySQL and PostgreSQL drivers
   - Updated PooledConnection to delegate isolation level methods to underlying connection
   - Default isolation levels set to database defaults (REPEATABLE READ for MySQL, READ COMMITTED for PostgreSQL)
   - Added comprehensive tests for transaction isolation levels in test_transaction_isolation.cpp

2. **Database Configuration Integration**:
   - Added integration between database configuration and connection classes
   - Created new `config_integration_example.cpp` with examples of different connection methods
   - Added `createConnection()` method to `DatabaseConfig` class
   - Added `createConnection()` and `createConnectionPool()` methods to `DatabaseConfigManager`
   - Added new methods to `DriverManager` to work with configuration classes
   - Added script to run the configuration integration example
   - Changed URL format from "cppdbc:" to "cpp_dbc:" for consistency
   - Updated all examples, tests, and code to use the new format

2. **Connection Pool Enhancements**:
   - Moved `ConnectionPoolConfig` from connection_pool.hpp to config/database_config.hpp
   - Enhanced `ConnectionPoolConfig` with more options and better encapsulation
   - Added new constructors and factory methods to `ConnectionPool`
   - Added integration between connection pool and database configuration
   - Improved code organization with forward declarations

3. **YAML Configuration Support**:
   - Added optional YAML configuration support to the library
   - Created database configuration classes in `include/cpp_dbc/config/database_config.hpp`
   - Implemented YAML configuration loader in `include/cpp_dbc/config/yaml_config_loader.hpp` and `src/config/yaml_config_loader.cpp`
   - Added `--yaml` option to `build.sh` and `build_cpp_dbc.sh` to enable YAML support
   - Modified CMakeLists.txt to conditionally include YAML-related files based on USE_CPP_YAML flag

2. **Examples Improvements**:
   - Added `--examples` option to `build.sh` and `build_cpp_dbc.sh` to build examples
   - Created YAML configuration example in `examples/config_example.cpp`
   - Added example YAML configuration file in `examples/example_config.yml`
   - Created script to run the configuration example in `examples/run_config_example.sh`
   - Fixed initialization issue in `examples/transaction_manager_example.cpp`

3. **Build System Enhancements**:
   - Modified `build.sh` to support `--yaml` and `--examples` options
   - Updated `build_cpp_dbc.sh` to support `--yaml` and `--examples` options
   - Fixed issue with Conan generators directory path in `build_cpp_dbc.sh`
   - Improved error handling in build scripts

4. **Enhanced Testing Support**:
   - Added `--test` option to `build.sh` to enable building tests
   - Created `run_test.sh` script in the root directory to run tests
   - Modified `build_cpp_dbc.sh` to accept the `--test` option
   - Changed default value of `CPP_DBC_BUILD_TESTS` to `OFF` in CMakeLists.txt
   - Fixed path issues in `run_test_cpp_dbc.sh`
   - Added `yaml-cpp` dependency to `libs/cpp_dbc/conanfile.txt` for tests
   - Added automatic project build detection in `run_test.sh`

2. **Improved Helper Script**:
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

3. **Enhanced Docker Container Build**:
   - Modified `build.dist.sh` to accept the same parameters as `build.sh`
   - Implemented automatic detection of shared library dependencies
   - Added mapping of libraries to their corresponding Debian packages
   - Ensured correct package names for special cases (e.g., libsasl2-2)
   - Improved Docker container creation with only necessary dependencies
   - Fixed numbering of build steps for better readability

3. **Reorganized Project Structure**:
   - Moved all content from `src/libs/cpp_dbc/` to `libs/cpp_dbc/` in the root of the project
   - Reorganized the internal structure of the library:
     - Created `src/` directory for implementation files (.cpp)
     - Created `include/cpp_dbc/` directory for header files (.hpp)
     - Moved drivers to their respective folders: `src/drivers/` and `include/cpp_dbc/drivers/`
   - Updated all CMake files to reflect the new directory structure
   - Updated include paths in all source files

4. **Modified Build Configuration**:
   - Changed the default build type from Release to Debug
   - Added support for `--release` argument to build in Release mode when needed
   - Ensured both the library and the main project use the same build type
   - Fixed issues with finding the correct `conan_toolchain.cmake` file based on build type

5. **Fixed VS Code Debugging Issues**:
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

6. Previous changes:
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

1. **Additional Database Support**:
   - Implementing drivers for additional database systems (Oracle, SQL Server)
   - Creating a common test suite for all database implementations

2. **Feature Enhancements**:
   - Adding support for batch operations
   - Implementing metadata retrieval functionality
   - Adding support for stored procedures and functions
   - Implementing connection event listeners
   - Enhancing BLOB functionality with compression and encryption

3. **Performance Optimizations**:
   - Statement caching
   - Connection validation improvements
   - Result set streaming for large datasets

4. **Documentation and Examples**:
   - Creating comprehensive API documentation
   - Developing more example applications
   - Writing performance benchmarks
   - Expanding YAML configuration support with more features
   - Adding more configuration formats (JSON, XML, etc.)

5. **Testing**:
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
