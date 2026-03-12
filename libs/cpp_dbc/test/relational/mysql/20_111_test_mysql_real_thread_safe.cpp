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

 @file 20_111_test_mysql_real_thread_safe.cpp
 @brief Thread-safety stress tests for MySQL database driver

*/

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <mutex>
#include <condition_variable>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/pool/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "20_001_test_mysql_real_common.hpp"

#if DB_DRIVER_THREAD_SAFE && USE_MYSQL

/**
 * @brief Thread-safety stress tests for MySQL driver
 *
 * These tests verify that the MySQL driver is thread-safe by:
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
TEST_CASE("MySQL Thread-Safety Tests", "[20_111_01_mysql_real_thread_safe]")
{
    // Skip these tests if we can't connect to MySQL
    if (!mysql_test_helpers::canConnectToMySQL())
    {
        SKIP("Cannot connect to MySQL database");
        return;
    }

    // Get MySQL configuration
    auto dbConfig = mysql_test_helpers::getMySQLConfig("dev_mysql");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Register the MySQL driver
    cpp_dbc::DriverManager::registerDriver(mysql_test_helpers::getMySQLDriver());

    SECTION("Multiple threads with individual connections")
    {
        // Setup: create test table using a single connection
        auto setupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(setupConn != nullptr);
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        setupConn->executeUpdate("CREATE TABLE thread_test (id INT PRIMARY KEY, value VARCHAR(100))");
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

                cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " started");

                try
                {
                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " opening connection...");

                    // Each thread gets its own connection
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

                    if (!conn)
                    {
                        errorCount += opsPerThread;
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " FAILED: dynamic_pointer_cast returned nullptr");
                        return;
                    }

                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " connection opened");

                    for (int j = 0; j < opsPerThread; j++)
                    {
                        //cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " inserting...");
                        try
                        {
                            int id = i * 1000 + j;

                            // Insert operation
                            auto pstmt = conn->prepareStatement("INSERT INTO thread_test (id, value) VALUES (?, ?)");
                            pstmt->setInt(1, id);
                            pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                            pstmt->executeUpdate();

                            //cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " selecting...");

                            // Select operation
                            auto selectStmt = conn->prepareStatement("SELECT * FROM thread_test WHERE id = ?");
                            selectStmt->setInt(1, id);
                            auto rs = selectStmt->executeQuery();

                            if (rs->next())
                            {
                                successCount++;
                                //cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " OK");
                            }
                            else
                            {
                                errorCount++;
                                cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " FAILED: row not found after insert");
                            }
                        }
                        catch (const std::exception &ex)
                        {
                            errorCount++;
                            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " error: " + std::string(ex.what()));
                        }
                    }

                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " closing connection...");
                    conn->close();
                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " finished");
                }
                catch (const std::exception &ex)
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
        auto cleanupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        cleanupConn->close();

        // We expect at least 95% of operations to succeed
        REQUIRE(successCount.load() >= (numThreads * opsPerThread * 0.95));
    }

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
                        auto rs = conn->executeQuery("SELECT 1 as test");
                        if (rs->next())
                        {
                            rs->getInt("test");
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

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Rapid connection test: " + std::to_string(successCount) + " successes, " + std::to_string(errorCount) + " errors");

        REQUIRE(successCount > numThreads * connectionsPerThread * 0.95); // At least 95% success rate
    }
}

#else
TEST_CASE("MySQL Thread-Safety Tests (skipped)", "[20_111_02_mysql_real_thread_safe]")
{
    SKIP("MySQL support is not enabled or thread-safety is disabled");
}
#endif // DB_DRIVER_THREAD_SAFE && USE_MYSQL