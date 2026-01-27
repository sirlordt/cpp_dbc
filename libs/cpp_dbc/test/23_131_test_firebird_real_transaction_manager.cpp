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

 @file test_firebird_real_transaction_manager.cpp
 @brief Tests for Firebird transaction management with real database driver

*/

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "23_001_test_firebird_real_common.hpp"

#if USE_FIREBIRD

// =============================================================================
// Firebird TransactionContext tests
// =============================================================================

TEST_CASE("Firebird TransactionContext tests", "[23_131_01_firebird_real_transaction_manager]")
{
    // Skip if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    INFO("Firebird TransactionContext tests");

    // Get Firebird configuration
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Create a real Firebird connection
    cpp_dbc::Firebird::FirebirdDBDriver driver;
    auto connBase = driver.connect(connStr, username, password);
    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(connBase);
    REQUIRE(conn != nullptr);

    // Create a transaction context with the real connection
    cpp_dbc::TransactionContext context(conn, "test-tx-id-firebird");

    // Check the transaction ID
    REQUIRE(context.transactionId == "test-tx-id-firebird");

    // Check the connection is valid
    REQUIRE(context.connection != nullptr);
    REQUIRE(context.connection == conn);

    // Check that the last access time is recent
    auto now = std::chrono::steady_clock::now();
    auto lastAccess = context.lastAccessTime;
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - lastAccess).count();
    REQUIRE(diff < 5); // Should be less than 5 seconds

    // Update the last access time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    context.lastAccessTime = std::chrono::steady_clock::now();

    // Check that the last access time was updated
    auto newLastAccess = context.lastAccessTime;
    REQUIRE(newLastAccess > lastAccess);

    // Verify the connection can execute queries (Firebird requires FROM RDB$DATABASE)
    auto rs = conn->executeQuery("SELECT 1 as TEST_VALUE FROM RDB$DATABASE");
    REQUIRE(rs != nullptr);
    REQUIRE(rs->next());
    REQUIRE(rs->getInt("TEST_VALUE") == 1); // Firebird returns uppercase column names

    // Close the connection
    conn->close();
}

// =============================================================================
// Firebird TransactionManager multi-threaded tests
// =============================================================================

TEST_CASE("Firebird TransactionManager multi-threaded tests", "[23_131_02_firebird_real_transaction_manager]")
{
    // Skip if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    // Get Firebird configuration
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    SECTION("Concurrent transactions with Firebird")
    {
        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(10);
        poolConfig.setMaxSize(20);
        poolConfig.setMinIdle(5);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setIdleTimeout(30000);
        poolConfig.setMaxLifetimeMillis(60000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1 FROM RDB$DATABASE");

        // Create a connection pool
        auto poolPtr = cpp_dbc::Firebird::FirebirdConnectionPool::create(poolConfig);
        auto &pool = *poolPtr;

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Number of threads and transactions per thread
        const int numThreads = 5;
        const int txPerThread = 10;

        // Atomic counter for successful transactions
        std::atomic<int> successCount(0);

        // Create and start threads
        std::vector<std::thread> threads;
        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([&manager, txPerThread, &successCount]()
                                          {
                for (int j = 0; j < txPerThread; j++) {
                    try {
                        // Begin a transaction
                        std::string txId = manager.beginTransaction();

                        // Get the connection
                        auto conn = manager.getTransactionDBConnection(txId);

                        // Execute a query (Firebird requires FROM RDB$DATABASE)
                        auto rs = conn->executeQuery("SELECT 1 as TEST_VALUE FROM RDB$DATABASE");
                        if (rs && rs->next()) {
                            // Verify result (Firebird returns uppercase column names)
                            rs->getInt("TEST_VALUE");
                        }

                        // Commit or rollback based on iteration
                        if (j % 2 == 0) {
                            manager.commitTransaction(txId);
                        } else {
                            manager.rollbackTransaction(txId);
                        }

                        // Increment success counter
                        successCount++;
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Firebird thread operation failed: " << e.what() << std::endl;
                    }
                } }));
        }

        // Wait for all threads to complete
        for (auto &t : threads)
        {
            t.join();
        }

        // Check that all transactions were processed
        REQUIRE(successCount == numThreads * txPerThread);
        REQUIRE(manager.getActiveTransactionCount() == 0);

        // Close the pool
        pool.close();
    }
}

// =============================================================================
// Firebird TransactionManager real database tests
// =============================================================================

