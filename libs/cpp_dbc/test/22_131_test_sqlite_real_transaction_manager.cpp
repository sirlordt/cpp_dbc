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

 @file test_sqlite_real_transaction_manager.cpp
 @brief Tests for SQLite transaction management with real database driver

*/

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "22_001_test_sqlite_real_common.hpp"

#if USE_SQLITE

// =============================================================================
// SQLite TransactionContext tests
// =============================================================================

TEST_CASE("SQLite TransactionContext tests", "[22_131_01_sqlite_real_transaction_manager]")
{
    INFO("SQLite TransactionContext tests");

    // Create a real SQLite in-memory connection
    cpp_dbc::SQLite::SQLiteDBDriver driver;
    auto connBase = driver.connect("cpp_dbc:sqlite://:memory:", "", "");
    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(connBase);
    REQUIRE(conn != nullptr);

    // Create a transaction context with the real connection
    cpp_dbc::TransactionContext context(conn, "test-tx-id-sqlite");

    // Check the transaction ID
    REQUIRE(context.transactionId == "test-tx-id-sqlite");

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

    // Verify the connection can execute queries
    auto rs = conn->executeQuery("SELECT 1 as test_value");
    REQUIRE(rs != nullptr);
    REQUIRE(rs->next());
    REQUIRE(rs->getInt("test_value") == 1);

    // Close the connection
    conn->close();
}

// =============================================================================
// SQLite TransactionManager multi-threaded tests
// =============================================================================

TEST_CASE("SQLite TransactionManager multi-threaded tests", "[22_131_02_sqlite_real_transaction_manager]")
{
    INFO("SQLite TransactionManager multi-threaded tests");

    // Register the SQLite driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

    // Use a file-based SQLite database for multi-threaded tests
    std::string dbPath = "test_sqlite_transaction_multithread.db";

    // Remove the database file if it exists
    if (std::filesystem::exists(dbPath))
    {
        std::filesystem::remove(dbPath);
    }

    std::string connStr = "cpp_dbc:sqlite://" + dbPath;

    SECTION("Concurrent transactions with SQLite")
    {
        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername("");
        poolConfig.setPassword("");
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(10);
        poolConfig.setMinIdle(2);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setIdleTimeout(30000);
        poolConfig.setMaxLifetimeMillis(60000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool
        auto poolPtr = cpp_dbc::SQLite::SQLiteConnectionPool::create(poolConfig);
        auto &pool = *poolPtr;

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Number of threads and transactions per thread
        const int numThreads = 3;
        const int txPerThread = 5;

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

                        // Execute a query
                        auto rs = conn->executeQuery("SELECT 1 as test_value");
                        if (rs && rs->next()) {
                            // Verify result
                            rs->getInt("test_value");
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
                        std::cerr << "SQLite thread operation failed: " << e.what() << std::endl;
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

    // Clean up the database file
    if (std::filesystem::exists(dbPath))
    {
        std::filesystem::remove(dbPath);
    }
}

// =============================================================================
// SQLite TransactionManager real database tests
// =============================================================================

TEST_CASE("Real SQLite transaction manager tests", "[22_131_03_sqlite_real_transaction_manager]")
{
    INFO("Real SQLite transaction manager tests");

    // Register the SQLite driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

    // Use a file-based SQLite database for transaction tests
    std::string dbPath = "test_sqlite_transaction.db";

    // Remove the database file if it exists
    if (std::filesystem::exists(dbPath))
    {
        std::filesystem::remove(dbPath);
    }

    std::string connStr = "cpp_dbc:sqlite://" + dbPath;

    SECTION("Basic transaction operations")
    {
        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername("");
        poolConfig.setPassword("");
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
        auto poolPtr = cpp_dbc::SQLite::SQLiteConnectionPool::create(poolConfig);
        auto &pool = *poolPtr;

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Create a test table and enable WAL mode for better concurrency
        auto conn = pool.getRelationalDBConnection();
        conn->executeUpdate("PRAGMA journal_mode=WAL");
        conn->executeUpdate("PRAGMA busy_timeout=5000");
        conn->executeUpdate("DROP TABLE IF EXISTS test_table");
        conn->executeUpdate("CREATE TABLE test_table (id INTEGER PRIMARY KEY, name TEXT)");
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
            auto pstmt = txConn->prepareStatement("INSERT INTO test_table (id, name) VALUES (?, ?)");
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
            REQUIRE(rs->getString("name") == "Transaction Test");
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
            auto pstmt = txConn->prepareStatement("INSERT INTO test_table (id, name) VALUES (?, ?)");
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

        SECTION("Multiple sequential transactions")
        {
            // Note: SQLite only allows one writer at a time, so we test
            // multiple transactions sequentially rather than concurrently.
            // This still validates the transaction manager's ability to
            // handle multiple transaction IDs and commit/rollback operations.

            // Transaction 1: Insert and commit
            std::string txId1 = manager.beginTransaction();
            REQUIRE(!txId1.empty());

            auto txConn1 = manager.getTransactionDBConnection(txId1);
            REQUIRE(txConn1 != nullptr);

            auto pstmt1 = txConn1->prepareStatement("INSERT INTO test_table (id, name) VALUES (?, ?)");
            pstmt1->setInt(1, 10);
            pstmt1->setString(2, "Transaction 1");
            pstmt1->executeUpdate();

            manager.commitTransaction(txId1);
            REQUIRE_FALSE(manager.isTransactionActive(txId1));

            // Transaction 2: Insert and rollback
            std::string txId2 = manager.beginTransaction();
            REQUIRE(!txId2.empty());
            REQUIRE(txId1 != txId2);

            auto txConn2 = manager.getTransactionDBConnection(txId2);
            REQUIRE(txConn2 != nullptr);

            auto pstmt2 = txConn2->prepareStatement("INSERT INTO test_table (id, name) VALUES (?, ?)");
            pstmt2->setInt(1, 20);
            pstmt2->setString(2, "Transaction 2");
            pstmt2->executeUpdate();

            manager.rollbackTransaction(txId2);
            REQUIRE_FALSE(manager.isTransactionActive(txId2));

            // Transaction 3: Insert and commit
            std::string txId3 = manager.beginTransaction();
            REQUIRE(!txId3.empty());
            REQUIRE(txId2 != txId3);
            REQUIRE(txId1 != txId3);

            auto txConn3 = manager.getTransactionDBConnection(txId3);
            REQUIRE(txConn3 != nullptr);

            auto pstmt3 = txConn3->prepareStatement("INSERT INTO test_table (id, name) VALUES (?, ?)");
            pstmt3->setInt(1, 30);
            pstmt3->setString(2, "Transaction 3");
            pstmt3->executeUpdate();

            manager.commitTransaction(txId3);
            REQUIRE_FALSE(manager.isTransactionActive(txId3));

            // Verify the data from committed transactions
            auto verifyConn = pool.getRelationalDBConnection();

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
            auto txConn = manager.getTransactionDBConnection(txId);

            // Insert data within the transaction
            auto pstmt = txConn->prepareStatement("INSERT INTO test_table (id, name) VALUES (?, ?)");
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
            REQUIRE(rs->getString("name") == "Isolation Test");

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
            auto pstmt = txConn->prepareStatement("INSERT INTO test_table (id, name) VALUES (?, ?)");
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

        // Close the pool
        pool.close();
    }

    // Clean up the database file
    if (std::filesystem::exists(dbPath))
    {
        std::filesystem::remove(dbPath);
    }
}

#endif
