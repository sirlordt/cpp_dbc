# C++ Database Connectivity (CPP_DBC) Library

## For Impatience

If you're in a hurry and want to get started quickly, check out the [Quick Start Guide](QUICKSTART.md) for common commands and examples.

```bash
# Run tests with rebuild, SQLite, PostgreSQL, MySQL, YAML, Valgrind, auto mode, run once
./helper.sh --bk-combo-01

# Clean build with PostgreSQL, MySQL, SQLite, YAML, tests, and examples
./helper.sh --mc-combo-01

# Clean build of distribution with PostgreSQL, MySQL, SQLite, YAML, tests, and examples
./helper.sh --kfc-combo-01
```

---

This project provides a C++ Database Connectivity library inspired by JDBC, with support for MySQL, PostgreSQL, and SQLite databases. The library includes connection pooling, transaction management, support for different transaction isolation levels, and comprehensive BLOB handling with image file support.

## Features

- **Database Abstraction**: Unified API for different database systems
- **Connection Pooling**: Efficient connection management
- **Transaction Management**: Support for distributed transactions
- **Transaction Isolation Levels**: JDBC-compatible isolation levels (READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE) with database-specific implementations
- **Prepared Statements**: Protection against SQL injection
- **Conditional Compilation**: Build with only the database drivers you need
- **Modern C++ Design**: Uses C++23 features and RAII principles
- **YAML Configuration**: Optional support for loading database configurations from YAML files
- **BLOB Support**: Complete implementation of Binary Large Object (BLOB) support for all database drivers, including image file storage and retrieval
- **JSON Support**: Native handling of JSON data types in MySQL and PostgreSQL with comprehensive query capabilities

## Database Support

The library currently supports:

- **MySQL**: Full support for MySQL databases (enabled by default)
- **PostgreSQL**: Full support for PostgreSQL databases (disabled by default)
- **SQLite**: Full support for SQLite databases (disabled by default)

Each database driver can be enabled or disabled at compile time to reduce dependencies. By default, only MySQL support is enabled.

## Project Structure

- **Core Components**:
  - `include/cpp_dbc/cpp_dbc.hpp`: Core interfaces and types
  - `include/cpp_dbc/connection_pool.hpp` & `src/connection_pool.cpp`: Connection pooling implementation
  - `include/cpp_dbc/transaction_manager.hpp` & `src/transaction_manager.cpp`: Transaction management implementation
  - `src/driver_manager.cpp`: Driver management implementation
  - `include/cpp_dbc/config/database_config.hpp`: Database configuration classes
  - `include/cpp_dbc/config/yaml_config_loader.hpp` & `src/config/yaml_config_loader.cpp`: YAML configuration loader

- **Database Drivers**:
  - `include/cpp_dbc/drivers/driver_mysql.hpp` & `src/drivers/driver_mysql.cpp`: MySQL implementation
  - `include/cpp_dbc/drivers/driver_postgresql.hpp` & `src/drivers/driver_postgresql.cpp`: PostgreSQL implementation
  - `include/cpp_dbc/drivers/driver_sqlite.hpp` & `src/drivers/driver_sqlite.cpp`: SQLite implementation

- **Examples**:
  - `examples/example.cpp`: Basic usage example
  - `examples/connection_pool_example.cpp`: Connection pool example
  - `examples/transaction_manager_example.cpp`: Transaction management example
  - `examples/config_example.cpp`: YAML configuration example
  - `examples/example_config.yml`: Example YAML configuration file
  - `examples/run_config_example.sh`: Script to run the configuration example

## Building the Library

### Prerequisites

Depending on which database drivers you enable, you'll need:

- For MySQL support: MySQL development libraries (`libmysqlclient-dev` on Debian/Ubuntu, `mysql-devel` on RHEL/CentOS)
- For PostgreSQL support: PostgreSQL development libraries (`libpq-dev` on Debian/Ubuntu, `postgresql-devel` on RHEL/CentOS)
- For SQLite support: SQLite development libraries (`libsqlite3-dev` on Debian/Ubuntu, `sqlite-devel` on RHEL/CentOS)

The build script will automatically check for and install these dependencies if needed.

### Build Options

The library supports conditional compilation of database drivers and features:

