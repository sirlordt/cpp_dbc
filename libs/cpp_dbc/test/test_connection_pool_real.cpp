// test_connection_pool_real.cpp
// Tests for connection pool with real database connections
//
// This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
// See the LICENSE.md file in the project root for more information.
//

#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/config/yaml_config_loader.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include "test_mysql_common.hpp"

std::string getConfigFilePath();

// Helper function to check if we can connect to PostgreSQL
static bool canConnectToPostgreSQL()
{
#if USE_POSTGRESQL
    try
    {
        // Load the YAML configuration
        std::string config_path = getConfigFilePath();
        YAML::Node config = YAML::LoadFile(config_path);

        // Find the dev_postgresql configuration
        YAML::Node dbConfig;
        for (size_t i = 0; i < config["databases"].size(); i++)
        {
            YAML::Node db = config["databases"][i];
            if (db["name"].as<std::string>() == "dev_postgresql")
            {
                dbConfig = YAML::Node(db);
                break;
            }
        }

        if (!dbConfig.IsDefined())
        {
            std::cerr << "PostgreSQL configuration not found in test_db_connections.yml" << std::endl;
            return false;
        }

        // Create connection string
        std::string type = dbConfig["type"].as<std::string>();
        std::string host = dbConfig["host"].as<std::string>();
        int port = dbConfig["port"].as<int>();
        std::string database = dbConfig["database"].as<std::string>();
        std::string username = dbConfig["username"].as<std::string>();
        std::string password = dbConfig["password"].as<std::string>();

        std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

        // Register the PostgreSQL driver
        cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

        // Attempt to connect to PostgreSQL
        std::cout << "Attempting to connect to PostgreSQL with connection string: " << connStr << std::endl;
        std::cout << "Username: " << username << ", Password: " << password << std::endl;

        auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

        // If we get here, the connection was successful
        std::cout << "PostgreSQL connection successful!" << std::endl;

        // Execute a simple query to verify the connection
        auto resultSet = conn->executeQuery("SELECT 1 as test_value");
        bool success = resultSet->next() && resultSet->getInt("test_value") == 1;

        // Close the connection
        conn->close();

        return success;
    }
    catch (const std::exception &e)
    {
        std::cerr << "PostgreSQL connection error: " << e.what() << std::endl;
        return false;
    }
#else
    std::cerr << "PostgreSQL support is not enabled" << std::endl;
    return false;
#endif
}

#if USE_MYSQL
// Test case for real MySQL connection pool
TEST_CASE("Real MySQL connection pool tests", "[mysql_connection_pool_real]")
{
    // Skip these tests if we can't connect to MySQL
    if (!mysql_test_helpers::canConnectToMySQL())
    {
        SKIP("Cannot connect to MySQL database");
        return;
    }

    // Load the YAML configuration
    std::string config_path = getConfigFilePath();
    YAML::Node config = YAML::LoadFile(config_path);

    // Find the dev_mysql configuration
    YAML::Node dbConfig;
    for (size_t i = 0; i < config["databases"].size(); i++)
    {
        YAML::Node db = config["databases"][i];
        if (db["name"].as<std::string>() == "dev_mysql")
        {
            dbConfig = YAML::Node(db);
            break;
        }
    }

    // Create connection parameters
    std::string type = dbConfig["type"].as<std::string>();
    std::string host = dbConfig["host"].as<std::string>();
    int port = dbConfig["port"].as<int>();
    std::string database = dbConfig["database"].as<std::string>();
    std::string username = dbConfig["username"].as<std::string>();
    std::string password = dbConfig["password"].as<std::string>();

    std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

    // Get test queries
    YAML::Node testQueries = config["test_queries"]["mysql"];
    std::string createTableQuery = testQueries["create_table"].as<std::string>();
    std::string insertDataQuery = testQueries["insert_data"].as<std::string>();
    std::string selectDataQuery = testQueries["select_data"].as<std::string>();
    std::string dropTableQuery = testQueries["drop_table"].as<std::string>();

    SECTION("Basic connection pool operations")
    {
        // Create a connection pool configuration
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
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
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool
        cpp_dbc::MySQL::MySQLConnectionPool pool(poolConfig);

        // Create a test table
        auto conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            int initialIdleCount = pool.getIdleConnectionCount();
            int initialActiveCount = pool.getActiveConnectionCount();
            int initialTotalCount = pool.getTotalConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool.getConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool.getConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 2);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool.getActiveConnectionCount() == 0);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount);
        }

        // Clean up
        auto cleanupConn = pool.getConnection();
        cleanupConn->executeUpdate(dropTableQuery);
        cleanupConn->close();

        // Close the pool
        pool.close();
    }
}
#endif

