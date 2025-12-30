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

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "test_mysql_common.hpp"
#include "test_postgresql_common.hpp"
#include "test_sqlite_common.hpp"
#include "test_firebird_common.hpp"

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

    // Get MySQL configuration using the helper function
    auto dbConfig = mysql_test_helpers::getMySQLConfig("dev_mysql");

    // Create connection parameters
    std::string type = dbConfig.getType();
    std::string host = dbConfig.getHost();
    std::string database = dbConfig.getDatabase();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    std::string connStr = dbConfig.createConnectionString();

    // Get test queries from config
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE)");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_table (id, name, value) VALUES (1, 'Test', 1.5)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_table");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE IF EXISTS test_table");

    SECTION("Basic connection pool operations")
    {
        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
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
        auto conn = pool.getRelationalDBConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool.getIdleDBConnectionCount();
            auto initialActiveCount = pool.getActiveDBConnectionCount();
            auto initialTotalCount = pool.getTotalDBConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool.getDBConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool.getActiveDBConnectionCount() == 1);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool.getDBConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool.getActiveDBConnectionCount() == 2);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool.getActiveDBConnectionCount() == 1);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool.getActiveDBConnectionCount() == 0);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount);
        }

        // Clean up
        auto cleanupConn = pool.getRelationalDBConnection();
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

    // Get PostgreSQL configuration using the helper function
    auto dbConfig = postgresql_test_helpers::getPostgreSQLConfig("dev_postgresql");

    // Create connection parameters
    std::string type = dbConfig.getType();
    std::string host = dbConfig.getHost();
    std::string database = dbConfig.getDatabase();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    std::string connStr = dbConfig.createConnectionString();

    // Get test queries from config
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE PRECISION)");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_table (id, name, value) VALUES (1, 'Test', 1.5)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_table");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE IF EXISTS test_table");

    SECTION("Basic connection pool operations")
    {
        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
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
        auto conn = pool.getRelationalDBConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool.getIdleDBConnectionCount();
            auto initialActiveCount = pool.getActiveDBConnectionCount();
            auto initialTotalCount = pool.getTotalDBConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool.getDBConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool.getActiveDBConnectionCount() == 1);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool.getDBConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool.getActiveDBConnectionCount() == 2);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool.getActiveDBConnectionCount() == 1);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool.getActiveDBConnectionCount() == 0);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount);
        }

        // Clean up
        auto cleanupConn = pool.getRelationalDBConnection();
        cleanupConn->executeUpdate(dropTableQuery);
        cleanupConn->close();

        // Close the pool
        pool.close();
    }
}
#endif

#if USE_SQLITE

// Test case for real SQLite connection pool
TEST_CASE("Real SQLite connection pool tests", "[sqlite_connection_pool_real]")
{
    // Skip these tests if we can't connect to SQLite
    if (!sqlite_test_helpers::canConnectToSQLite())
    {
        SKIP("Cannot connect to SQLite database");
        return;
    }

    // Get SQLite configuration using the helper function
    auto dbConfig = sqlite_test_helpers::getSQLiteConfig("dev_sqlite");

    // Create connection parameters
    std::string type = dbConfig.getType();
    std::string database = dbConfig.getDatabase();
    std::string username = ""; // SQLite doesn't use username
    std::string password = ""; // SQLite doesn't use password

    std::string connStr = dbConfig.createConnectionString();

    // Get test queries from config
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT, value REAL)");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_table (id, name, value) VALUES (1, 'Test', 1.5)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_table");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE IF EXISTS test_table");

    // Create a connection pool configuration
    cpp_dbc::config::DBConnectionPoolConfig poolConfig;
    poolConfig.setUrl(connStr);
    poolConfig.setUsername(username);
    poolConfig.setPassword(password);

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
    // Load the configuration using DatabaseConfigManager
    std::string config_path = common_test_helpers::getConfigFilePath();
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

    // Load the SQLite pool configuration from DatabaseConfigManager
    auto poolConfigOpt = configManager.getDBConnectionPoolConfig("sqlite_pool");
    if (poolConfigOpt.has_value())
    {
        const cpp_dbc::config::DBConnectionPoolConfig &poolCfg = poolConfigOpt.value().get();

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
        auto conn = pool.getRelationalDBConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool.getIdleDBConnectionCount();
            auto initialActiveCount = pool.getActiveDBConnectionCount();
            auto initialTotalCount = pool.getTotalDBConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool.getDBConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool.getActiveDBConnectionCount() == 1);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool.getDBConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool.getActiveDBConnectionCount() == 2);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool.getActiveDBConnectionCount() == 1);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool.getActiveDBConnectionCount() == 0);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount);
        }

        // Clean up
        auto cleanupConn = pool.getRelationalDBConnection();
        cleanupConn->executeUpdate(dropTableQuery);
        cleanupConn->close();

        // Close the pool
        pool.close();
    }
}
#endif

#if USE_FIREBIRD
// Test case for real Firebird connection pool
TEST_CASE("Real Firebird connection pool tests", "[firebird_connection_pool_real]")
{
    // Skip these tests if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    // Get Firebird configuration using the helper function
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");

    // Create connection parameters
    std::string type = dbConfig.getType();
    std::string host = dbConfig.getHost();
    std::string database = dbConfig.getDatabase();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    std::string connStr = dbConfig.createConnectionString();

    // Get test queries from config
    // Note: "VALUE" is a reserved word in Firebird, so we use "amount" instead
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "CREATE TABLE test_table (id INTEGER PRIMARY KEY, name VARCHAR(100), amount DOUBLE PRECISION)");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_table (id, name, amount) VALUES (1, 'Test', 1.5)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_table");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE test_table");

    SECTION("Basic connection pool operations")
    {
        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
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
        poolConfig.setValidationQuery("SELECT 1 FROM RDB$DATABASE");

        // Create a connection pool
        cpp_dbc::Firebird::FirebirdConnectionPool pool(poolConfig);

        // Create a test table (drop first if exists using exception handling)
        auto conn = pool.getRelationalDBConnection();
        try
        {
            conn->executeUpdate(dropTableQuery);
        }
        catch (const cpp_dbc::DBException &)
        {
            // Table might not exist, ignore error
        }
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool.getIdleDBConnectionCount();
            auto initialActiveCount = pool.getActiveDBConnectionCount();
            auto initialTotalCount = pool.getTotalDBConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool.getDBConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool.getActiveDBConnectionCount() == 1);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool.getDBConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool.getActiveDBConnectionCount() == 2);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool.getActiveDBConnectionCount() == 1);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool.getActiveDBConnectionCount() == 0);
            REQUIRE(pool.getIdleDBConnectionCount() == initialIdleCount);
        }

        // Clean up
        auto cleanupConn = pool.getRelationalDBConnection();
        try
        {
            cleanupConn->executeUpdate(dropTableQuery);
        }
        catch (const cpp_dbc::DBException &)
        {
            // Ignore cleanup errors
        }
        cleanupConn->close();

        // Close the pool
        pool.close();
    }
}
#endif