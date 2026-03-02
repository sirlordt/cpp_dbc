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
 * @file 25_141_test_mongodb_real_connection_pool.cpp
 * @brief Tests for MongoDB connection pool implementation
 */

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/document/document_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "25_001_test_mongodb_real_common.hpp"

#if USE_MONGODB
// Test case for real MongoDB connection pool
TEST_CASE("Real MongoDB connection pool tests", "[25_141_01_mongodb_real_connection_pool]")
{
    // Skip these tests if we can't connect to MongoDB
    if (!mongodb_test_helpers::canConnectToMongoDB())
    {
        SKIP("Cannot connect to MongoDB database");
        return;
    }

    // Get MongoDB configuration using the helper function
    auto dbConfig = mongodb_test_helpers::getMongoDBConfig("dev_mongodb");

    // Create connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    // Build MongoDB connection string
    std::string connStr = mongodb_test_helpers::buildMongoDBConnectionString(dbConfig);

    // Get test collection name from config
    std::string testCollectionName = dbConfig.getOption("collection__test", "test_collection");
    std::string testPoolCollectionName = testCollectionName + "_pool";

    SECTION("Basic connection pool operations")
    {
        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfigLocal;
        poolConfigLocal.setUrl(connStr);
        poolConfigLocal.setUsername(username);
        poolConfigLocal.setPassword(password);
        poolConfigLocal.setInitialSize(5);
        poolConfigLocal.setMaxSize(10);
        poolConfigLocal.setMinIdle(3);
        poolConfigLocal.setConnectionTimeout(3500);
        poolConfigLocal.setIdleTimeout(10000);
        poolConfigLocal.setMaxLifetimeMillis(30000);
        poolConfigLocal.setTestOnBorrow(true);
        poolConfigLocal.setTestOnReturn(true);
        poolConfigLocal.setValidationQuery("{\"ping\": 1}");
        // poolConfigLocal.setValidationInterval(1000);

        // Create a connection pool using factory method
        auto poolResult = cpp_dbc::MongoDB::MongoDBConnectionPool::create(std::nothrow, poolConfigLocal);
        if (!poolResult.has_value())
        {
            throw poolResult.error();
        }
        auto pool = poolResult.value();

        // Clean up test collection from previous runs
        auto conn = pool->getDocumentDBConnection();
        if (conn->collectionExists(testPoolCollectionName))
        {
            conn->dropCollection(testPoolCollectionName);
        }
        conn->createCollection(testPoolCollectionName);
        conn->close();

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

        // Clean up
        auto cleanupConn = pool->getDocumentDBConnection();
        try
        {
            if (cleanupConn->collectionExists(testPoolCollectionName))
            {
                cleanupConn->dropCollection(testPoolCollectionName);
            }
        }
        catch (const cpp_dbc::DBException &)
        {
            // Intentionally empty — ignore cleanup errors
        }
        cleanupConn->close();

        // Close the pool
        pool->close();
    }

    // Test advanced pool features
    SECTION("Advanced pool features")
    {
        // Create a connection pool configuration with testOnReturn enabled
        cpp_dbc::config::DBConnectionPoolConfig poolConfigLocal;
        poolConfigLocal.setUrl(connStr);
        poolConfigLocal.setUsername(username);
        poolConfigLocal.setPassword(password);
        poolConfigLocal.setInitialSize(5);
        poolConfigLocal.setMaxSize(10);
        poolConfigLocal.setMinIdle(3);
        poolConfigLocal.setConnectionTimeout(5000);
        poolConfigLocal.setIdleTimeout(10000);
        poolConfigLocal.setMaxLifetimeMillis(30000);
        poolConfigLocal.setTestOnBorrow(true);
        poolConfigLocal.setTestOnReturn(true);
        poolConfigLocal.setValidationQuery("{\"ping\": 1}");

        // Create a connection pool
        auto poolResult = cpp_dbc::MongoDB::MongoDBConnectionPool::create(std::nothrow, poolConfigLocal);
        if (!poolResult.has_value())
        {
            throw poolResult.error();
        }
        auto pool = poolResult.value();

        /*
        // Test connection validation
        SECTION("Connection validation")
        {
            auto conn = pool->getDocumentDBConnection();
            REQUIRE(conn != nullptr);

            REQUIRE(conn->ping());

            conn->close();

            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getIdleDBConnectionCount() >= 1);
        }

        // Test pool growth
        SECTION("Pool growth")
        {
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            std::vector<std::shared_ptr<cpp_dbc::DocumentDBConnection>> connections;

            const size_t numConnectionsToRequest = initialTotalCount + 2;
            for (size_t i = 0; i < numConnectionsToRequest; i++)
            {
                connections.push_back(pool->getDocumentDBConnection());
                REQUIRE(connections.back() != nullptr);
            }

            REQUIRE(pool->getActiveDBConnectionCount() == numConnectionsToRequest);
            REQUIRE(pool->getTotalDBConnectionCount() > initialTotalCount);

            for (auto &conn : connections)
            {
                conn->close();
            }

            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getIdleDBConnectionCount() >= initialIdleCount);
        }

        // Test invalid connection replacement on return
        SECTION("Invalid connection replacement on return")
        {
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            REQUIRE(pool->getActiveDBConnectionCount() == 0);

            auto conn = pool->getDocumentDBConnection();
            REQUIRE(conn != nullptr);
            REQUIRE(pool->getActiveDBConnectionCount() == 1);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount - 1);

            auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::DocumentPooledDBConnection>(conn);
            REQUIRE(pooledConn != nullptr);

            auto underlyingConn = pooledConn->getUnderlyingDocumentConnection();
            REQUIRE(underlyingConn != nullptr);

            // Close the underlying connection directly - this invalidates the pooled connection
            underlyingConn->close();

            // Now return the (now invalid) connection to the pool
            // The pool should detect it's invalid and replace it
            conn->close();

            // Poll for the pool to process the replacement instead of fixed sleep
            auto startTime = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::milliseconds(5000);
            bool poolStateConverged = false;

            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getActiveDBConnectionCount() == 0 &&
                    pool->getTotalDBConnectionCount() == initialTotalCount &&
                    pool->getIdleDBConnectionCount() == initialIdleCount)
                {
                    poolStateConverged = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            REQUIRE(poolStateConverged);
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getTotalDBConnectionCount() == initialTotalCount);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount);

            // Verify replacement connection works
            auto newConn = pool->getDocumentDBConnection();
            REQUIRE(newConn != nullptr);
            REQUIRE(newConn->ping());
            newConn->close();
        }

        // Test multiple invalid connections replacement
        SECTION("Multiple invalid connections replacement")
        {
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(initialIdleCount >= 3); // Need at least 3 idle connections for this test

            std::vector<std::shared_ptr<cpp_dbc::DocumentDBConnection>> connections;
            const size_t numConnections = 3;

            for (size_t i = 0; i < numConnections; i++)
            {
                auto conn = pool->getDocumentDBConnection();
                REQUIRE(conn != nullptr);
                connections.push_back(conn);
            }

            REQUIRE(pool->getActiveDBConnectionCount() == numConnections);
            REQUIRE(pool->getIdleDBConnectionCount() == (initialIdleCount - numConnections));

            // Invalidate all connections by closing their underlying connections
            for (auto &conn : connections)
            {
                auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::DocumentPooledDBConnection>(conn);
                REQUIRE(pooledConn != nullptr);

                auto underlyingConn = pooledConn->getUnderlyingDocumentConnection();
                REQUIRE(underlyingConn != nullptr);
                underlyingConn->close();
            }

            // Return all invalid connections to the pool
            for (auto &conn : connections)
            {
                conn->close();
            }

            // Poll for the pool to process replacements instead of fixed sleep
            auto startTime = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::milliseconds(5000);
            bool poolStateConverged = false;

            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getActiveDBConnectionCount() == 0 &&
                    pool->getTotalDBConnectionCount() == initialTotalCount &&
                    pool->getIdleDBConnectionCount() == initialIdleCount)
                {
                    poolStateConverged = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            REQUIRE(poolStateConverged);
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getTotalDBConnectionCount() == initialTotalCount);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount);

            // Verify all replacement connections work
            for (size_t i = 0; i < numConnections; i++)
            {
                auto newConn = pool->getDocumentDBConnection();
                REQUIRE(newConn != nullptr);
                REQUIRE(newConn->ping());
                newConn->close();
            }
        }
            */

        // Test concurrent connections under load
        SECTION("Connection pool under load")
        {
            const uint64_t numOperations = 40;
            std::atomic<int> successCount(0);
            std::atomic<int> failureCount(0);
            std::vector<std::thread> threads;

            for (uint64_t i = 0; i < numOperations; i++)
            {
                threads.push_back(std::thread([&pool, &successCount, &failureCount, i]()
                                              {
                    try {
                        auto loadConn = pool->getDocumentDBConnection();
                        if (!loadConn)
                        {
                            failureCount++;
                            return;
                        }

                        bool isAlive = loadConn->ping();
                        if (!isAlive)
                        {
                            failureCount++;
                            loadConn->close();
                            return;
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(10 + (i % 10)));
                        loadConn->close();
                        successCount++;
                    }
                    catch (const std::exception& ex) {
                        failureCount++;
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Load operation " + std::to_string(i) + " error: " + std::string(ex.what()));
                    } }));
            }

            for (auto &t : threads)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }

            if (failureCount > 0)
            {
                WARN("failureCount: " << failureCount);
            }
            else
            {
                SUCCEED("failureCount: 0");
            }
            REQUIRE(successCount >= static_cast<int>(numOperations * 0.95));
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            auto idleCount = pool->getIdleDBConnectionCount();
            REQUIRE(idleCount >= 3);  // At least minIdle connections
            REQUIRE(idleCount <= 10); // No more than maxSize connections
        }

        // Close the pool
        pool->close();

        // Verify pool is no longer running
        REQUIRE_FALSE(pool->isRunning());
    }
}
#endif
