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

 @file test_integration.cpp
 @brief Implementation for the cpp_dbc library

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
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "test_mysql_common.hpp"
#include "test_postgresql_common.hpp"
#include "test_sqlite_common.hpp"
#include "test_firebird_common.hpp"

#include "test_mocks.hpp"

// Integration test case
TEST_CASE("Integration test with mock database", "[integration]")
{
    // Create mock data for our tests
    std::vector<std::map<std::string, std::string>> userData = {
        {{"id", "1"}, {"name", "John"}, {"email", "john@example.com"}},
        {{"id", "2"}, {"name", "Jane"}, {"email", "jane@example.com"}},
        {{"id", "3"}, {"name", "Bob"}, {"email", "bob@example.com"}}};

    std::vector<std::string> userColumns = {"id", "name", "email"};

    // Create result set provider function (usado en el código comentado)
    [[maybe_unused]] auto createUserResultSet = [&userData, &userColumns](const std::string &sql) -> std::shared_ptr<cpp_dbc::RelationalDBResultSet>
    {
        // Simple SQL parsing to determine what to return
        if (sql.find("SELECT") != std::string::npos && sql.find("users") != std::string::npos)
        {
            return std::make_shared<cpp_dbc_test::MockResultSet>();
        }
        else if (sql == "SELECT 1")
        {
            return std::make_shared<cpp_dbc_test::MockResultSet>();
        }

        // Default empty result set
        return std::make_shared<cpp_dbc_test::MockResultSet>();
    };

    // Create update result provider function (usado en el código comentado)
    [[maybe_unused]] auto createUpdateResult = [](const std::string &sql) -> int
    {
        if (sql.find("INSERT") != std::string::npos)
        {
            return 1; // One row inserted
        }
        else if (sql.find("UPDATE") != std::string::npos)
        {
            return 2; // Two rows updated
        }
        else if (sql.find("DELETE") != std::string::npos)
        {
            return 3; // Three rows deleted
        }

        return 0; // Default: no rows affected
    };

    // Create prepared statement provider function (usado en el código comentado)
    [[maybe_unused]] auto createPreparedStatement = [&userData, &userColumns](const std::string &sql) -> std::shared_ptr<cpp_dbc::RelationalDBPreparedStatement>
    {
        auto queryProvider = [&userData, &userColumns, sql]() -> std::shared_ptr<cpp_dbc::RelationalDBResultSet>
        {
            if (sql.find("SELECT") != std::string::npos && sql.find("users") != std::string::npos)
            {
                return std::make_shared<cpp_dbc_test::MockResultSet>();
            }
            else if (sql == "SELECT 1")
            {
                return std::make_shared<cpp_dbc_test::MockResultSet>();
            }

            // Default empty result set
            return std::make_shared<cpp_dbc_test::MockResultSet>();
        };

        auto updateProvider = [sql]() -> int
        {
            if (sql.find("INSERT") != std::string::npos)
            {
                return 1; // One row inserted
            }
            else if (sql.find("UPDATE") != std::string::npos)
            {
                return 2; // Two rows updated
            }
            else if (sql.find("DELETE") != std::string::npos)
            {
                return 3; // Three rows deleted
            }

            return 0; // Default: no rows affected
        };

        return std::make_shared<cpp_dbc_test::MockPreparedStatement>();
    };

    // No necesitamos esta función, ya que usamos directamente el DriverManager
    // auto createDBConnection = [&createUserResultSet, &createUpdateResult, &createPreparedStatement](
    //                             const std::string & /*url*/, const std::string & /*user*/, const std::string & /*password*/) -> std::shared_ptr<cpp_dbc::Connection>
    // {
    //     return std::make_shared<cpp_dbc_test::MockConnection>();
    // };

    // Register the mock driver
    cpp_dbc::DriverManager::registerDriver("mock", std::make_shared<cpp_dbc_test::MockDriver>());

    SECTION("Integration test with direct connection")
    {
        // Get a connection directly from the driver manager
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(
                "cpp_dbc:mock://localhost:1234/mockdb",
                "mockuser",
                "mockpass"));

        // Check that we got a connection
        REQUIRE(conn != nullptr);

        // Execute a simple query
        auto rs = conn->executeQuery("SELECT 1");
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());
        REQUIRE(rs->getString("value") == "1");

        // Execute a query that returns user data
        rs = conn->executeQuery("SELECT * FROM users");
        REQUIRE(rs != nullptr);

        // Check that we can iterate through the result set
        int count = 0;
        while (rs->next())
        {
            count++;
            REQUIRE(rs->getInt("id") == count);

            if (count == 1)
            {
                REQUIRE(rs->getString("name") == "John");
                REQUIRE(rs->getString("email") == "john@example.com");
            }
            else if (count == 2)
            {
                REQUIRE(rs->getString("name") == "Jane");
                REQUIRE(rs->getString("email") == "jane@example.com");
            }
            else if (count == 3)
            {
                REQUIRE(rs->getString("name") == "Bob");
                REQUIRE(rs->getString("email") == "bob@example.com");
            }
        }

        REQUIRE(count == 3);

        // Test prepared statement
        auto stmt = conn->prepareStatement("SELECT * FROM users WHERE id = ?");
        REQUIRE(stmt != nullptr);

        stmt->setInt(1, 2);
        rs = stmt->executeQuery();
        REQUIRE(rs != nullptr);
        REQUIRE(rs->next());

        // Debug output to see what's actually being returned
        // WARN("DEBUG ID: " << rs->getInt("id") << ", Name: " << rs->getString("name") << std::endl;

        REQUIRE(rs->getString("name") == "John");

        // Test update
        auto updateCount = conn->executeUpdate("UPDATE users SET name = 'Updated' WHERE id = 1");
        REQUIRE(updateCount == 2);

        // Close the connection
        conn->close();
        REQUIRE(conn->isClosed());
    }

    SECTION("Integration test with connection pool")
    {
        // Create a connection pool using factory method
        auto pool = cpp_dbc::RelationalDBConnectionPool::create(
            "cpp_dbc:mock://localhost:1234/mockdb",
            "mockuser",
            "mockpass",
            std::map<std::string, std::string>(), // options
            3,                                    // initialSize
            10,                                   // maxSize
            2,                                    // minIdle
            5000,                                 // maxWaitMillis
            1000,                                 // validationTimeoutMillis
            30000,                                // idleTimeoutMillis
            60000,                                // maxLifetimeMillis
            true,                                 // testOnBorrow
            false,                                // testOnReturn
            "SELECT 1"                            // validationQuery
        );

        // Get a connection from the pool
        auto conn = pool->getRelationalDBConnection();
        REQUIRE(conn != nullptr);

        // Execute a query
        auto rs = conn->executeQuery("SELECT * FROM users");
        REQUIRE(rs != nullptr);

        // Check that we can iterate through the result set
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        REQUIRE(count == 3);

        // Return the connection to the pool
        conn->close();

        // Get multiple connections
        std::vector<std::shared_ptr<cpp_dbc::RelationalDBConnection>> connections;
        for (int i = 0; i < 5; i++)
        {
            connections.push_back(pool->getRelationalDBConnection());
        }

        REQUIRE(pool->getActiveDBConnectionCount() == 5);

        // Return connections to the pool
        for (auto &c : connections)
        {
            c->close();
        }

        REQUIRE(pool->getActiveDBConnectionCount() == 0);

        // Close the pool
        pool->close();
    }

    SECTION("Integration test with transaction manager")
    {
        // Create a connection pool using factory method
        auto pool = cpp_dbc::RelationalDBConnectionPool::create(
            "cpp_dbc:mock://localhost:1234/mockdb",
            "mockuser",
            "mockpass",
            std::map<std::string, std::string>(), // options
            3,                                    // initialSize
            10,                                   // maxSize
            2,                                    // minIdle
            5000,                                 // maxWaitMillis
            1000,                                 // validationTimeoutMillis
            30000,                                // idleTimeoutMillis
            60000,                                // maxLifetimeMillis
            true,                                 // testOnBorrow
            false,                                // testOnReturn
            "SELECT 1"                            // validationQuery
        );

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(*pool);

        // Begin a transaction
        std::string txId = manager.beginTransaction();
        REQUIRE(!txId.empty());

        // Get the connection associated with the transaction
        auto conn = manager.getTransactionDBConnection(txId);
        REQUIRE(conn != nullptr);

        // Execute a query within the transaction
        auto rs = conn->executeQuery("SELECT * FROM users");
        REQUIRE(rs != nullptr);

        // Check that we can iterate through the result set
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        REQUIRE(count == 3);

        // Execute an update within the transaction
        auto updateCount = conn->executeUpdate("UPDATE users SET name = 'Updated' WHERE id = 1");
        REQUIRE(updateCount == 2);

        // Commit the transaction
        manager.commitTransaction(txId);

        // Check that the transaction is no longer active
        REQUIRE_FALSE(manager.isTransactionActive(txId));

        // Begin another transaction
        txId = manager.beginTransaction();
        REQUIRE(!txId.empty());

        // Get the connection associated with the transaction
        conn = manager.getTransactionDBConnection(txId);
        REQUIRE(conn != nullptr);

        // Execute an update within the transaction
        updateCount = conn->executeUpdate("DELETE FROM users WHERE id = 3");
        REQUIRE(updateCount == 3);

        // Rollback the transaction
        manager.rollbackTransaction(txId);

        // Check that the transaction is no longer active
        REQUIRE_FALSE(manager.isTransactionActive(txId));

        INFO("Time after REQUIRE_FALSE: " << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    SECTION("Integration test with configuration")
    {
        INFO("Time at start of configuration test: " << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Create a database configuration
        cpp_dbc::config::DatabaseConfig dbConfig(
            "mock_db",
            "mock",
            "localhost",
            1234,
            "mockdb",
            "mockuser",
            "mockpass");

        // Set some options
        dbConfig.setOption("connect_timeout", "5");
        dbConfig.setOption("charset", "utf8mb4");

        // Create a connection string
        std::string connStr = dbConfig.createConnectionString();
        REQUIRE(connStr == "cpp_dbc:mock://localhost:1234/mockdb");

        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setName("test_pool");
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(10);
        poolConfig.withDatabaseConfig(dbConfig);

        REQUIRE(poolConfig.getUrl() == "cpp_dbc:mock://localhost:1234/mockdb");
        REQUIRE(poolConfig.getUsername() == "mockuser");
        REQUIRE(poolConfig.getPassword() == "mockpass");

        // Create a database config manager
        cpp_dbc::config::DatabaseConfigManager manager;
        manager.addDatabaseConfig(dbConfig);

        // Get the database configuration by name
        auto dbConfigOpt = manager.getDatabaseByName("mock_db");
        REQUIRE(dbConfigOpt.has_value());
        const auto &retrievedDbConfig = dbConfigOpt->get();
        REQUIRE(retrievedDbConfig.getName() == "mock_db");
        REQUIRE(retrievedDbConfig.getType() == "mock");

        // Add the connection pool configuration
        manager.addDBConnectionPoolConfig(poolConfig);

        // Get the connection pool configuration by name
        auto poolConfigOpt = manager.getDBConnectionPoolConfig("test_pool");
        REQUIRE(poolConfigOpt.has_value());
        const auto &retrievedPoolConfig = poolConfigOpt->get();
        REQUIRE(retrievedPoolConfig.getName() == "test_pool");
        REQUIRE(retrievedPoolConfig.getInitialSize() == 3);
        REQUIRE(retrievedPoolConfig.getMaxSize() == 10);
    }
}

// Test case for loading the actual test_db_connections.yml file
TEST_CASE("Load and use test_db_connections.yml", "[integration]")
{
    SECTION("Load test_db_connections.yml")
    {
        // Load the YAML configuration
        std::string config_path = common_test_helpers::getConfigFilePath();
        std::ifstream file(config_path);
        REQUIRE(file.good());
        file.close();

        // Check that the file exists and can be opened
        REQUIRE(std::ifstream(config_path).good());

        // We'll skip the actual YAML parsing here since it depends on the YAML-CPP library
        // which might not be available in all builds
    }
}

// Test case for real database integration with all available drivers
TEST_CASE("Real database integration with all drivers", "[integration][real]")
{
    // Register all available drivers
#if USE_MYSQL
    cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
    // std::cout << "MySQL driver registered" << std::endl;
#endif

#if USE_POSTGRESQL
    cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());
    // std::cout << "PostgreSQL driver registered" << std::endl;
#endif

#if USE_SQLITE
    cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());
    // std::cout << "SQLite driver registered" << std::endl;
#endif

#if USE_FIREBIRD
    cpp_dbc::DriverManager::registerDriver("firebird", std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());
    // std::cout << "Firebird driver registered" << std::endl;
#endif

    SECTION("Test connection to all available databases")
    {
#if USE_CPP_YAML
        // Load the configuration using DatabaseConfigManager
        std::string config_path = common_test_helpers::getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Get all database configurations
        const auto &allDatabases = configManager.getAllDatabases();

        for (const auto &dbConfig : allDatabases)
        {
            std::string name = dbConfig.getName();
            std::string type = dbConfig.getType();

            // Skip if the driver is not enabled
#if !USE_MYSQL
            if (type == "mysql")
                continue;
#endif
#if !USE_POSTGRESQL
            if (type == "postgresql")
                continue;
#endif
#if !USE_SQLITE
            if (type == "sqlite")
                continue;
#endif
#if !USE_FIREBIRD
            if (type == "firebird")
                continue;
#endif

            // std::cout << "Testing connection to " << name << " (" << type << ")" << std::endl;

            try
            {
                // Create connection string
                std::string connStr = dbConfig.createConnectionString();
                std::string username = (type == "sqlite") ? "" : dbConfig.getUsername();
                std::string password = (type == "sqlite") ? "" : dbConfig.getPassword();

                if (type == "sqlite")
                {
                    try
                    {
                        // std::cout << "DEBUG: Processing SQLite connection" << std::endl;
                        // std::cout << "DEBUG: SQLite database path: " << dbConfig.getDatabase() << std::endl;
                        // std::cout << "DEBUG: SQLite connection string: " << connStr << std::endl;

                        // Attempt to connect with extra debug info
                        // std::cout << "DEBUG: About to get SQLite connection" << std::endl;
                        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                        // std::cout << "DEBUG: SQLite connection obtained successfully" << std::endl;

                        // Execute a simple query to verify the connection
                        // std::cout << "DEBUG: About to execute SQLite query" << std::endl;
                        auto resultSet = conn->executeQuery("SELECT 1 as test_value");
                        // std::cout << "DEBUG: SQLite query executed successfully" << std::endl;

                        if (resultSet->next())
                        {
                            // std::cout << "DEBUG: SQLite result set has data" << std::endl;
                            // std::cout << "Connection to " << name << " successful" << std::endl;
                            REQUIRE(resultSet->getInt("test_value") == 1);
                        }

                        // Close the connection
                        // std::cout << "DEBUG: About to close SQLite connection" << std::endl;
                        conn->close();
                        // std::cout << "DEBUG: SQLite connection closed successfully" << std::endl;
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "DEBUG: SQLite exception: " << e.what() << std::endl;
                        throw; // Re-throw to be caught by the outer catch block
                    }
                    catch (...)
                    {
                        std::cerr << "DEBUG: Unknown SQLite exception occurred" << std::endl;
                        throw; // Re-throw to be caught by the outer catch block
                    }
                }
                else if (type == "firebird")
                {
                    try
                    {
                        // Attempt to connect
                        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

                        // Execute a simple query to verify the connection
                        // Firebird uses "SELECT 1 FROM RDB$DATABASE" for a simple test query
                        auto resultSet = conn->executeQuery("SELECT 1 as test_value FROM RDB$DATABASE");

                        if (resultSet->next())
                        {
                            // std::cout << "Connection to " << name << " successful" << std::endl;
                            // Firebird returns uppercase column names
                            REQUIRE(resultSet->getInt("TEST_VALUE") == 1);
                        }

                        // Close the connection
                        conn->close();
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "DEBUG: Firebird exception: " << e.what() << std::endl;
                        throw; // Re-throw to be caught by the outer catch block
                    }
                    catch (...)
                    {
                        std::cerr << "DEBUG: Unknown Firebird exception occurred" << std::endl;
                        throw; // Re-throw to be caught by the outer catch block
                    }
                }
                else
                {
                    // Attempt to connect
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

                    // Execute a simple query to verify the connection
                    auto resultSet = conn->executeQuery("SELECT 1 as test_value");
                    if (resultSet->next())
                    {
                        // std::cout << "Connection to " << name << " successful" << std::endl;
                        REQUIRE(resultSet->getInt("test_value") == 1);
                    }

                    // Close the connection
                    conn->close();
                }
            }
            catch (const cpp_dbc::DBException &e)
            {
                // std::cout << "Connection to " << name << " failed: " << e.what() << std::endl;
                // We'll just warn instead of failing the test, since the database might not be available
                WARN("Connection to " << name << " failed: " << e.what_s());
            }
        }
#else
        // Skip this test if YAML is not enabled
        SKIP("YAML support is not enabled, cannot load database configurations");
#endif
    }
}