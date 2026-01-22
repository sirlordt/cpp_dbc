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
 * @file test_mongodb_connection_pool.cpp
 * @brief Tests for MongoDB connection pool implementation
 */

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <random>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/document/document_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "test_mongodb_common.hpp"

#if USE_MONGODB
// Test case for real MongoDB connection pool
TEST_CASE("Real MongoDB connection pool tests", "[mongodb_connection_pool_real]")
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
    std::string type = dbConfig.getType();
    std::string host = dbConfig.getHost();
    std::string database = dbConfig.getDatabase();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    // Build MongoDB connection string
    std::string connStr = mongodb_test_helpers::buildMongoDBConnectionString(dbConfig);

    // Get test collection name from config
    std::string testCollectionName = dbConfig.getOption("collection__test", "test_collection");
    std::string testPoolCollectionName = testCollectionName + "_pool";

    SECTION("Basic connection pool operations")
    {
        // Get a MongoDB driver and register it with the DriverManager
        auto driver = mongodb_test_helpers::getMongoDBDriver();
        cpp_dbc::DriverManager::registerDriver(driver);

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
        poolConfig.setValidationQuery("{\"ping\": 1}"); // MongoDB ping command

        // Create a connection pool
        auto pool = cpp_dbc::MongoDB::MongoDBConnectionPool::create(poolConfig);

        // Create a test collection
        auto conn = pool->getDocumentDBConnection();

        // Drop collection if it exists (cleanup from previous test runs)
        if (conn->collectionExists(testPoolCollectionName))
        {
            conn->dropCollection(testPoolCollectionName);
        }

        // Create a new collection for tests
        auto collection = conn->createCollection(testPoolCollectionName);
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

        // Test document operations through pooled connections
        SECTION("Document operations with pooled connections")
        {
            // Get a connection from the pool
            auto conn1 = pool->getDocumentDBConnection();
            REQUIRE(conn1 != nullptr);

            // Get the test collection
            auto collection1 = conn1->getCollection(testPoolCollectionName);
            REQUIRE(collection1 != nullptr);

            // Insert test documents
            uint64_t docCount = 5;
            std::vector<std::string> docIds;

            for (uint64_t i = 0; i < docCount; i++)
            {
                // Create and insert a document
                std::string docJson = mongodb_test_helpers::generateTestDocument(
                    static_cast<int>(i),
                    "Test " + std::to_string(i),
                    static_cast<double>(i) * 10.5);
                auto insertResult = collection->insertOne(docJson);

                REQUIRE(insertResult.acknowledged);
                REQUIRE(!insertResult.insertedId.empty());

                docIds.push_back(insertResult.insertedId);
            }

            // Return connection to pool
            conn->close();

            // Get another connection and verify documents
            auto conn2 = pool->getDocumentDBConnection();
            REQUIRE(conn2 != nullptr);

            auto collection2 = conn2->getCollection(testPoolCollectionName);
            REQUIRE(collection2 != nullptr);

            // Count documents
            auto count = collection2->countDocuments();
            REQUIRE(count == docCount);

            // Find a document by ID
            auto doc = collection2->findById(docIds[0]);
            REQUIRE(doc != nullptr);

            // Return the second connection
            conn2->close();
        }

        // Test concurrent connections
        SECTION("Concurrent connections")
        {
            // Get initial document count before adding more
            auto initialConn = pool->getDocumentDBConnection();
            auto initialCollection = initialConn->getCollection(testPoolCollectionName);
            auto initialCount = initialCollection->countDocuments();
            initialConn->close();

            const uint64_t numThreads = 8;
            std::atomic<int> successCount(0);
            std::vector<std::thread> threads;

            for (uint64_t i = 0; i < numThreads; i++)
            {
                threads.push_back(std::thread([&pool, &successCount, i, testPoolCollectionName]()
                                              {
                    try {
                        // Get connection from pool
                        auto threadConn = pool->getDocumentDBConnection();
                        
                        // Get collection
                        auto threadCollection = threadConn->getCollection(testPoolCollectionName);
                        
                        // Insert a document
                        std::string docJson = mongodb_test_helpers::generateTestDocument(
                            static_cast<int>(i * 1000),
                            "Thread " + std::to_string(i),
                            static_cast<double>(i) * 100.5
                        );
                        
                        auto result = threadCollection->insertOne(docJson);
                        
                        // Close the connection
                        threadConn->close();
                        
                        // Increment success count
                        successCount++;
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

            // Get connection and verify document count
            auto verifyConn = pool->getDocumentDBConnection();
            auto verifyCollection = verifyConn->getCollection(testPoolCollectionName);
            auto totalCount = verifyCollection->countDocuments();

            // Count should be initial count + numThreads
            REQUIRE(totalCount == initialCount + numThreads);

            verifyConn->close();
        }

        // Test connection pool under load
        SECTION("Connection pool under load")
        {
            const uint64_t numOperations = 50; // More operations than max connections
            std::atomic<int> successCount(0);
            std::atomic<int> failureCount(0);
            std::vector<std::thread> threads;

            for (uint64_t i = 0; i < numOperations; i++)
            {
                threads.push_back(std::thread([&pool, &successCount, &failureCount, i]()
                                              {
                    try {
                        // Get connection from pool (may block if pool is exhausted)
                        auto loadConn = pool->getDocumentDBConnection();
                        if (!loadConn)
                        {
                            failureCount++;
                            return;
                        }

                        // Do some quick work
                        bool isAlive = loadConn->ping();
                        if (!isAlive)
                        {
                            failureCount++;
                            loadConn->close();
                            return;
                        }

                        // Simulate some work
                        std::this_thread::sleep_for(std::chrono::milliseconds(10 + (i % 10)));

                        // Close the connection
                        loadConn->close();

                        // Increment success count
                        successCount++;
                    }
                    catch (const std::exception& ex) {
                        failureCount++;
                        std::cerr << "Load operation " << i << " error: " << ex.what() << std::endl;
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

            // Thread-safe assertions on main thread
            REQUIRE(failureCount == 0);
            REQUIRE(successCount == numOperations);

            // Verify pool returned to initial state
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            auto idleCount = pool->getIdleDBConnectionCount();
            REQUIRE(idleCount >= 3);  // At least minIdle connections
            REQUIRE(idleCount <= 10); // No more than maxSize connections
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
            // Ignore cleanup errors
        }
        cleanupConn->close();

        // Close the pool
        pool->close();
    }

    // Test advanced pool features
    SECTION("Advanced pool features")
    {
        // Get a MongoDB driver and register it with the DriverManager
        auto driver = mongodb_test_helpers::getMongoDBDriver();
        cpp_dbc::DriverManager::registerDriver(driver);

        // Create a connection pool configuration with smaller size
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);           // Initial size (enough for invalid connection tests)
        poolConfig.setMaxSize(10);              // Max size
        poolConfig.setMinIdle(3);               // Min idle (need at least 3 for multiple invalid connection test)
        poolConfig.setConnectionTimeout(2000);  // Shorter timeout
        poolConfig.setIdleTimeout(10000);       // Shorter idle timeout
        poolConfig.setMaxLifetimeMillis(30000); // Shorter max lifetime
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(true);               // Test on return as well
        poolConfig.setValidationQuery("{\"ping\": 1}"); // MongoDB ping command

        // Create a connection pool
        auto pool = cpp_dbc::MongoDB::MongoDBConnectionPool::create(poolConfig);

        // Test connection validation
        SECTION("Connection validation")
        {
            // Get a connection
            auto conn = pool->getDocumentDBConnection();
            REQUIRE(conn != nullptr);

            // Verify connection works
            REQUIRE(conn->ping());

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

            std::vector<std::shared_ptr<cpp_dbc::DocumentDBConnection>> connections;

            // Get more connections than initialSize (should cause pool to grow)
            // With initialSize=5, we need to request more than 5 to force growth
            const size_t numConnectionsToRequest = initialTotalCount + 2;
            for (size_t i = 0; i < numConnectionsToRequest; i++)
            {
                connections.push_back(pool->getDocumentDBConnection());
                REQUIRE(connections.back() != nullptr);
            }

            // Verify pool grew
            REQUIRE(pool->getActiveDBConnectionCount() == numConnectionsToRequest);
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

        // Test invalid connection replacement
        SECTION("Invalid connection replacement on return")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            REQUIRE(pool->getActiveDBConnectionCount() == 0);

            // Get a connection from the pool
            auto conn = pool->getDocumentDBConnection();
            REQUIRE(conn != nullptr);
            REQUIRE(pool->getActiveDBConnectionCount() == 1);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount - 1);

            // Get the underlying connection and close it directly to invalidate
            auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::DocumentPooledDBConnection>(conn);
            REQUIRE(pooledConn != nullptr);

            auto underlyingConn = pooledConn->getUnderlyingDocumentConnection();
            REQUIRE(underlyingConn != nullptr);

            // Close the underlying connection directly - this invalidates the pooled connection
            underlyingConn->close();

            // Now return the (now invalid) connection to the pool
            // The pool should detect it's invalid and replace it
            conn->close();

            // Give the pool a moment to process the replacement
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Verify pool statistics:
            // - activeConnections should be 0 (connection was returned)
            // - totalConnections should remain the same (invalid connection was replaced)
            // - idleConnections should be back to initial (replacement went to idle)
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getTotalDBConnectionCount() == initialTotalCount);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount);

            // Verify we can still get a working connection from the pool
            auto newConn = pool->getDocumentDBConnection();
            REQUIRE(newConn != nullptr);
            REQUIRE(newConn->ping()); // Should work - it's the replacement connection
            newConn->close();
        }

        // Test multiple invalid connections replacement
        SECTION("Multiple invalid connections replacement")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(initialIdleCount >= 3); // Need at least 3 idle connections for this test

            // Get multiple connections
            std::vector<std::shared_ptr<cpp_dbc::DocumentDBConnection>> connections;
            const size_t numConnections = 3;

            for (size_t i = 0; i < numConnections; i++)
            {
                auto conn = pool->getDocumentDBConnection();
                REQUIRE(conn != nullptr);
                connections.push_back(conn);
            }

            REQUIRE(pool->getActiveDBConnectionCount() == numConnections);
            // Use signed arithmetic to avoid underflow
            REQUIRE(pool->getIdleDBConnectionCount() == (initialIdleCount - numConnections));

            // Invalidate all connections by closing their underlying connections
            for (auto &conn : connections)
            {
                auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::DocumentPooledDBConnection>(conn);
                REQUIRE(pooledConn != nullptr);

                auto underlyingConn = pooledConn->getUnderlyingDocumentConnection();
                underlyingConn->close();
            }

            // Return all invalid connections to the pool
            for (auto &conn : connections)
            {
                conn->close();
            }

            // Give the pool time to process replacements
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // Verify pool statistics
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

        // Close the pool
        pool->close();

        // Verify pool is no longer running
        REQUIRE_FALSE(pool->isRunning());
    }
}
#endif