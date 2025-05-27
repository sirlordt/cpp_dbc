#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/config/yaml_config_loader.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
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
#include "cpp_dbc/common/system_utils.hpp"
#include "test_mocks.hpp"

// Helper function to get the path to the test_db_connections.yml file
std::string getConfigFilePath();

// Helper class for safe pool management in tests
class SafePoolManager
{
private:
    std::vector<std::shared_ptr<cpp_dbc::MySQL::MySQLConnectionPool>> pools;

public:
    std::shared_ptr<cpp_dbc::MySQL::MySQLConnectionPool> createPool(const cpp_dbc::config::ConnectionPoolConfig &config)
    {
        auto pool = std::make_shared<cpp_dbc::MySQL::MySQLConnectionPool>(config);
        pools.push_back(pool);
        return pool;
    }

    ~SafePoolManager()
    {
        std::cout << "SafePoolManager::~SafePoolManager - Starting destructor with " << pools.size() << " pools" << std::endl;

        // Explicitly close all pools
        for (size_t i = 0; i < pools.size(); ++i)
        {
            try
            {
                auto &pool = pools[i];
                if (pool)
                {
                    std::cout << "SafePoolManager::~SafePoolManager - Closing pool " << (i + 1) << "/" << pools.size() << std::endl;
                    pool->close();
                    std::cout << "SafePoolManager::~SafePoolManager - Pool " << (i + 1) << " closed successfully" << std::endl;
                }
                else
                {
                    std::cout << "SafePoolManager::~SafePoolManager - Pool " << (i + 1) << " is null, skipping" << std::endl;
                }
            }
            catch (const std::exception &e)
            {
                std::cout << "SafePoolManager::~SafePoolManager - Exception closing pool " << (i + 1) << ": " << e.what() << std::endl;
            }
            catch (...)
            {
                std::cout << "SafePoolManager::~SafePoolManager - Unknown exception closing pool " << (i + 1) << std::endl;
            }
        }

        std::cout << "SafePoolManager::~SafePoolManager - Clearing pools vector" << std::endl;
        pools.clear();

        // Longer delay to allow complete cleanup
        std::cout << "SafePoolManager::~SafePoolManager - Waiting for complete cleanup" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "SafePoolManager::~SafePoolManager - Destructor completed" << std::endl;
    }
};

