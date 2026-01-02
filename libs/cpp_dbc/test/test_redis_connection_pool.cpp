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
 * @file test_redis_connection_pool.cpp
 * @brief Tests for Redis connection pool implementation
 */

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>

#include <catch2/catch_test_macros.hpp>

#include "test_main.hpp"
#include "test_redis_common.hpp"

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/kv/kv_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>

#if USE_REDIS
// Test case for real Redis connection pool
TEST_CASE("Real Redis connection pool tests", "[redis_connection_pool_real]")
{
    // Skip these tests if we can't connect to Redis
    if (!redis_test_helpers::canConnectToRedis())
    {
        SKIP("Cannot connect to Redis database");
        return;
    }

    // Get Redis configuration using the helper function
    auto dbConfig = redis_test_helpers::getRedisConfig("dev_redis");

    // Create connection parameters
    std::string type = dbConfig.getType();
    std::string host = dbConfig.getHost();
    std::string database = dbConfig.getDatabase();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    // Build Redis connection string
    std::string connStr = redis_test_helpers::buildRedisConnectionString(dbConfig);

    // Generate a random key prefix for test isolation
    std::string testKeyPrefix = "test_pool_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_";
    std::string testHashKey = testKeyPrefix + "hash";
    std::string testListKey = testKeyPrefix + "list";
    std::string testStringKey = testKeyPrefix + "string";
    std::string testCounterKey = testKeyPrefix + "counter";

    SECTION("Basic connection pool operations")
    {
        // Get a Redis driver and register it with the DriverManager
        auto driver = redis_test_helpers::getRedisDriver();
        cpp_dbc::DriverManager::registerDriver("redis", driver);

        // Use the full connection string including the cpp_dbc: prefix if present
        std::string poolConnStr = connStr;

        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(poolConnStr);
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
        poolConfig.setValidationQuery("PING"); // Redis ping command

        // Create a connection pool
        auto pool = cpp_dbc::Redis::RedisConnectionPool::create(poolConfig);

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialActiveCount = pool->getActiveDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool->getDBConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool->getActiveDBConnectionCount() == 1);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool->getDBConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool->getActiveDBConnectionCount() == 2);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool->getActiveDBConnectionCount() == 1);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount);
        }

        // Test Redis operations through pooled connections
        SECTION("Redis operations with pooled connections")
        {
            // Get a connection from the pool
            auto conn = pool->getKVDBConnection();
            REQUIRE(conn != nullptr);

            // Set some test values
            REQUIRE(conn->setString(testStringKey, "Hello Redis!"));
            REQUIRE(conn->hashSet(testHashKey, "field1", "value1"));
            REQUIRE(conn->hashSet(testHashKey, "field2", "value2"));
            REQUIRE(conn->listPushRight(testListKey, "item1"));
            REQUIRE(conn->listPushRight(testListKey, "item2"));
            REQUIRE(conn->setString(testCounterKey, "10"));

            // Return connection to pool
            conn->close();

            // Get another connection and verify values
            auto conn2 = pool->getKVDBConnection();
            REQUIRE(conn2 != nullptr);

            // Check string value
            REQUIRE(conn2->getString(testStringKey) == "Hello Redis!");

            // Check hash values
            REQUIRE(conn2->hashGet(testHashKey, "field1") == "value1");
            REQUIRE(conn2->hashGet(testHashKey, "field2") == "value2");

            // Check list values
            auto listItems = conn2->listRange(testListKey, 0, -1);
            REQUIRE(listItems.size() == 2);
            REQUIRE(listItems[0] == "item1");
            REQUIRE(listItems[1] == "item2");

            // Check counter operations
            auto newValue = conn2->increment(testCounterKey);
            REQUIRE(newValue == 11);
            newValue = conn2->increment(testCounterKey, 5);
            REQUIRE(newValue == 16);

            // Return the second connection
            conn2->close();
        }

        // Test concurrent connections
        SECTION("Concurrent connections")
        {
            const uint64_t numThreads = 8;
            std::atomic<int> successCount(0);
            std::vector<std::thread> threads;

            for (uint64_t i = 0; i < numThreads; i++)
            {
                threads.push_back(std::thread([&pool, &successCount, i, testKeyPrefix]()
                                              {
                    try {
                        // Get connection from pool
                        auto threadConn = pool->getKVDBConnection();
                        
                        // Generate unique key for this thread
                        std::string threadKey = testKeyPrefix + "thread_" + std::to_string(i);
                        
                        // Set a value
                        bool setResult = threadConn->setString(threadKey, "Thread " + std::to_string(i));
                        
                        // Get the value back
                        std::string value = threadConn->getString(threadKey);
                        
                        // Verify value
                        bool valueMatches = (value == "Thread " + std::to_string(i));
                        
                        // Clean up
                        threadConn->deleteKey(threadKey);
                        
                        // Close the connection
                        threadConn->close();
                        
                        // Increment success count if all operations succeeded
                        if (setResult && valueMatches) {
                            successCount++;
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Thread " << i << " error: " << e.what() << std::endl;
                    } }));
            }

            // Wait for all threads to complete
            for (auto &t : threads)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }

            // Verify all threads succeeded
            REQUIRE(successCount == numThreads);
        }

        // Test connection pool under load
        SECTION("Connection pool under load")
        {
            const uint64_t numOperations = 50; // More operations than max connections
            std::atomic<int> successCount(0);
            std::vector<std::thread> threads;

            for (uint64_t i = 0; i < numOperations; i++)
            {
                threads.push_back(std::thread([&pool, &successCount, i]()
                                              {
                    try {
                        // Get connection from pool (may block if pool is exhausted)
                        auto loadConn = pool->getKVDBConnection();
                        
                        // Do a simple ping
                        std::string pong = loadConn->ping();
                        bool pingOk = (pong == "PONG");
                        
                        // Simulate some work
                        std::this_thread::sleep_for(std::chrono::milliseconds(10 + (i % 10)));
                        
                        // Close the connection
                        loadConn->close();
                        
                        // Increment success count if ping succeeded
                        if (pingOk) {
                            successCount++;
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Load operation " << i << " error: " << e.what() << std::endl;
                    } }));
            }

            // Wait for all threads to complete
            for (auto &t : threads)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }

            // Verify all operations succeeded
            REQUIRE(successCount == numOperations);

            // Verify pool returned to initial state
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            auto idleCount = pool->getIdleDBConnectionCount();
            REQUIRE(idleCount >= 3);  // At least minIdle connections
            REQUIRE(idleCount <= 10); // No more than maxSize connections
        }

        // Clean up test keys
        auto cleanupConn = pool->getKVDBConnection();
        try
        {
            // Find and delete all test keys
            auto keys = cleanupConn->scanKeys(testKeyPrefix + "*");
            if (!keys.empty())
            {
                cleanupConn->deleteKeys(keys);
            }
        }
        catch (const cpp_dbc::DBException &)
        {
            // Ignore cleanup errors
        }
        cleanupConn->close();

        // Close the pool
        pool->close();
    }

    // Test advanced pool features
    SECTION("Advanced pool features")
    {
        // Get a Redis driver and register it with the DriverManager
        auto driver = redis_test_helpers::getRedisDriver();
        cpp_dbc::DriverManager::registerDriver("redis", driver);

        // Use the full connection string including the cpp_dbc: prefix if present
        std::string poolConnStr = connStr;

        // Create a connection pool configuration with smaller size
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(poolConnStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(2);           // Smaller initial size
        poolConfig.setMaxSize(5);               // Smaller max size
        poolConfig.setMinIdle(1);               // Smaller min idle
        poolConfig.setConnectionTimeout(2000);  // Shorter timeout
        poolConfig.setIdleTimeout(10000);       // Shorter idle timeout
        poolConfig.setMaxLifetimeMillis(30000); // Shorter max lifetime
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(true);      // Test on return as well
        poolConfig.setValidationQuery("PING"); // Redis ping command

        // Create a connection pool
        auto pool = cpp_dbc::Redis::RedisConnectionPool::create(poolConfig);

        // Test connection validation
        SECTION("Connection validation")
        {
            // Get a connection
            auto conn = pool->getKVDBConnection();
            REQUIRE(conn != nullptr);

            // Set a test key
            std::string testKey = testKeyPrefix + "validation";
            REQUIRE(conn->setString(testKey, "Test Value"));

            // Verify test key
            REQUIRE(conn->getString(testKey) == "Test Value");

            // Clean up test key
            REQUIRE(conn->deleteKey(testKey));

            // Return it to the pool
            conn->close();

            // Pool stats should reflect the return
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getIdleDBConnectionCount() >= 1);
        }

        // Test pool growth
        SECTION("Pool growth")
        {
            // Get initial stats
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            std::vector<std::shared_ptr<cpp_dbc::KVDBConnection>> connections;

            // Get more connections than initialSize (should cause pool to grow)
            for (int i = 0; i < 4; i++)
            {
                connections.push_back(pool->getKVDBConnection());
                REQUIRE(connections.back() != nullptr);
            }

            // Verify pool grew
            REQUIRE(pool->getActiveDBConnectionCount() == 4);
            REQUIRE(pool->getTotalDBConnectionCount() > initialTotalCount);

            // Return all connections
            for (auto &conn : connections)
            {
                conn->close();
            }

            // Verify all returned
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getIdleDBConnectionCount() >= initialIdleCount);
        }

        // Clean up test keys
        auto cleanupConn = pool->getKVDBConnection();
        try
        {
            // Find and delete all test keys
            auto keys = cleanupConn->scanKeys(testKeyPrefix + "*");
            if (!keys.empty())
            {
                cleanupConn->deleteKeys(keys);
            }
        }
        catch (const cpp_dbc::DBException &)
        {
            // Ignore cleanup errors
        }
        cleanupConn->close();

        // Close the pool
        pool->close();

        // Verify pool is no longer running
        REQUIRE_FALSE(pool->isRunning());
    }
}
#endif