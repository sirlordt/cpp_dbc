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

1. MySQL, PostgreSQL, and SQLite databases
2. Connection pooling for all supported databases
3. Transaction management with isolation levels
4. Prepared statements and result sets
5. BLOB (Binary Large Object) support for all database drivers
6. YAML configuration for database connections and pools
7. Comprehensive testing for JOIN operations and BLOB handling
8. Debug output options for troubleshooting

The code is organized in a modular fashion with clear separation between interfaces and implementations, following object-oriented design principles.

## Recent Changes

Recent changes to the codebase include:

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

3. **Test Script Enhancements**:
   - Enhanced test script functionality
   - Added `--run-test="tag"` option to run specific tests by tag
   - Added support for multiple test tags using + separator (e.g., "tag1+tag2+tag3")
   - Added debug options to run_test.sh script
   - Improved test command construction with better parameter handling

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
