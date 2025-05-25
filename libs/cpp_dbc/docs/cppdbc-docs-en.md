# CPP_DBC Library Documentation

This document provides a comprehensive guide to the CPP_DBC library, a C++ Database Connectivity library inspired by JDBC.

## Table of Contents
- [Core Components](#core-components)
- [MySQL Implementation](#mysql-implementation)
- [PostgreSQL Implementation](#postgresql-implementation)
- [Connection Pool](#connection-pool)
- [Transaction Manager](#transaction-manager)
- [Configuration System](#configuration-system)
- [Build and Installation](#build-and-installation)

---

## Core Components
*Components defined in cpp_dbc.hpp*

### SQLException
A custom exception class for SQL-related errors.

**Methods:**
- `SQLException(const std::string& message)`: Constructor that takes an error message.

### Types Enum
Represents SQL parameter types.

**Values:**
- `INTEGER`: For integer values
- `FLOAT`: For floating-point values
- `DOUBLE`: For double-precision values
- `VARCHAR`: For variable-length character strings
- `DATE`: For date values
- `TIMESTAMP`: For timestamp values
- `BOOLEAN`: For boolean values
- `BLOB`: For binary large objects

### ResultSet
An abstract base class representing a result set from a SQL query.

**Methods:**
- `next()`: Advances to the next row, returns true if successful.
- `isBeforeFirst()`: Returns true if the cursor is before the first row.
- `isAfterLast()`: Returns true if the cursor is after the last row.
- `getRow()`: Returns the current row number.
- `getInt(int/string)`: Retrieves the value of the designated column as an int.
- `getLong(int/string)`: Retrieves the value of the designated column as a long.
- `getDouble(int/string)`: Retrieves the value of the designated column as a double.
- `getString(int/string)`: Retrieves the value of the designated column as a string.
- `getBoolean(int/string)`: Retrieves the value of the designated column as a boolean.
- `isNull(int/string)`: Returns true if the value of the designated column is null.
- `getColumnNames()`: Returns a vector of all column names.
- `getColumnCount()`: Returns the number of columns in the result set.

### PreparedStatement
An abstract base class representing a precompiled SQL statement.

**Methods:**
- `setInt(int, int)`: Sets the designated parameter to the given int value.
- `setLong(int, long)`: Sets the designated parameter to the given long value.
- `setDouble(int, double)`: Sets the designated parameter to the given double value.
- `setString(int, string)`: Sets the designated parameter to the given string value.
- `setBoolean(int, bool)`: Sets the designated parameter to the given boolean value.
- `setNull(int, Types)`: Sets the designated parameter to SQL NULL.
- `executeQuery()`: Executes the query and returns a ResultSet.
- `executeUpdate()`: Executes the update and returns the number of affected rows.
- `execute()`: Executes the SQL statement and returns true if the result is a ResultSet.

### Connection
An abstract base class representing a connection to a database.

**Methods:**
- `close()`: Closes the connection.
- `isClosed()`: Returns true if the connection is closed.
- `prepareStatement(string)`: Creates a PreparedStatement for the given SQL statement.
- `executeQuery(string)`: Executes the given SQL query and returns a ResultSet.
- `executeUpdate(string)`: Executes the given SQL update and returns the number of affected rows.
- `setAutoCommit(bool)`: Sets the auto-commit mode.
- `getAutoCommit()`: Returns the auto-commit mode.
- `commit()`: Commits the current transaction.
- `rollback()`: Rolls back the current transaction.

### Driver
An abstract base class representing a database driver.

**Methods:**
- `connect(string, string, string)`: Establishes a connection to the database.
- `acceptsURL(string)`: Returns true if the driver can connect to the given URL.

### DriverManager
A manager class to register and retrieve driver instances.

**Methods:**
- `registerDriver(string, Driver)`: Registers a driver with the given name.
- `getConnection(string, string, string)`: Gets a connection to the database specified by the URL.

---

## MySQL Implementation
*Components defined in drivers/driver_mysql.hpp and drivers/driver_mysql.cpp*

### MySQLResultSet
Implementation of ResultSet for MySQL.

**Methods:**
Same as ResultSet, plus:
- `MySQLResultSet(MYSQL_RES*)`: Constructor that takes a MySQL result set.

### MySQLPreparedStatement
Implementation of PreparedStatement for MySQL.

**Methods:**
Same as PreparedStatement, plus:
- `MySQLPreparedStatement(MYSQL*, string)`: Constructor that takes a MySQL connection and SQL statement.

### MySQLConnection
Implementation of Connection for MySQL.

**Methods:**
Same as Connection, plus:
- `MySQLConnection(string, int, string, string, string)`: Constructor that takes host, port, database, user, and password.

### MySQLDriver
Implementation of Driver for MySQL.

**Methods:**
Same as Driver, plus:
- `MySQLDriver()`: Constructor that initializes the MySQL library.
- `parseURL(string, string&, int&, string&)`: Parses a connection URL.

---

## PostgreSQL Implementation
*Components defined in drivers/driver_postgresql.hpp and drivers/driver_postgresql.cpp*

### PostgreSQLResultSet
Implementation of ResultSet for PostgreSQL.

**Methods:**
Same as ResultSet, plus:
- `PostgreSQLResultSet(PGresult*)`: Constructor that takes a PostgreSQL result.

### PostgreSQLPreparedStatement
Implementation of PreparedStatement for PostgreSQL.

**Methods:**
Same as PreparedStatement, plus:
- `PostgreSQLPreparedStatement(PGconn*, string)`: Constructor that takes a PostgreSQL connection and SQL statement.

### PostgreSQLConnection
Implementation of Connection for PostgreSQL.

**Methods:**
Same as Connection, plus:
- `PostgreSQLConnection(string, int, string, string, string)`: Constructor that takes host, port, database, user, and password.
- `generateStatementName()`: Generates a unique name for prepared statements.

### PostgreSQLDriver
Implementation of Driver for PostgreSQL.

**Methods:**
Same as Driver, plus:
- `PostgreSQLDriver()`: Constructor.
- `parseURL(string, string&, int&, string&)`: Parses a connection URL.

---

## Connection Pool
*Components defined in connection_pool.hpp and connection_pool.cpp*

### ConnectionPoolConfig
Configuration structure for connection pools.

**Properties:**
- `url`: The database URL
- `username`: The database username
- `password`: The database password
- `initialSize`: Initial number of connections (default 5)
- `maxSize`: Maximum number of connections (default 20)
- `minIdle`: Minimum number of idle connections (default 3)
- `maxWaitMillis`: Maximum wait time for a connection (default 30000 ms)
- `validationTimeoutMillis`: Timeout for connection validation (default 5000 ms)
- `idleTimeoutMillis`: Maximum idle time for a connection (default 300000 ms)
- `maxLifetimeMillis`: Maximum lifetime of a connection (default 1800000 ms)
- `testOnBorrow`: Whether to test connections before borrowing (default true)
- `testOnReturn`: Whether to test connections when returning to pool (default false)
- `validationQuery`: Query used to validate connections (default "SELECT 1")

### ConnectionPool
Manages a pool of database connections.

**Methods:**
- `ConnectionPool(ConnectionPoolConfig)`: Constructor that takes a pool configuration.
- `getConnection()`: Gets a connection from the pool.
- `getActiveConnectionCount()`: Returns the number of active connections.
- `getIdleConnectionCount()`: Returns the number of idle connections.
- `getTotalConnectionCount()`: Returns the total number of connections.
- `close()`: Closes the pool and all connections.
- `createConnection()`: Creates a new physical connection (internal).
- `createPooledConnection()`: Creates a new pooled connection wrapper (internal).
- `validateConnection(Connection)`: Validates a connection (internal).
- `returnConnection(PooledConnection)`: Returns a connection to the pool (internal).
- `maintenanceTask()`: Maintenance thread function (internal).

### PooledConnection
Wraps a physical connection to provide pooling functionality.

**Methods:**
Same as Connection, plus:
- `PooledConnection(Connection, ConnectionPool*)`: Constructor that takes a connection and pool.
- `getCreationTime()`: Returns the creation time of the connection.
- `getLastUsedTime()`: Returns the last used time of the connection.
- `setActive(bool)`: Sets whether the connection is active.
- `isActive()`: Returns whether the connection is active.
- `getUnderlyingConnection()`: Returns the underlying physical connection.

---

## Transaction Manager
*Components defined in transaction_manager.hpp and transaction_manager.cpp*

### TransactionContext
Structure to hold transaction state.

**Properties:**
- `connection`: The database connection
- `creationTime`: The time the transaction was created
- `lastAccessTime`: The last time the transaction was accessed
- `transactionId`: The unique ID of the transaction
- `active`: Whether the transaction is active

### TransactionManager
Manages transactions across different threads.

**Methods:**
- `TransactionManager(ConnectionPool&)`: Constructor that takes a connection pool.
- `beginTransaction()`: Starts a new transaction and returns its ID.
- `getTransactionConnection(string)`: Gets the connection for a transaction.
- `commitTransaction(string)`: Commits a transaction by its ID.
- `rollbackTransaction(string)`: Rolls back a transaction by its ID.
- `isTransactionActive(string)`: Returns whether a transaction is active.
- `getActiveTransactionCount()`: Returns the number of active transactions.
- `setTransactionTimeout(long)`: Sets the transaction timeout.
- `close()`: Closes the transaction manager.
- `cleanupTask()`: Cleanup thread function (internal).
- `generateUUID()`: Generates a unique ID (internal).

---

## Configuration System
*Components defined in config/database_config.hpp and config/yaml_config_loader.hpp*

### DatabaseConfig
Represents a database configuration.

**Methods:**
- `setName(string)`: Sets the name of the database configuration.
- `getName()`: Gets the name of the database configuration.
- `setType(string)`: Sets the database type (mysql, postgresql, etc.).
- `getType()`: Gets the database type.
- `setHost(string)`: Sets the database host.
- `getHost()`: Gets the database host.
- `setPort(int)`: Sets the database port.
- `getPort()`: Gets the database port.
- `setDatabase(string)`: Sets the database name.
- `getDatabase()`: Gets the database name.
- `setUsername(string)`: Sets the database username.
- `getUsername()`: Gets the database username.
- `setPassword(string)`: Sets the database password.
- `getPassword()`: Gets the database password.
- `setOption(string, string)`: Sets a database-specific option.
- `getOption(string)`: Gets a database-specific option.
- `getOptions()`: Gets all database-specific options.
- `createConnectionString()`: Creates a connection string for this configuration.

### ConnectionPoolConfig
Represents a connection pool configuration.

**Properties:**
- `initialSize`: Initial number of connections
- `maxSize`: Maximum number of connections
- `connectionTimeout`: Connection timeout in milliseconds
- `idleTimeout`: Idle timeout in milliseconds
- `validationInterval`: Validation interval in milliseconds

**Methods:**
- `setName(string)`: Sets the name of the connection pool configuration.
- `getName()`: Gets the name of the connection pool configuration.
- `setInitialSize(int)`: Sets the initial size of the connection pool.
- `getInitialSize()`: Gets the initial size of the connection pool.
- `setMaxSize(int)`: Sets the maximum size of the connection pool.
- `getMaxSize()`: Gets the maximum size of the connection pool.
- `setConnectionTimeout(int)`: Sets the connection timeout.
- `getConnectionTimeout()`: Gets the connection timeout.
- `setIdleTimeout(int)`: Sets the idle timeout.
- `getIdleTimeout()`: Gets the idle timeout.
- `setValidationInterval(int)`: Sets the validation interval.
- `getValidationInterval()`: Gets the validation interval.

### TestQueries
Represents a collection of test queries for different database types.

**Methods:**
- `setConnectionTest(string)`: Sets the connection test query.
- `getConnectionTest()`: Gets the connection test query.
- `setQuery(string, string, string)`: Sets a test query for a specific database type.
- `getQuery(string, string)`: Gets a test query for a specific database type.
- `getQueries(string)`: Gets all test queries for a specific database type.

### DatabaseConfigManager
Manages database configurations.

**Methods:**
- `addDatabaseConfig(DatabaseConfig)`: Adds a database configuration.
- `getDatabaseByName(string)`: Gets a database configuration by name.
- `getDatabases()`: Gets all database configurations.
- `addConnectionPoolConfig(ConnectionPoolConfig)`: Adds a connection pool configuration.
- `getConnectionPoolByName(string)`: Gets a connection pool configuration by name.
- `getConnectionPools()`: Gets all connection pool configurations.
- `setTestQueries(TestQueries)`: Sets the test queries.
- `getTestQueries()`: Gets the test queries.

### YamlConfigLoader
Loads database configurations from YAML files.

**Methods:**
- `loadFromFile(string)`: Loads database configurations from a YAML file.
- `loadFromString(string)`: Loads database configurations from a YAML string.

---

## Build and Installation

### Prerequisites
- C++23 compiler
- MySQL development libraries (for MySQL support)
- PostgreSQL development libraries (for PostgreSQL support, optional)
- yaml-cpp library (for YAML configuration support, optional)
- CMake 3.15 or later
- Conan for dependency management

### Building with Scripts

The library provides build scripts to simplify the build process:

```bash
# Build with MySQL support only (default)
./build.sh

# Build with MySQL and PostgreSQL support
./build.sh --mysql --postgres

# Build with PostgreSQL support only
./build.sh --mysql-off --postgres

# Enable YAML configuration support
./build.sh --yaml

# Build examples
./build.sh --examples

# Build tests
./build.sh --test

# Build in Release mode
./build.sh --release

# Enable YAML and build examples
./build.sh --yaml --examples

# Build Docker container
./build.dist.sh

# Build Docker container with PostgreSQL and YAML support
./build.dist.sh --postgres --yaml
```

The build script:
1. Checks for and installs required dependencies
2. Configures the library with CMake
3. Builds and installs the library to the `build/libs/cpp_dbc` directory
4. Builds the main application

The `build.dist.sh` script:
1. Builds the project with the specified options
2. Automatically detects shared library dependencies
3. Generates a Dockerfile with only the necessary dependencies
4. Builds and tags the Docker image

### Helper Script

The project includes a helper script (`helper.sh`) that provides various utilities:

```bash
# Build the project with MySQL support
./helper.sh --build

# Build with PostgreSQL support
./helper.sh --build --postgres

# Build with YAML support
./helper.sh --build --yaml

# Build and run tests
./helper.sh --test --run-test

# Check executable dependencies locally
./helper.sh --ldd-bin

# Check executable dependencies inside the container
./helper.sh --ldd [container_name]

# Run the executable
./helper.sh --run-bin

# Multiple commands can be combined
./helper.sh --build --yaml --examples --run-bin
```

### Running the YAML Configuration Example

To run the YAML configuration example:

```bash
# First, build the library with YAML support and examples
./build.sh --yaml --examples

# Then run the example
./libs/cpp_dbc/examples/run_config_example.sh
```

This will load the example YAML configuration file and display the database configurations.

### Manual Build with CMake

You can also build the library manually with CMake:

```bash
# Create build directory for the library
mkdir -p libs/cpp_dbc/build
cd libs/cpp_dbc/build

# Configure with CMake (MySQL enabled, PostgreSQL disabled, YAML disabled)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_CPP_YAML=OFF -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configure with YAML support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_CPP_YAML=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Build and install
cmake --build . --target install

# Return to root directory
cd ../../..

# Create build directory for the main application
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_CPP_YAML=OFF -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

# Configure with YAML support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_CPP_YAML=ON -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

# Build
cmake --build .
```

### Using as a Library

To use CPP_DBC in your own project:

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
    $<$<BOOL:${USE_CPP_YAML}>:USE_CPP_YAML=1>
    $<$<NOT:$<BOOL:${USE_CPP_YAML}>>:USE_CPP_YAML=0>
)
```

In your C++ code:

```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif

// Use the library...

// Using YAML configuration
#ifdef USE_CPP_YAML
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/config/yaml_config_loader.hpp>

// Load configuration from YAML file
cpp_dbc::config::DatabaseConfigManager configManager =
    cpp_dbc::config::YamlConfigLoader::loadFromFile("config.yml");

// Get a specific database configuration
const auto* dbConfig = configManager.getDatabaseByName("dev_mysql");
if (dbConfig) {
    // Use the configuration to create a connection
    std::string connStr = dbConfig->createConnectionString();
    auto conn = cpp_dbc::DriverManager::getConnection(
        connStr, dbConfig->getUsername(), dbConfig->getPassword()
    );
    
    // Use the connection...
    conn->close();
}
#endif
