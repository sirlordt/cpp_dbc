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

 @file 22_111_test_sqlite_real_thread_safe.cpp
 @brief Thread-safety stress tests for SQLite database driver

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
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/pool/relational/relational_db_connection_pool.hpp>

#include "22_001_test_sqlite_real_common.hpp"

#if DB_DRIVER_THREAD_SAFE && USE_SQLITE

/**
 * @brief Thread-safety stress tests for SQLite driver
 *
 * These tests verify that the SQLite driver is thread-safe by:
 * 1. Multiple threads each with their own connection to a file-based database
 * 2. Connection pool under high concurrency
 * 3. Rapid connection open/close from multiple threads
 * 4. Stress testing with high concurrency using connection pool
 *
 * Note: SQLite has specific threading modes (single-thread, multi-thread, serialized)
 * These tests assume SQLite is compiled with SQLITE_THREADSAFE=1 (serialized mode)
 *
 * Each thread should have its own connection for proper thread-safety.
 * Sharing a single connection across threads is not recommended even with
 * thread-safe drivers, as it can lead to logical issues with transactions
 * and statement state.
 */
TEST_CASE("SQLite Thread-Safety Tests", "[22_111_01_sqlite_real_thread_safe]")
{
    // Get SQLite configuration using the helper function
    auto dbConfig = sqlite_test_helpers::getSQLiteConfig("test_sqlite");

    // CRITICAL: Cleanup SQLite database files before test to prevent "readonly database" errors
    // under Helgrind/Valgrind when running multiple iterations (WAL mode issue)
    // Only cleanup if using file-based database (not :memory:)
    if (dbConfig.getDatabase() != ":memory:")
    {
        // sqlite_test_helpers::cleanupSQLiteTestFiles(dbConfig.getDatabase());
    }

    std::string connStr = dbConfig.createConnectionString();

    // Register the SQLite driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

    SECTION("Multiple threads with individual connections")
    {
        // Setup: create test table using a single connection
        auto setupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));
        REQUIRE(setupConn != nullptr);
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        setupConn->executeUpdate("CREATE TABLE thread_test (id INTEGER PRIMARY KEY, value TEXT)");
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
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));

                    // Enable WAL mode for better concurrency
                    //conn->executeUpdate("PRAGMA journal_mode=WAL");

                    for (int j = 0; j < opsPerThread; j++)
                    {
                        try
                        {
                            int id = i * 1000 + j;

                            // Insert operation with retry for SQLITE_BUSY
                            bool inserted = false;
                            for (int retry = 0; retry < 5 && !inserted; retry++)
                            {
                                try
                                {
                                    auto pstmt = conn->prepareStatement("INSERT INTO thread_test (id, value) VALUES (?, ?)");
                                    pstmt->setInt(1, id);
                                    pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                                    pstmt->executeUpdate();

                                    // RESOURCE MANAGEMENT FIX (2026-02-15): Close statement after use
                                    // to prevent resource leaks and file locks
                                    pstmt->close();

                                    inserted = true;
                                }
                                catch (const std::exception& ex1)
                                {
                                    // Intentionally ignored — retry loop handles SQLITE_BUSY by sleeping and retrying
                                    std::this_thread::sleep_for(std::chrono::milliseconds(10 * (retry + 1)));
                                }
                            }

                            if (inserted)
                            {
                                // Select operation
                                auto selectStmt = conn->prepareStatement("SELECT * FROM thread_test WHERE id = ?");
                                selectStmt->setInt(1, id);
                                auto rs = selectStmt->executeQuery();

                                if (rs->next())
                                {
                                    successCount++;
                                }

                                // RESOURCE MANAGEMENT FIX (2026-02-15): Close result set and statement after use
                                // to prevent resource leaks and file locks
                                rs->close();
                                selectStmt->close();
                            }
                            else
                            {
                                errorCount++;
                            }
                        }
                        catch (const std::exception& ex)
                        {
                            errorCount++;
                            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " error: " + std::string(ex.what()));
                        }
                    }

                    conn->close();
                }
                catch (const std::exception& ex)
                {
                    errorCount += opsPerThread;
                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " connection error: " + std::string(ex.what()));
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

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Multiple threads with individual connections: " + std::to_string(successCount) + " successes, " + std::to_string(errorCount) + " errors");

        // Clean up
        auto cleanupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        cleanupConn->close();

        // We expect at least 95% of operations to succeed
        REQUIRE(successCount.load() >= (numThreads * opsPerThread * 0.95));
    }

    // Pool-based concurrent access, concurrent reads, and high-concurrency stress tests
    // were moved to 22_141_test_sqlite_real_connection_pool.cpp

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
                        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));

                        // Do a simple operation
                        auto rs = conn->executeQuery("SELECT 1 as test");
                        if (rs->next())
                        {
                            rs->getInt("test");
                        }

                        // RESOURCE MANAGEMENT FIX (2026-02-15): Close result set after use
                        // to prevent resource leaks and file locks
                        rs->close();

                        conn->close();
                        successCount++;
                    }
                    catch (const std::exception& e)
                    {
                        errorCount++;
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", std::string("Connection error: ") + e.what());
                    }
                } });
        }

        for (auto &t : threads)
        {
            t.join();
        }

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Rapid connection test: " + std::to_string(successCount) + " successes, " + std::to_string(errorCount) + " errors");

        REQUIRE(successCount > numThreads * connectionsPerThread * 0.95); // At least 95% success rate
    }
}

#else
TEST_CASE("SQLite Thread-Safety Tests (skipped)", "[22_111_02_sqlite_real_thread_safe]")
{
    SKIP("SQLite support is not enabled or thread-safety is disabled");
}
#endif // DB_DRIVER_THREAD_SAFE && USE_SQLITE