# C++ Database Connectivity (CPP_DBC) Library

This project provides a C++ Database Connectivity library inspired by JDBC, with support for MySQL and PostgreSQL databases.

## Features

- **Database Abstraction**: Unified API for different database systems
- **Connection Pooling**: Efficient connection management
- **Transaction Management**: Support for distributed transactions
- **Transaction Isolation Levels**: JDBC-compatible isolation levels (READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE)
- **Prepared Statements**: Protection against SQL injection
- **Conditional Compilation**: Build with only the database drivers you need
- **Modern C++ Design**: Uses C++23 features and RAII principles
- **YAML Configuration**: Optional support for loading database configurations from YAML files

## Database Support

The library currently supports:

- **MySQL**: Full support for MySQL databases (enabled by default)
- **PostgreSQL**: Full support for PostgreSQL databases (disabled by default)

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

The build script will automatically check for and install these dependencies if needed.

### Build Options

The library supports conditional compilation of database drivers and features:

- `USE_MYSQL`: Enable/disable MySQL support (ON by default)
- `USE_POSTGRESQL`: Enable/disable PostgreSQL support (OFF by default)
- `USE_CPP_YAML`: Enable/disable YAML configuration support (OFF by default)
- `CPP_DBC_BUILD_EXAMPLES`: Enable/disable building examples (OFF by default)
- `DEBUG_CONNECTION_POOL`: Enable debug output for ConnectionPool (OFF by default)
- `DEBUG_TRANSACTION_MANAGER`: Enable debug output for TransactionManager (OFF by default)

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
```

### Valgrind Memory Leak Suppression

When running tests with Valgrind, you might encounter "still reachable" memory leaks from the PostgreSQL driver. These leaks come from the GSSAPI/Kerberos libraries used by libpq (PostgreSQL's client library), not from our code directly.

To handle these leaks, the project includes:

1. A Valgrind suppression file (`libs/cpp_dbc/valgrind-suppressions.txt`) that ignores specific memory leaks from the GSSAPI/Kerberos libraries.
2. Code improvements in the PostgreSQL driver to minimize memory leaks.

To run tests with the suppression file:

```bash
# Run tests with Valgrind using the suppression file
./run_test.sh --valgrind
```

For more details about this issue and its solution, see the [Valgrind PostgreSQL Memory Leak documentation](memory-bank/valgrind_postgresql_memory_leak.md).

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
./helper.sh --ldd

# Check shared library dependencies of the executable locally
./helper.sh --ldd-bin

# Run the executable
./helper.sh --run-bin

# Multiple commands can be combined
./helper.sh --clean-build --clean-conan-cache --build
```

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
)
```

The library exports the following CMake variables that you can use to check which database drivers are enabled:

- `CPP_DBC_USE_MYSQL`: Set to ON if MySQL support is enabled
- `CPP_DBC_USE_POSTGRESQL`: Set to ON if PostgreSQL support is enabled

You can use these variables to conditionally include code in your project:

```cmake
if(CPP_DBC_USE_MYSQL)
    # MySQL-specific code
endif()

if(CPP_DBC_USE_POSTGRESQL)
    # PostgreSQL-specific code
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
        const auto* dbConfig = configManager.getDatabaseByName("dev_mysql");
        if (dbConfig) {
            // Use the configuration to create a connection
            std::string connStr = dbConfig->createConnectionString();
            std::cout << "Connection String: " << connStr << std::endl;
            std::cout << "Username: " << dbConfig->getUsername() << std::endl;
            std::cout << "Password: " << dbConfig->getPassword() << std::endl;
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
