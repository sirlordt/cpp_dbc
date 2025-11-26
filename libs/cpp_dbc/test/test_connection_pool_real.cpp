/**

 * Copyright 2025 Tomas R Moreno P <tomasr.morenop@gmail.com>. All Rights Reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file test_connection_pool_real.cpp
 @brief Tests for database connections

*/

#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/config/yaml_config_loader.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include "test_mysql_common.hpp"
#include "test_postgresql_common.hpp"

std::string getConfigFilePath();

#if USE_MYSQL
// Test case for real MySQL connection pool
TEST_CASE("Real MySQL connection pool tests", "[mysql_connection_pool_real]")
{
    // Skip these tests if we can't connect to MySQL
    if (!mysql_test_helpers::canConnectToMySQL())
    {
        SKIP("Cannot connect to MySQL database");
        return;
    }

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
    // Load the configuration using DatabaseConfigManager
    std::string config_path = getConfigFilePath();
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

    // Find the dev_mysql configuration
    auto dbConfigOpt = configManager.getDatabaseByName("dev_mysql");
    if (!dbConfigOpt.has_value())
    {
        SKIP("MySQL configuration 'dev_mysql' not found in config file");
        return;
    }
    const cpp_dbc::config::DatabaseConfig &dbConfig = dbConfigOpt.value().get();

    // Create connection parameters
    std::string type = dbConfig.getType();
    std::string host = dbConfig.getHost();
    // int port = dbConfig.getPort();
    std::string database = dbConfig.getDatabase();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    std::string connStr = dbConfig.createConnectionString();

    // Get test queries
    const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();
    std::string createTableQuery = testQueries.getQuery("mysql", "create_table");
    std::string insertDataQuery = testQueries.getQuery("mysql", "insert_data");
    std::string selectDataQuery = testQueries.getQuery("mysql", "select_data");
    std::string dropTableQuery = testQueries.getQuery("mysql", "drop_table");
#else
    // Create connection parameters with default values when YAML is disabled
    std::string type = "mysql";
    std::string host = "localhost";
    int port = 3306;
    std::string database = "Test01DB";
    std::string username = "root";
    std::string password = "dsystems";
    std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

    // Default test queries
    std::string createTableQuery = "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE)";
    std::string insertDataQuery = "INSERT INTO test_table (id, name, value) VALUES (1, 'Test', 1.5)";
    std::string selectDataQuery = "SELECT * FROM test_table";
    std::string dropTableQuery = "DROP TABLE IF EXISTS test_table";
#endif

    SECTION("Basic connection pool operations")
    {
        // Create a connection pool configuration
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        // poolConfig.setPort(port);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(10);
        poolConfig.setMinIdle(3);
        poolConfig.setConnectionTimeout(5000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setIdleTimeout(30000);
        poolConfig.setMaxLifetimeMillis(60000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool
        cpp_dbc::MySQL::MySQLConnectionPool pool(poolConfig);

        // Create a test table
        auto conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool.getIdleConnectionCount();
            auto initialActiveCount = pool.getActiveConnectionCount();
            auto initialTotalCount = pool.getTotalConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool.getConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool.getConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 2);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool.getActiveConnectionCount() == 0);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount);
        }

        // Clean up
        auto cleanupConn = pool.getConnection();
        cleanupConn->executeUpdate(dropTableQuery);
        cleanupConn->close();

        // Close the pool
        pool.close();
    }
}
#endif

