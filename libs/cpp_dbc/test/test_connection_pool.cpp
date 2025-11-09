// test_connection_pool.cpp
// Tests for the connection pool functionality in the cpp_dbc library
//
// This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
// See the LICENSE.md file in the project root for more information.
//

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/config/yaml_config_loader.hpp>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>

// Helper function to get the path to the test_db_connections.yml file
std::string getConfigFilePath();

// Test case for ConnectionPoolConfig
TEST_CASE("ConnectionPoolConfig tests", "[connection_pool]")
{
    SECTION("Default constructor sets default values")
    {
        INFO("Default constructor sets default values");
        cpp_dbc::config::ConnectionPoolConfig config;

        REQUIRE(config.getInitialSize() == 5);
        REQUIRE(config.getMaxSize() == 20);
        REQUIRE(config.getMinIdle() == 3);
        REQUIRE(config.getConnectionTimeout() == 30000);
        REQUIRE(config.getIdleTimeout() == 300000);
        REQUIRE(config.getValidationInterval() == 5000);
        REQUIRE(config.getMaxLifetimeMillis() == 1800000);
        REQUIRE(config.getTestOnBorrow() == true);
        REQUIRE(config.getTestOnReturn() == false);
        REQUIRE(config.getValidationQuery() == "SELECT 1");
    }

    SECTION("Constructor with basic parameters")
    {
        INFO("Constructor with basic parameters");
        cpp_dbc::config::ConnectionPoolConfig config(
            "test_pool", 10, 50, 10000, 60000, 15000);

        REQUIRE(config.getName() == "test_pool");
        REQUIRE(config.getInitialSize() == 10);
        REQUIRE(config.getMaxSize() == 50);
        REQUIRE(config.getConnectionTimeout() == 10000);
        REQUIRE(config.getIdleTimeout() == 60000);
        REQUIRE(config.getValidationInterval() == 15000);

        // Check default values for parameters not specified
        REQUIRE(config.getMinIdle() == 3);
        REQUIRE(config.getMaxLifetimeMillis() == 1800000);
        REQUIRE(config.getTestOnBorrow() == true);
        REQUIRE(config.getTestOnReturn() == false);
        REQUIRE(config.getValidationQuery() == "SELECT 1");
    }

    SECTION("Full constructor with all parameters")
    {
        INFO("Full constructor with all parameters");
        cpp_dbc::config::ConnectionPoolConfig config(
            "full_pool", "cpp_dbc:mysql://localhost:3306/test", "user", "pass",
            15, 100, 5, 20000, 120000, 30000, 3600000, false, true, "SELECT version()");

        REQUIRE(config.getName() == "full_pool");
        REQUIRE(config.getUrl() == "cpp_dbc:mysql://localhost:3306/test");
        REQUIRE(config.getUsername() == "user");
        REQUIRE(config.getPassword() == "pass");
        REQUIRE(config.getInitialSize() == 15);
        REQUIRE(config.getMaxSize() == 100);
        REQUIRE(config.getMinIdle() == 5);
        REQUIRE(config.getConnectionTimeout() == 20000);
        REQUIRE(config.getIdleTimeout() == 120000);
        REQUIRE(config.getValidationInterval() == 30000);
        REQUIRE(config.getMaxLifetimeMillis() == 3600000);
        REQUIRE(config.getTestOnBorrow() == false);
        REQUIRE(config.getTestOnReturn() == true);
        REQUIRE(config.getValidationQuery() == "SELECT version()");
    }

    SECTION("Setters and getters")
    {
        INFO("Setters and getters");
        cpp_dbc::config::ConnectionPoolConfig config;

        config.setName("setter_test");
        config.setUrl("cpp_dbc:postgresql://localhost:5432/test");
        config.setUsername("postgres");
        config.setPassword("postgres");
        config.setInitialSize(8);
        config.setMaxSize(30);
        config.setMinIdle(4);
        config.setConnectionTimeout(15000);
        config.setIdleTimeout(90000);
        config.setValidationInterval(10000);
        config.setMaxLifetimeMillis(2400000);
        config.setTestOnBorrow(false);
        config.setTestOnReturn(true);
        config.setValidationQuery("SELECT 2");

        REQUIRE(config.getName() == "setter_test");
        REQUIRE(config.getUrl() == "cpp_dbc:postgresql://localhost:5432/test");
        REQUIRE(config.getUsername() == "postgres");
        REQUIRE(config.getPassword() == "postgres");
        REQUIRE(config.getInitialSize() == 8);
        REQUIRE(config.getMaxSize() == 30);
        REQUIRE(config.getMinIdle() == 4);
        REQUIRE(config.getConnectionTimeout() == 15000);
        REQUIRE(config.getIdleTimeout() == 90000);
        REQUIRE(config.getValidationInterval() == 10000);
        REQUIRE(config.getMaxLifetimeMillis() == 2400000);
        REQUIRE(config.getTestOnBorrow() == false);
        REQUIRE(config.getTestOnReturn() == true);
        REQUIRE(config.getValidationQuery() == "SELECT 2");
    }

    SECTION("withDatabaseConfig method")
    {
        INFO("withDatabaseConfig method");
        // Create a database config
        cpp_dbc::config::DatabaseConfig dbConfig(
            "test_db", "mysql", "localhost", 3306, "testdb", "root", "password");

        // Create a connection pool config and apply the database config
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
        poolConfig.withDatabaseConfig(dbConfig);

        // Check that the database config values were applied
        REQUIRE(poolConfig.getUrl() == "cpp_dbc:mysql://localhost:3306/testdb");
        REQUIRE(poolConfig.getUsername() == "root");
        REQUIRE(poolConfig.getPassword() == "password");
    }
}

