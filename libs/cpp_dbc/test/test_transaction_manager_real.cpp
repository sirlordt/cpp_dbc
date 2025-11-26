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

 @file test_transaction_manager_real.cpp
 @brief Tests for transaction management

*/

#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
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
#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
#include <cpp_dbc/config/yaml_config_loader.hpp>
#endif
#include "test_mysql_common.hpp"
#include "test_postgresql_common.hpp"

// Helper function to get the path to the test_db_connections.yml file
std::string getConfigFilePath();

#if USE_MYSQL
// Test case for real MySQL transaction manager
TEST_CASE("Real MySQL transaction manager tests", "[transaction_manager_real]")
{
    // Skip these tests if we can't connect to MySQL
    if (!mysql_test_helpers::canConnectToMySQL())
    {
        SKIP("Cannot connect to MySQL database");
        return;
    }

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
    // Load the YAML configuration
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
    // Default values when YAML is disabled
    std::string type = "mysql";
    std::string host = "localhost";
    int port = 3306;
    std::string database = "Test01DB";
    std::string username = "root";
    std::string password = "dsystems";

    std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

    // Default test queries
    std::string createTableQuery = "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE)";
    std::string insertDataQuery = "INSERT INTO test_table (id, name, value) VALUES (?, ?, 1.5)";
    std::string selectDataQuery = "SELECT * FROM test_table";
    std::string dropTableQuery = "DROP TABLE IF EXISTS test_table";
#endif

    SECTION("Basic transaction operations")
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
        cpp_dbc::MySQL::MySQLConnectionPool pool(poolConfig);

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Create a test table
        auto conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        SECTION("Commit transaction")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();
            REQUIRE(!txId.empty());

            // Get the connection associated with the transaction
            auto txConn = manager.getTransactionConnection(txId);
            REQUIRE(txConn != nullptr);

            // Insert data within the transaction
            auto pstmt = txConn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 1);
            pstmt->setString(2, "Transaction Test");
            auto result = pstmt->executeUpdate();
            REQUIRE(result == 1);

            // Commit the transaction
            manager.commitTransaction(txId);

            // Verify the transaction is no longer active
            REQUIRE_FALSE(manager.isTransactionActive(txId));

            // Verify the data was committed
            auto verifyConn = pool.getConnection();
            auto rs = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 1");
            REQUIRE(rs->next());
            REQUIRE(rs->getString("name") == "Transaction Test");
            verifyConn->close();
        }

        SECTION("Rollback transaction")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();
            REQUIRE(!txId.empty());

            // Get the connection associated with the transaction
            auto txConn = manager.getTransactionConnection(txId);
            REQUIRE(txConn != nullptr);

            // Insert data within the transaction
            auto pstmt = txConn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 2);
            pstmt->setString(2, "Rollback Test");
            auto result = pstmt->executeUpdate();
            REQUIRE(result == 1);

            // Rollback the transaction
            manager.rollbackTransaction(txId);

            // Verify the transaction is no longer active
            REQUIRE_FALSE(manager.isTransactionActive(txId));

            // Verify the data was not committed
            auto verifyConn = pool.getConnection();
            auto rs = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 2");
            REQUIRE_FALSE(rs->next()); // Should be no rows
            verifyConn->close();
        }

        SECTION("Multiple transactions")
        {
            // Begin multiple transactions
            std::string txId1 = manager.beginTransaction();
            std::string txId2 = manager.beginTransaction();
            std::string txId3 = manager.beginTransaction();

            REQUIRE(txId1 != txId2);
            REQUIRE(txId2 != txId3);
            REQUIRE(txId1 != txId3);

            // Get connections for each transaction
            auto txConn1 = manager.getTransactionConnection(txId1);
            auto txConn2 = manager.getTransactionConnection(txId2);
            auto txConn3 = manager.getTransactionConnection(txId3);

            REQUIRE(txConn1 != nullptr);
            REQUIRE(txConn2 != nullptr);
            REQUIRE(txConn3 != nullptr);

            // Insert data in each transaction
            auto pstmt1 = txConn1->prepareStatement(insertDataQuery);
            pstmt1->setInt(1, 10);
            pstmt1->setString(2, "Transaction 1");
            pstmt1->executeUpdate();

            auto pstmt2 = txConn2->prepareStatement(insertDataQuery);
            pstmt2->setInt(1, 20);
            pstmt2->setString(2, "Transaction 2");
            pstmt2->executeUpdate();

            auto pstmt3 = txConn3->prepareStatement(insertDataQuery);
            pstmt3->setInt(1, 30);
            pstmt3->setString(2, "Transaction 3");
            pstmt3->executeUpdate();

            // Commit first transaction
            manager.commitTransaction(txId1);

            // Rollback second transaction
            manager.rollbackTransaction(txId2);

            // Commit third transaction
            manager.commitTransaction(txId3);

            // Verify transactions are no longer active
            REQUIRE_FALSE(manager.isTransactionActive(txId1));
            REQUIRE_FALSE(manager.isTransactionActive(txId2));
            REQUIRE_FALSE(manager.isTransactionActive(txId3));

            // Verify the data from committed transactions
            auto verifyConn = pool.getConnection();

            // Transaction 1 (committed)
            auto rs1 = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 10");
            REQUIRE(rs1->next());
            REQUIRE(rs1->getString("name") == "Transaction 1");

            // Transaction 2 (rolled back)
            auto rs2 = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 20");
            REQUIRE_FALSE(rs2->next()); // Should be no rows

            // Transaction 3 (committed)
            auto rs3 = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 30");
            REQUIRE(rs3->next());
            REQUIRE(rs3->getString("name") == "Transaction 3");

            verifyConn->close();
        }

        SECTION("Transaction isolation")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();
            auto txConn = manager.getTransactionConnection(txId);

            // Insert data within the transaction
            auto pstmt = txConn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 100);
            pstmt->setString(2, "Isolation Test");
            pstmt->executeUpdate();

            // Get a separate connection (not in the transaction)
            auto regularConn = pool.getConnection();

            // Verify the data is not visible outside the transaction
            auto rs = regularConn->executeQuery("SELECT * FROM test_table WHERE id = 100");
            REQUIRE_FALSE(rs->next()); // Should be no rows

            // Commit the transaction
            manager.commitTransaction(txId);

            // Now the data should be visible
            rs = regularConn->executeQuery("SELECT * FROM test_table WHERE id = 100");
            REQUIRE(rs->next());
            REQUIRE(rs->getString("name") == "Isolation Test");

            regularConn->close();
        }

        SECTION("Transaction timeout")
        {
            // Set a very short transaction timeout
            manager.setTransactionTimeout(1); // 1 second

            // Begin a transaction
            std::string txId = manager.beginTransaction();
            auto txConn = manager.getTransactionConnection(txId);

            // Insert data within the transaction
            auto pstmt = txConn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 200);
            pstmt->setString(2, "Timeout Test");
            pstmt->executeUpdate();

            // Wait for the transaction to timeout
            std::this_thread::sleep_for(std::chrono::seconds(2));

            // The transaction should no longer be active
            REQUIRE_FALSE(manager.isTransactionActive(txId));

            // Verify the data was not committed
            auto verifyConn = pool.getConnection();
            auto rs = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 200");
            REQUIRE_FALSE(rs->next()); // Should be no rows
            verifyConn->close();

            // Reset the transaction timeout to a reasonable value
            manager.setTransactionTimeout(30); // 30 seconds
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
// Test case for real PostgreSQL transaction manager
TEST_CASE("Real PostgreSQL transaction manager tests", "[transaction_manager_real]")
{
    // Skip these tests if we can't connect to PostgreSQL
    if (!postgresql_test_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
    // Load the YAML configuration
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
    // Default values when YAML is disabled
    std::string type = "postgresql";
    std::string host = "localhost";
    int port = 5432;
    std::string database = "Test01DB";
    std::string username = "postgres";
    std::string password = "dsystems";

    std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

    // Default test queries
    std::string createTableQuery = "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE PRECISION)";
    std::string insertDataQuery = "INSERT INTO test_table (id, name, value) VALUES ($1, $2, 1.5)";
    std::string selectDataQuery = "SELECT * FROM test_table";
    std::string dropTableQuery = "DROP TABLE IF EXISTS test_table";
#endif

    SECTION("Basic transaction operations")
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

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Create a test table
        auto conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        SECTION("Commit transaction")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();
            REQUIRE(!txId.empty());

            // Get the connection associated with the transaction
            auto txConn = manager.getTransactionConnection(txId);
            REQUIRE(txConn != nullptr);

            // Insert data within the transaction
            auto pstmt = txConn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 1);
            pstmt->setString(2, "Transaction Test");
            auto result = pstmt->executeUpdate();
            REQUIRE(result == 1);

            // Commit the transaction
            manager.commitTransaction(txId);

            // Verify the transaction is no longer active
            REQUIRE_FALSE(manager.isTransactionActive(txId));

            // Verify the data was committed
            auto verifyConn = pool.getConnection();
            auto rs = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 1");
            REQUIRE(rs->next());
            REQUIRE(rs->getString("name") == "Transaction Test");
            verifyConn->close();
        }

        SECTION("Rollback transaction")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();
            REQUIRE(!txId.empty());

            // Get the connection associated with the transaction
            auto txConn = manager.getTransactionConnection(txId);
            REQUIRE(txConn != nullptr);

            // Insert data within the transaction
            auto pstmt = txConn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 2);
            pstmt->setString(2, "Rollback Test");
            auto result = pstmt->executeUpdate();
            REQUIRE(result == 1);

            // Rollback the transaction
            manager.rollbackTransaction(txId);

            // Verify the transaction is no longer active
            REQUIRE_FALSE(manager.isTransactionActive(txId));

            // Verify the data was not committed
            auto verifyConn = pool.getConnection();
            auto rs = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 2");
            REQUIRE_FALSE(rs->next()); // Should be no rows
            verifyConn->close();
        }

        SECTION("PostgreSQL specific transaction isolation levels")
        {
            // PostgreSQL supports different transaction isolation levels

            // Begin a transaction with READ COMMITTED isolation level
            auto conn1 = pool.getConnection();
            conn1->executeUpdate("BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED");

            // Insert data
            auto pstmt1 = conn1->prepareStatement(insertDataQuery);
            pstmt1->setInt(1, 300);
            pstmt1->setString(2, "Isolation Level Test");
            pstmt1->executeUpdate();

            // Begin another transaction with READ COMMITTED isolation level
            auto conn2 = pool.getConnection();
            conn2->executeUpdate("BEGIN TRANSACTION ISOLATION LEVEL READ COMMITTED");

            // Try to update the same row
            auto pstmt2 = conn2->prepareStatement("UPDATE test_table SET name = 'Updated Name' WHERE id = 300");

            // In SERIALIZABLE isolation, this should wait for the first transaction to complete
            // But we'll just check that the row exists
            auto rs2 = conn2->executeQuery("SELECT * FROM test_table WHERE id = 300");

            // The row should not be visible in the second transaction until the first one commits
            REQUIRE_FALSE(rs2->next());

            // Commit the first transaction
            conn1->executeUpdate("COMMIT");
            conn1->close();

            // Now the row should be visible in the second transaction
            rs2 = conn2->executeQuery("SELECT * FROM test_table WHERE id = 300");
            REQUIRE(rs2->next());
            REQUIRE(rs2->getString("name") == "Isolation Level Test");

            // Update the row in the second transaction
            pstmt2->executeUpdate();

            // Commit the second transaction
            conn2->executeUpdate("COMMIT");
            conn2->close();

            // Verify the update
            auto verifyConn = pool.getConnection();
            auto rs = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 300");
            REQUIRE(rs->next());
            REQUIRE(rs->getString("name") == "Updated Name");
            verifyConn->close();
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