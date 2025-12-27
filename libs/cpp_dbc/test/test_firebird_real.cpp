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

 @file test_firebird_real.cpp
 @brief Tests for Firebird database operations with real connections

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
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "test_firebird_common.hpp"
#include "test_mocks.hpp"

// Helper function to get the path to the test_db_connections.yml file
std::string getConfigFilePath();

#if USE_FIREBIRD
// Test case for real Firebird connection
TEST_CASE("Real Firebird connection tests", "[firebird_real]")
{
    // Skip these tests if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    // Get Firebird configuration using the centralized function
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");

    // Extract connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Get test queries from the config options
    // Note: Firebird doesn't support IF NOT EXISTS, so we handle table existence differently
    // Use RECREATE TABLE which is Firebird's equivalent of DROP TABLE IF EXISTS + CREATE TABLE
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "RECREATE TABLE test_table (id INTEGER NOT NULL PRIMARY KEY, name VARCHAR(100), value DOUBLE PRECISION)");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_table (id, name, value) VALUES (?, ?, ?)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_table WHERE id = ?");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE test_table");

    SECTION("Basic Firebird operations")
    {
        // Register the Firebird driver (safe registration)
        cpp_dbc::DriverManager::registerDriver("firebird", std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Use RECREATE TABLE - no need to drop first, it handles both cases
        auto result = conn->executeUpdate(createTableQuery);
        REQUIRE(result == 0); // CREATE TABLE should return 0 affected rows

        // Insert data using a prepared statement
        auto pstmt = conn->prepareStatement(insertDataQuery);
        REQUIRE(pstmt != nullptr);

        // Insert multiple rows
        for (int i = 1; i <= 10; i++)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Test Name " + std::to_string(i));
            pstmt->setDouble(3, static_cast<double>(i) * 1.5);
            auto insertResult = pstmt->executeUpdate();
            REQUIRE(insertResult == 1); // Each insert should affect 1 row
        }
        pstmt->close(); // Close prepared statement after use (required for Firebird)

        // Select data using a prepared statement
        auto selectStmt = conn->prepareStatement(selectDataQuery);
        REQUIRE(selectStmt != nullptr);

        // Select a specific row
        selectStmt->setInt(1, 5);
        auto rs = selectStmt->executeQuery();
        REQUIRE(rs != nullptr);

        // Verify the result
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("ID") == 5); // Firebird returns uppercase column names
        REQUIRE(rs->getString("NAME") == "Test Name 5");
        REQUIRE_FALSE(rs->next()); // Should only be one row
        rs->close();               // Close ResultSet before next query (required for Firebird)
        selectStmt->close();       // Close prepared statement after use (required for Firebird)

        // Select all rows using direct query
        rs = conn->executeQuery("SELECT * FROM test_table ORDER BY id");
        REQUIRE(rs != nullptr);

        // Verify all rows
        int count = 0;
        while (rs->next())
        {
            count++;
            REQUIRE(rs->getInt("ID") == count);
            REQUIRE(rs->getString("NAME") == "Test Name " + std::to_string(count));
        }
        REQUIRE(count == 10);
        rs->close(); // Close ResultSet before UPDATE (required for Firebird)

        // Update data
        auto updateResult = conn->executeUpdate("UPDATE test_table SET name = 'Updated Name' WHERE id = 3");
        REQUIRE(updateResult == 1); // Should affect 1 row

        // Verify the update
        rs = conn->executeQuery("SELECT * FROM test_table WHERE id = 3");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("NAME") == "Updated Name");
        rs->close(); // Close ResultSet before DELETE (required for Firebird)

        // Delete data
        auto deleteResult = conn->executeUpdate("DELETE FROM test_table WHERE id > 5");
        REQUIRE(deleteResult == 5); // Should delete 5 rows (6-10)

        // Verify the delete
        rs = conn->executeQuery("SELECT COUNT(*) as cnt FROM test_table");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt(0) == 5); // Should be 5 rows left (use column index for Firebird)
        rs->close();                 // Close ResultSet before dropping table (required for Firebird)

        // Drop the test table
        result = conn->executeUpdate(dropTableQuery);
        REQUIRE(result == 0); // DROP TABLE should return 0 affected rows

        // Close the connection
        conn->close();
    }

    SECTION("Firebird connection pool")
    {
        // Create a connection pool configuration with shorter timeouts for tests
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(2);           // Reduced for tests
        poolConfig.setMaxSize(5);               // Reduced for tests
        poolConfig.setMinIdle(1);               // Reduced for tests
        poolConfig.setConnectionTimeout(10000); // Shorter timeout
        poolConfig.setValidationInterval(500);  // Shorter interval
        poolConfig.setIdleTimeout(5000);        // Shorter idle timeout
        poolConfig.setMaxLifetimeMillis(10000); // Shorter lifetime
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(true);
        poolConfig.setValidationQuery("SELECT 1 FROM RDB$DATABASE");

        // Create a connection pool
        auto poolPtr = std::make_shared<cpp_dbc::Firebird::FirebirdConnectionPool>(poolConfig);

        // Create a test table using RECREATE TABLE
        auto conn = poolPtr->getDBConnection();
        conn->executeUpdate(createTableQuery);

        conn->returnToPool();

        // Test multiple connections in parallel
        const int numThreads = 10;
        const int opsPerThread = 5;

        std::atomic<int> successCount(0);
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([poolPtr, i, opsPerThread, insertDataQuery, &successCount]()
                                          {
                for (int j = 0; j < opsPerThread; j++) {
                    try {
                        // Get a connection from the pool
                        auto conn_thread = poolPtr->getDBConnection();

                        // Insert a row
                        int id = i * 100 + j;
                        auto pstmt = conn_thread->prepareStatement(insertDataQuery);
                        pstmt->setInt(1, id);
                        pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                        pstmt->setDouble(3, static_cast<double>(id) * 0.5);
                        pstmt->executeUpdate();

                        conn_thread->returnToPool();

                        // Increment success counter
                        successCount++;
                    }
                    catch (const std::exception& e) {
                        std::cout << "Thread operation failed: " << e.what() << std::endl;
                    }
                } }));
        }

        // Wait for all threads to complete
        for (std::thread &t : threads)
        {
            t.join();
        }

        // Verify that all operations were successful
        REQUIRE((successCount.load() == numThreads * opsPerThread || successCount.load() == (numThreads * opsPerThread - 1)));

        // Verify the data
        conn = poolPtr->getDBConnection();

        auto rs = conn->executeQuery("SELECT COUNT(*) as cnt FROM test_table");
        REQUIRE(rs->next());
        int count = rs->getInt(0); // Use column index for Firebird
        rs->close();               // Close ResultSet before dropping table (required for Firebird)
        conn->executeUpdate(dropTableQuery);
        REQUIRE(count == numThreads * opsPerThread);

        // Clean up
        conn->returnToPool();
    }

    SECTION("Firebird transaction management")
    {
        // Create a connection pool configuration with shorter timeouts for tests
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(2);           // Reduced for tests
        poolConfig.setMaxSize(3);               // Reduced for tests
        poolConfig.setMinIdle(1);               // Reduced for tests
        poolConfig.setConnectionTimeout(2000);  // Shorter timeout
        poolConfig.setValidationInterval(500);  // Shorter interval
        poolConfig.setIdleTimeout(5000);        // Shorter idle timeout
        poolConfig.setMaxLifetimeMillis(10000); // Shorter lifetime
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1 FROM RDB$DATABASE");

        // Create a connection pool
        auto poolPtr = std::make_shared<cpp_dbc::Firebird::FirebirdConnectionPool>(poolConfig);
        auto &pool = *poolPtr;

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Create a test table using RECREATE TABLE
        auto conn = pool.getDBConnection();
        conn->executeUpdate(createTableQuery);
        conn->returnToPool();

        SECTION("Commit transaction")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();
            REQUIRE(!txId.empty());

            // Get the connection associated with the transaction
            conn = manager.getTransactionDBConnection(txId);
            REQUIRE(conn != nullptr);

            // Insert data within the transaction
            auto pstmt = conn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 1);
            pstmt->setString(2, "Transaction Test");
            pstmt->setDouble(3, 1.5);
            auto result = pstmt->executeUpdate();
            REQUIRE(result == 1);

            // Commit the transaction
            manager.commitTransaction(txId);

            // Verify the data was committed
            conn = pool.getDBConnection();
            auto rs = conn->executeQuery("SELECT * FROM test_table WHERE id = 1");
            REQUIRE(rs->next());
            REQUIRE(rs->getString("NAME") == "Transaction Test");
            rs->close(); // Close ResultSet before closing connection (required for Firebird)
            conn->returnToPool();
        }

        SECTION("Rollback transaction")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();

            // Get the connection associated with the transaction
            conn = manager.getTransactionDBConnection(txId);

            // Insert data within the transaction
            auto pstmt = conn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 2);
            pstmt->setString(2, "Rollback Test");
            pstmt->setDouble(3, 2.5);
            auto result = pstmt->executeUpdate();
            REQUIRE(result == 1);

            // Rollback the transaction
            manager.rollbackTransaction(txId);

            // Verify the data was not committed
            conn = pool.getDBConnection();
            auto rs = conn->executeQuery("SELECT * FROM test_table WHERE id = 2");
            REQUIRE_FALSE(rs->next()); // Should be no rows
            rs->close();               // Close ResultSet before closing connection (required for Firebird)
            conn->returnToPool();
        }

        // Close the pool before dropping the table to avoid blocking
        pool.close();

        // Clean up - use a direct connection to drop the table
        auto directConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        directConn->executeUpdate(dropTableQuery);
        directConn->close();
    }

    SECTION("Firebird metadata retrieval")
    {
        // Register the Firebird driver (safe registration)
        cpp_dbc::DriverManager::registerDriver("firebird", std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

        // Use RECREATE TABLE - Firebird's equivalent of DROP TABLE IF EXISTS + CREATE TABLE
        conn->executeUpdate(
            "RECREATE TABLE test_types ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "int_col INTEGER, "
            "double_col DOUBLE PRECISION, "
            "varchar_col VARCHAR(100), "
            "text_col BLOB SUB_TYPE TEXT, "
            "date_col DATE, "
            "timestamp_col TIMESTAMP, "
            "bool_col SMALLINT"
            ")");

        // Insert test data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_types (id, int_col, double_col, varchar_col, date_col, timestamp_col, bool_col) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setInt(2, 42);
        pstmt->setDouble(3, 3.14159);
        pstmt->setString(4, "Hello, World!");
        pstmt->setString(5, "2023-01-15");          // Date as string
        pstmt->setString(6, "2023-01-15 14:30:00"); // Timestamp as string
        pstmt->setInt(7, 1);                        // Boolean as smallint

        pstmt->executeUpdate();
        pstmt->close(); // Close prepared statement after use (required for Firebird)

        // Test retrieving different data types
        auto rs = conn->executeQuery("SELECT id, int_col, double_col, varchar_col, date_col, timestamp_col, bool_col FROM test_types");
        REQUIRE(rs->next());

        // Test each data type (Firebird returns uppercase column names)
        REQUIRE(rs->getInt("ID") == 1);
        REQUIRE(rs->getInt("INT_COL") == 42);
        REQUIRE((rs->getDouble("DOUBLE_COL") > 3.14 && rs->getDouble("DOUBLE_COL") < 3.15));
        REQUIRE(rs->getString("VARCHAR_COL") == "Hello, World!");

        // Test column metadata
        auto columnNames = rs->getColumnNames();
        REQUIRE(columnNames.size() == 7);
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "ID") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "INT_COL") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "DOUBLE_COL") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "VARCHAR_COL") != columnNames.end());
        rs->close(); // Close ResultSet before UPDATE (required for Firebird)

        // Test NULL values
        conn->executeUpdate("UPDATE test_types SET int_col = NULL, varchar_col = NULL WHERE id = 1");
        rs = conn->executeQuery("SELECT * FROM test_types WHERE id = 1");
        REQUIRE(rs->next());
        REQUIRE(rs->isNull("INT_COL"));
        REQUIRE(rs->isNull("VARCHAR_COL"));
        rs->close(); // Close the ResultSet before dropping the table (required for Firebird)

        // Clean up
        conn->executeUpdate("DROP TABLE test_types");

        // Close the connection
        conn->close();
    }

    SECTION("Firebird stress test")
    {
        // Create a connection pool configuration with shorter timeouts for tests
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(3);           // Reduced for tests
        poolConfig.setMaxSize(10);              // Reduced for tests
        poolConfig.setMinIdle(2);               // Reduced for tests
        poolConfig.setConnectionTimeout(2000);  // Shorter timeout
        poolConfig.setValidationInterval(500);  // Shorter interval
        poolConfig.setIdleTimeout(5000);        // Shorter idle timeout
        poolConfig.setMaxLifetimeMillis(10000); // Shorter lifetime
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1 FROM RDB$DATABASE");

        // Create a connection pool
        auto poolPtr = std::make_shared<cpp_dbc::Firebird::FirebirdConnectionPool>(poolConfig);
        auto &pool = *poolPtr;

        // Create a test table using RECREATE TABLE
        auto conn = pool.getDBConnection();
        conn->executeUpdate(createTableQuery);
        conn->returnToPool();

        // Test with fewer concurrent threads for stability
        const int numThreads = 20;
        const int opsPerThread = 50;

        std::atomic<int> successCount(0);
        std::vector<std::thread> threads;

        auto startTime = std::chrono::steady_clock::now();

        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([&pool, i, opsPerThread, insertDataQuery, selectDataQuery, &successCount]()
                                          {
                for (int j = 0; j < opsPerThread; j++) {
                    try {
                        // Get a connection from the pool
                        auto conn_thread = pool.getDBConnection();

                        // Insert a row
                        auto pstmt = conn_thread->prepareStatement(insertDataQuery);
                        int id = i * 1000 + j;
                        pstmt->setInt(1, id);
                        pstmt->setString(2, "Stress Test " + std::to_string(id));
                        pstmt->setDouble(3, static_cast<double>(id) * 0.1);
                        pstmt->executeUpdate();

                        // Select the row back
                        auto selectStmt = conn_thread->prepareStatement(selectDataQuery);
                        selectStmt->setInt(1, id);
                        auto rs = selectStmt->executeQuery();

                        if (rs->next() && rs->getInt("ID") == id &&
                            rs->getString("NAME") == "Stress Test " + std::to_string(id)) {
                            successCount++;
                        }

                        // Return the connection to the pool
                        conn_thread->returnToPool();
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Thread operation failed: " << e.what() << std::endl;
                    }
                } }));
        }

        // Wait for all threads to complete
        for (auto &t : threads)
        {
            t.join();
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << "Firebird stress test completed in " << duration << " ms" << std::endl;
        std::cout << "Operations per second: " << (numThreads * opsPerThread * 1000.0 / static_cast<double>(duration)) << std::endl;

        // Verify that all operations were successful
        REQUIRE(successCount == numThreads * opsPerThread);

        // Verify the total number of rows
        conn = pool.getDBConnection();
        auto rs = conn->executeQuery("SELECT COUNT(*) as cnt FROM test_table");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt(0) == numThreads * opsPerThread); // Use column index for Firebird
        rs->close();                                         // Close ResultSet before dropping table (required for Firebird)
        conn->returnToPool();

        // Close the pool before dropping the table to avoid blocking
        // This ensures all connections are closed and transactions are committed
        poolPtr->close();

        // Create a new direct connection to drop the table
        auto directConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        directConn->executeUpdate(dropTableQuery);
        directConn->close();
    }
}
#else
// Skip tests if Firebird support is not enabled
TEST_CASE("Real Firebird connection tests (skipped)", "[firebird_real]")
{
    SKIP("Firebird support is not enabled");
}
#endif