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

 @file 23_111_test_firebird_real_thread_safe.cpp
 @brief Thread-safety stress tests for Firebird database driver

*/

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <mutex>
#include <condition_variable>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/pool/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "23_001_test_firebird_real_common.hpp"

#if DB_DRIVER_THREAD_SAFE && USE_FIREBIRD

/**
 * @brief Thread-safety stress tests for Firebird driver
 *
 * These tests verify that the Firebird driver is thread-safe by:
 * 1. Multiple threads each with their own connection
 * 2. Connection pool under high concurrency
 * 3. Rapid connection open/close from multiple threads
 * 4. Stress testing with high concurrency using connection pool
 *
 * Note: Each thread should have its own connection for proper thread-safety.
 * Sharing a single connection across threads is not recommended even with
 * thread-safe drivers, as it can lead to logical issues with transactions
 * and statement state.
 */
TEST_CASE("Firebird Thread-Safety Tests", "[23_111_01_firebird_real_thread_safe]")
{
    // Skip these tests if we can't connect to Firebird
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

    // Register the Firebird driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

    SECTION("Multiple threads with individual connections")
    {
        // Setup: create test table using a single connection
        auto setupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(setupConn != nullptr);
        // Use RECREATE TABLE - Firebird's equivalent of DROP TABLE IF EXISTS + CREATE TABLE
        setupConn->executeUpdate("RECREATE TABLE thread_test (id INTEGER NOT NULL PRIMARY KEY, val_data VARCHAR(100))");
        setupConn->close();

        const int numThreads = 10;
        const int opsPerThread = 20;
        std::atomic<int> successCount(0);
        std::atomic<int> errorCount(0);
        std::vector<std::thread> threads;

        // Barrier to synchronize thread start
        std::mutex startMutex;
        std::condition_variable startCv;
        bool startFlag = false;

        for (int i = 0; i < numThreads; i++)
        {
            threads.emplace_back([&, i]()
                                 {
                // Wait for all threads to be ready
                {
                    std::unique_lock<std::mutex> lock(startMutex);
                    startCv.wait(lock, [&]() { return startFlag; });
                }

                try
                {
                    // Each thread gets its own connection
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                    
                    for (int j = 0; j < opsPerThread; j++)
                    {
                        try
                        {
                            int id = i * 1000 + j;
                            
                            // Insert operation
                            auto pstmt = conn->prepareStatement("INSERT INTO thread_test (id, val_data) VALUES (?, ?)");
                            pstmt->setInt(1, id);
                            pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                            pstmt->executeUpdate();

                            // Select operation
                            auto selectStmt = conn->prepareStatement("SELECT * FROM thread_test WHERE id = ?");
                            selectStmt->setInt(1, id);
                            auto rs = selectStmt->executeQuery();
                            
                            if (rs->next())
                            {
                                successCount++;
                            }
                        }
                        catch (const std::exception& e)
                        {
                            errorCount++;
                            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " error: " + std::string(e.what()));
                        }
                    }
                    
                    conn->close();
                }
                catch (const std::exception& e)
                {
                    errorCount += opsPerThread;
                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " connection error: " + std::string(e.what()));
                } });
        }

        // Start all threads simultaneously
        {
            std::lock_guard<std::mutex> lock(startMutex);
            startFlag = true;
        }
        startCv.notify_all();

        // Wait for all threads
        for (auto &t : threads)
        {
            t.join();
        }

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Multiple threads with individual connections: " + std::to_string(successCount.load()) + " successes, " + std::to_string(errorCount.load()) + " errors");

        // Clean up
        auto cleanupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        try
        {
            cleanupConn->executeUpdate("DROP TABLE thread_test");
        }
        catch (...)
        {
        }
        cleanupConn->close();

        // We expect at least 95% of operations to succeed
        REQUIRE(successCount.load() >= (numThreads * opsPerThread * 0.95));
    }

    // Pool-based concurrent access, concurrent reads, and high-concurrency stress tests
    // were moved to 23_141_test_firebird_real_connection_pool.cpp

    SECTION("Rapid connection open/close stress test")
    {
        const int numThreads = 10;
        const int connectionsPerThread = 10;
        std::atomic<int> successCount(0);
        std::atomic<int> errorCount(0);
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++)
        {
            threads.emplace_back([&]()
                                 {
                for (int j = 0; j < connectionsPerThread; j++)
                {
                    try
                    {
                        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                        
                        // Do a simple operation
                        auto rs = conn->executeQuery("SELECT 1 as test FROM RDB$DATABASE");
                        if (rs->next())
                        {
                            rs->getInt(0); // Use column index for Firebird
                        }
                        
                        conn->close();
                        successCount++;
                    }
                    catch (const std::exception& e)
                    {
                        errorCount++;
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Connection error: " + std::string(e.what()));
                    }
                } });
        }

        for (auto &t : threads)
        {
            t.join();
        }

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Rapid connection test: " + std::to_string(successCount.load()) + " successes, " + std::to_string(errorCount.load()) + " errors");

        REQUIRE(successCount > numThreads * connectionsPerThread * 0.95); // At least 95% success rate
    }
}

#else
TEST_CASE("Firebird Thread-Safety Tests (skipped)", "[23_111_02_firebird_real_thread_safe]")
{
    SKIP("Firebird support is not enabled or thread-safety is disabled");
}
#endif // DB_DRIVER_THREAD_SAFE && USE_FIREBIRD