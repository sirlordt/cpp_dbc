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

 @file 22_141_test_sqlite_real_connection_pool.cpp
 @brief Tests for SQLite connection pool

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
#include <cpp_dbc/pool/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "22_001_test_sqlite_real_common.hpp"

#if USE_SQLITE

// Test case for real SQLite connection pool
TEST_CASE("Real SQLite connection pool tests", "[22_141_01_sqlite_real_connection_pool]")
{
    // Skip these tests if we can't connect to SQLite
    if (!sqlite_test_helpers::canConnectToSQLite())
    {
        SKIP("Cannot connect to SQLite database");
        return;
    }

    // Get SQLite configuration using the helper function
    auto dbConfig = sqlite_test_helpers::getSQLiteConfig("dev_sqlite");

    // Create connection parameters
    std::string type = dbConfig.getType();
    std::string database = dbConfig.getDatabase();
    std::string username = ""; // SQLite doesn't use username
    std::string password = ""; // SQLite doesn't use password

    std::string connStr = dbConfig.createConnectionString();

    // Get test queries from config
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT, value REAL)");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_table (id, name, value) VALUES (1, 'Test', 1.5)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_table");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE IF EXISTS test_table");

    // Create a connection pool configuration
    cpp_dbc::config::DBConnectionPoolConfig poolConfig;
    poolConfig.setUri(connStr);
    poolConfig.setUsername(username);
    poolConfig.setPassword(password);

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
    // Load the configuration using DatabaseConfigManager
    std::string config_path = common_test_helpers::getConfigFilePath();
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

    // Load the SQLite pool configuration from DatabaseConfigManager
    auto poolConfigOpt = configManager.getDBConnectionPoolConfig("sqlite_pool");
    if (poolConfigOpt.has_value())
    {
        const cpp_dbc::config::DBConnectionPoolConfig &poolCfg = poolConfigOpt.value().get();

        // Set pool parameters from the configuration
        poolConfig.setInitialSize(poolCfg.getInitialSize());
        poolConfig.setMaxSize(poolCfg.getMaxSize());
        poolConfig.setMinIdle(5); // Default value
        poolConfig.setConnectionTimeout(poolCfg.getConnectionTimeout());
        poolConfig.setValidationInterval(poolCfg.getValidationInterval());
        poolConfig.setIdleTimeout(poolCfg.getIdleTimeout());
    }