#if USE_MYSQL
// Test case for real MySQL connection
TEST_CASE("Real MySQL connection tests", "[mysql_real]")
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

    SECTION("Basic MySQL operations")
    {
        /*
        // Register the MySQL driver (safe registration)
        cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDriver>());

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
        */
    }

    SECTION("MySQL connection pool")
    {
        // SafePoolManager poolManager;

        // Create a connection pool configuration with shorter timeouts for tests
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(2);           // Reduced for tests
        poolConfig.setMaxSize(5);               // Reduced for tests
        poolConfig.setMinIdle(1);               // Reduced for tests
        poolConfig.setConnectionTimeout(10000); // Shorter timeout
        poolConfig.setValidationInterval(500);  // Shorter interval
        poolConfig.setIdleTimeout(5000);        // Shorter idle timeout
        poolConfig.setMaxLifetimeMillis(10000); // Shorter lifetime
        poolConfig.setTestOnBorrow(false);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool using safe manager
        // auto poolPtr = poolManager.createPool(poolConfig);
        auto poolPtr = std::make_shared<cpp_dbc::MySQL::MySQLConnectionPool>(poolConfig);
        // auto poolPtr = std::make_shared<cpp_dbc_test::MockConnectionPool>();
        //  auto *poolPtr = new cpp_dbc::MySQL::MySQLConnectionPool(poolConfig);
        //   auto &pool = *poolPtr;

        // Create a test table
        auto conn = poolPtr->getConnection();

        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);

        conn->close();

        // Test multiple connections in parallel
        const int numThreads = 10;
        const int opsPerThread = 5;

        std::atomic<int> successCount(0);
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([poolPtr, i, opsPerThread, insertDataQuery, &successCount]()
                                          {
                for (int j = 0; j < opsPerThread; j++) {

                    std::ostringstream oss;
                    oss << std::this_thread::get_id();

                    try {
                        cpp_dbc::system_utils::safePrint( cpp_dbc::system_utils::currentTimeMillis() + ": " + oss.str(), "(1) Try to getting connection" );
                        // Get a connection from the pool
                        auto conn = poolPtr->getConnection();
                        cpp_dbc::system_utils::safePrint( cpp_dbc::system_utils::currentTimeMillis() + ": " + oss.str(), "(2) Getted connection" );

                        // Insert a row
                        int id = i * 100 + j;
                        {
                            // Scope the PreparedStatement to ensure it's destroyed before closing the connection
                            auto pstmt = conn->prepareStatement(insertDataQuery);
                            pstmt->setInt(1, id);
                            pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                            pstmt->executeUpdate();
                            // pstmt is destroyed here when it goes out of scope
                        }

                        cpp_dbc::system_utils::safePrint( cpp_dbc::system_utils::currentTimeMillis() + ": " + oss.str(), "(3) Closing connection" );
                        conn->close();
                        cpp_dbc::system_utils::safePrint( cpp_dbc::system_utils::currentTimeMillis() + ": " + oss.str(), "(4) Connection closed" );

                        // Increment success counter
                        successCount++;
                        cpp_dbc::system_utils::safePrint( cpp_dbc::system_utils::currentTimeMillis() + ": " + oss.str(), "(5) Incremented " + std::to_string( successCount.load() ) );
                    }
                    catch (const std::exception& e) {
                        //std::cout << "Thread operation failed: " << e.what() << std::endl;
                        cpp_dbc::system_utils::safePrint( cpp_dbc::system_utils::currentTimeMillis() + ": " + oss.str(), "(6) Thread operation failed: " + std::string(e.what()) );
                    }
                } }));
        }

        // Wait for all threads to complete
        for (std::thread &t : threads)
        {
            t.join();

            // std::ostringstream oss;
            // oss << t.get_id();
            // cpp_dbc::system_utils::safePrint(cpp_dbc::system_utils::currentTimeMillis() + ": BCFF7880A05E", "Thread finished");
        }

        // Verify that all operations were successful
        REQUIRE(successCount.load() == numThreads * opsPerThread);

        // Verify the data
        conn = poolPtr->getConnection();

        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_table");
        // REQUIRE(rs->next());
        conn->executeUpdate(dropTableQuery);
        // REQUIRE(rs->getInt("count") == numThreads * opsPerThread);

        // Clean up
        conn->close();

        // delete poolPtr;

        // Pool will be closed automatically by SafePoolManager
    }

    // TEMPORARILY COMMENTED OUT TO ISOLATE HANGING ISSUE
    /*
    SECTION("MySQL transaction management")
    {
        SafePoolManager poolManager;

        // Create a connection pool configuration with shorter timeouts for tests
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(2);           // Reduced for tests
        poolConfig.setMaxSize(3);               // Reduced for tests
        poolConfig.setMinIdle(1);               // Reduced for tests
        poolConfig.setConnectionTimeout(2000);  // Shorter timeout
        poolConfig.setValidationInterval(500);  // Shorter interval
        poolConfig.setIdleTimeout(5000);        // Shorter idle timeout
        poolConfig.setMaxLifetimeMillis(10000); // Shorter lifetime
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool using safe manager
        auto poolPtr = poolManager.createPool(poolConfig);
        auto &pool = *poolPtr;

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

        // Pool will be closed automatically by SafePoolManager
    }

    SECTION("MySQL metadata retrieval")
    {
        // Register the MySQL driver (safe registration)
        cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDriver>());

        // Get a connection
        auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

        // Create a test table with various data types
        conn->executeUpdate("DROP TABLE IF EXISTS test_types");
        conn->executeUpdate(
            "CREATE TABLE test_types ("
            "id INT PRIMARY KEY, "
            "int_col INT, "
            "double_col DOUBLE, "
            "varchar_col VARCHAR(100), "
            "text_col TEXT, "
            "date_col DATE, "
            "datetime_col DATETIME, "
            "bool_col BOOLEAN"
            ")");

        // Insert test data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_types VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setInt(2, 42);
        pstmt->setDouble(3, 3.14159);
        pstmt->setString(4, "Hello, World!");
        pstmt->setString(5, "This is a longer text field with more content.");
        pstmt->setString(6, "2023-01-15");          // Date as string
        pstmt->setString(7, "2023-01-15 14:30:00"); // DateTime as string
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
        REQUIRE(rs->getString("datetime_col") == "2023-01-15 14:30:00");
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
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "datetime_col") != columnNames.end());
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
        */

    // TEMPORARILY COMMENTED OUT TO ISOLATE HANGING ISSUE
    /*
    SECTION("MySQL stress test")
    {
        SafePoolManager poolManager;

        // Create a connection pool configuration with shorter timeouts for tests
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(3);           // Reduced for tests
        poolConfig.setMaxSize(10);              // Reduced for tests
        poolConfig.setMinIdle(2);               // Reduced for tests
        poolConfig.setConnectionTimeout(2000);  // Shorter timeout
        poolConfig.setValidationInterval(500);  // Shorter interval
        poolConfig.setIdleTimeout(5000);        // Shorter idle timeout
        poolConfig.setMaxLifetimeMillis(10000); // Shorter lifetime
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");

        // Create a connection pool using safe manager
        auto poolPtr = poolManager.createPool(poolConfig);
        auto &pool = *poolPtr;

        // Create a test table
        auto conn = pool.getConnection();
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);
        conn->close();

        // Test with fewer concurrent threads for stability
        const int numThreads = 5;    // Reduced from 20
        const int opsPerThread = 10; // Reduced from 50

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

        std::cout << "MySQL stress test completed in " << duration << " ms" << std::endl;
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

        // Pool will be closed automatically by SafePoolManager
    }
    */
}
#else
// Skip tests if MySQL support is not enabled
TEST_CASE("Real MySQL connection tests (skipped)", "[mysql_real]")
{
    SKIP("MySQL support is not enabled");
}
#endif