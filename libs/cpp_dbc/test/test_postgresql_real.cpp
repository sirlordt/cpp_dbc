#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/config/yaml_config_loader.hpp>
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

// Helper function to get the path to the test_db_connections.yml file
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

#if USE_POSTGRESQL
// Test case for real PostgreSQL connection
TEST_CASE("Real PostgreSQL connection tests", "[postgresql_real]")
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

    SECTION("Basic PostgreSQL operations")
    {
        // Register the PostgreSQL driver
        cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

        // Get a connection
        auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);
        REQUIRE(conn != nullptr);

        // Create a test table
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        int result = conn->executeUpdate(createTableQuery);
        REQUIRE(result == 0); // CREATE TABLE should return 0 affected rows

        // Insert data using a prepared statement
        auto pstmt = conn->prepareStatement(insertDataQuery);
        REQUIRE(pstmt != nullptr);

        // Insert multiple rows
        for (int i = 1; i <= 10; i++)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Test Name " + std::to_string(i));
            int insertResult = pstmt->executeUpdate();
            REQUIRE(insertResult == 1); // Each insert should affect 1 row
        }

        // Select data using a prepared statement
        auto selectStmt = conn->prepareStatement(selectDataQuery);
        REQUIRE(selectStmt != nullptr);

        // Select a specific row
        selectStmt->setInt(1, 5);
        auto rs = selectStmt->executeQuery();
        REQUIRE(rs != nullptr);

        // Verify the result
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 5);
        REQUIRE(rs->getString("name") == "Test Name 5");
        REQUIRE_FALSE(rs->next()); // Should only be one row

        // Select all rows using direct query
        rs = conn->executeQuery("SELECT * FROM test_table ORDER BY id");
        REQUIRE(rs != nullptr);

        // Verify all rows
        int count = 0;
        while (rs->next())
        {
            count++;
            REQUIRE(rs->getInt("id") == count);
            REQUIRE(rs->getString("name") == "Test Name " + std::to_string(count));
        }
        REQUIRE(count == 10);

        // Update data
        int updateResult = conn->executeUpdate("UPDATE test_table SET name = 'Updated Name' WHERE id = 3");
        REQUIRE(updateResult == 1); // Should affect 1 row

        // Verify the update
        rs = conn->executeQuery("SELECT * FROM test_table WHERE id = 3");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("name") == "Updated Name");

        // Delete data
        int deleteResult = conn->executeUpdate("DELETE FROM test_table WHERE id > 5");
        REQUIRE(deleteResult == 5); // Should delete 5 rows (6-10)

        // Verify the delete
        rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_table");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("count") == 5); // Should be 5 rows left

        // Drop the test table
        result = conn->executeUpdate(dropTableQuery);
        REQUIRE(result == 0); // DROP TABLE should return 0 affected rows

        // Close the connection
        conn->close();
    }

    /*
    SECTION("PostgreSQL connection pool")
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

        // Test multiple connections in parallel
        const int numThreads = 5;
        const int opsPerThread = 10;

        std::atomic<int> successCount(0);
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([&pool, i, opsPerThread, insertDataQuery, &successCount]()
                                          {
                for (int j = 0; j < opsPerThread; j++) {
                    try {
                        // Get a connection from the pool
                        auto conn = pool.getConnection();

                        // Insert a row
                        auto pstmt = conn->prepareStatement(insertDataQuery);
                        int id = i * 100 + j;
                        pstmt->setInt(1, id);
                        pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                        pstmt->executeUpdate();

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

        // Verify that all operations were successful
        REQUIRE(successCount == numThreads * opsPerThread);

        // Verify the data
        conn = pool.getConnection();
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_table");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("count") == numThreads * opsPerThread);

        // Clean up
        conn->executeUpdate(dropTableQuery);
        conn->close();

        // Close the pool
        pool.close();
    }

    SECTION("PostgreSQL transaction management")
    {
        // Create a connection pool configuration
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(5);
        poolConfig.setMinIdle(2);
        poolConfig.setConnectionTimeout(5000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setIdleTimeout(30000);
        poolConfig.setMaxLifetimeMillis(60000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool
        cpp_dbc::PostgreSQL::PostgreSQLConnectionPool pool(poolConfig);

        // Create a transaction manager
        cpp_dbc::TransactionManager manager(pool);

        // Create a test table
        auto conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        SECTION("Commit transaction")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();
            REQUIRE(!txId.empty());

            // Get the connection associated with the transaction
            conn = manager.getTransactionConnection(txId);
            REQUIRE(conn != nullptr);

            // Insert data within the transaction
            auto pstmt = conn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 1);
            pstmt->setString(2, "Transaction Test");
            int result = pstmt->executeUpdate();
            REQUIRE(result == 1);

            // Commit the transaction
            manager.commitTransaction(txId);

            // Verify the data was committed
            conn = pool.getConnection();
            auto rs = conn->executeQuery("SELECT * FROM test_table WHERE id = 1");
            REQUIRE(rs->next());
            REQUIRE(rs->getString("name") == "Transaction Test");
            conn->close();
        }

        SECTION("Rollback transaction")
        {
            // Begin a transaction
            std::string txId = manager.beginTransaction();

            // Get the connection associated with the transaction
            conn = manager.getTransactionConnection(txId);

            // Insert data within the transaction
            auto pstmt = conn->prepareStatement(insertDataQuery);
            pstmt->setInt(1, 2);
            pstmt->setString(2, "Rollback Test");
            int result = pstmt->executeUpdate();
            REQUIRE(result == 1);

            // Rollback the transaction
            manager.rollbackTransaction(txId);

            // Verify the data was not committed
            conn = pool.getConnection();
            auto rs = conn->executeQuery("SELECT * FROM test_table WHERE id = 2");
            REQUIRE_FALSE(rs->next()); // Should be no rows
            conn->close();
        }

        // Clean up
        conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery);
        conn->close();

        // Close the pool
        pool.close();
    }
    */

    SECTION("PostgreSQL metadata retrieval")
    {
        // Register the PostgreSQL driver
        cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

        // Get a connection
        auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

        // Create a test table with various data types
        conn->executeUpdate("DROP TABLE IF EXISTS test_types");
        conn->executeUpdate(
            "CREATE TABLE test_types ("
            "id INT PRIMARY KEY, "
            "int_col INT, "
            "double_col DOUBLE PRECISION, "
            "varchar_col VARCHAR(100), "
            "text_col TEXT, "
            "date_col DATE, "
            "timestamp_col TIMESTAMP, "
            "bool_col BOOLEAN"
            ")");

        // Insert test data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_types VALUES ($1, $2, $3, $4, $5, $6, $7, $8)");

        pstmt->setInt(1, 1);
        pstmt->setInt(2, 42);
        pstmt->setDouble(3, 3.14159);
        pstmt->setString(4, "Hello, World!");
        pstmt->setString(5, "This is a longer text field with more content.");
        pstmt->setDate(6, "2023-01-15");
        pstmt->setTimestamp(7, "2023-01-15 14:30:00");
        pstmt->setBoolean(8, true);

        pstmt->executeUpdate();

        // Test retrieving different data types
        auto rs = conn->executeQuery("SELECT * FROM test_types");
        REQUIRE(rs->next());

        // Test each data type
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getInt("int_col") == 42);
        REQUIRE((rs->getDouble("double_col") > 3.14 && rs->getDouble("double_col") < 3.15));
        REQUIRE(rs->getString("varchar_col") == "Hello, World!");
        REQUIRE(rs->getString("text_col") == "This is a longer text field with more content.");
        REQUIRE(rs->getString("date_col") == "2023-01-15");
        REQUIRE(rs->getString("timestamp_col").find("2023-01-15") != std::string::npos);
        REQUIRE(rs->getBoolean("bool_col") == true);

        // Test column metadata
        auto columnNames = rs->getColumnNames();
        REQUIRE(columnNames.size() == 8);
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "id") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "int_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "double_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "varchar_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "text_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "date_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "timestamp_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "bool_col") != columnNames.end());

        // Test NULL values
        conn->executeUpdate("UPDATE test_types SET int_col = NULL, varchar_col = NULL WHERE id = 1");
        rs = conn->executeQuery("SELECT * FROM test_types");
        REQUIRE(rs->next());
        REQUIRE(rs->isNull("int_col"));
        REQUIRE(rs->isNull("varchar_col"));

        // Clean up
        conn->executeUpdate("DROP TABLE test_types");

        // Close the connection
        conn->close();
    }

    /*
    SECTION("PostgreSQL stress test")
    {
        // Create a connection pool configuration
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(20);
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

        // Test with many concurrent threads
        const int numThreads = 20;
        const int opsPerThread = 50;

        std::atomic<int> successCount(0);
        std::vector<std::thread> threads;

        auto startTime = std::chrono::steady_clock::now();

        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([&pool, i, opsPerThread, insertDataQuery, selectDataQuery, &successCount]()
                                          {
                for (int j = 0; j < opsPerThread; j++) {
                    try {
                        // Get a connection from the pool
                        auto conn = pool.getConnection();

                        // Insert a row
                        auto pstmt = conn->prepareStatement(insertDataQuery);
                        int id = i * 1000 + j;
                        pstmt->setInt(1, id);
                        pstmt->setString(2, "Stress Test " + std::to_string(id));
                        pstmt->executeUpdate();

                        // Select the row back
                        auto selectStmt = conn->prepareStatement(selectDataQuery);
                        selectStmt->setInt(1, id);
                        auto rs = selectStmt->executeQuery();

                        if (rs->next() && rs->getInt("id") == id &&
                            rs->getString("name") == "Stress Test " + std::to_string(id)) {
                            successCount++;
                        }

                        // Return the connection to the pool
                        conn->close();
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

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << "PostgreSQL stress test completed in " << duration << " ms" << std::endl;
        std::cout << "Operations per second: " << (numThreads * opsPerThread * 1000.0 / duration) << std::endl;

        // Verify that all operations were successful
        REQUIRE(successCount == numThreads * opsPerThread);

        // Verify the total number of rows
        conn = pool.getConnection();
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_table");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("count") == numThreads * opsPerThread);

        // Clean up
        conn->executeUpdate(dropTableQuery);
        conn->close();

        // Close the pool
        pool.close();
    }

    SECTION("PostgreSQL specific features")
    {
        // Register the PostgreSQL driver
        cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

        // Get a connection
        auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

        // Test JSON data type
        conn->executeUpdate("DROP TABLE IF EXISTS test_json");
        conn->executeUpdate("CREATE TABLE test_json (id INT PRIMARY KEY, data JSONB)");

        // Insert JSON data
        conn->executeUpdate("INSERT INTO test_json VALUES (1, '{\"name\": \"John\", \"age\": 30, \"city\": \"New York\"}')");

        // Query JSON data
        auto rs = conn->executeQuery("SELECT data->>'name' as name, (data->>'age')::int as age FROM test_json WHERE id = 1");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("name") == "John");
        REQUIRE(rs->getInt("age") == 30);

        // Test array data type
        conn->executeUpdate("DROP TABLE IF EXISTS test_array");
        conn->executeUpdate("CREATE TABLE test_array (id INT PRIMARY KEY, int_array INT[], text_array TEXT[])");

        // Insert array data
        conn->executeUpdate("INSERT INTO test_array VALUES (1, '{1,2,3}', '{\"one\",\"two\",\"three\"}')");

        // Query array data
        rs = conn->executeQuery("SELECT int_array[1] as first_int, text_array[2] as second_text FROM test_array WHERE id = 1");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("first_int") == 1);
        REQUIRE(rs->getString("second_text") == "two");

        // Clean up
        conn->executeUpdate("DROP TABLE test_json");
        conn->executeUpdate("DROP TABLE test_array");

        // Close the connection
        conn->close();
    }
    */
}
#else
// Skip tests if PostgreSQL support is not enabled
TEST_CASE("Real PostgreSQL connection tests (skipped)", "[postgresql_real]")
{
    SKIP("PostgreSQL support is not enabled");
}
#endif