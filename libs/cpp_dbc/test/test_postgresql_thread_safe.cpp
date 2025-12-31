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

 @file test_postgresql_thread_safe.cpp
 @brief Thread-safety stress tests for PostgreSQL database driver

*/

// Only compile these tests if DB_DRIVER_THREAD_SAFE is enabled
#if DB_DRIVER_THREAD_SAFE

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <random>
#include <mutex>
#include <condition_variable>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "test_postgresql_common.hpp"

#if USE_POSTGRESQL

/**
 * @brief Thread-safety stress tests for PostgreSQL driver
 *
 * These tests verify that the PostgreSQL driver is thread-safe by:
 * 1. Multiple threads each with their own connection
 * 2. Connection pool under high concurrency
 * 3. Rapid connection open/close from multiple threads
 * 4. Stress testing with high concurrency using connection pool
 *
 * Note: Each thread should have its own connection for proper thread-safety.
 * Sharing a single connection across threads is not recommended even with
 * thread-safe drivers, as it can lead to logical issues with transactions
 * and statement state.
 */
TEST_CASE("PostgreSQL Thread-Safety Tests", "[postgresql_thread_safe]")
{
    // Skip these tests if we can't connect to PostgreSQL
    if (!postgresql_test_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

    // Get PostgreSQL configuration
    auto dbConfig = postgresql_test_helpers::getPostgreSQLConfig("dev_postgresql");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Register the PostgreSQL driver
    cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());

    SECTION("Multiple threads with individual connections")
    {
        // Setup: create test table using a single connection
        auto setupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(setupConn != nullptr);
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        setupConn->executeUpdate("CREATE TABLE thread_test (id INT PRIMARY KEY, value VARCHAR(100))");
        setupConn->close();

        const int numThreads = 10;
        const int opsPerThread = 20;
        std::atomic<int> successCount(0);
        std::atomic<int> errorCount(0);
        std::vector<std::thread> threads;

        // Barrier to synchronize thread start
        std::mutex startMutex;
        std::condition_variable startCv;
        bool startFlag = false;

        for (int i = 0; i < numThreads; i++)
        {
            threads.emplace_back([&, i]()
                                 {
                // Wait for all threads to be ready
                {
                    std::unique_lock<std::mutex> lock(startMutex);
                    startCv.wait(lock, [&]() { return startFlag; });
                }

                try
                {
                    // Each thread gets its own connection
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                    
                    for (int j = 0; j < opsPerThread; j++)
                    {
                        try
                        {
                            int id = i * 1000 + j;
                            
                            // Insert operation
                            auto pstmt = conn->prepareStatement("INSERT INTO thread_test (id, value) VALUES ($1, $2)");
                            pstmt->setInt(1, id);
                            pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                            pstmt->executeUpdate();

                            // Select operation
                            auto selectStmt = conn->prepareStatement("SELECT * FROM thread_test WHERE id = $1");
                            selectStmt->setInt(1, id);
                            auto rs = selectStmt->executeQuery();
                            
                            if (rs->next())
                            {
                                successCount++;
                            }
                        }
                        catch (const std::exception& e)
                        {
                            errorCount++;
                            std::cerr << "Thread " << i << " op " << j << " error: " << e.what() << std::endl;
                        }
                    }
                    
                    conn->close();
                }
                catch (const std::exception& e)
                {
                    errorCount += opsPerThread;
                    std::cerr << "Thread " << i << " connection error: " << e.what() << std::endl;
                } });
        }

        // Start all threads simultaneously
        {
            std::lock_guard<std::mutex> lock(startMutex);
            startFlag = true;
        }
        startCv.notify_all();

        // Wait for all threads
        for (auto &t : threads)
        {
            t.join();
        }

        std::cout << "Multiple threads with individual connections: " << successCount << " successes, " << errorCount << " errors" << std::endl;

        // Clean up
        auto cleanupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        cleanupConn->close();

        // We expect most operations to succeed
        REQUIRE(successCount > 0);
    }

    SECTION("Connection pool concurrent access")
    {
        // Create connection pool
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(20);
        poolConfig.setMinIdle(2);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setValidationQuery("SELECT 1");

        auto pool = cpp_dbc::PostgreSQL::PostgreSQLConnectionPool::create(poolConfig);

        // Setup: create test table
        auto setupConn = pool->getRelationalDBConnection();
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        setupConn->executeUpdate("CREATE TABLE thread_test (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE PRECISION)");
        setupConn->returnToPool();

        const int numThreads = 10;
        const int opsPerThread = 20;
        std::atomic<int> successCount(0);
        std::atomic<int> errorCount(0);
        std::vector<std::thread> threads;
        std::atomic<int> idCounter(0);

        for (int i = 0; i < numThreads; i++)
        {
            threads.emplace_back([&, i]()
                                 {
                for (int j = 0; j < opsPerThread; j++)
                {
                    try
                    {
                        auto conn = pool->getRelationalDBConnection();
                        int id = idCounter.fetch_add(1);
                        
                        // Insert with prepared statement
                        auto pstmt = conn->prepareStatement("INSERT INTO thread_test (id, name, value) VALUES ($1, $2, $3)");
                        pstmt->setInt(1, id);
                        pstmt->setString(2, "Name " + std::to_string(id));
                        pstmt->setDouble(3, id * 1.5);
                        pstmt->executeUpdate();
                        
                        conn->returnToPool();
                        successCount++;
                    }
                    catch (const std::exception& e)
                    {
                        errorCount++;
                        std::cerr << "Thread " << i << " error: " << e.what() << std::endl;
                    }
                } });
        }

        for (auto &t : threads)
        {
            t.join();
        }

        std::cout << "Connection pool concurrent access: " << successCount << " successes, " << errorCount << " errors" << std::endl;

        // Clean up
        auto cleanupConn = pool->getRelationalDBConnection();
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        cleanupConn->returnToPool();

        REQUIRE(successCount > 0);
    }

    SECTION("Concurrent read operations with connection pool")
    {
        // Create connection pool
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(20);
        poolConfig.setMinIdle(2);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setValidationQuery("SELECT 1");

        auto pool = cpp_dbc::PostgreSQL::PostgreSQLConnectionPool::create(poolConfig);

        // Setup: create and populate test table
        auto setupConn = pool->getRelationalDBConnection();
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        setupConn->executeUpdate("CREATE TABLE thread_test (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE PRECISION)");

        // Insert test data
        for (int i = 0; i < 100; i++)
        {
            auto pstmt = setupConn->prepareStatement("INSERT INTO thread_test (id, name, value) VALUES ($1, $2, $3)");
            pstmt->setInt(1, i);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->executeUpdate();
        }
        setupConn->returnToPool();

        const int numThreads = 10;
        const int readsPerThread = 50;
        std::atomic<int> readCount(0);
        std::atomic<int> errorCount(0);
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++)
        {
            threads.emplace_back([&, i]()
                                 {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> idDist(0, 99);

                for (int j = 0; j < readsPerThread; j++)
                {
                    try
                    {
                        auto conn = pool->getRelationalDBConnection();
                        int targetId = idDist(gen);
                        
                        auto pstmt = conn->prepareStatement("SELECT * FROM thread_test WHERE id = $1");
                        pstmt->setInt(1, targetId);
                        auto rs = pstmt->executeQuery();
                        
                        if (rs->next())
                        {
                            // Read all columns
                            int id = rs->getInt("id");
                            std::string name = rs->getString("name");
                            double value = rs->getDouble("value");
                            (void)id; (void)name; (void)value;
                            readCount++;
                        }
                        
                        conn->returnToPool();
                    }
                    catch (const std::exception& e)
                    {
                        errorCount++;
                    }
                } });
        }

        for (auto &t : threads)
        {
            t.join();
        }

        std::cout << "Concurrent read operations: " << readCount << " reads, " << errorCount << " errors" << std::endl;

        // Clean up
        auto cleanupConn = pool->getRelationalDBConnection();
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        cleanupConn->returnToPool();

        // Most reads should succeed
        REQUIRE(readCount > numThreads * readsPerThread * 0.9);
    }

    SECTION("High concurrency stress test")
    {
        // Create connection pool for stress test
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(20);
        poolConfig.setMinIdle(2);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setValidationQuery("SELECT 1");

        auto pool = cpp_dbc::PostgreSQL::PostgreSQLConnectionPool::create(poolConfig);

        // Create test table
        auto setupConn = pool->getRelationalDBConnection();
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_stress_test");
        setupConn->executeUpdate("CREATE TABLE thread_stress_test (id SERIAL PRIMARY KEY, thread_id INT, op_id INT, data VARCHAR(255))");
        setupConn->returnToPool();

        const int numThreads = 30;
        const int opsPerThread = 50;
        std::atomic<int> insertCount(0);
        std::atomic<int> selectCount(0);
        std::atomic<int> updateCount(0);
        std::atomic<int> errorCount(0);
        std::vector<std::thread> threads;

        auto startTime = std::chrono::steady_clock::now();

        for (int i = 0; i < numThreads; i++)
        {
            threads.emplace_back([&, i]()
                                 {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> opDist(0, 2);

                for (int j = 0; j < opsPerThread; j++)
                {
                    try
                    {
                        auto conn = pool->getRelationalDBConnection();
                        int op = opDist(gen);

                        switch (op)
                        {
                            case 0: // Insert
                            {
                                auto pstmt = conn->prepareStatement(
                                    "INSERT INTO thread_stress_test (thread_id, op_id, data) VALUES ($1, $2, $3)");
                                pstmt->setInt(1, i);
                                pstmt->setInt(2, j);
                                pstmt->setString(3, "Data from thread " + std::to_string(i) + " op " + std::to_string(j));
                                pstmt->executeUpdate();
                                insertCount++;
                                break;
                            }
                            case 1: // Select
                            {
                                auto rs = conn->executeQuery("SELECT COUNT(*) as cnt FROM thread_stress_test");
                                if (rs->next())
                                {
                                    rs->getInt("cnt");
                                }
                                selectCount++;
                                break;
                            }
                            case 2: // Update
                            {
                                conn->executeUpdate("UPDATE thread_stress_test SET data = 'updated' WHERE thread_id = " + std::to_string(i) + " AND id IN (SELECT id FROM thread_stress_test WHERE thread_id = " + std::to_string(i) + " LIMIT 1)");
                                updateCount++;
                                break;
                            }
                        }

                        conn->returnToPool();
                    }
                    catch (const std::exception& e)
                    {
                        errorCount++;
                    }
                } });
        }

        for (auto &t : threads)
        {
            t.join();
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << "High concurrency stress test completed in " << duration << " ms" << std::endl;
        std::cout << "  Inserts: " << insertCount << std::endl;
        std::cout << "  Selects: " << selectCount << std::endl;
        std::cout << "  Updates: " << updateCount << std::endl;
        std::cout << "  Errors: " << errorCount << std::endl;
        if (duration > 0)
        {
            std::cout << "  Operations per second: " << ((insertCount + selectCount + updateCount) * 1000.0 / static_cast<double>(duration)) << std::endl;
        }

        // Clean up
        auto cleanupConn = pool->getRelationalDBConnection();
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_stress_test");
        cleanupConn->returnToPool();

        // Most operations should succeed
        int totalOps = insertCount + selectCount + updateCount;
        REQUIRE(totalOps > numThreads * opsPerThread * 0.8); // At least 80% success rate
    }

    SECTION("Rapid connection open/close stress test")
    {
        const int numThreads = 10;
        const int connectionsPerThread = 10;
        std::atomic<int> successCount(0);
        std::atomic<int> errorCount(0);
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++)
        {
            threads.emplace_back([&]()
                                 {
                for (int j = 0; j < connectionsPerThread; j++)
                {
                    try
                    {
                        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                        
                        // Do a simple operation
                        auto rs = conn->executeQuery("SELECT 1 as test");
                        if (rs->next())
                        {
                            rs->getInt("test");
                        }
                        
                        conn->close();
                        successCount++;
                    }
                    catch (const std::exception& e)
                    {
                        errorCount++;
                        std::cerr << "Connection error: " << e.what() << std::endl;
                    }
                } });
        }

        for (auto &t : threads)
        {
            t.join();
        }

        std::cout << "Rapid connection test: " << successCount << " successes, " << errorCount << " errors" << std::endl;

        REQUIRE(successCount > numThreads * connectionsPerThread * 0.9); // At least 90% success rate
    }
}

#else
TEST_CASE("PostgreSQL Thread-Safety Tests (skipped)", "[postgresql_thread_safe]")
{
    SKIP("PostgreSQL support is not enabled");
}
#endif // USE_POSTGRESQL

#else // DB_DRIVER_THREAD_SAFE
// Empty test case when thread safety is disabled
#include <catch2/catch_test_macros.hpp>
TEST_CASE("PostgreSQL Thread-Safety Tests (disabled)", "[postgresql_thread_safe]")
{
    SKIP("Thread-safety tests are disabled when DB_DRIVER_THREAD_SAFE=0");
}
#endif // DB_DRIVER_THREAD_SAFE