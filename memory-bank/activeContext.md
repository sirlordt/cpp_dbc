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

1. MySQL and PostgreSQL databases
2. Connection pooling
3. Transaction management
4. Prepared statements and result sets

The code is organized in a modular fashion with clear separation between interfaces and implementations, following object-oriented design principles.

## Recent Changes

Recent changes to the codebase include:

1. **Database Configuration Integration**:
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
   - Added `--ldd` option to check executable dependencies inside the container
   - Added `--ldd-bin` option to check executable dependencies locally
   - Added `--run-bin` option to run the executable
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
   - Implementing drivers for additional database systems (Oracle, SQLite, SQL Server)
   - Creating a common test suite for all database implementations

2. **Feature Enhancements**:
   - Adding support for batch operations
   - Implementing metadata retrieval functionality
   - Adding support for stored procedures and functions
   - Implementing connection event listeners

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

4. **Thread Safety**:
   - Proper synchronization is critical for connection pooling
   - Condition variables enable efficient waiting for resources
   - Atomic operations provide thread-safe counters without heavy locks