#if USE_POSTGRESQL
// Test case for real PostgreSQL connection pool
TEST_CASE("Real PostgreSQL connection pool tests", "[postgresql_connection_pool_real]")
{
    // Skip these tests if we can't connect to PostgreSQL
    if (!canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

    // Load the YAML configuration
    std::string config_path = getConfigFilePath();
    YAML::Node config = YAML::LoadFile(config_path);

    // Find the dev_postgresql configuration
    YAML::Node dbConfig;
    for (size_t i = 0; i < config["databases"].size(); i++)
    {
        YAML::Node db = config["databases"][i];
        if (db["name"].as<std::string>() == "dev_postgresql")
        {
            dbConfig = YAML::Node(db);
            break;
        }
    }

    // Create connection parameters
    std::string type = dbConfig["type"].as<std::string>();
    std::string host = dbConfig["host"].as<std::string>();
    int port = dbConfig["port"].as<int>();
    std::string database = dbConfig["database"].as<std::string>();
    std::string username = dbConfig["username"].as<std::string>();
    std::string password = dbConfig["password"].as<std::string>();

    std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

    // Get test queries
    YAML::Node testQueries = config["test_queries"]["postgresql"];
    std::string createTableQuery = testQueries["create_table"].as<std::string>();
    std::string insertDataQuery = testQueries["insert_data"].as<std::string>();
    std::string selectDataQuery = testQueries["select_data"].as<std::string>();
    std::string dropTableQuery = testQueries["drop_table"].as<std::string>();

    SECTION("Basic connection pool operations")
    {
        // Create a connection pool configuration
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
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
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool
        cpp_dbc::PostgreSQL::PostgreSQLConnectionPool pool(poolConfig);

        // Create a test table
        auto conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            int initialIdleCount = pool.getIdleConnectionCount();
            int initialActiveCount = pool.getActiveConnectionCount();
            int initialTotalCount = pool.getTotalConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool.getConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool.getConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 2);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool.getActiveConnectionCount() == 0);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount);
        }

        // Clean up
        auto cleanupConn = pool.getConnection();
        cleanupConn->executeUpdate(dropTableQuery);
        cleanupConn->close();

        // Close the pool
        pool.close();
    }
}
#endif

#if USE_SQLITE
#include <cpp_dbc/drivers/driver_sqlite.hpp>

// Helper function to check if we can connect to SQLite
static bool canConnectToSQLite()
{
    try
    {
        // Load the YAML configuration
        std::string config_path = getConfigFilePath();
        YAML::Node config = YAML::LoadFile(config_path);

        // Find the dev_sqlite configuration
        YAML::Node dbConfig;
        for (size_t i = 0; i < config["databases"].size(); i++)
        {
            YAML::Node db = config["databases"][i];
            if (db["name"].as<std::string>() == "dev_sqlite")
            {
                dbConfig = YAML::Node(db);
                break;
            }
        }

        if (!dbConfig.IsDefined())
        {
            std::cerr << "SQLite configuration not found in test_db_connections.yml" << std::endl;
            return false;
        }

        // Create connection string
        std::string type = dbConfig["type"].as<std::string>();
        std::string database = dbConfig["database"].as<std::string>();
        std::string username = ""; // SQLite doesn't use username
        std::string password = ""; // SQLite doesn't use password

        std::string connStr = "cpp_dbc:" + type + "://" + database;

        // Register the SQLite driver
        cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDriver>());

        // Attempt to connect to SQLite
        std::cout << "Attempting to connect to SQLite with connection string: " << connStr << std::endl;

        auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

        // If we get here, the connection was successful
        std::cout << "SQLite connection successful!" << std::endl;

        // Execute a simple query to verify the connection
        auto resultSet = conn->executeQuery("SELECT 1 as test_value");
        bool success = resultSet->next() && resultSet->getInt("test_value") == 1;

        // Close the connection
        conn->close();

        return success;
    }
    catch (const std::exception &e)
    {
        std::cerr << "SQLite connection error: " << e.what() << std::endl;
        return false;
    }
}

