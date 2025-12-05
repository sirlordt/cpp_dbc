# CPP_DBC Project Progress

## Current Status

The CPP_DBC library appears to be in a functional state with the following components implemented:

1. **Core Interfaces**: All core interfaces (`Connection`, `PreparedStatement`, `ResultSet`, `Driver`) are defined
2. **MySQL Implementation**: Complete implementation of MySQL driver
3. **PostgreSQL Implementation**: Complete implementation of PostgreSQL driver
4. **SQLite Implementation**: Complete implementation of SQLite driver
5. **Connection Pool**: Fully implemented with configuration options for MySQL, PostgreSQL, and SQLite
6. **Transaction Manager**: Fully implemented with transaction tracking, timeout, and improved resource management
7. **Connection Options**: Support for database-specific connection options in all drivers
8. **BLOB Support**: Complete implementation of Binary Large Object (BLOB) support for all database drivers
9. **Logging System**: Structured logging system with dedicated log directories and automatic rotation
10. **VSCode Integration**: Complete VSCode configuration with build tasks and extension management
11. **JSON Support**: Complete implementation of JSON data type support for MySQL and PostgreSQL
12. **Code Quality**: Comprehensive warning flags and compile-time checks with improved variable naming
13. **Benchmark System**: Comprehensive benchmark system for database operations with different data sizes

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

1. **Benchmark Baseline and Comparison System**:
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
     - Updated TODO.md to mark benchmark task as completed

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

2. **Distribution Package Build System (DEB and RPM)**:
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

2. **JSON Data Type Support**:
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
   - Added test.jpg file for BLOB testing
   - Updated CMakeLists.txt to copy test.jpg to the build directory
   - Added comprehensive test cases for image file BLOB operations in:
     * `test_mysql_real_blob.cpp`
     * `test_postgresql_real_blob.cpp`
     * `test_sqlite_real_blob.cpp`
   - Added tests for storing, retrieving, and verifying image data integrity
   - Added tests for writing retrieved image data to temporary files

2. **BLOB Support Implementation**:
   - Added comprehensive BLOB (Binary Large Object) support for all database drivers
   - Implemented base classes: Blob, InputStream, OutputStream
   - Added memory-based implementations: MemoryBlob, MemoryInputStream, MemoryOutputStream
   - Added file-based implementations: FileInputStream, FileOutputStream
   - Implemented database-specific BLOB classes: MySQLBlob, PostgreSQLBlob, SQLiteBlob
   - Added BLOB support methods to ResultSet and PreparedStatement interfaces
   - Added test cases for BLOB operations in all database drivers
   - Added test.jpg file for BLOB testing
   - Updated Spanish documentation with BLOB support information

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

4. **Valgrind Suppressions Removal**:
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

4. **SQLite Driver Implementation**:
   - Added SQLite database driver support with full implementation
   - Added SQLite connection string format support
   - Added SQLite-specific transaction isolation level handling
   - Added SQLite test cases and configuration
   - Updated build scripts with `--sqlite` option
   - Added SQLite dependency detection and installation in build scripts
   - Added SQLite configuration in test YAML file

2. **Connection Pool and Transaction Manager Improvements**:
   - Enhanced connection pool with better connection handling
   - Added transaction isolation level preservation when returning connections to pool
   - Improved connection closing mechanism with proper cleanup
   - Added better handling of connection errors
   - Enhanced transaction manager with better resource management
   - Improved connection return to pool after transaction completion

3. **Database Configuration Integration**:
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
   - Added database_config.cpp implementation file

3. **YAML Configuration Support**:
   - Added optional YAML configuration support to the library
   - Created database configuration classes in `include/cpp_dbc/config/database_config.hpp`
   - Implemented YAML configuration loader in `include/cpp_dbc/config/yaml_config_loader.hpp` and `src/config/yaml_config_loader.cpp`
   - Added `--yaml` option to `build.sh` and `build_cpp_dbc.sh` to enable YAML support
   - Modified CMakeLists.txt to conditionally include YAML-related files based on USE_CPP_YAML flag
   - Fixed issue with Conan generators directory path in `build_cpp_dbc.sh`

2. **Examples Improvements**:
   - Added `--examples` option to `build.sh` and `build_cpp_dbc.sh` to build examples
   - Created YAML configuration example in `examples/config_example.cpp`
   - Added example YAML configuration file in `examples/example_config.yml`
   - Created script to run the configuration example in `examples/run_config_example.sh`
   - Fixed initialization issue in `examples/transaction_manager_example.cpp`

3. **Project Structure Reorganization**:
   - Moved all content from `src/libs/cpp_dbc/` to `libs/cpp_dbc/` in the root of the project.
   - Reorganized the internal structure of the library with separate `src/` and `include/cpp_dbc/` directories.
   - Updated all CMake files and include paths to reflect the new directory structure.
   - This provides a cleaner, more standard library organization that follows C++ best practices.

2. **Build Configuration**:
   - Changed the default build type from Release to Debug to facilitate development and debugging.
   - Added support for `--release` argument to build in Release mode when needed.
   - Fixed issues with finding the correct `conan_toolchain.cmake` file based on build type.
   - Ensured both the library and the main project use the same build type.

3. **VS Code Debugging Issues**:
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

4. **Installation Directory**:
   - Modified the installation path to use `/home/dsystems/Desktop/projects/cpp/cpp_dbc/build/libs/cpp_dbc` for consistency.
   - Updated the main CMakeLists.txt to look for the library in the new location.

5. **Testing Infrastructure**:
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

6. **Enhanced Helper Script**:
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

7. **Improved Docker Container Build**:
   - Enhanced `build.dist.sh` to accept the same parameters as `build.sh`
   - Implemented automatic detection of shared library dependencies
   - Added mapping of libraries to their corresponding Debian packages
   - Ensured correct package names for special cases (e.g., libsasl2-2)
   - Improved Docker container creation with only necessary dependencies
   - Fixed numbering of build steps for better readability

7. **Previous Fixed Issues**:
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