- `USE_MYSQL`: Enable/disable MySQL support (ON by default)
- `USE_POSTGRESQL`: Enable/disable PostgreSQL support (OFF by default)
- `USE_SQLITE`: Enable/disable SQLite support (OFF by default)
- `USE_CPP_YAML`: Enable/disable YAML configuration support (OFF by default)
- `CPP_DBC_BUILD_EXAMPLES`: Enable/disable building examples (OFF by default)
- `DEBUG_CONNECTION_POOL`: Enable debug output for ConnectionPool (OFF by default)
- `DEBUG_TRANSACTION_MANAGER`: Enable debug output for TransactionManager (OFF by default)
- `DEBUG_SQLITE`: Enable debug output for SQLite driver (OFF by default)

### Using the build scripts

#### Library Build Script

The `libs/cpp_dbc/build_cpp_dbc.sh` script handles dependencies and builds the cpp_dbc library:

```bash
# Build with MySQL support only (default)
./libs/cpp_dbc/build_cpp_dbc.sh

# Enable PostgreSQL support
./libs/cpp_dbc/build_cpp_dbc.sh --postgres

# Enable both MySQL and PostgreSQL
./libs/cpp_dbc/build_cpp_dbc.sh --mysql --postgres

# Enable SQLite support
./libs/cpp_dbc/build_cpp_dbc.sh --sqlite

# Enable all database drivers
./libs/cpp_dbc/build_cpp_dbc.sh --mysql --postgres --sqlite

# Disable MySQL support
./libs/cpp_dbc/build_cpp_dbc.sh --mysql-off

# Build in Release mode (default is Debug)
./libs/cpp_dbc/build_cpp_dbc.sh --release

# Enable YAML configuration support
./libs/cpp_dbc/build_cpp_dbc.sh --yaml

# Build examples
./libs/cpp_dbc/build_cpp_dbc.sh --examples

# Enable debug output for ConnectionPool
./libs/cpp_dbc/build_cpp_dbc.sh --debug-pool

# Enable debug output for TransactionManager
./libs/cpp_dbc/build_cpp_dbc.sh --debug-txmgr

# Enable debug output for SQLite driver
./libs/cpp_dbc/build_cpp_dbc.sh --debug-sqlite

# Enable all debug output
./libs/cpp_dbc/build_cpp_dbc.sh --debug-all

# Enable YAML and build examples
./libs/cpp_dbc/build_cpp_dbc.sh --yaml --examples

# Show help
./libs/cpp_dbc/build_cpp_dbc.sh --help
```

This script:
1. Checks for and installs required development libraries
2. Configures the library with CMake
3. Builds and installs the library to the `build/libs/cpp_dbc` directory

#### Main Build Script

The `build.sh` script builds the main application, passing all parameters to the library build script:

```bash
# Build with MySQL support only (default in Debug mode)
./build.sh

# Enable PostgreSQL support
./build.sh --postgres

# Enable both MySQL and PostgreSQL
./build.sh --mysql --postgres

# Enable SQLite support
./build.sh --sqlite

# Enable all database drivers
./build.sh --mysql --postgres --sqlite

# Disable MySQL support
./build.sh --mysql-off

# Build in Release mode
./build.sh --release

# Enable PostgreSQL and disable MySQL
./build.sh --postgres --mysql-off

# Enable YAML configuration support
./build.sh --yaml

# Build examples
./build.sh --examples

# Enable YAML and build examples
./build.sh --yaml --examples

# Enable debug output for ConnectionPool
./build.sh --debug-pool

# Enable debug output for TransactionManager
./build.sh --debug-txmgr

# Enable debug output for SQLite driver
./build.sh --debug-sqlite

# Enable all debug output
./build.sh --debug-all

# Show help
./build.sh --help
```

The main build script forwards command-line arguments to the library build script and stops if the library build fails.

The script will:

1. Check for and install required development libraries
2. Create the build directory if it doesn't exist
3. Generate Conan files
4. Configure the project with CMake using the specified options
5. Build the project in Debug mode by default (or Release mode if specified)

#### Test Build and Execution

The project includes scripts for building and running tests:

```bash
# Build with tests enabled
./build.sh --test

# Run the tests
./run_test.sh

# Run tests with specific options
./run_test.sh --valgrind  # Run tests with Valgrind
./run_test.sh --asan      # Run tests with AddressSanitizer
./run_test.sh --ctest     # Run tests using CTest
./run_test.sh --rebuild   # Force rebuild of tests before running
./run_test.sh --run-test="tag1+tag2"  # Run specific tests by tag
./run_test.sh --debug-pool  # Enable debug output for ConnectionPool
./run_test.sh --debug-txmgr  # Enable debug output for TransactionManager
./run_test.sh --debug-sqlite  # Enable debug output for SQLite driver
./run_test.sh --debug-all  # Enable all debug output
```