#if USE_POSTGRESQL
// Test case for real PostgreSQL connection pool
TEST_CASE("Real PostgreSQL connection pool tests", "[postgresql_connection_pool_real]")
{
    // Skip these tests if we can't connect to PostgreSQL
    if (!postgresql_test_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
    // Load the configuration using DatabaseConfigManager
    std::string config_path = getConfigFilePath();
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

    // Find the dev_postgresql configuration
    auto dbConfigOpt = configManager.getDatabaseByName("dev_postgresql");
    if (!dbConfigOpt.has_value())
    {
        SKIP("PostgreSQL configuration 'dev_postgresql' not found in config file");
        return;
    }
    const cpp_dbc::config::DatabaseConfig &dbConfig = dbConfigOpt.value().get();

    // Create connection parameters
    std::string type = dbConfig.getType();
    std::string host = dbConfig.getHost();
    // int port = dbConfig.getPort();
    std::string database = dbConfig.getDatabase();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    std::string connStr = dbConfig.createConnectionString();

    // Get test queries
    const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();
    std::string createTableQuery = testQueries.getQuery("postgresql", "create_table");
    std::string insertDataQuery = testQueries.getQuery("postgresql", "insert_data");
    std::string selectDataQuery = testQueries.getQuery("postgresql", "select_data");
    std::string dropTableQuery = testQueries.getQuery("postgresql", "drop_table");
#else
    // Create connection parameters with default values when YAML is disabled
    std::string type = "postgresql";
    std::string host = "localhost";
    int port = 5432;
    std::string database = "Test01DB";
    std::string username = "postgres";
    std::string password = "dsystems";
    std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

    // Default test queries
    std::string createTableQuery = "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE PRECISION)";
    std::string insertDataQuery = "INSERT INTO test_table (id, name, value) VALUES (1, 'Test', 1.5)";
    std::string selectDataQuery = "SELECT * FROM test_table";
    std::string dropTableQuery = "DROP TABLE IF EXISTS test_table";
#endif

    SECTION("Basic connection pool operations")
    {
        // Create a connection pool configuration
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(10);
        poolConfig.setMinIdle(3);
        poolConfig.setConnectionTimeout(5000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setIdleTimeout(30000);
        poolConfig.setMaxLifetimeMillis(60000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool
        cpp_dbc::PostgreSQL::PostgreSQLConnectionPool pool(poolConfig);

        // Create a test table
        auto conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool.getIdleConnectionCount();
            auto initialActiveCount = pool.getActiveConnectionCount();
            auto initialTotalCount = pool.getTotalConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool.getConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool.getConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 2);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool.getActiveConnectionCount() == 0);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount);
        }

        // Clean up
        auto cleanupConn = pool.getConnection();
        cleanupConn->executeUpdate(dropTableQuery);
        cleanupConn->close();

        // Close the pool
        pool.close();
    }
}
#endif

#if USE_SQLITE
#include <cpp_dbc/drivers/driver_sqlite.hpp>

// Helper function to check if we can connect to SQLite
static bool canConnectToSQLite()
{
    try
    {
#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
        // Load the configuration using DatabaseConfigManager
        std::string config_path = getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Find the dev_sqlite configuration
        auto dbConfigOpt = configManager.getDatabaseByName("dev_sqlite");
        if (!dbConfigOpt.has_value())
        {
            std::cerr << "SQLite configuration not found in test_db_connections.yml" << std::endl;
            return false;
        }
        const cpp_dbc::config::DatabaseConfig &dbConfig = dbConfigOpt.value().get();

        // Create connection string
        std::string type = dbConfig.getType();
        std::string database = dbConfig.getDatabase();
        std::string username = ""; // SQLite doesn't use username
        std::string password = ""; // SQLite doesn't use password
#else
        // Create connection string with default values when YAML is disabled
        std::string type = "sqlite";
        std::string database = ":memory:";
        std::string username = ""; // SQLite doesn't use username
        std::string password = ""; // SQLite doesn't use password
#endif

        std::string connStr = "cpp_dbc:" + type + "://" + database;

        // Register the SQLite driver
        cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDriver>());

        // Attempt to connect to SQLite
        std::cout << "Attempting to connect to SQLite with connection string: " << connStr << std::endl;

        auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

        // If we get here, the connection was successful
        std::cout << "SQLite connection successful!" << std::endl;

        // Execute a simple query to verify the connection
        auto resultSet = conn->executeQuery("SELECT 1 as test_value");
        bool success = resultSet->next() && resultSet->getInt("test_value") == 1;

        // Close the connection
        conn->close();

        return success;
    }
    catch (const std::exception &e)
    {
        std::cerr << "SQLite connection error: " << e.what() << std::endl;
        return false;
    }
}

