# C++ Database Connectivity (CPP_DBC) Library

This project provides a C++ Database Connectivity library inspired by JDBC, with support for MySQL and PostgreSQL databases.

## Features

- **Database Abstraction**: Unified API for different database systems
- **Connection Pooling**: Efficient connection management
- **Transaction Management**: Support for distributed transactions
- **Prepared Statements**: Protection against SQL injection
- **Conditional Compilation**: Build with only the database drivers you need
- **Modern C++ Design**: Uses C++23 features and RAII principles

## Database Support

The library currently supports:

- **MySQL**: Full support for MySQL databases (enabled by default)
- **PostgreSQL**: Full support for PostgreSQL databases (disabled by default)

Each database driver can be enabled or disabled at compile time to reduce dependencies. By default, only MySQL support is enabled.

## Project Structure

- **Core Components**:
  - `cpp_dbc.hpp`: Core interfaces and types
  - `connection_pool.hpp/cpp`: Connection pooling implementation
  - `transaction_manager.hpp/cpp`: Transaction management implementation

- **Database Drivers**:
  - `drivers/driver_mysql.hpp/cpp`: MySQL implementation
  - `drivers/driver_postgresql.hpp/cpp`: PostgreSQL implementation

- **Examples**:
  - `examples/CPPDBC_Example.cpp`: Basic usage example
  - `examples/CPPDBC_ConnectionPool_Example.cpp`: Connection pool example
  - `examples/CPPDBC_TransactionManager_Example.cpp`: Transaction management example

## Building the Library

### Prerequisites

Depending on which database drivers you enable, you'll need:

- For MySQL support: MySQL development libraries (`libmysqlclient-dev` on Debian/Ubuntu, `mysql-devel` on RHEL/CentOS)
- For PostgreSQL support: PostgreSQL development libraries (`libpq-dev` on Debian/Ubuntu, `postgresql-devel` on RHEL/CentOS)

The build script will automatically check for and install these dependencies if needed.

### Build Options

The library supports conditional compilation of database drivers:

- `USE_MYSQL`: Enable/disable MySQL support (ON by default)
- `USE_POSTGRESQL`: Enable/disable PostgreSQL support (OFF by default)

### Using the build scripts

#### Library Build Script

The `src/libs/cpp_dbc/build_cpp_dbc.sh` script handles dependencies and builds the cpp_dbc library:

```bash
# Build with MySQL support only (default)
./src/libs/cpp_dbc/build_cpp_dbc.sh

# Enable PostgreSQL support
./src/libs/cpp_dbc/build_cpp_dbc.sh --postgres

# Enable both MySQL and PostgreSQL
./src/libs/cpp_dbc/build_cpp_dbc.sh --mysql --postgres

# Disable MySQL support
./src/libs/cpp_dbc/build_cpp_dbc.sh --mysql-off

# Show help
./src/libs/cpp_dbc/build_cpp_dbc.sh --help
```

This script:
1. Checks for and installs required development libraries
2. Configures the library with CMake
3. Builds and installs the library to the `build/cpp_dbc` directory

#### Main Build Script

The `build.sh` script builds the main application, passing all parameters to the library build script:

```bash
# Build with MySQL support only (default)
./build.sh

# Enable PostgreSQL support
./build.sh --postgres

# Enable both MySQL and PostgreSQL
./build.sh --mysql --postgres

# Disable MySQL support
./build.sh --mysql-off

# Enable PostgreSQL and disable MySQL
./build.sh --postgres --mysql-off

# Show help
./build.sh --help
```

The main build script simply forwards all command-line arguments to the library build script and stops if the library build fails.

The script will:

1. Check for and install required development libraries
2. Create the build directory if it doesn't exist
3. Generate Conan files
4. Configure the project with CMake using the specified options
5. Build the project

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
        "cppdbc:mysql://localhost:3306/testdb",
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

## Distribution

The `build.dist.sh` script generates a Dockerfile for the application and builds the Docker image:

1. Processes variables from `.dist_build` and `.env_dist` files
2. Builds the project to detect dependencies
3. Analyzes dependencies to determine required packages
4. Generates a Dockerfile with only the necessary dependencies
5. Builds and tags the Docker image