All test output is automatically logged to files in the `logs/test/` directory with timestamps in the filenames. The system automatically rotates logs, keeping the 4 most recent files.

You can analyze test logs for failures and memory issues:

```bash
# Check the most recent test log file for failures and memory issues
./helper.sh --check-test-log

# Check a specific test log file
./helper.sh --check-test-log=logs/test/output-20251109-152030.log
```

### BLOB Support

The library provides comprehensive support for Binary Large Object (BLOB) operations:

1. **Core BLOB Classes**:
   - Base classes for BLOB handling with consistent interfaces
   - Memory-based and file-based implementations
   - Support for reading and writing binary data

2. **Database-Specific Implementations**:
   - MySQL: Support for BLOB data types with MySQLBlob implementation
   - PostgreSQL: Support for BYTEA data type with PostgreSQLBlob implementation
   - SQLite: Support for BLOB data type with SQLiteBlob implementation

3. **Image File Support**:
   - Helper functions for reading and writing binary files
   - Support for storing and retrieving image files
   - Integrity verification for binary data
   - Comprehensive test cases with test.jpg file

### JSON Support

The library provides comprehensive support for JSON data types:

1. **MySQL JSON Support**:
   - Support for the JSON data type
   - JSON functions: JSON_EXTRACT, JSON_SEARCH, JSON_CONTAINS, JSON_ARRAYAGG
   - JSON indexing for performance optimization
   - JSON validation and error handling

2. **PostgreSQL JSON Support**:
   - Support for both JSON and JSONB data types
   - JSON operators: @>, <@, ?, ?|, ?&
   - JSON modification functions: jsonb_set, jsonb_insert
   - GIN indexing for JSONB fields
   - JSON path expressions and transformations

3. **Testing**:
   - Comprehensive test cases in test_mysql_real_json.cpp and test_postgresql_real_json.cpp
   - Random JSON data generation for performance testing
   - Tests for various JSON operations, validation, and error handling

### Memory Leak Prevention

The project includes several improvements to prevent memory leaks:

1. Enhanced SQLite connection management with better resource handling:
   - SQLiteConnection inherits from std::enable_shared_from_this
   - Replaced raw pointer tracking with weak_ptr in activeConnections list
   - Improved connection cleanup with weak_ptr-based reference tracking
   - Added proper error handling for shared_from_this() usage
   - Added safeguards to ensure connections are created with make_shared

2. Improved PostgreSQL driver with better memory management:
   - Enhanced connection options handling
   - Better cleanup of resources on connection close
   - Improved error handling with detailed error codes

To run memory leak checks:

```bash
# Run tests with Valgrind
./run_test.sh --valgrind

# Run tests with AddressSanitizer
./run_test.sh --asan
```

For more details about memory leak issues and their solutions, see the [Valgrind PostgreSQL Memory Leak documentation](memory-bank/valgrind_postgresql_memory_leak.md) and [AddressSanitizer Issues documentation](memory-bank/asan_issues.md).

### Exception Handling and Stack Trace Capture

The library includes a robust exception handling system with stack trace capture capabilities:

1. **Enhanced DBException Class**:
   - Unique error marks for better error identification
   - Separated error marks from error messages
   - Call stack capture for better debugging
   - Methods to retrieve and print stack traces

2. **Stack Trace Capture**:
   - Integration with backward-cpp library for stack trace capture
   - Automatic capture of call stack when exceptions are thrown
   - Filtering of irrelevant stack frames
   - Human-readable stack trace output

3. **Error Identification**:
   - Unique alphanumeric error marks for each error location
   - Consistent error format across all database drivers
   - Improved error messages with context information
   - Better debugging capabilities with file and line information

4. **Usage Example**:
   ```cpp
   try {
       // Database operations
   } catch (const cpp_dbc::DBException& e) {
       std::cerr << "Error: " << e.what() << std::endl;
       std::cerr << "Error Mark: " << e.getMark() << std::endl;
       e.printCallStack();
   }
   ```

The `run_test.sh` script will automatically build the project and tests if they haven't been built yet.

