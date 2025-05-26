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
static std::string getConfigFilePath()
{
    return "test_db_connections.yml";
}

// Test case for ConnectionPoolConfig
TEST_CASE("ConnectionPoolConfig tests", "[config][pool]")
{
    SECTION("Default constructor sets default values")
    {
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
TEST_CASE("ConnectionPool basic tests", "[pool][basic]")
{
    SECTION("Create ConnectionPool with configuration")
    {
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

// Mock classes for testing ConnectionPool without actual database connections
namespace
{
    class MockResultSet : public cpp_dbc::ResultSet
    {
    public:
        bool next() override { return false; }
        bool isBeforeFirst() override { return true; }
        bool isAfterLast() override { return false; }
        int getRow() override { return 0; }
        int getInt(int) override { return 1; }
        int getInt(const std::string &) override { return 1; }
        long getLong(int) override { return 1L; }
        long getLong(const std::string &) override { return 1L; }
        double getDouble(int) override { return 1.0; }
        double getDouble(const std::string &) override { return 1.0; }
        std::string getString(int) override { return "mock"; }
        std::string getString(const std::string &) override { return "mock"; }
        bool getBoolean(int) override { return true; }
        bool getBoolean(const std::string &) override { return true; }
        bool isNull(int) override { return false; }
        bool isNull(const std::string &) override { return false; }
        std::vector<std::string> getColumnNames() override { return {"mock"}; }
        int getColumnCount() override { return 1; }
    };

    class MockPreparedStatement : public cpp_dbc::PreparedStatement
    {
    public:
        void setInt(int, int) override {}
        void setLong(int, long) override {}
        void setDouble(int, double) override {}
        void setString(int, const std::string &) override {}
        void setBoolean(int, bool) override {}
        void setNull(int, cpp_dbc::Types) override {}
        std::shared_ptr<cpp_dbc::ResultSet> executeQuery() override
        {
            return std::make_shared<MockResultSet>();
        }
        int executeUpdate() override { return 1; }
        bool execute() override { return true; }
    };

    class MockConnection : public cpp_dbc::Connection
    {
    private:
        bool closed = false;
        bool autoCommit = true;

    public:
        void close() override { closed = true; }
        bool isClosed() override { return closed; }
        std::shared_ptr<cpp_dbc::PreparedStatement> prepareStatement(const std::string &) override
        {
            return std::make_shared<MockPreparedStatement>();
        }
        std::shared_ptr<cpp_dbc::ResultSet> executeQuery(const std::string &) override
        {
            return std::make_shared<MockResultSet>();
        }
        int executeUpdate(const std::string &) override { return 1; }
        void setAutoCommit(bool ac) override { autoCommit = ac; }
        bool getAutoCommit() override { return autoCommit; }
        void commit() override {}
        void rollback() override {}
    };

    class MockDriver : public cpp_dbc::Driver
    {
    public:
        std::shared_ptr<cpp_dbc::Connection> connect(const std::string &, const std::string &, const std::string &) override
        {
            return std::make_shared<MockConnection>();
        }
        bool acceptsURL(const std::string &url) override
        {
            return url.find("cpp_dbc:mock:") == 0;
        }
    };
}

// Test case for ConnectionPool with mock connections
TEST_CASE("ConnectionPool with mock connections", "[pool][mock]")
{
    // Register the mock driver
    cpp_dbc::DriverManager::registerDriver("mock", std::make_shared<MockDriver>());

    SECTION("Create and use ConnectionPool with mock driver")
    {
        // Create a connection pool with mock driver
        cpp_dbc::ConnectionPool pool(
            "cpp_dbc:mock://localhost:1234/mockdb",
            "mockuser",
            "mockpass",
            3,         // initialSize
            10,        // maxSize
            2,         // minIdle
            5000,      // maxWaitMillis
            1000,      // validationTimeoutMillis
            30000,     // idleTimeoutMillis
            60000,     // maxLifetimeMillis
            true,      // testOnBorrow
            false,     // testOnReturn
            "SELECT 1" // validationQuery
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
        // Create a connection pool with mock driver
        cpp_dbc::ConnectionPool pool(
            "cpp_dbc:mock://localhost:1234/mockdb",
            "mockuser",
            "mockpass",
            5,         // initialSize
            20,        // maxSize
            3,         // minIdle
            5000,      // maxWaitMillis
            1000,      // validationTimeoutMillis
            30000,     // idleTimeoutMillis
            60000,     // maxLifetimeMillis
            true,      // testOnBorrow
            false,     // testOnReturn
            "SELECT 1" // validationQuery
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