#endif

    SECTION("Basic connection pool operations")
    {
        // Create local pool config based on base config
        cpp_dbc::config::DBConnectionPoolConfig poolConfigLocal = poolConfig;
        poolConfigLocal.setInitialSize(5);
        poolConfigLocal.setMaxSize(10);
        poolConfigLocal.setMinIdle(5);
        poolConfigLocal.setConnectionTimeout(5000);
        poolConfigLocal.setValidationInterval(1000);
        poolConfigLocal.setIdleTimeout(30000);
        poolConfigLocal.setMaxLifetimeMillis(60000); // Default value
        poolConfigLocal.setTestOnBorrow(true);
        poolConfigLocal.setTestOnReturn(false);

        // Set the transaction isolation level to SERIALIZABLE for SQLite
        poolConfigLocal.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

        // Create a connection pool using factory method
        auto poolResult = cpp_dbc::SQLite::SQLiteConnectionPool::create(std::nothrow, poolConfigLocal);
        if (!poolResult.has_value())
        {
            throw poolResult.error();
        }
        auto pool = poolResult.value();

        // Create a test table
        auto conn = pool->getRelationalDBConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
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
        auto cleanupConn = pool->getRelationalDBConnection();
        cleanupConn->executeUpdate(dropTableQuery);
        cleanupConn->close();

        // Close the pool
        pool->close();
    }

    // Test advanced pool features
    SECTION("Advanced pool features")
    {
        // Create a connection pool configuration with testOnReturn enabled
        cpp_dbc::config::DBConnectionPoolConfig poolConfigLocal;
        poolConfigLocal.setUri(connStr);
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
        poolConfigLocal.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

        // Create a connection pool
        auto poolResult2 = cpp_dbc::SQLite::SQLiteConnectionPool::create(std::nothrow, poolConfigLocal);
        if (!poolResult2.has_value())
        {
            throw poolResult2.error();
        }
        auto pool = poolResult2.value();

        // Test connection validation
        SECTION("Connection validation")
        {
            auto conn = pool->getRelationalDBConnection();
            REQUIRE(conn != nullptr);

            auto rs = conn->executeQuery("SELECT 1");
            REQUIRE(rs != nullptr);
            REQUIRE(rs->next());

            // RESOURCE MANAGEMENT FIX (2026-02-15): Close result set after use
            // to prevent resource leaks and file locks
            rs->close();

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
            REQUIRE(pool->getActiveDBConnectionCount() == 1);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount - 1);

            auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::RelationalPooledDBConnection>(conn);
            REQUIRE(pooledConn != nullptr);

            auto underlyingConn = pooledConn->getUnderlyingConnection(std::nothrow);
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
            auto newConn = pool->getRelationalDBConnection();
            REQUIRE(newConn != nullptr);
            auto rs = newConn->executeQuery("SELECT 1");
            REQUIRE(rs != nullptr);
            REQUIRE(rs->next());

            // RESOURCE MANAGEMENT FIX (2026-02-15): Close result set after use
            // to prevent resource leaks and file locks
            rs->close();

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

            REQUIRE(pool->getActiveDBConnectionCount() == numConnections);
            REQUIRE(pool->getIdleDBConnectionCount() == (initialIdleCount - numConnections));

            // Invalidate all connections
            for (auto &conn : connections)
            {
                auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::RelationalPooledDBConnection>(conn);
                REQUIRE(pooledConn != nullptr);
                auto underlyingConn = pooledConn->getUnderlyingConnection(std::nothrow);
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
            const auto timeout = std::chrono::milliseconds(10000);
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
                auto newConn = pool->getRelationalDBConnection();
                REQUIRE(newConn != nullptr);
                auto rs = newConn->executeQuery("SELECT 1");
                REQUIRE(rs != nullptr);
                REQUIRE(rs->next());

                // RESOURCE MANAGEMENT FIX (2026-02-15): Close result set after use
                // to prevent resource leaks and file locks
                rs->close();

                newConn->close();
            }
        }

        // Test concurrent connections under load
        // Note: SQLite has limited concurrency support, so we use fewer operations
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
                        auto loadConn = pool->getRelationalDBConnection();
                        if (!loadConn)
                        {
                            failureCount++;
                            return;
                        }

                        auto rs = loadConn->executeQuery("SELECT 1");
                        if (!rs || !rs->next())
                        {
                            failureCount++;
                            if (rs) {
                                rs->close();
                            }
                            loadConn->close();
                            return;
                        }

                        // RESOURCE MANAGEMENT FIX (2026-02-15): Close result set after use
                        // to prevent resource leaks and file locks
                        rs->close();

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

            // Thread-safe assertions on main thread
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
            REQUIRE(idleCount >= 3);
            REQUIRE(idleCount <= 10);
        }

        pool->close();
        REQUIRE_FALSE(pool->isRunning());
    }

    // ========================================================================
    // Runtime pool configuration tests
    // ========================================================================
    SECTION("Runtime pool configuration")
    {
        cpp_dbc::config::DBConnectionPoolConfig poolConfigLocal;
        poolConfigLocal.setUri(connStr);
        poolConfigLocal.setUsername(username);
        poolConfigLocal.setPassword(password);
        poolConfigLocal.setInitialSize(3);
        poolConfigLocal.setMaxSize(5);
        poolConfigLocal.setMinIdle(2);
        poolConfigLocal.setConnectionTimeout(3500);
        poolConfigLocal.setIdleTimeout(30000);
        poolConfigLocal.setMaxLifetimeMillis(60000);
        poolConfigLocal.setTestOnBorrow(true);
        poolConfigLocal.setTestOnReturn(false);
        poolConfigLocal.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

        auto poolResult = cpp_dbc::SQLite::SQLiteConnectionPool::create(std::nothrow, poolConfigLocal);
        REQUIRE(poolResult.has_value());
        auto pool = poolResult.value();

        SECTION("setConnectionTimeout changes pool default timeout")
        {
            REQUIRE(pool->getConnectionTimeout(std::nothrow) == 3500);

            pool->setConnectionTimeout(std::nothrow, 200);
            REQUIRE(pool->getConnectionTimeout(std::nothrow) == 200);

            std::vector<std::shared_ptr<cpp_dbc::RelationalDBConnection>> conns;
            for (size_t i = 0; i < 5; ++i)
            {
                auto c = pool->getRelationalDBConnection();
                REQUIRE(c != nullptr);
                conns.push_back(c);
            }
            REQUIRE(pool->getActiveDBConnectionCount() == 5);

            // Call without explicit timeout — must use pool default (200ms)
            auto start = std::chrono::steady_clock::now();
            auto result = pool->getRelationalDBConnection(std::nothrow);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            REQUIRE_FALSE(result.has_value());
            REQUIRE(elapsed.count() >= 150);
            REQUIRE(elapsed.count() < 2000);

            for (auto &c : conns)
            {
                c->close();
            }
        }

        SECTION("setIdleTimeout and setMaxLifetimeMillis getters")
        {
            REQUIRE(pool->getIdleTimeout(std::nothrow) == 30000);
            REQUIRE(pool->getMaxLifetimeMillis(std::nothrow) == 60000);

            pool->setIdleTimeout(std::nothrow, 5000);
            pool->setMaxLifetimeMillis(std::nothrow, 15000);

            REQUIRE(pool->getIdleTimeout(std::nothrow) == 5000);
            REQUIRE(pool->getMaxLifetimeMillis(std::nothrow) == 15000);
        }

        SECTION("setTestOnBorrow and setTestOnReturn toggle validation")
        {
            REQUIRE(pool->getTestOnBorrow(std::nothrow) == true);
            REQUIRE(pool->getTestOnReturn(std::nothrow) == false);

            pool->setTestOnBorrow(std::nothrow, false);
            pool->setTestOnReturn(std::nothrow, true);

            REQUIRE(pool->getTestOnBorrow(std::nothrow) == false);
            REQUIRE(pool->getTestOnReturn(std::nothrow) == true);

            auto conn = pool->getRelationalDBConnection();
            REQUIRE(conn != nullptr);
            auto rs = conn->executeQuery("SELECT 1");
            REQUIRE(rs != nullptr);
            REQUIRE(rs->next());
            rs->close();
            conn->close();

            pool->setTestOnBorrow(std::nothrow, true);
            pool->setTestOnReturn(std::nothrow, false);

            REQUIRE(pool->getTestOnBorrow(std::nothrow) == true);
            REQUIRE(pool->getTestOnReturn(std::nothrow) == false);
        }

        SECTION("setMaxSize grows and shrinks pool")
        {
            REQUIRE(pool->getMaxSize(std::nothrow) == 5);

            auto growResult = pool->setMaxSize(std::nothrow, 8);
            REQUIRE(growResult.has_value());
            REQUIRE(pool->getMaxSize(std::nothrow) == 8);

            std::vector<std::shared_ptr<cpp_dbc::RelationalDBConnection>> conns;
            for (size_t i = 0; i < 7; ++i)
            {
                auto c = pool->getRelationalDBConnection();
                REQUIRE(c != nullptr);
                conns.push_back(c);
            }
            REQUIRE(pool->getActiveDBConnectionCount() == 7);

            for (auto &c : conns)
            {
                c->close();
            }
            conns.clear();

            auto shrinkResult = pool->setMaxSize(std::nothrow, 3);
            REQUIRE(shrinkResult.has_value());
            REQUIRE(pool->getMaxSize(std::nothrow) == 3);

            // Poll for convergence (30s for Helgrind compatibility)
            auto startTime = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::milliseconds(30000);
            bool converged = false;

            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getTotalDBConnectionCount() <= 3)
                {
                    converged = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            REQUIRE(converged);
            REQUIRE(pool->getTotalDBConnectionCount() <= 3);

            auto zeroResult = pool->setMaxSize(std::nothrow, 0);
            REQUIRE_FALSE(zeroResult.has_value());
        }

        SECTION("setMinIdle adjusts minimum idle connections")
        {
            REQUIRE(pool->getMinIdle(std::nothrow) == 2);

            auto result = pool->setMinIdle(std::nothrow, 4);
            REQUIRE(result.has_value());
            REQUIRE(pool->getMinIdle(std::nothrow) == 4);

            // Poll for maintenance thread to replenish (60s for Helgrind compatibility)
            auto startTime = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::milliseconds(60000);
            bool replenished = false;

            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getIdleDBConnectionCount() >= 4)
                {
                    replenished = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            REQUIRE(replenished);
            REQUIRE(pool->getIdleDBConnectionCount() >= 4);

            auto result2 = pool->setMinIdle(std::nothrow, 1);
            REQUIRE(result2.has_value());
            REQUIRE(pool->getMinIdle(std::nothrow) == 1);

            auto badResult = pool->setMinIdle(std::nothrow, pool->getMaxSize(std::nothrow) + 1);
            REQUIRE_FALSE(badResult.has_value());
        }

        SECTION("setMaxSize shrinks while connections are active")
        {
            // Borrow all 5 connections
            std::vector<std::shared_ptr<cpp_dbc::RelationalDBConnection>> conns;
            for (size_t i = 0; i < 5; ++i)
            {
                auto c = pool->getRelationalDBConnection();
                REQUIRE(c != nullptr);
                conns.push_back(c);
            }
            REQUIRE(pool->getActiveDBConnectionCount() == 5);

            // Shrink maxSize to 2 while all 5 are borrowed
            auto shrinkResult = pool->setMaxSize(std::nothrow, 2);
            REQUIRE(shrinkResult.has_value());
            REQUIRE(pool->getMaxSize(std::nothrow) == 2);

            // Active connections must still work
            for (auto &c : conns)
            {
                auto pingResult = c->ping(std::nothrow);
                REQUIRE(pingResult.has_value());
                REQUIRE(pingResult.value());
            }

            // Return all connections — they go back to idle (returnConnection
            // does not check maxSize for valid connections)
            for (auto &c : conns)
            {
                c->close();
            }
            conns.clear();

            // Re-apply setMaxSize to force immediate eviction of excess idle
            // connections. Without this, eviction depends on the maintenance
            // thread cycle (~30s), which is too slow under Helgrind.
            auto reapplyResult = pool->setMaxSize(std::nothrow, 2);
            REQUIRE(reapplyResult.has_value());

            // Poll for convergence (30s for Helgrind)
            auto startTime = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::milliseconds(30000);
            bool converged = false;

            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getTotalDBConnectionCount() <= 2)
                {
                    converged = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            REQUIRE(converged);
            REQUIRE(pool->getTotalDBConnectionCount() <= 2);

            // Pool still functional after shrink
            auto c = pool->getRelationalDBConnection();
            REQUIRE(c != nullptr);
            auto pingResult = c->ping(std::nothrow);
            REQUIRE(pingResult.has_value());
            REQUIRE(pingResult.value());
            c->close();
        }

        SECTION("setMaxSize auto-adjusts minIdle when shrinking below it")
        {
            // Set minIdle to 4 first
            auto setResult = pool->setMinIdle(std::nothrow, 4);
            REQUIRE(setResult.has_value());
            REQUIRE(pool->getMinIdle(std::nothrow) == 4);

            // Poll for replenishment (60s for Helgrind)
            auto startTime = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::milliseconds(60000);
            bool replenished = false;

            while (std::chrono::steady_clock::now() - startTime < timeout)
            {
                if (pool->getIdleDBConnectionCount() >= 4)
                {
                    replenished = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            REQUIRE(replenished);

            // Shrink maxSize to 2 — must auto-adjust minIdle from 4 to 2
            auto shrinkResult = pool->setMaxSize(std::nothrow, 2);
            REQUIRE(shrinkResult.has_value());
            REQUIRE(pool->getMaxSize(std::nothrow) == 2);
            REQUIRE(pool->getMinIdle(std::nothrow) == 2);

            // Poll for convergence (30s for Helgrind)
            auto startTime2 = std::chrono::steady_clock::now();
            const auto timeout2 = std::chrono::milliseconds(30000);
            bool converged = false;

            while (std::chrono::steady_clock::now() - startTime2 < timeout2)
            {
                if (pool->getTotalDBConnectionCount() <= 2)
                {
                    converged = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            REQUIRE(converged);
            REQUIRE(pool->getTotalDBConnectionCount() <= 2);

            // Pool still functional
            auto c = pool->getRelationalDBConnection();
            REQUIRE(c != nullptr);
            auto pingResult = c->ping(std::nothrow);
            REQUIRE(pingResult.has_value());
            REQUIRE(pingResult.value());
            c->close();
        }

        SECTION("Per-call timeout via getRelationalDBConnection(timeoutMs)")
        {
            std::vector<std::shared_ptr<cpp_dbc::RelationalDBConnection>> conns;
            for (size_t i = 0; i < 5; ++i)
            {
                auto c = pool->getRelationalDBConnection();
                REQUIRE(c != nullptr);
                conns.push_back(c);
            }
            REQUIRE(pool->getActiveDBConnectionCount() == 5);

            auto start = std::chrono::steady_clock::now();
            auto result = pool->getRelationalDBConnection(std::nothrow, 200);
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            REQUIRE_FALSE(result.has_value());
            REQUIRE(elapsed.count() >= 150);
            REQUIRE(elapsed.count() < 2000);

            std::thread releaser([&conns]()
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                conns.back()->close();
                conns.pop_back();
            });

            auto start2 = std::chrono::steady_clock::now();
            auto result2 = pool->getRelationalDBConnection(std::nothrow, 3000);
            auto elapsed2 = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start2);

            releaser.join();

            REQUIRE(result2.has_value());
            REQUIRE(elapsed2.count() >= 50);
            REQUIRE(elapsed2.count() < 2500);

            result2.value()->close();

            for (auto &c : conns)
            {
                c->close();
            }
        }

        SECTION("Connection pool under load with runtime timeout")
        {
            pool->setConnectionTimeout(std::nothrow, 6000);

            const uint64_t numOperations = 30;
            std::atomic<int> successCount(0);
            std::atomic<int> failureCount(0);
            std::vector<std::thread> threads;

            for (uint64_t i = 0; i < numOperations; i++)
            {
                threads.push_back(std::thread([&pool, &successCount, &failureCount, i]()
                {
                    try
                    {
                        auto loadConn = pool->getRelationalDBConnection(6000);
                        if (!loadConn)
                        {
                            failureCount++;
                            return;
                        }

                        auto rs = loadConn->executeQuery("SELECT 1");
                        if (!rs || !rs->next())
                        {
                            failureCount++;
                            if (rs)
                            {
                                rs->close();
                            }
                            loadConn->close();
                            return;
                        }

                        rs->close();
                        std::this_thread::sleep_for(std::chrono::milliseconds(10 + (i % 10)));
                        loadConn->close();
                        successCount++;
                    }
                    catch (const std::exception &ex)
                    {
                        failureCount++;
                        cpp_dbc::system_utils::logWithTimesMillis("TEST",
                            "Load operation " + std::to_string(i) + " error: " + std::string(ex.what()));
                    }
                }));
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
        }

        pool->close();
        REQUIRE_FALSE(pool->isRunning());
    }
}
#endif
