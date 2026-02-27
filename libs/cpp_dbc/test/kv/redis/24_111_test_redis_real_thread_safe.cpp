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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file 24_111_test_redis_real_thread_safe.cpp
 * @brief Thread-safety stress tests for Redis database driver
 */

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include <catch2/catch_test_macros.hpp>

#include "10_000_test_main.hpp"
#include "24_001_test_redis_real_common.hpp"

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#if DB_DRIVER_THREAD_SAFE && USE_REDIS

/**
 * @brief Thread-safety stress tests for Redis driver
 *
 * These tests verify that the Redis driver is thread-safe by:
 * 1. Multiple threads each with their own connection
 * 2. Rapid connection open/close from multiple threads
 */
TEST_CASE("Redis Thread-Safety Tests", "[24_111_01_redis_real_thread_safe]")
{
    // Skip these tests if we can't connect to Redis
    if (!redis_test_helpers::canConnectToRedis())
    {
        SKIP("Cannot connect to Redis database");
        return;
    }

    // Get Redis configuration
    auto dbConfig = redis_test_helpers::getRedisConfig("dev_redis");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = redis_test_helpers::buildRedisConnectionString(dbConfig);

    // Register the Redis driver
    cpp_dbc::DriverManager::registerDriver(redis_test_helpers::getRedisDriver());

    SECTION("Multiple threads with individual connections")
    {
        const int numThreads = 10;
        const int opsPerThread = 20;
        std::atomic<int> successCount(0);
        std::atomic<int> errorCount(0);
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++)
        {
            threads.emplace_back([&, i]()
                                 {
                cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " started");

                try
                {
                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " opening connection...");

                    // Each thread gets its own connection
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::KVDBConnection>(
                        cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

                    if (!conn)
                    {
                        errorCount += opsPerThread;
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " FAILED: dynamic_pointer_cast returned nullptr");
                        return;
                    }

                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " connection opened");

                    for (int j = 0; j < opsPerThread; j++)
                    {
                        //cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " setting key...");
                        try
                        {
                            std::string key = "thread_test_" + std::to_string(i) + "_" + std::to_string(j);
                            std::string value = "Thread " + std::to_string(i) + " Op " + std::to_string(j);

                            // Set operation
                            conn->setString(key, value);

                            //cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " getting key...");

                            // Get operation
                            std::string retrieved = conn->getString(key);

                            if (retrieved == value)
                            {
                                successCount++;
                                //cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " OK");
                            }
                            else
                            {
                                errorCount++;
                                cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " FAILED: retrieved value mismatch");
                            }

                            // Clean up key
                            conn->deleteKey(key);
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

        for (auto &t : threads)
        {
            t.join();
        }

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Multiple threads with individual connections: " + std::to_string(successCount) + " successes, " + std::to_string(errorCount) + " errors");

        const int expectedAttempts = numThreads * opsPerThread;
        REQUIRE(successCount >= static_cast<int>(expectedAttempts * 0.95));
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
                        auto conn = std::dynamic_pointer_cast<cpp_dbc::KVDBConnection>(
                            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

                        if (!conn)
                        {
                            errorCount++;
                            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Connection FAILED: dynamic_pointer_cast returned nullptr");
                            continue;
                        }

                        // Do a simple operation
                        if (conn->ping())
                        {
                            successCount++;
                        }

                        conn->close();
                    }
                    catch (const std::exception &ex)
                    {
                        errorCount++;
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Connection error: " + std::string(ex.what()));
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
TEST_CASE("Redis Thread-Safety Tests (skipped)", "[24_111_02_redis_real_thread_safe]")
{
    SKIP("Redis support is not enabled or thread-safety is disabled");
}
#endif // DB_DRIVER_THREAD_SAFE && USE_REDIS