TEST_CASE("Real Firebird transaction manager tests", "[23_131_03_firebird_real_transaction_manager]")
{
    // Skip these tests if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    // Get Firebird configuration
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");

    // Get connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Get test queries
    std::string createTableQuery = dbConfig.getOption("query__create_table");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data");
    std::string selectDataQuery = dbConfig.getOption("query__select_data");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table");

    SECTION("Basic transaction operations")
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

        // Create a connection pool using factory method
        auto poolPtr = cpp_dbc::Firebird::FirebirdConnectionPool::create(poolConfig);
        auto &pool = *poolPtr;

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Create a test table
        auto conn = pool.getRelationalDBConnection();
        try
        {
            conn->executeUpdate(dropTableQuery); // Drop table if it exists
        }
        catch (...)
        {
            // Ignore errors if table doesn't exist
        }
        conn->executeUpdate(createTableQuery);
        conn->close();

        SECTION("Commit transaction")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();
            REQUIRE(!txId.empty());

            // Get the connection associated with the transaction
            auto txConn = manager.getTransactionDBConnection(txId);
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
            auto verifyConn = pool.getRelationalDBConnection();
            auto rs = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 1");
            REQUIRE(rs->next());
            REQUIRE(rs->getString("NAME") == "Transaction Test"); // Firebird returns uppercase column names
            verifyConn->close();
        }

        SECTION("Rollback transaction")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();
            REQUIRE(!txId.empty());

            // Get the connection associated with the transaction
            auto txConn = manager.getTransactionDBConnection(txId);
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
            auto verifyConn = pool.getRelationalDBConnection();
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
            auto txConn1 = manager.getTransactionDBConnection(txId1);
            auto txConn2 = manager.getTransactionDBConnection(txId2);
            auto txConn3 = manager.getTransactionDBConnection(txId3);

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
            auto verifyConn = pool.getRelationalDBConnection();

            // Transaction 1 (committed)
            auto rs1 = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 10");
            REQUIRE(rs1->next());
            REQUIRE(rs1->getString("NAME") == "Transaction 1"); // Firebird returns uppercase column names

            // Transaction 2 (rolled back)
            auto rs2 = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 20");
            REQUIRE_FALSE(rs2->next()); // Should be no rows

            // Transaction 3 (committed)
            auto rs3 = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 30");
            REQUIRE(rs3->next());
            REQUIRE(rs3->getString("NAME") == "Transaction 3"); // Firebird returns uppercase column names

            verifyConn->close();
        }

        SECTION("Transaction isolation")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();
            auto txConn = manager.getTransactionDBConnection(txId);

            // Insert data within the transaction
            auto pstmt = txConn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 100);
            pstmt->setString(2, "Isolation Test");
            pstmt->executeUpdate();

            // Get a separate connection (not in the transaction)
            auto regularConn = pool.getRelationalDBConnection();

            // Verify the data is not visible outside the transaction
            auto rs = regularConn->executeQuery("SELECT * FROM test_table WHERE id = 100");
            REQUIRE_FALSE(rs->next()); // Should be no rows

            // Commit the transaction
            manager.commitTransaction(txId);

            // Now the data should be visible
            rs = regularConn->executeQuery("SELECT * FROM test_table WHERE id = 100");
            REQUIRE(rs->next());
            REQUIRE(rs->getString("NAME") == "Isolation Test"); // Firebird returns uppercase column names

            regularConn->close();
        }

        SECTION("Transaction timeout")
        {
            // Set a very short transaction timeout
            manager.setTransactionTimeout(1); // 1 second

            // Begin a transaction
            std::string txId = manager.beginTransaction();
            auto txConn = manager.getTransactionDBConnection(txId);

            // Insert data within the transaction
            auto pstmt = txConn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 200);
            pstmt->setString(2, "Timeout Test");
            pstmt->executeUpdate();

            // Poll for transaction timeout instead of using a fixed sleep
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (manager.isTransactionActive(txId) && std::chrono::steady_clock::now() < deadline)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // The transaction should no longer be active
            REQUIRE_FALSE(manager.isTransactionActive(txId));

            // Verify the data was not committed
            auto verifyConn = pool.getRelationalDBConnection();
            auto rs = verifyConn->executeQuery("SELECT * FROM test_table WHERE id = 200");
            REQUIRE_FALSE(rs->next()); // Should be no rows
            verifyConn->close();

            // Reset the transaction timeout to a reasonable value
            manager.setTransactionTimeout(30); // 30 seconds
        }

        // Close the pool first to release all connections and their transactions
        // This is necessary because Firebird DDL operations require exclusive access
        pool.close();

        // Clean up using a direct connection (not from pool)
        cpp_dbc::Firebird::FirebirdDBDriver driver;
        auto cleanupConnBase = driver.connect(connStr, username, password);
        auto cleanupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cleanupConnBase);
        if (cleanupConn)
        {
            try
            {
                cleanupConn->executeUpdate(dropTableQuery);
            }
            catch (...)
            {
                // Ignore errors if table doesn't exist
            }
            cleanupConn->close();
        }
    }
}

#endif
