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

 @file 23_141_test_firebird_real_connection_pool.cpp
 @brief Tests for Firebird connection pool

*/

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "23_001_test_firebird_real_common.hpp"

#if USE_FIREBIRD
// Test case for real Firebird connection pool
TEST_CASE("Real Firebird connection pool tests", "[23_141_01_firebird_real_connection_pool]")
{
    // Skip these tests if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    // Get Firebird configuration using the helper function
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");

    // Create connection parameters
    std::string host = dbConfig.getHost();
    std::string database = dbConfig.getDatabase();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    std::string connStr = dbConfig.createConnectionString();

    // Get test queries from config
    // Note: "VALUE" is a reserved word in Firebird, so we use "amount" instead
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "CREATE TABLE test_table (id INTEGER PRIMARY KEY, name VARCHAR(100), amount DOUBLE PRECISION)");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_table (id, name, amount) VALUES (1, 'Test', 1.5)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_table");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE test_table");

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
        poolConfigLocal.setConnectionTimeout(5000);
        poolConfigLocal.setValidationInterval(1000);
        poolConfigLocal.setIdleTimeout(30000);
        poolConfigLocal.setMaxLifetimeMillis(60000);
        poolConfigLocal.setTestOnBorrow(true);
        poolConfigLocal.setTestOnReturn(false);
        poolConfigLocal.setValidationQuery("SELECT 1 FROM RDB$DATABASE");

        // Create a connection pool using factory method
        auto pool = cpp_dbc::Firebird::FirebirdConnectionPool::create(poolConfigLocal);

        // Create a test table (drop first if exists using exception handling)
        auto conn = pool->getRelationalDBConnection();
        try
        {
            conn->executeUpdate(dropTableQuery);
        }
        catch (const cpp_dbc::DBException &)
        {
            // Table might not exist, ignore error
        }
        conn->executeUpdate(createTableQuery);
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

            // Poll for expected state after getting first connection
            auto startTime = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::milliseconds(5000);
            bool stateReached = false;
            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getActiveDBConnectionCount() >= 1 &&
                    pool->getIdleDBConnectionCount() <= initialIdleCount)
                {
                    stateReached = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            REQUIRE(stateReached);

            // Get another connection
            auto conn2 = pool->getDBConnection();
            REQUIRE(conn2 != nullptr);

            // Poll for expected state after getting second connection
            startTime = std::chrono::steady_clock::now();
            stateReached = false;
            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getActiveDBConnectionCount() >= 2 &&
                    pool->getIdleDBConnectionCount() <= initialIdleCount)
                {
                    stateReached = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            REQUIRE(stateReached);

            // Return the first connection
            conn1->close();

            // Poll for expected state after returning first connection
            startTime = std::chrono::steady_clock::now();
            stateReached = false;
            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getActiveDBConnectionCount() <= 1)
                {
                    stateReached = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            REQUIRE(stateReached);

            // Return the second connection
            conn2->close();

            // Poll for expected state after returning all connections
            startTime = std::chrono::steady_clock::now();
            stateReached = false;
            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getActiveDBConnectionCount() == 0 &&
                    pool->getIdleDBConnectionCount() >= 3)
                {
                    stateReached = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            REQUIRE(stateReached);
        }

        // Clean up
        auto cleanupConn = pool->getRelationalDBConnection();
        try
        {
            cleanupConn->executeUpdate(dropTableQuery);
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
        poolConfigLocal.setValidationQuery("SELECT 1 FROM RDB$DATABASE");

        // Create a connection pool
        auto pool = cpp_dbc::Firebird::FirebirdConnectionPool::create(poolConfigLocal);

        // Test connection validation
        SECTION("Connection validation")
        {
            auto conn = pool->getRelationalDBConnection();
            REQUIRE(conn != nullptr);

            auto rs = conn->executeQuery("SELECT 1 FROM RDB$DATABASE");
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

            std::vector<std::shared_ptr<cpp_dbc::RelationalDBConnection>> connections;

            const size_t numConnectionsToRequest = initialTotalCount + 2;
            for (size_t i = 0; i < numConnectionsToRequest; i++)
            {
                connections.push_back(pool->getRelationalDBConnection());
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

            auto conn = pool->getRelationalDBConnection();
            REQUIRE(conn != nullptr);

            auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::RelationalPooledDBConnection>(conn);
            REQUIRE(pooledConn != nullptr);

            auto underlyingConn = pooledConn->getUnderlyingRelationalConnection();
            REQUIRE(underlyingConn != nullptr);

            // Close the underlying connection directly - this invalidates the pooled connection
            underlyingConn->close();

            // Return the invalid connection to the pool
            conn->close();

            // Poll for the pool to process the replacement instead of fixed sleep
            auto startTime = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::milliseconds(5000);
            bool poolStateConverged = false;

            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getActiveDBConnectionCount() == 0 &&
                    pool->getTotalDBConnectionCount() >= initialTotalCount &&
                    pool->getIdleDBConnectionCount() >= 3)
                {
                    poolStateConverged = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            REQUIRE(poolStateConverged);
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getTotalDBConnectionCount() >= initialTotalCount);
            REQUIRE(pool->getIdleDBConnectionCount() >= 3);

            // Verify replacement connection works
            auto newConn = pool->getRelationalDBConnection();
            REQUIRE(newConn != nullptr);
            auto rs = newConn->executeQuery("SELECT 1 FROM RDB$DATABASE");
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
            REQUIRE(initialIdleCount >= 3);

            std::vector<std::shared_ptr<cpp_dbc::RelationalDBConnection>> connections;
            const size_t numConnections = 3;

            for (size_t i = 0; i < numConnections; i++)
            {
                auto conn = pool->getRelationalDBConnection();
                REQUIRE(conn != nullptr);
                connections.push_back(conn);
            }

            // Invalidate all connections
            for (auto &conn : connections)
            {
                auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::RelationalPooledDBConnection>(conn);
                REQUIRE(pooledConn != nullptr);
                auto underlyingConn = pooledConn->getUnderlyingRelationalConnection();
                REQUIRE(underlyingConn != nullptr);
                underlyingConn->close();
            }

            // Return all invalid connections
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
                    pool->getTotalDBConnectionCount() >= initialTotalCount &&
                    pool->getIdleDBConnectionCount() >= 3)
                {
                    poolStateConverged = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            REQUIRE(poolStateConverged);
            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getTotalDBConnectionCount() >= initialTotalCount);
            REQUIRE(pool->getIdleDBConnectionCount() >= 3);

            // Verify all replacement connections work
            for (size_t i = 0; i < numConnections; i++)
            {
                auto newConn = pool->getRelationalDBConnection();
                REQUIRE(newConn != nullptr);
                auto rs = newConn->executeQuery("SELECT 1 FROM RDB$DATABASE");
                REQUIRE(rs != nullptr);
                REQUIRE(rs->next());
                newConn->close();
            }
        }

        // Test concurrent connections under load
        SECTION("Connection pool under load")
        {
            // Cap concurrent operations to pool's max size to avoid connection timeout issues
            const uint64_t numOperations = 10; // matches poolConfigLocal.setMaxSize(10)
            std::atomic<int> successCount(0);
            std::atomic<int> failureCount(0);
            std::vector<std::thread> threads;

            for (uint64_t i = 0; i < numOperations; i++)
            {
                threads.push_back(std::thread([&pool, &successCount, &failureCount, i]()
                                              {
                    try {
                        auto loadConn = pool->getRelationalDBConnection();
                        if (!loadConn)
                        {
                            failureCount++;
                            return;
                        }

                        auto rs = loadConn->executeQuery("SELECT 1 FROM RDB$DATABASE");
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
                        std::cerr << "Load operation " << i << " error: " << ex.what() << std::endl;
                    } }));
            }

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
