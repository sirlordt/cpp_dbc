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
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
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
    poolConfig.setUrl(connStr);
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
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(10);
        poolConfig.setMinIdle(5);
        poolConfig.setConnectionTimeout(5000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setIdleTimeout(30000);
        poolConfig.setMaxLifetimeMillis(60000); // Default value
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Set the transaction isolation level to SERIALIZABLE for SQLite
        poolConfig.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

        // Create a connection pool using factory method
        auto pool = cpp_dbc::SQLite::SQLiteConnectionPool::create(poolConfig);

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
        poolConfigLocal.setValidationQuery("SELECT 1");
        poolConfigLocal.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

        // Create a connection pool
        auto pool = cpp_dbc::SQLite::SQLiteConnectionPool::create(poolConfigLocal);

        // Test connection validation
        SECTION("Connection validation")
        {
            auto conn = pool->getRelationalDBConnection();
            REQUIRE(conn != nullptr);

            auto rs = conn->executeQuery("SELECT 1");
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
            REQUIRE(pool->getActiveDBConnectionCount() == 1);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount - 1);

            auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::RelationalPooledDBConnection>(conn);
            REQUIRE(pooledConn != nullptr);

            auto underlyingConn = pooledConn->getUnderlyingRelationalConnection();
            REQUIRE(underlyingConn != nullptr);

            // Close the underlying connection directly - this invalidates the pooled connection
            underlyingConn->close();

            // Return the invalid connection to the pool
            conn->close();

            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            REQUIRE(pool->getActiveDBConnectionCount() == 0);
            REQUIRE(pool->getTotalDBConnectionCount() == initialTotalCount);
            REQUIRE(pool->getIdleDBConnectionCount() == initialIdleCount);

            // Verify replacement connection works
            auto newConn = pool->getRelationalDBConnection();
            REQUIRE(newConn != nullptr);
            auto rs = newConn->executeQuery("SELECT 1");
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

            REQUIRE(pool->getActiveDBConnectionCount() == numConnections);
            REQUIRE(pool->getIdleDBConnectionCount() == (initialIdleCount - numConnections));

            // Invalidate all connections
            for (auto &conn : connections)
            {
                auto pooledConn = std::dynamic_pointer_cast<cpp_dbc::RelationalPooledDBConnection>(conn);
                REQUIRE(pooledConn != nullptr);
                auto underlyingConn = pooledConn->getUnderlyingRelationalConnection();
                underlyingConn->close();
            }

            // Return all invalid connections
            for (auto &conn : connections)
            {
                conn->close();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));

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
                newConn->close();
            }
        }

        // Test concurrent connections under load
        // Note: SQLite has limited concurrency support, so we use fewer operations
        SECTION("Connection pool under load")
        {
            const uint64_t numOperations = 20; // Fewer operations for SQLite
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
