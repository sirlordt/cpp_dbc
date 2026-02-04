# CPP_DBC Library Documentation

This document provides a comprehensive guide to the CPP_DBC library, a C++ Database Connectivity library inspired by JDBC, with support for MySQL, PostgreSQL, SQLite, Firebird SQL, MongoDB, ScyllaDB, and Redis databases. All public header files include Doxygen-compatible `/** @brief ... */` documentation with inline code examples, `@param`/`@return`/`@see` tags, and are ready for HTML/PDF API reference generation.

## Table of Contents
- [Core Components](#core-components)
- [MySQL Implementation](#mysql-implementation)
- [PostgreSQL Implementation](#postgresql-implementation)
- [SQLite Implementation](#sqlite-implementation)
- [Firebird Implementation](#firebird-implementation)
- [MongoDB Implementation](#mongodb-implementation)
- [ScyllaDB Implementation](#scylladb-implementation)
- [Redis Implementation](#redis-implementation)
- [Connection Pool](#connection-pool)
- [Transaction Manager](#transaction-manager)
- [Configuration System](#configuration-system)
- [Build and Installation](#build-and-installation)

---

## Core Components
*Components defined in cpp_dbc.hpp*

### DBException
A custom exception class for database-related errors.

**Methods:**
- `DBException(const std::string& message)`: Constructor that takes an error message.
- `DBException(const std::string& mark, const std::string& message, const std::vector<system_utils::StackFrame>& callStack)`: Constructor that takes an error mark, message, and call stack.
- `what_s()`: Returns the error message as a std::string (safer than what()).
- `getMark()`: Returns the unique error mark.
- `getCallStack()`: Returns the call stack captured when the exception was created.
- `printCallStack()`: Prints the call stack to standard error.

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

### BLOB Memory Safety

All BLOB implementations use smart pointers (`std::weak_ptr`) for safe connection references:

- **FirebirdBlob**: Uses `weak_ptr<FirebirdConnection>` with `getConnection()` helper
- **MySQLBlob**: Uses `weak_ptr<MYSQL>` with `getMySQLConnection()` helper
- **PostgreSQLBlob**: Uses `weak_ptr<PGconn>` with `getPGConnection()` helper
- **SQLiteBlob**: Uses `weak_ptr<sqlite3>` with `getSQLiteConnection()` helper

All BLOB classes have an `isConnectionValid()` method to check if the connection is still valid. Operations throw `DBException` if the connection has been closed, preventing use-after-free errors.

### TransactionIsolationLevel Enum
Represents transaction isolation levels following the JDBC standard.

**Values:**
- `TRANSACTION_NONE`: No transactions supported
- `TRANSACTION_READ_UNCOMMITTED`: Allows dirty reads, non-repeatable reads, and phantom reads
- `TRANSACTION_READ_COMMITTED`: Prevents dirty reads, but allows non-repeatable reads and phantom reads
- `TRANSACTION_REPEATABLE_READ`: Prevents dirty reads and non-repeatable reads, but allows phantom reads
- `TRANSACTION_SERIALIZABLE`: Prevents dirty reads, non-repeatable reads, and phantom reads

### ResultSet
An abstract base class representing a result set from a SQL query.

**Methods:**
- `next()`: Advances to the next row, returns true if successful.
- `isBeforeFirst()`: Returns true if the cursor is before the first row.
- `isAfterLast()`: Returns true if the cursor is after the last row.
- `getRow()`: Returns the current row number.
- `getInt(int/string)`: Retrieves the value of the designated column as an int.
- `getLong(int/string)`: Retrieves the value of the designated column as an int64_t (portable across all platforms).
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
- `setLong(int, int64_t)`: Sets the designated parameter to the given int64_t value (portable across all platforms).
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
- `setTransactionIsolation(TransactionIsolationLevel)`: Sets the transaction isolation level.
- `getTransactionIsolation()`: Returns the current transaction isolation level.

### Driver
An abstract base class representing a database driver.

**Methods:**
- `connect(string, string, string, map<string, string>)`: Establishes a connection to the database with optional connection options.
- `connectRelational(string, string, string, map<string, string>)`: Establishes a relational database connection with optional connection options.
- `acceptsURL(string)`: Returns true if the driver can connect to the given URL.

### DriverManager
A manager class to register and retrieve driver instances.

**Methods:**
- `registerDriver(string, Driver)`: Registers a driver with the given name.
- `getDBConnection(string, string, string, map<string, string>)`: Gets a connection to the database specified by the URL with optional connection options.
- `getDBConnection(DatabaseConfig)`: Gets a connection using a database configuration object.

---

## MySQL Implementation
*Components defined in drivers/relational/driver_mysql.hpp and src/drivers/relational/driver_mysql.cpp*

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
- `MySQLConnection(string, int, string, string, string, map<string, string>)`: Constructor that takes host, port, database, user, password, and optional connection options.
- `setTransactionIsolation(TransactionIsolationLevel)`: Sets the transaction isolation level for MySQL (default: REPEATABLE READ).
- `getTransactionIsolation()`: Returns the current transaction isolation level.

### MySQLDBDriver
Implementation of Driver for MySQL.

**Methods:**
Same as Driver, plus:
- `MySQLDBDriver()`: Constructor that initializes the MySQL library.
- `parseURL(string, string&, int&, string&)`: Parses a connection URL.

---

## PostgreSQL Implementation
*Components defined in drivers/relational/driver_postgresql.hpp and src/drivers/relational/driver_postgresql.cpp*

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
- `PostgreSQLConnection(string, int, string, string, string, map<string, string>)`: Constructor that takes host, port, database, user, password, and optional connection options.
- `generateStatementName()`: Generates a unique name for prepared statements.
- `setTransactionIsolation(TransactionIsolationLevel)`: Sets the transaction isolation level for PostgreSQL (default: READ COMMITTED).
- `getTransactionIsolation()`: Returns the current transaction isolation level.

### PostgreSQLDBDriver
Implementation of Driver for PostgreSQL.

**Methods:**
Same as Driver, plus:
- `PostgreSQLDBDriver()`: Constructor.
- `parseURL(string, string&, int&, string&)`: Parses a connection URL.

---

## SQLite Implementation
*Components defined in drivers/relational/driver_sqlite.hpp and src/drivers/relational/driver_sqlite.cpp*

### SQLiteResultSet
Implementation of ResultSet for SQLite.

**Methods:**
Same as ResultSet, plus:
- `SQLiteResultSet(sqlite3_stmt*, bool)`: Constructor that takes a SQLite statement and ownership flag.

### SQLitePreparedStatement
Implementation of PreparedStatement for SQLite.

**Methods:**
Same as PreparedStatement, plus:
- `SQLitePreparedStatement(sqlite3*, string)`: Constructor that takes a SQLite database connection and SQL statement.

### SQLiteConnection
Implementation of Connection for SQLite.

**Methods:**
Same as Connection, plus:
- `SQLiteConnection(string, map<string, string>)`: Constructor that takes a database path and optional connection options.
- `setTransactionIsolation(TransactionIsolationLevel)`: Sets the transaction isolation level for SQLite (only SERIALIZABLE is supported).
- `getTransactionIsolation()`: Returns the current transaction isolation level.
- `registerStatement(std::weak_ptr<SQLitePreparedStatement>)`: Registers a statement with the connection for proper cleanup.
- `unregisterStatement(std::weak_ptr<SQLitePreparedStatement>)`: Unregisters a statement from the connection.

**Inheritance:**
- Inherits from `Connection` and `std::enable_shared_from_this<SQLiteConnection>` for proper resource management.

**Smart Pointer Usage:**
- Uses `shared_ptr<sqlite3>` with custom deleter (`SQLiteDbDeleter`) for connection handle
- PreparedStatements use `weak_ptr<sqlite3>` to safely detect when connection is closed
- Active statements tracked via `set<weak_ptr<SQLitePreparedStatement>>` to avoid preventing destruction

### SQLiteDBDriver
Implementation of Driver for SQLite.

**Methods:**
Same as Driver, plus:
- `SQLiteDBDriver()`: Constructor.
- `parseURL(string, string&)`: Parses a connection URL.

---

## Firebird Implementation
*Components defined in drivers/relational/driver_firebird.hpp and src/drivers/relational/driver_firebird.cpp*

### FirebirdResultSet
Implementation of ResultSet for Firebird SQL.

**Methods:**
Same as ResultSet, plus:
- `FirebirdResultSet(isc_stmt_handle, XSQLDA*, weak_ptr<isc_db_handle>)`: Constructor that takes a Firebird statement handle, SQLDA structure, and connection reference.

### FirebirdPreparedStatement
Implementation of PreparedStatement for Firebird SQL.

**Methods:**
Same as PreparedStatement, plus:
- `FirebirdPreparedStatement(shared_ptr<isc_db_handle>, isc_tr_handle*, string)`: Constructor that takes a Firebird database handle, transaction handle, and SQL statement.

### FirebirdConnection
Implementation of Connection for Firebird SQL.

**Methods:**
Same as Connection, plus:
- `FirebirdConnection(string, int, string, string, string, map<string, string>)`: Constructor that takes host, port, database, user, password, and optional connection options.
- `setTransactionIsolation(TransactionIsolationLevel)`: Sets the transaction isolation level for Firebird (default: READ COMMITTED).
- `getTransactionIsolation()`: Returns the current transaction isolation level.
- `returnToPool()`: Returns the connection to the pool with proper transaction cleanup.

**Smart Pointer Usage:**
- Uses `shared_ptr<isc_db_handle>` with custom deleter for connection handle
- PreparedStatements use `weak_ptr<isc_db_handle>` to safely detect when connection is closed
- SQLDA structures managed with proper memory allocation/deallocation

### FirebirdDBDriver
Implementation of Driver for Firebird SQL.

**Methods:**
Same as Driver, plus:
- `FirebirdDBDriver()`: Constructor.
- `parseURL(string, string&, int&, string&)`: Parses a connection URL.
- `acceptsURL(string)`: Returns true only for `cpp_dbc:firebird://` URLs.
- `command(map<string, any>)`: Executes driver-specific commands (e.g., "create_database").
- `createDatabase(string, string, string, map<string, string>)`: Creates a new Firebird database with optional page size and charset.

**Database Creation:**
The Firebird driver supports creating new databases programmatically:

```cpp
auto driver = std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>();

// Using the command method
std::map<std::string, std::any> params = {
    {"command", std::string("create_database")},
    {"url", std::string("cpp_dbc:firebird://localhost:3050/path/to/new.fdb")},
    {"user", std::string("SYSDBA")},
    {"password", std::string("masterkey")},
    {"page_size", std::string("4096")},  // optional, default: 4096
    {"charset", std::string("UTF8")}     // optional, default: UTF8
};
driver->command(params);

// Or using the createDatabase method directly
driver->createDatabase(
    "cpp_dbc:firebird://localhost:3050/path/to/new.fdb",
    "SYSDBA",
    "masterkey",
    {{"page_size", "8192"}, {"charset", "UTF8"}}
);
```

### FirebirdConnectionPool
Implementation of ConnectionPool for Firebird SQL databases.

**Methods:**
- `FirebirdConnectionPool(string, string, string)`: Constructor that takes a URL, username, and password.
- `FirebirdConnectionPool(ConnectionPoolConfig)`: Constructor that takes a pool configuration.

---

## MongoDB Implementation
*Components defined in drivers/document/driver_mongodb.hpp and src/drivers/document/driver_mongodb.cpp*

MongoDB is a document database that stores data in flexible, JSON-like documents. The CPP_DBC library provides a complete MongoDB driver implementation with support for CRUD operations, collection management, and cursor-based iteration.

### DocumentDBData
An abstract base class representing document data.

**Methods:**
- `toJson()`: Returns the document as a JSON string.
- `getString(string)`: Gets a string value by key.
- `getInt(string)`: Gets an integer value by key.
- `getDouble(string)`: Gets a double value by key.
- `getBool(string)`: Gets a boolean value by key.
- `hasField(string)`: Returns true if the document has the specified field.

### DocumentDBCursor
An abstract base class representing a cursor for iterating over documents.

**Methods:**
- `next()`: Advances to the next document, returns true if successful.
- `getData()`: Returns the current document data.
- `hasNext()`: Returns true if there are more documents.

### DocumentDBCollection
An abstract base class representing a collection of documents.

**Methods:**
- `insertOne(string)`: Inserts a single document (JSON string).
- `insertMany(vector<string>)`: Inserts multiple documents.
- `find(string)`: Finds documents matching the filter, returns a cursor.
- `findOne(string)`: Finds a single document matching the filter.
- `updateOne(string, string)`: Updates a single document matching the filter.
- `updateMany(string, string)`: Updates multiple documents matching the filter.
- `deleteOne(string)`: Deletes a single document matching the filter.
- `deleteMany(string)`: Deletes multiple documents matching the filter.
- `countDocuments(string)`: Counts documents matching the filter.
- `createIndex(string)`: Creates an index on the collection.
- `dropIndex(string)`: Drops an index from the collection.

### DocumentDBConnection
An abstract base class representing a connection to a document database.

**Methods:**
- `close()`: Closes the connection.
- `isClosed()`: Returns true if the connection is closed.
- `getCollection(string)`: Gets a collection by name.
- `createCollection(string)`: Creates a new collection.
- `dropCollection(string)`: Drops a collection.
- `listCollections()`: Lists all collections in the database.

### DocumentDBDriver
An abstract base class representing a document database driver.

**Methods:**
- `connectDocument(string, string, string, map<string, string>)`: Establishes a document database connection.
- `acceptsURL(string)`: Returns true if the driver can connect to the given URL.

### MongoDBData
Implementation of DocumentDBData for MongoDB.

**Methods:**
Same as DocumentDBData, plus:
- `MongoDBData(bsoncxx::document::value)`: Constructor that takes a BSON document.
- `getDocument()`: Returns the underlying BSON document.

### MongoDBCursor
Implementation of DocumentDBCursor for MongoDB.

**Methods:**
Same as DocumentDBCursor, plus:
- `MongoDBCursor(mongocxx::cursor)`: Constructor that takes a MongoDB cursor.

### MongoDBCollection
Implementation of DocumentDBCollection for MongoDB.

**Methods:**
Same as DocumentDBCollection, plus:
- `MongoDBCollection(mongocxx::collection)`: Constructor that takes a MongoDB collection.

### MongoDBConnection
Implementation of DocumentDBConnection for MongoDB.

**Methods:**
Same as DocumentDBConnection, plus:
- `MongoDBConnection(string, int, string, string, string, map<string, string>)`: Constructor that takes host, port, database, user, password, and optional connection options.
- `getURL()`: Returns the connection URL.

**Smart Pointer Usage:**
- Uses smart pointers for proper resource management
- Thread-safe operations (conditionally compiled with `DB_DRIVER_THREAD_SAFE`)

### MongoDBDriver
Implementation of DocumentDBDriver for MongoDB.

**Methods:**
Same as DocumentDBDriver, plus:
- `MongoDBDriver()`: Constructor.
- `parseURL(string, string&, int&, string&)`: Parses a connection URL.
- `acceptsURL(string)`: Returns true only for `mongodb://` URLs.

**Connection URL Format:**
```
mongodb://host:port/database
mongodb://username:password@host:port/database?authSource=admin
```

**Usage Example:**
```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>

// Register the MongoDB driver
cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>());

// Connect to MongoDB
auto conn = cpp_dbc::DriverManager::getDocumentDBConnection(
    "mongodb://localhost:27017/mydb", "user", "password");

// Get a collection
auto collection = conn->getCollection("users");

// Insert a document
collection->insertOne(R"({"name": "John", "age": 30})");

// Find documents
auto cursor = collection->find(R"({"age": {"$gte": 25}})");
while (cursor->next()) {
    auto data = cursor->getData();
    std::cout << data->toJson() << std::endl;
}

// Update a document
collection->updateOne(
    R"({"name": "John"})",
    R"({"$set": {"age": 31}})"
);

// Delete a document
collection->deleteOne(R"({"name": "John"})");

// Close the connection
conn->close();
#endif
```

---

## Connection Pool
*Components defined in connection_pool.hpp and connection_pool.cpp*

### SQLiteConnectionPool
Implementation of ConnectionPool for SQLite databases.

**Methods:**
- `SQLiteConnectionPool(string, string, string)`: Constructor that takes a URL, username, and password.
- `SQLiteConnectionPool(ConnectionPoolConfig)`: Constructor that takes a pool configuration.

### DBConnectionPoolConfig
Configuration structure for connection pools.

**Properties:**
- `url`: The database URL
- `username`: The database username
- `password`: The database password
- `options`: Map of connection options specific to the database type
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
- `ConnectionPool(string, string, string, map<string, string>, int, int, int, int, int, int, int, bool, bool, string)`: Constructor that takes individual configuration parameters.
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
- `PooledConnection(Connection, weak_ptr<ConnectionPool>, shared_ptr<atomic<bool>>, ConnectionPool*)`: Constructor that takes a connection, weak pool reference, pool alive flag, and raw pool pointer.
- `getCreationTime()`: Returns the creation time of the connection.
- `getLastUsedTime()`: Returns the last used time of the connection.
- `setActive(bool)`: Sets whether the connection is active.
- `isActive()`: Returns whether the connection is active.
- `getUnderlyingConnection()`: Returns the underlying physical connection.
- `setTransactionIsolation(TransactionIsolationLevel)`: Delegates to the underlying connection.
- `getTransactionIsolation()`: Delegates to the underlying connection.
- `isPoolValid()`: Returns whether the pool is still alive (checks the shared atomic flag).

**Memory Safety:**
- Uses `weak_ptr<ConnectionPool>` for pool reference
- Uses `shared_ptr<atomic<bool>>` (`m_poolAlive`) to track pool lifetime
- The `close()` method checks `isPoolValid()` before returning connection to pool
- Prevents use-after-free when pool is destroyed while connections are in use

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
- `getTransactionDBConnection(string)`: Gets the connection for a transaction.
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
- `getOptionsObj()`: Gets the ConnectionOptions object.
- `getOptions()`: Gets all database-specific options as a map.
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
- `getDatabaseByName(string)`: Gets an optional reference to a database configuration by name.
- `getDatabases()`: Gets all database configurations.
- `addConnectionPoolConfig(ConnectionPoolConfig)`: Adds a connection pool configuration.
- `getConnectionPoolConfig(string)`: Gets an optional reference to a connection pool configuration by name.
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
- SQLite development libraries (for SQLite support, optional)
- Firebird development libraries (for Firebird SQL support, optional)
- MongoDB C++ Driver (libmongoc-dev, libbson-dev, libmongocxx-dev, libbsoncxx-dev for MongoDB support, optional)
- yaml-cpp library (for YAML configuration support, optional)
- libdw library (part of elfutils, for enhanced stack traces, optional)
- CMake 3.15 or later
- Conan for dependency management

### Compiler Warnings and Code Quality

The cpp_dbc library is built with comprehensive warning flags and compile-time checks to ensure high code quality:

```cmake
# Warning flags used by the library
-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wcast-qual -Wformat=2 -Wunused
-Werror=return-type -Werror=switch -Wdouble-promotion -Wfloat-equal -Wundef
-Wpointer-arith -Wcast-align
```

These warning flags help catch potential issues:

- `-Wall -Wextra -Wpedantic`: Standard warning flags
- `-Wconversion`: Warns about implicit conversions that may change a value
- `-Wshadow`: Warns when a variable declaration shadows another variable
- `-Wcast-qual`: Warns about casts that remove type qualifiers
- `-Wformat=2`: Enables additional format string checks
- `-Wunused`: Warns about unused variables and functions
- `-Werror=return-type`: Treats missing return statements as errors
- `-Werror=switch`: Treats switch statement issues as errors
- `-Wdouble-promotion`: Warns about implicit float to double promotions
- `-Wfloat-equal`: Warns about floating-point equality comparisons
- `-Wundef`: Warns about undefined identifiers in preprocessor expressions
- `-Wpointer-arith`: Warns about suspicious pointer arithmetic
- `-Wcast-align`: Warns about pointer casts that increase alignment requirements

The library also includes special handling for backward.hpp to silence -Wundef warnings:

```cmake
# Define macros for backward.hpp to silence -Wundef warnings
target_compile_definitions(cpp_dbc PUBLIC
    BACKWARD_HAS_UNWIND=0
    BACKWARD_HAS_LIBUNWIND=0
    BACKWARD_HAS_BACKTRACE=0
    BACKWARD_HAS_BFD=0
    BACKWARD_HAS_DWARF=0
    BACKWARD_HAS_BACKTRACE_SYMBOL=0
    BACKWARD_HAS_PDB_SYMBOL=0
)
```

Code quality improvements include:
- Using m_ prefix for member variables to avoid -Wshadow warnings
- Adding static_cast<> for numeric conversions to avoid -Wconversion warnings
- Changing int return types to uint64_t for executeUpdate() methods
- Improving exception handling to avoid variable shadowing
- Using `int64_t` instead of `long` for cross-platform portability (Windows/Linux/macOS)
- Cross-platform time functions (`localtime_s` on Windows, `localtime_r` on Unix)
- `[[nodiscard]]` attribute for methods returning `expected<T, DBException>`
- Security improvements in SQLite BLOB operations with `validateIdentifier()`
- Safe exception handling in destructors with `std::scoped_lock`

### Building with Scripts

The library provides build scripts to simplify the build process:

```bash
# Build with MySQL support only (default)
./build.sh

# Build with MySQL and PostgreSQL support
./build.sh --mysql --postgres

# Build with PostgreSQL support only
./build.sh --mysql-off --postgres

# Build with SQLite support
./build.sh --sqlite

# Build with Firebird SQL support
./build.sh --firebird

# Build with MongoDB support
./build.sh --mongodb

# Build with MySQL, PostgreSQL, SQLite, Firebird and MongoDB support
./build.sh --mysql --postgres --sqlite --firebird --mongodb

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

# Disable libdw support for stack traces
./build.sh --dw-off

# Disable thread-safe database driver operations (for single-threaded performance)
./build.sh --db-driver-thread-safe-off

# Build Docker container
./build.dist.sh

# Build Docker container with PostgreSQL and YAML support
./build.dist.sh --postgres --yaml

# Build Docker container with SQLite support
./build.dist.sh --sqlite

# Build Docker container with Firebird support
./build.dist.sh --firebird

# Build Docker container with MongoDB support
./build.dist.sh --mongodb

# Build Docker container with all database drivers
./build.dist.sh --postgres --sqlite --firebird --mongodb --yaml

# Build Docker container without libdw support
./build.dist.sh --dw-off

# Build Docker container without thread-safe driver operations
./build.dist.sh --db-driver-thread-safe-off

# Build distribution packages (.deb and .rpm)
./libs/cpp_dbc/build_dist_pkg.sh

# Build packages for multiple distributions
./libs/cpp_dbc/build_dist_pkg.sh --distro=ubuntu:24.04+ubuntu:22.04+debian:12+debian:13+fedora:42+fedora:43

# Build packages with specific options
./libs/cpp_dbc/build_dist_pkg.sh --build=yaml,mysql,postgres,sqlite,debug,dw,examples

# Build packages with specific version
./libs/cpp_dbc/build_dist_pkg.sh --version=1.0.1
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

The `build_dist_pkg.sh` script:
1. Creates Docker containers based on target distributions
2. Builds the cpp_dbc library with specified options
3. Creates distribution packages (.deb for Debian/Ubuntu, .rpm for Fedora)
4. Copies the packages to the build directory

### Helper Script

The project includes a helper script (`helper.sh`) that provides various utilities:

```bash
# Build the project with MySQL support
./helper.sh --build

# Build with PostgreSQL support
./helper.sh --build --postgres

# Build with SQLite support
./helper.sh --build --sqlite

# Build with Firebird support
./helper.sh --build --firebird

# Build with YAML support
./helper.sh --build --yaml

# Build without libdw support
./helper.sh --build --dw-off

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

# Configure with CMake (MySQL enabled, PostgreSQL disabled, SQLite disabled, Firebird disabled, MongoDB disabled, YAML disabled, libdw enabled, thread-safe enabled)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_FIREBIRD=OFF -DUSE_MONGODB=OFF -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DDB_DRIVER_THREAD_SAFE=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configure with MongoDB support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_FIREBIRD=OFF -DUSE_MONGODB=ON -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configure with Firebird support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_FIREBIRD=ON -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configure without thread-safe driver operations (for single-threaded performance)
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DDB_DRIVER_THREAD_SAFE=OFF -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configure with YAML support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configure with SQLite support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=ON -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=ON -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Configure without libdw support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=OFF -DBACKWARD_HAS_DW=OFF -DCMAKE_INSTALL_PREFIX="../../../build/libs/cpp_dbc"

# Build and install
cmake --build . --target install

# Return to root directory
cd ../../..

# Create build directory for the main application
mkdir -p build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=OFF -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

# Configure with YAML support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=OFF -DUSE_CPP_YAML=ON -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

# Configure with SQLite support
# cmake .. -DCMAKE_BUILD_TYPE=Debug -DUSE_MYSQL=ON -DUSE_POSTGRESQL=OFF -DUSE_SQLITE=ON -DUSE_CPP_YAML=OFF -DCMAKE_PREFIX_PATH=../build/libs/cpp_dbc

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
    $<$<BOOL:${USE_SQLITE}>:USE_SQLITE=1>
    $<$<NOT:$<BOOL:${USE_SQLITE}>>:USE_SQLITE=0>
    $<$<BOOL:${USE_FIREBIRD}>:USE_FIREBIRD=1>
    $<$<NOT:$<BOOL:${USE_FIREBIRD}>>:USE_FIREBIRD=0>
    $<$<BOOL:${USE_MONGODB}>:USE_MONGODB=1>
    $<$<NOT:$<BOOL:${USE_MONGODB}>>:USE_MONGODB=0>
    $<$<BOOL:${USE_CPP_YAML}>:USE_CPP_YAML=1>
    $<$<NOT:$<BOOL:${USE_CPP_YAML}>>:USE_CPP_YAML=0>
    $<$<BOOL:${BACKWARD_HAS_DW}>:BACKWARD_HAS_DW=1>
    $<$<NOT:$<BOOL:${BACKWARD_HAS_DW}>>:BACKWARD_HAS_DW=0>
    $<$<BOOL:${DB_DRIVER_THREAD_SAFE}>:DB_DRIVER_THREAD_SAFE=1>
    $<$<NOT:$<BOOL:${DB_DRIVER_THREAD_SAFE}>>:DB_DRIVER_THREAD_SAFE=0>
)
```

In your C++ code:

```cpp
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif
#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif
#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif
#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
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
auto dbConfigOpt = configManager.getDatabaseByName("dev_mysql");
if (dbConfigOpt) {
    // Use the configuration to create a connection
    const auto& dbConfig = dbConfigOpt->get();
    std::string connStr = dbConfig.createConnectionString();
    auto conn = cpp_dbc::DriverManager::getDBConnection(
        connStr, dbConfig.getUsername(), dbConfig.getPassword()
    );
    
    // Use the connection...
    conn->close();
}
#endif