#### Running the YAML Configuration Example

To run the YAML configuration example:

```bash
# First, build the library with YAML support and examples
./build.sh --yaml --examples

# Then run the example
./libs/cpp_dbc/examples/run_config_example.sh
```

This will load the example YAML configuration file and display the database configurations.

#### Helper Script

The project includes a helper script (`helper.sh`) that provides various utilities:

```bash
# Build the project
./helper.sh --build

# Clean the build directory
./helper.sh --clean-build

# Clear the Conan cache
./helper.sh --clean-conan-cache

# Build the tests
./helper.sh --test

# Run the tests
./helper.sh --run-test

# Check shared library dependencies of the executable inside the container
./helper.sh --ldd-bin-ctr

# Check shared library dependencies of the executable locally
./helper.sh --ldd-build-bin

# Run the executable
./helper.sh --run-build-bin

# Multiple commands can be combined
./helper.sh --clean-build --clean-conan-cache --build
```

### VSCode Integration

The project includes VSCode configuration files in the `.vscode` directory:

- `c_cpp_properties.json`: Configures C/C++ IntelliSense with proper include paths and preprocessor definitions
- `tasks.json`: Defines build tasks for the project
- `build_with_props.sh`: Script to extract preprocessor definitions from `c_cpp_properties.json` and pass them to build.sh

To build the project using VSCode tasks:

1. Press `Ctrl+Shift+B` to run the default build task
2. Select "CMake: build" to build using the default configuration
3. Select "Build with C++ Properties" to build using the preprocessor definitions from `c_cpp_properties.json`

The "Auto install extensions" task runs automatically when opening the folder and installs recommended extensions.

### Manual CMake configuration

You can also configure the build manually with CMake:

```bash
mkdir -p build && cd build
# Default: MySQL enabled, PostgreSQL disabled
cmake ..

# Enable PostgreSQL
cmake .. -DUSE_POSTGRESQL=ON

# Disable MySQL, enable PostgreSQL
cmake .. -DUSE_MYSQL=OFF -DUSE_POSTGRESQL=ON

# Build the project
cmake --build .
```

## Using as a Library in Other CMake Projects

The CPP_DBC library is designed to be used as an external dependency in other CMake projects. After installing the library, you can use it in your CMake project as follows:

```cmake
# In your CMakeLists.txt
find_package(cpp_dbc REQUIRED)

# Create your executable or library
add_executable(your_app main.cpp)

# Link against cpp_dbc
target_link_libraries(your_app PRIVATE cpp_dbc::cpp_dbc)

# Add compile definitions for the database drivers
target_compile_definitions(your_app PRIVATE
    $<$<BOOL:${USE_MYSQL}>:USE_MYSQL=1>
    $<$<NOT:$<BOOL:${USE_MYSQL}>>:USE_MYSQL=0>
    $<$<BOOL:${USE_POSTGRESQL}>:USE_POSTGRESQL=1>
    $<$<NOT:$<BOOL:${USE_POSTGRESQL}>>:USE_POSTGRESQL=0>
    $<$<BOOL:${USE_SQLITE}>:USE_SQLITE=1>
    $<$<NOT:$<BOOL:${USE_SQLITE}>>:USE_SQLITE=0>
)
```

The library exports the following CMake variables that you can use to check which database drivers are enabled:

- `CPP_DBC_USE_MYSQL`: Set to ON if MySQL support is enabled
- `CPP_DBC_USE_POSTGRESQL`: Set to ON if PostgreSQL support is enabled
- `CPP_DBC_USE_SQLITE`: Set to ON if SQLite support is enabled

You can use these variables to conditionally include code in your project:

```cmake
if(CPP_DBC_USE_MYSQL)
    # MySQL-specific code
endif()

if(CPP_DBC_USE_POSTGRESQL)
    # PostgreSQL-specific code
endif()

if(CPP_DBC_USE_SQLITE)
    # SQLite-specific code
endif()
```

### Include Paths

When using the library as an external dependency, include the headers as follows:

```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#if USE_SQLITE
#include <cpp_dbc/drivers/driver_sqlite.hpp>
#endif
```

## Usage Example

