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

 @file test_transaction_manager.cpp
 @brief Tests for transaction management

*/

#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include "test_mocks.hpp"
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>

// Use the mock classes from test_mocks.hpp
using namespace cpp_dbc_test;

// Test case for TransactionManager basic operations
TEST_CASE("TransactionManager basic tests", "[transaction_manager]")
{
    // Register the mock driver directly
    auto mockDriver = std::make_shared<MockDriver>();
    cpp_dbc::DriverManager::registerDriver("mock", mockDriver);

    // Create a mock connection pool
    MockConnectionPool pool;

    SECTION("Create TransactionManager")
    {
        INFO("Create TransactionManager");
        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Check initial state
        REQUIRE(manager.getActiveTransactionCount() == 0);
    }

    SECTION("Begin and commit transaction")
    {
        INFO("Begin and commit transaction");

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Begin a transaction
        std::string txId = manager.beginTransaction();
        REQUIRE(!txId.empty());

        // Check that the transaction is active
        REQUIRE(manager.isTransactionActive(txId));
        REQUIRE(manager.getActiveTransactionCount() == 1);

        // Get the connection associated with the transaction
        auto conn = manager.getTransactionConnection(txId);
        REQUIRE(conn != nullptr);

        // Check that autoCommit is disabled on the connection
        REQUIRE(conn->getAutoCommit() == false);

        // Execute a query using the connection
        auto rs = conn->executeQuery("SELECT 1");
        REQUIRE(rs != nullptr);

        // Commit the transaction
        manager.commitTransaction(txId);

        // Check that the transaction is no longer active
        REQUIRE_FALSE(manager.isTransactionActive(txId));
        REQUIRE(manager.getActiveTransactionCount() == 0);
    }

    SECTION("Begin and rollback transaction")
    {
        INFO("Begin and rollback transaction");
        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Begin a transaction
        std::string txId = manager.beginTransaction();
        REQUIRE(!txId.empty());

        // Get the connection associated with the transaction
        auto conn = manager.getTransactionConnection(txId);
        REQUIRE(conn != nullptr);

        // Execute a query using the connection
        auto rs = conn->executeQuery("SELECT 1");
        REQUIRE(rs != nullptr);

        // Rollback the transaction
        manager.rollbackTransaction(txId);

        // Check that the transaction is no longer active
        REQUIRE_FALSE(manager.isTransactionActive(txId));
        REQUIRE(manager.getActiveTransactionCount() == 0);
    }

    SECTION("Multiple transactions")
    {
        INFO("Multiple transactions");
        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Begin multiple transactions
        std::string txId1 = manager.beginTransaction();
        std::string txId2 = manager.beginTransaction();
        std::string txId3 = manager.beginTransaction();

        REQUIRE(manager.getActiveTransactionCount() == 3);

        // Get connections for each transaction
        auto conn1 = manager.getTransactionConnection(txId1);
        auto conn2 = manager.getTransactionConnection(txId2);
        auto conn3 = manager.getTransactionConnection(txId3);

        REQUIRE(conn1 != nullptr);
        REQUIRE(conn2 != nullptr);
        REQUIRE(conn3 != nullptr);

        // Commit one transaction
        manager.commitTransaction(txId1);
        REQUIRE(manager.getActiveTransactionCount() == 2);
        REQUIRE_FALSE(manager.isTransactionActive(txId1));
        REQUIRE(manager.isTransactionActive(txId2));
        REQUIRE(manager.isTransactionActive(txId3));

        // Rollback one transaction
        manager.rollbackTransaction(txId2);
        REQUIRE(manager.getActiveTransactionCount() == 1);
        REQUIRE_FALSE(manager.isTransactionActive(txId1));
        REQUIRE_FALSE(manager.isTransactionActive(txId2));
        REQUIRE(manager.isTransactionActive(txId3));

        // Commit the last transaction
        manager.commitTransaction(txId3);
        REQUIRE(manager.getActiveTransactionCount() == 0);
        REQUIRE_FALSE(manager.isTransactionActive(txId1));
        REQUIRE_FALSE(manager.isTransactionActive(txId2));
        REQUIRE_FALSE(manager.isTransactionActive(txId3));
    }
}

// Test case for TransactionManager multi-threaded operations
TEST_CASE("TransactionManager multi-threaded tests", "[transaction_manager]")
{
    // Register the mock driver directly
    auto mockDriver = std::make_shared<MockDriver>();
    cpp_dbc::DriverManager::registerDriver("mock", mockDriver);

    // Create a mock connection pool
    MockConnectionPool pool;

    SECTION("Concurrent transactions")
    {
        INFO("Concurrent transactions");
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
                        auto conn = manager.getTransactionConnection(txId);

                        // Execute a query
                        auto rs = conn->executeQuery("SELECT 1");

                        // Commit or rollback randomly
                        if (j % 2 == 0) {
                            manager.commitTransaction(txId);
                        } else {
                            manager.rollbackTransaction(txId);
                        }

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

        // Check that all transactions were processed
        REQUIRE(successCount == numThreads * txPerThread);
        REQUIRE(manager.getActiveTransactionCount() == 0);
    }
}

// Test case for TransactionContext
TEST_CASE("TransactionContext tests", "[transaction_manager]")
{
    INFO("TransactionContext tests");
    // Create a connection for testing
    auto conn = std::make_shared<MockConnection>();

    // Create a transaction context
    cpp_dbc::TransactionContext context(conn, "test-tx-id");

    // Check the transaction ID
    REQUIRE(context.transactionId == "test-tx-id");

    // Check the connection
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
}
