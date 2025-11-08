# Changelog

## 2025-11-08 05:49:00 AM -0800 [Current]

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