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
 * @file 26_141_test_scylladb_real_connection_pool.cpp
 * @brief Tests for Columnar (ScyllaDB) connection pool implementation
 */

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <fstream>
#include <random>
#include <cmath>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/columnar/columnar_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "26_001_test_scylladb_real_common.hpp"

#if USE_SCYLLADB
// Test case for real ScyllaDB connection pool
TEST_CASE("Real ScyllaDB connection pool tests", "[26_141_01_scylladb_real_connection_pool]")
{
    // Skip these tests if we can't connect to ScyllaDB
    if (!scylla_test_helpers::canConnectToScylla())
    {
        SKIP("Cannot connect to ScyllaDB database");
        return;
    }

    // Get ScyllaDB configuration using the helper function
    auto dbConfig = scylla_test_helpers::getScyllaConfig("dev_scylla");

    // Create connection parameters
    std::string type = dbConfig.getType();
    std::string host = dbConfig.getHost();
    std::string keyspace = dbConfig.getDatabase();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    // int port = dbConfig.getPort();

    // Build ScyllaDB connection URL
    // Scylla/Cassandra URL usually format: scylladb://host:port/keyspace
    // Or we can use the createConnectionString method if the driver supports it
    std::string connStr = dbConfig.createConnectionString();

    // Get test queries from config
    std::string createKeyspaceQuery = dbConfig.getOption("query__create_keyspace",
                                                         "CREATE KEYSPACE IF NOT EXISTS test_keyspace WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "CREATE TABLE IF NOT EXISTS test_keyspace.test_table (id int PRIMARY KEY, name text, value double)");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_keyspace.test_table (id, name, value) VALUES (?, ?, ?)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_keyspace.test_table WHERE id = ?");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE IF EXISTS test_keyspace.test_table");

    SECTION("Basic connection pool operations")
    {
        // Get a ScyllaDB driver and register it with the DriverManager (if not already registered)
        // Note: The pool creates connections via DriverManager, so generic Scylla driver must be registered.
        // Usually done in test_main or statically. We assume it's available or we register it.
        // cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

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
        poolConfig.setValidationQuery("SELECT now() FROM system.local");

        // Create a connection pool
        auto pool = cpp_dbc::ScyllaDB::ScyllaConnectionPool::create(poolConfig);

        // Initialize schema
        auto conn = pool->getColumnarDBConnection();
        try
        {
            // We might need to create keyspace first if the connection string doesn't include it or if it doesn't exist
            // But usually connection string requires existing keyspace or connects to system.
            // Assuming the helper config provides a usable connection string.
            // If connection string points to a specific keyspace, it must exist.
            // For testing, we might want to connect to system first to create keyspace?
            // Ignoring complex setup, assuming we can execute queries.

            // Try to create keyspace and table
            conn->executeUpdate(createKeyspaceQuery);
            conn->executeUpdate(createTableQuery);

            // Clear table
            conn->executeUpdate("TRUNCATE test_keyspace.test_table");
        }
        catch (const std::exception &e)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Setup warning: " + std::string(e.what()));
        }
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

        // Test operations through pooled connections
        SECTION("Operations with pooled connections")
        {
            // Get a connection from the pool
            auto conn1 = pool->getColumnarDBConnection();
            REQUIRE(conn1 != nullptr);

            // Insert test data using prepared statement
            auto pstmt = conn1->prepareStatement(insertDataQuery);

            const int numRows = 5;
            for (int i = 0; i < numRows; ++i)
            {
                pstmt->setInt(1, i);
                pstmt->setString(2, "Test " + std::to_string(i));
                pstmt->setDouble(3, static_cast<double>(i) * 1.5);
                pstmt->executeUpdate();
            }

            // Return connection to pool
            conn1->close();

            // Get another connection and verify data
            auto conn2 = pool->getColumnarDBConnection();
            REQUIRE(conn2 != nullptr);

            auto pstmtSelect = conn2->prepareStatement(selectDataQuery);
            pstmtSelect->setInt(1, 0); // Select id 0
            auto rs = pstmtSelect->executeQuery();

            REQUIRE(rs->next());
            REQUIRE(rs->getInt("id") == 0);
            REQUIRE(rs->getString("name") == "Test 0");
            REQUIRE(std::abs(rs->getDouble("value") - 0.0) < 0.001);

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
                threads.push_back(std::thread([&pool, &successCount, i, insertDataQuery]()
                                              {
                    try {
                        // Get connection from pool
                        auto threadConn = pool->getColumnarDBConnection();
                        
                        // Insert a record (using distinct IDs to avoid conflict)
                        auto pstmt = threadConn->prepareStatement(insertDataQuery);
                        int id = 100 + static_cast<int>(i);
                        pstmt->setInt(1, id);
                        pstmt->setString(2, "Thread " + std::to_string(i));
                        pstmt->setDouble(3, static_cast<double>(id) * 1.1);
                        pstmt->executeUpdate();
                        
                        // Close the connection
                        threadConn->close();
                        
                        // Increment success count
                        successCount++;
                    }
                    catch (const std::exception& e) {
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " error: " + std::string(e.what()));
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

        // Test invalid connection replacement on return
        SECTION("Invalid connection replacement on return")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            REQUIRE(pool->getActiveDBConnectionCount() == 0);

            // Get a connection from the pool
            auto conn_local = pool->getColumnarDBConnection();
            REQUIRE(conn_local != nullptr);
            REQUIRE(pool->getActiveDBConnectionCount() == 1);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount - 1);

            // Get the underlying connection and close it directly to invalidate
            auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarPooledDBConnection>(conn_local);
            REQUIRE(pooledConn != nullptr);

            auto underlyingConn = pooledConn->getUnderlyingColumnarConnection();
            REQUIRE(underlyingConn != nullptr);

            // Close the underlying connection directly - this invalidates the pooled connection
            underlyingConn->close();

            // Now return the (now invalid) connection to the pool
            // The pool should detect it's invalid and replace it
            conn_local->close();

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
            auto newConn = pool->getColumnarDBConnection();
            REQUIRE(newConn != nullptr);

            // Validate by executing a simple query (ScyllaDB doesn't have ping like Redis)
            auto rs = newConn->executeQuery("SELECT now() FROM system.local");
            REQUIRE(rs != nullptr);
            REQUIRE(rs->next());
            newConn->close();
        }

        // Test multiple invalid connections replacement
        SECTION("Multiple invalid connections replacement")
        {
            // Get initial pool statistics
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(initialIdleCount >= 2); // Need at least 2 idle connections for this test

            // Get multiple connections
            std::vector<std::shared_ptr<cpp_dbc::ColumnarDBConnection>> connections;
            const size_t numConnections = 2;

            for (size_t i = 0; i < numConnections; i++)
            {
                auto invalidConn = pool->getColumnarDBConnection();
                REQUIRE(invalidConn != nullptr);
                connections.push_back(invalidConn);
            }

            REQUIRE(pool->getActiveDBConnectionCount() == numConnections);

            // Invalidate all connections by closing their underlying connections
            for (auto &invalidConn : connections)
            {
                auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarPooledDBConnection>(invalidConn);
                REQUIRE(pooledConn != nullptr);

                auto underlyingConn = pooledConn->getUnderlyingColumnarConnection();
                underlyingConn->close();
            }

            // Return all invalid connections to the pool
            for (auto &invalidConn : connections)
            {
                invalidConn->close();
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
                auto newConn = pool->getColumnarDBConnection();
                REQUIRE(newConn != nullptr);

                // Validate by executing a simple query
                auto rs = newConn->executeQuery("SELECT now() FROM system.local");
                REQUIRE(rs != nullptr);
                REQUIRE(rs->next());
                newConn->close();
            }
        }

        // Clean up
        auto cleanupConn = pool->getColumnarDBConnection();
        try
        {
            cleanupConn->executeUpdate(dropTableQuery);
            // Typically we don't drop the keyspace as it might be shared or system
            // cleanupConn->executeUpdate("DROP KEYSPACE IF EXISTS test_keyspace");
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
        // Create a connection pool configuration with testOnReturn enabled
        cpp_dbc::config::DBConnectionPoolConfig poolConfigLocal;
        poolConfigLocal.setUrl(connStr);
        poolConfigLocal.setUsername(username);
        poolConfigLocal.setPassword(password);
        poolConfigLocal.setInitialSize(5);
        poolConfigLocal.setMaxSize(10);
        poolConfigLocal.setMinIdle(3);
        poolConfigLocal.setConnectionTimeout(2000);
        poolConfigLocal.setIdleTimeout(10000);
        poolConfigLocal.setMaxLifetimeMillis(30000);
        poolConfigLocal.setTestOnBorrow(true);
        poolConfigLocal.setTestOnReturn(true);
        poolConfigLocal.setValidationQuery("SELECT now() FROM system.local");

        auto pool = cpp_dbc::ScyllaDB::ScyllaConnectionPool::create(poolConfigLocal);

        // Test connection validation
        SECTION("Connection validation")
        {
            auto conn = pool->getColumnarDBConnection();
            REQUIRE(conn != nullptr);

            auto rs = conn->executeQuery("SELECT now() FROM system.local");
            REQUIRE(rs != nullptr);
            REQUIRE(rs->next());

            conn->close();

            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getIdleDBConnectionCount() >= 1);
        }

        // Test pool growth
        SECTION("Pool growth")
        {
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            std::vector<std::shared_ptr<cpp_dbc::ColumnarDBConnection>> connections;

            const size_t numConnectionsToRequest = initialTotalCount + 2;
            for (size_t i = 0; i < numConnectionsToRequest; i++)
            {
                connections.push_back(pool->getColumnarDBConnection());
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

            auto conn = pool->getColumnarDBConnection();
            REQUIRE(conn != nullptr);
            REQUIRE(pool->getActiveDBConnectionCount() == 1);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount - 1);

            auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarPooledDBConnection>(conn);
            REQUIRE(pooledConn != nullptr);

            auto underlyingConn = pooledConn->getUnderlyingColumnarConnection();
            REQUIRE(underlyingConn != nullptr);

            underlyingConn->close();
            conn->close();

            // Poll for the pool to process the replacement
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

            auto newConn = pool->getColumnarDBConnection();
            REQUIRE(newConn != nullptr);
            auto rs = newConn->executeQuery("SELECT now() FROM system.local");
            REQUIRE(rs != nullptr);
            REQUIRE(rs->next());
            newConn->close();
        }

        // Test multiple invalid connections replacement
        SECTION("Multiple invalid connections replacement")
        {
            auto initialIdleCount = pool->getIdleDBConnectionCount();
            auto initialTotalCount = pool->getTotalDBConnectionCount();

            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(initialIdleCount >= 2);

            std::vector<std::shared_ptr<cpp_dbc::ColumnarDBConnection>> connections;
            const size_t numConnections = 2;

            for (size_t i = 0; i < numConnections; i++)
            {
                auto invalidConn = pool->getColumnarDBConnection();
                REQUIRE(invalidConn != nullptr);
                connections.push_back(invalidConn);
            }

            REQUIRE(pool->getActiveDBConnectionCount() == numConnections);

            for (auto &invalidConn : connections)
            {
                auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarPooledDBConnection>(invalidConn);
                REQUIRE(pooledConn != nullptr);

                auto underlyingConn = pooledConn->getUnderlyingColumnarConnection();
                underlyingConn->close();
            }

            for (auto &invalidConn : connections)
            {
                invalidConn->close();
            }

            // Poll for the pool to process replacements
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

            for (size_t i = 0; i < numConnections; i++)
            {
                auto newConn = pool->getColumnarDBConnection();
                REQUIRE(newConn != nullptr);
                auto rs = newConn->executeQuery("SELECT now() FROM system.local");
                REQUIRE(rs != nullptr);
                REQUIRE(rs->next());
                newConn->close();
            }
        }

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
                        auto loadConn = pool->getColumnarDBConnection();
                        if (!loadConn)
                        {
                            failureCount++;
                            return;
                        }

                        auto rs = loadConn->executeQuery("SELECT now() FROM system.local");
                        if (!rs || !rs->next())
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

            REQUIRE(failureCount == 0);
            REQUIRE(successCount == numOperations);
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            auto idleCount = pool->getIdleDBConnectionCount();
            REQUIRE(idleCount >= 3);
            REQUIRE(idleCount <= 10);
        }

        pool->close();
        REQUIRE_FALSE(pool->isRunning());
    }
}
#endif