// Test case for real SQLite connection pool
TEST_CASE("Real SQLite connection pool tests", "[sqlite_connection_pool_real]")
{
    // Skip these tests if we can't connect to SQLite
    if (!canConnectToSQLite())
    {
        SKIP("Cannot connect to SQLite database");
        return;
    }

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
    // Load the configuration using DatabaseConfigManager
    std::string config_path = getConfigFilePath();
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

    // Find the dev_sqlite configuration
    auto dbConfigOpt = configManager.getDatabaseByName("dev_sqlite");
    if (!dbConfigOpt.has_value())
    {
        SKIP("SQLite configuration 'dev_sqlite' not found in config file");
        return;
    }
    const cpp_dbc::config::DatabaseConfig &dbConfig = dbConfigOpt.value().get();

    // Create connection parameters
    std::string type = dbConfig.getType();
    std::string database = dbConfig.getDatabase();
    std::string username = ""; // SQLite doesn't use username
    std::string password = ""; // SQLite doesn't use password

    std::string connStr = dbConfig.createConnectionString();

    // Get test queries
    const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();
    std::string createTableQuery = testQueries.getQuery("sqlite", "create_table");
    std::string insertDataQuery = testQueries.getQuery("sqlite", "insert_data");
    std::string selectDataQuery = testQueries.getQuery("sqlite", "select_data");
    std::string dropTableQuery = testQueries.getQuery("sqlite", "drop_table");
#else
    // Create connection parameters with default values when YAML is disabled
    std::string type = "sqlite";
    std::string database = ":memory:";
    std::string username = ""; // SQLite doesn't use username
    std::string password = ""; // SQLite doesn't use password

    std::string connStr = "cpp_dbc:" + type + "://" + database;

    // Default test queries
    std::string createTableQuery = "CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT, value REAL)";
    std::string insertDataQuery = "INSERT INTO test_table (id, name, value) VALUES (1, 'Test', 1.5)";
    std::string selectDataQuery = "SELECT * FROM test_table";
    std::string dropTableQuery = "DROP TABLE IF EXISTS test_table";
#endif

    // Create a connection pool configuration
    cpp_dbc::config::ConnectionPoolConfig poolConfig;
    poolConfig.setUrl(connStr);
    poolConfig.setUsername(username);
    poolConfig.setPassword(password);

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
    // Load the SQLite pool configuration from DatabaseConfigManager
    auto poolConfigOpt = configManager.getConnectionPoolConfig("sqlite_pool");
    if (poolConfigOpt.has_value())
    {
        const cpp_dbc::config::ConnectionPoolConfig &poolCfg = poolConfigOpt.value().get();

        // Set pool parameters from the configuration
        poolConfig.setInitialSize(poolCfg.getInitialSize());
        poolConfig.setMaxSize(poolCfg.getMaxSize());
        poolConfig.setMinIdle(5); // Default value
        poolConfig.setConnectionTimeout(poolCfg.getConnectionTimeout());
        poolConfig.setValidationInterval(poolCfg.getValidationInterval());
        poolConfig.setIdleTimeout(poolCfg.getIdleTimeout());
    }
#endif

    SECTION("Basic connection pool operations")
    {
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(10);
        poolConfig.setMinIdle(5);
        poolConfig.setConnectionTimeout(5000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setIdleTimeout(30000);
        poolConfig.setMaxLifetimeMillis(60000); // Default value
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Set the transaction isolation level to SERIALIZABLE for SQLite
        poolConfig.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

        // Create a connection pool
        cpp_dbc::SQLite::SQLiteConnectionPool pool(poolConfig);

        // Create a test table
        auto conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool.getIdleConnectionCount();
            auto initialActiveCount = pool.getActiveConnectionCount();
            auto initialTotalCount = pool.getTotalConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool.getConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool.getConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 2);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool.getActiveConnectionCount() == 0);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount);
        }

        // Clean up
        auto cleanupConn = pool.getConnection();
        cleanupConn->executeUpdate(dropTableQuery);
        cleanupConn->close();

        // Close the pool
        pool.close();
    }
}
#endif