// Test case for real SQLite connection pool
TEST_CASE("Real SQLite connection pool tests", "[sqlite_connection_pool_real]")
{
    // Skip these tests if we can't connect to SQLite
    if (!canConnectToSQLite())
    {
        SKIP("Cannot connect to SQLite database");
        return;
    }

    // Load the YAML configuration
    std::string config_path = getConfigFilePath();
    YAML::Node config = YAML::LoadFile(config_path);

    // Find the dev_sqlite configuration
    YAML::Node dbConfig;
    for (size_t i = 0; i < config["databases"].size(); i++)
    {
        YAML::Node db = config["databases"][i];
        if (db["name"].as<std::string>() == "dev_sqlite")
        {
            dbConfig = YAML::Node(db);
            break;
        }
    }

    // Create connection parameters
    std::string type = dbConfig["type"].as<std::string>();
    std::string database = dbConfig["database"].as<std::string>();
    std::string username = ""; // SQLite doesn't use username
    std::string password = ""; // SQLite doesn't use password

    std::string connStr = "cpp_dbc:" + type + "://" + database;

    // Get test queries
    YAML::Node testQueries = config["test_queries"]["sqlite"];
    std::string createTableQuery = testQueries["create_table"].as<std::string>();
    std::string insertDataQuery = testQueries["insert_data"].as<std::string>();
    std::string selectDataQuery = testQueries["select_data"].as<std::string>();
    std::string dropTableQuery = testQueries["drop_table"].as<std::string>();

    SECTION("Basic connection pool operations")
    {
        // Load the SQLite pool configuration from YAML
        YAML::Node poolNode = config["connection_pool"]["sqlite_pool"];

        // Create a connection pool configuration
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);

        // Set pool parameters from the YAML configuration
        poolConfig.setInitialSize(poolNode["initial_size"].as<int>());
        poolConfig.setMaxSize(poolNode["max_size"].as<int>());
        poolConfig.setMinIdle(5); // Default value
        poolConfig.setConnectionTimeout(poolNode["connection_timeout"].as<long>());
        poolConfig.setValidationInterval(poolNode["validation_interval"].as<long>());
        poolConfig.setIdleTimeout(poolNode["idle_timeout"].as<long>());
        poolConfig.setMaxLifetimeMillis(60000); // Default value
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Set the transaction isolation level to SERIALIZABLE for SQLite
        poolConfig.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

        // Create a connection pool
        cpp_dbc::SQLite::SQLiteConnectionPool pool(poolConfig);

        // Create a test table
        auto conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test getting and returning connections
        SECTION("Get and return connections")
        {
            // Get initial pool statistics
            int initialIdleCount = pool.getIdleConnectionCount();
            int initialActiveCount = pool.getActiveConnectionCount();
            int initialTotalCount = pool.getTotalConnectionCount();

            REQUIRE(initialActiveCount == 0);
            REQUIRE(initialIdleCount >= 3);  // minIdle
            REQUIRE(initialTotalCount >= 3); // minIdle

            // Get a connection
            auto conn1 = pool.getConnection();
            REQUIRE(conn1 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Get another connection
            auto conn2 = pool.getConnection();
            REQUIRE(conn2 != nullptr);
            REQUIRE(pool.getActiveConnectionCount() == 2);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 2);

            // Return the first connection
            conn1->close();
            REQUIRE(pool.getActiveConnectionCount() == 1);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount - 1);

            // Return the second connection
            conn2->close();
            REQUIRE(pool.getActiveConnectionCount() == 0);
            REQUIRE(pool.getIdleConnectionCount() == initialIdleCount);
        }

        // Clean up
        auto cleanupConn = pool.getConnection();
        cleanupConn->executeUpdate(dropTableQuery);
        cleanupConn->close();

        // Close the pool
        pool.close();
    }
}
#endif