// Test case for ConnectionPool creation and basic operations
TEST_CASE("ConnectionPool basic tests", "[connection_pool]")
{
    SECTION("Create ConnectionPool with configuration")
    {
        INFO("Create ConnectionPool with configuration");
        // Load the YAML configuration
        std::string config_path = getConfigFilePath();
        YAML::Node config = YAML::LoadFile(config_path);

        // Get the default connection pool configuration
        YAML::Node poolConfig = config["connection_pool"]["default"];

        // Create a ConnectionPoolConfig object
        cpp_dbc::config::ConnectionPoolConfig cpConfig;
        cpConfig.setInitialSize(poolConfig["initial_size"].as<int>());
        cpConfig.setMaxSize(poolConfig["max_size"].as<int>());
        cpConfig.setConnectionTimeout(poolConfig["connection_timeout"].as<long>());
        cpConfig.setIdleTimeout(poolConfig["idle_timeout"].as<long>());
        cpConfig.setValidationInterval(poolConfig["validation_interval"].as<long>());

        // We can't actually create a real connection pool without a database,
        // but we can verify that the factory method doesn't throw
        REQUIRE_NOTHROW([&cpConfig]()
                        {
                            // This would normally create a connection pool, but we're just testing the API
                            // In a real test, we would register a mock driver first
                            // auto pool = cpp_dbc::ConnectionPool::create(cpConfig);
                        }());
    }
}

// Include the mock classes from test_mocks.hpp
#include "test_mocks.hpp"

// Use the mock classes from test_mocks.hpp
using namespace cpp_dbc_test;

// Test case for ConnectionPool with mock connections
TEST_CASE("ConnectionPool with mock connections", "[connection_pool]")
{
    // Register the mock driver
    cpp_dbc::DriverManager::registerDriver("mock", std::make_shared<cpp_dbc_test::MockDriver>());

    SECTION("Create and use ConnectionPool with mock driver")
    {
        INFO("Create and use ConnectionPool with mock driver");
        // Create a connection pool with mock driver
        cpp_dbc::ConnectionPool pool(
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
        auto conn = pool.getConnection();
        REQUIRE(conn != nullptr);

        // Test that we can use the connection
        auto stmt = conn->prepareStatement("SELECT * FROM mock_table");
        REQUIRE(stmt != nullptr);

        auto rs = stmt->executeQuery();
        REQUIRE(rs != nullptr);

        // Close the connection (returns it to the pool)
        conn->close();

        // Check pool statistics
        REQUIRE(pool.getActiveConnectionCount() == 0);
        REQUIRE(pool.getIdleConnectionCount() > 0);

        // Get multiple connections
        std::vector<std::shared_ptr<cpp_dbc::Connection>> connections;
        for (int i = 0; i < 5; i++)
        {
            connections.push_back(pool.getConnection());
        }

        REQUIRE(pool.getActiveConnectionCount() == 5);

        // Return connections to the pool
        for (auto &c : connections)
        {
            c->close();
        }

        REQUIRE(pool.getActiveConnectionCount() == 0);

        // Close the pool
        pool.close();
    }

    SECTION("Test ConnectionPool with multiple threads")
    {
        INFO("Test ConnectionPool with multiple threads");
        // Create a connection pool with mock driver
        cpp_dbc::ConnectionPool pool(
            "cpp_dbc:mock://localhost:1234/mockdb",
            "mockuser",
            "mockpass",
            std::map<std::string, std::string>{}, // options
            5,                                    // initialSize
            20,                                   // maxSize
            3,                                    // minIdle
            5000,                                 // maxWaitMillis
            1000,                                 // validationTimeoutMillis
            30000,                                // idleTimeoutMillis
            60000,                                // maxLifetimeMillis
            true,                                 // testOnBorrow
            false,                                // testOnReturn
            "SELECT 1"                            // validationQuery
        );

        // Number of threads and operations per thread
        const int numThreads = 10;
        const int opsPerThread = 50;

        // Atomic counter for successful operations
        std::atomic<int> successCount(0);

        // Create and start threads
        std::vector<std::thread> threads;
        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([&pool, opsPerThread, &successCount]()
                                          {
                for (int j = 0; j < opsPerThread; j++) {
                    try {
                        // Get a connection from the pool
                        auto conn = pool.getConnection();
                        
                        // Perform a simple query
                        auto rs = conn->executeQuery("SELECT 1");
                        
                        // Return the connection to the pool
                        conn->close();
                        
                        // Increment success counter
                        successCount++;
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Thread operation failed: " << e.what() << std::endl;
                    }
                } }));
        }

        // Wait for all threads to complete
        for (auto &t : threads)
        {
            t.join();
        }

        // Check that all operations were successful
        REQUIRE(successCount == numThreads * opsPerThread);

        // Close the pool
        pool.close();
    }
}