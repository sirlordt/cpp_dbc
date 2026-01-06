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

 @file test_postgresql_real.cpp
 @brief Tests for PostgreSQL database operations

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
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "test_postgresql_common.hpp"

// Helper function to get the path to the test_db_connections.yml file
std::string getConfigFilePath();

#if USE_POSTGRESQL
// Test case for real PostgreSQL connection
TEST_CASE("Real PostgreSQL connection tests", "[postgresql_real]")
{
    // Skip these tests if we can't connect to PostgreSQL
    if (!postgresql_test_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

    // Get PostgreSQL configuration
    auto dbConfig = postgresql_test_helpers::getPostgreSQLConfig("dev_postgresql");

    // Get connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Get test queries
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "CREATE TABLE test_table (id INT PRIMARY KEY, name VARCHAR(100))");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_table (id, name) VALUES ($1, $2)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_table WHERE id = $1");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE IF EXISTS test_table");

    SECTION("Basic PostgreSQL operations")
    {
        // Register the PostgreSQL driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
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
            auto insertResult = pstmt->executeUpdate();
            REQUIRE(insertResult == 1); // Each insert should affect 1 row
        }

        // Select data using a prepared statement
        auto selectStmt = conn->prepareStatement(selectDataQuery);
        REQUIRE(selectStmt != nullptr);

        // Select a specific row
        selectStmt->setInt(1, 5);
        auto rs = selectStmt->executeQuery();
        REQUIRE(rs != nullptr);

        // Verify the result
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 5);
        REQUIRE(rs->getString("name") == "Test Name 5");
        REQUIRE_FALSE(rs->next()); // Should only be one row

        // Select all rows using direct query
        rs = conn->executeQuery("SELECT * FROM test_table ORDER BY id");
        REQUIRE(rs != nullptr);

        // Verify all rows
        int count = 0;
        while (rs->next())
        {
            count++;
            REQUIRE(rs->getInt("id") == count);
            REQUIRE(rs->getString("name") == "Test Name " + std::to_string(count));
        }
        REQUIRE(count == 10);

        // Update data
        auto updateResult = conn->executeUpdate("UPDATE test_table SET name = 'Updated Name' WHERE id = 3");
        REQUIRE(updateResult == 1); // Should affect 1 row

        // Verify the update
        rs = conn->executeQuery("SELECT * FROM test_table WHERE id = 3");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("name") == "Updated Name");

        // Delete data
        auto deleteResult = conn->executeUpdate("DELETE FROM test_table WHERE id > 5");
        REQUIRE(deleteResult == 5); // Should delete 5 rows (6-10)

        // Verify the delete
        rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_table");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("count") == 5); // Should be 5 rows left

        // Drop the test table
        result = conn->executeUpdate(dropTableQuery);
        REQUIRE(result == 0); // DROP TABLE should return 0 affected rows

        // Close the connection
        conn->close();
    }

    SECTION("PostgreSQL connection pool")
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

        // Create a connection pool using factory method
        auto poolPtr = cpp_dbc::PostgreSQL::PostgreSQLConnectionPool::create(poolConfig);
        auto &pool = *poolPtr;

        // Create a test table
        auto conn = pool.getRelationalDBConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test multiple connections in parallel
        const int numThreads = 5;
        const int opsPerThread = 10;

        std::atomic<int> successCount(0);
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([&pool, i, opsPerThread, insertDataQuery, &successCount]()
                                          {
                for (int j = 0; j < opsPerThread; j++) {
                    try {
                        // Get a connection from the pool
                        auto conn_thread = pool.getRelationalDBConnection();

                        // Insert a row
                        auto pstmt = conn_thread->prepareStatement(insertDataQuery);
                        int id = i * 100 + j;
                        pstmt->setInt(1, id);
                        pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                        pstmt->executeUpdate();

                        // Return the connection to the pool
                        conn_thread->close();

                        // Increment success counter
                        successCount++;
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

        // Verify that all operations were successful
        REQUIRE(successCount == numThreads * opsPerThread);

        // Verify the data
        conn = pool.getRelationalDBConnection();
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_table");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("count") == numThreads * opsPerThread);

        // Clean up
        conn->executeUpdate(dropTableQuery);
        conn->close();

        // Close the pool
        pool.close();
    }

    SECTION("PostgreSQL transaction management")
    {
        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(5);
        poolConfig.setMinIdle(2);
        poolConfig.setConnectionTimeout(5000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setIdleTimeout(30000);
        poolConfig.setMaxLifetimeMillis(60000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool using factory method
        auto poolPtr = cpp_dbc::PostgreSQL::PostgreSQLConnectionPool::create(poolConfig);
        auto &pool = *poolPtr;

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Create a test table
        auto conn = pool.getRelationalDBConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

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
            auto result = pstmt->executeUpdate();
            REQUIRE(result == 1);

            // Commit the transaction
            manager.commitTransaction(txId);

            // Verify the data was committed
            conn = pool.getRelationalDBConnection();
            auto rs = conn->executeQuery("SELECT * FROM test_table WHERE id = 1");
            REQUIRE(rs->next());
            REQUIRE(rs->getString("name") == "Transaction Test");
            conn->close();
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
            auto result = pstmt->executeUpdate();
            REQUIRE(result == 1);

            // Rollback the transaction
            manager.rollbackTransaction(txId);

            // Verify the data was not committed
            conn = pool.getRelationalDBConnection();
            auto rs = conn->executeQuery("SELECT * FROM test_table WHERE id = 2");
            REQUIRE_FALSE(rs->next()); // Should be no rows
            conn->close();
        }

        // Clean up
        conn = pool.getRelationalDBConnection();
        conn->executeUpdate(dropTableQuery);
        conn->close();

        // Close the pool
        pool.close();
    }

    SECTION("PostgreSQL metadata retrieval")
    {
        // Register the PostgreSQL driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

        // Create a test table with various data types
        conn->executeUpdate("DROP TABLE IF EXISTS test_types");
        conn->executeUpdate(
            "CREATE TABLE test_types ("
            "id INT PRIMARY KEY, "
            "int_col INT, "
            "double_col DOUBLE PRECISION, "
            "varchar_col VARCHAR(100), "
            "text_col TEXT, "
            "date_col DATE, "
            "timestamp_col TIMESTAMP, "
            "bool_col BOOLEAN"
            ")");

        // Insert test data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_types VALUES ($1, $2, $3, $4, $5, $6, $7, $8)");

        pstmt->setInt(1, 1);
        pstmt->setInt(2, 42);
        pstmt->setDouble(3, 3.14159);
        pstmt->setString(4, "Hello, World!");
        pstmt->setString(5, "This is a longer text field with more content.");
        pstmt->setDate(6, "2023-01-15");
        pstmt->setTimestamp(7, "2023-01-15 14:30:00");
        pstmt->setBoolean(8, true);

        pstmt->executeUpdate();

        // Test retrieving different data types
        auto rs = conn->executeQuery("SELECT * FROM test_types");
        REQUIRE(rs->next());

        // Test each data type
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getInt("int_col") == 42);
        REQUIRE((rs->getDouble("double_col") > 3.14 && rs->getDouble("double_col") < 3.15));
        REQUIRE(rs->getString("varchar_col") == "Hello, World!");
        REQUIRE(rs->getString("text_col") == "This is a longer text field with more content.");
        REQUIRE(rs->getString("date_col") == "2023-01-15");
        REQUIRE(rs->getString("timestamp_col").find("2023-01-15") != std::string::npos);
        REQUIRE(rs->getBoolean("bool_col") == true);

        // Test column metadata
        auto columnNames = rs->getColumnNames();
        REQUIRE(columnNames.size() == 8);
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "id") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "int_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "double_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "varchar_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "text_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "date_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "timestamp_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "bool_col") != columnNames.end());

        // Test NULL values
        conn->executeUpdate("UPDATE test_types SET int_col = NULL, varchar_col = NULL WHERE id = 1");
        rs = conn->executeQuery("SELECT * FROM test_types");
        REQUIRE(rs->next());
        REQUIRE(rs->isNull("int_col"));
        REQUIRE(rs->isNull("varchar_col"));

        // Clean up
        conn->executeUpdate("DROP TABLE test_types");

        // Close the connection
        conn->close();
    }

    SECTION("PostgreSQL stress test")
    {
        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(20);
        poolConfig.setMinIdle(3);
        poolConfig.setConnectionTimeout(5000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setIdleTimeout(30000);
        poolConfig.setMaxLifetimeMillis(60000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool using factory method
        auto poolPtr = cpp_dbc::PostgreSQL::PostgreSQLConnectionPool::create(poolConfig);
        auto &pool = *poolPtr;

        // Create a test table
        auto conn = pool.getRelationalDBConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test with many concurrent threads
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
                        auto conn_thread = pool.getRelationalDBConnection();

                        // Insert a row
                        auto pstmt = conn_thread->prepareStatement(insertDataQuery);
                        int id = i * 1000 + j;
                        pstmt->setInt(1, id);
                        pstmt->setString(2, "Stress Test " + std::to_string(id));
                        pstmt->executeUpdate();

                        // Select the row back
                        auto selectStmt = conn_thread->prepareStatement(selectDataQuery);
                        selectStmt->setInt(1, id);
                        auto rs = selectStmt->executeQuery();

                        if (rs->next() && rs->getInt("id") == id &&
                            rs->getString("name") == "Stress Test " + std::to_string(id)) {
                            successCount++;
                        }

                        // Return the connection to the pool
                        conn_thread->close();
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

        std::cout << "PostgreSQL stress test completed in " << duration << " ms" << std::endl;
        std::cout << "Operations per second: " << (numThreads * opsPerThread * 1000.0 / static_cast<double>(duration)) << std::endl;

        // Verify that all operations were successful
        REQUIRE(successCount == numThreads * opsPerThread);

        // Verify the total number of rows
        conn = pool.getRelationalDBConnection();
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_table");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("count") == numThreads * opsPerThread);

        // Clean up
        conn->executeUpdate(dropTableQuery);
        conn->close();

        // Close the pool
        pool.close();
    }

    SECTION("PostgreSQL specific features")
    {
        // Register the PostgreSQL driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

        // Test JSON data type
        conn->executeUpdate("DROP TABLE IF EXISTS test_json");
        conn->executeUpdate("CREATE TABLE test_json (id INT PRIMARY KEY, data JSONB)");

        // Insert JSON data
        conn->executeUpdate("INSERT INTO test_json VALUES (1, '{\"name\": \"John\", \"age\": 30, \"city\": \"New York\"}')");

        // Query JSON data
        auto rs = conn->executeQuery("SELECT data->>'name' as name, (data->>'age')::int as age FROM test_json WHERE id = 1");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("name") == "John");
        REQUIRE(rs->getInt("age") == 30);

        // Test array data type
        conn->executeUpdate("DROP TABLE IF EXISTS test_array");
        conn->executeUpdate("CREATE TABLE test_array (id INT PRIMARY KEY, int_array INT[], text_array TEXT[])");

        // Insert array data
        conn->executeUpdate("INSERT INTO test_array VALUES (1, '{1,2,3}', '{\"one\",\"two\",\"three\"}')");

        // Query array data
        rs = conn->executeQuery("SELECT int_array[1] as first_int, text_array[2] as second_text FROM test_array WHERE id = 1");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("first_int") == 1);
        REQUIRE(rs->getString("second_text") == "two");

        // Clean up
        conn->executeUpdate("DROP TABLE test_json");
        conn->executeUpdate("DROP TABLE test_array");

        // Close the connection
        conn->close();
    }
}
#else
// Skip tests if PostgreSQL support is not enabled
TEST_CASE("Real PostgreSQL connection tests (skipped)", "[postgresql_real]")
{
    SKIP("PostgreSQL support is not enabled");
}
#endif