```cpp
#include "cpp_dbc/cpp_dbc.hpp"

#if USE_MYSQL
#include "cpp_dbc/drivers/driver_mysql.hpp"
#endif

#if USE_POSTGRESQL
#include "cpp_dbc/drivers/driver_postgresql.hpp"
#endif

#if USE_SQLITE
#include "cpp_dbc/drivers/driver_sqlite.hpp"
#endif

int main() {
    // Register available drivers
#if USE_MYSQL
    cpp_dbc::DriverManager::registerDriver("mysql",
        std::make_shared<cpp_dbc::MySQL::MySQLDriver>());
#endif

#if USE_POSTGRESQL
    cpp_dbc::DriverManager::registerDriver("postgresql",
        std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());
#endif

#if USE_SQLITE
    cpp_dbc::DriverManager::registerDriver("sqlite",
        std::make_shared<cpp_dbc::SQLite::SQLiteDriver>());
#endif

    // Get a connection (will use whichever driver is available)
    auto conn = cpp_dbc::DriverManager::getConnection(
        "cpp_dbc:mysql://localhost:3306/testdb",
        "username",
        "password"
    );

    // Use the connection
    auto resultSet = conn->executeQuery("SELECT * FROM users");
    
    while (resultSet->next()) {
        std::cout << resultSet->getInt("id") << ": "
                  << resultSet->getString("name") << std::endl;
    }
    
    conn->close();
    
    return 0;
}
```

## YAML Configuration Example

The library provides optional support for loading database configurations from YAML files:

```cpp
#include "cpp_dbc/config/database_config.hpp"

#ifdef USE_CPP_YAML
#include "cpp_dbc/config/yaml_config_loader.hpp"
#endif

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file.yml>" << std::endl;
        return 1;
    }

#ifdef USE_CPP_YAML
    try {
        // Load configuration from YAML file
        std::cout << "Loading configuration from YAML file: " << argv[1] << std::endl;
        cpp_dbc::config::DatabaseConfigManager configManager =
            cpp_dbc::config::YamlConfigLoader::loadFromFile(argv[1]);
        
        // Get a specific database configuration
        auto dbConfigOpt = configManager.getDatabaseByName("dev_mysql");
        if (dbConfigOpt) {
            // Use the configuration to create a connection
            const auto& dbConfig = dbConfigOpt->get();
            std::string connStr = dbConfig.createConnectionString();
            std::cout << "Connection String: " << connStr << std::endl;
            std::cout << "Username: " << dbConfig.getUsername() << std::endl;
            std::cout << "Password: " << dbConfig.getPassword() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cerr << "YAML support not enabled. Rebuild with --yaml option." << std::endl;
    return 1;
#endif

    return 0;
}
```

Example YAML configuration file:

```yaml
# Database configurations
databases:
  - name: dev_mysql
    type: mysql
    host: localhost
    port: 3306
    database: TestDB
    username: root
    password: password
    options:
      connect_timeout: 5
      charset: utf8mb4
      autocommit: true

  - name: prod_postgresql
    type: postgresql
    host: db.example.com
    port: 5432
    database: ProdDB
    username: prod_user
    password: prod_password
    options:
      connect_timeout: 10
      application_name: cpp_dbc_prod
      client_encoding: UTF8
      sslmode: require

# Connection pool configurations
connection_pool:
  default:
    initial_size: 5
    max_size: 20
    connection_timeout: 5000
    idle_timeout: 60000
    validation_interval: 30000
```

## Distribution

The `build.dist.sh` script generates a Dockerfile for the application and builds the Docker image:

```bash
# Build Docker image with default options
./build.dist.sh

# Enable PostgreSQL support
./build.dist.sh --postgres

# Enable YAML configuration support
./build.dist.sh --yaml

# Build in Release mode
./build.dist.sh --release

# Build with tests
./build.dist.sh --test

# Build with examples
./build.dist.sh --examples

# Show help
./build.dist.sh --help
```

The script performs the following steps:

1. Processes variables from `.dist_build` and `.env_dist` files
2. Builds the project with the specified options
3. Automatically analyzes the executable to detect shared library dependencies
4. Maps libraries to their corresponding Debian packages
5. Generates a Dockerfile with only the necessary dependencies
6. Builds and tags the Docker image

The automatic dependency detection ensures that the Docker container includes all required libraries for the executable to run properly, regardless of which database drivers or features are enabled.

You can use the `--ldd` option in the helper script to check the shared library dependencies inside the container:

```bash
./helper.sh --ldd
```

This will run the `ldd` command inside the Docker container on the executable, showing all shared libraries that the executable depends on within the container environment.
