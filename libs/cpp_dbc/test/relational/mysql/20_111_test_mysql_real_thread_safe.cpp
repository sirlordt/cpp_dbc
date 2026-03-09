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

 @file 20_111_test_mysql_real_thread_safe.cpp
 @brief Thread-safety stress tests for MySQL database driver

*/

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <mutex>
#include <condition_variable>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/pool/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "20_001_test_mysql_real_common.hpp"

#if DB_DRIVER_THREAD_SAFE && USE_MYSQL

/**
 * @brief Thread-safety stress tests for MySQL driver
 *
 * These tests verify that the MySQL driver is thread-safe by:
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
TEST_CASE("MySQL Thread-Safety Tests", "[20_111_01_mysql_real_thread_safe]")
{
    // Skip these tests if we can't connect to MySQL
    if (!mysql_test_helpers::canConnectToMySQL())
    {
        SKIP("Cannot connect to MySQL database");
        return;
    }

    // Get MySQL configuration
    auto dbConfig = mysql_test_helpers::getMySQLConfig("dev_mysql");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Register the MySQL driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

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

                cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " started");

                try
                {
                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " opening connection...");

                    // Each thread gets its own connection
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

                    if (!conn)
                    {
                        errorCount += opsPerThread;
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " FAILED: dynamic_pointer_cast returned nullptr");
                        return;
                    }

                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " connection opened");

                    for (int j = 0; j < opsPerThread; j++)
                    {
                        //cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " inserting...");
                        try
                        {
                            int id = i * 1000 + j;

                            // Insert operation
                            auto pstmt = conn->prepareStatement("INSERT INTO thread_test (id, value) VALUES (?, ?)");
                            pstmt->setInt(1, id);
                            pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                            pstmt->executeUpdate();

                            //cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " selecting...");

                            // Select operation
                            auto selectStmt = conn->prepareStatement("SELECT * FROM thread_test WHERE id = ?");
                            selectStmt->setInt(1, id);
                            auto rs = selectStmt->executeQuery();

                            if (rs->next())
                            {
                                successCount++;
                                //cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " OK");
                            }
                            else
                            {
                                errorCount++;
                                cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " FAILED: row not found after insert");
                            }
                        }
                        catch (const std::exception &ex)
                        {
                            errorCount++;
                            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " op " + std::to_string(j) + " error: " + std::string(ex.what()));
                        }
                    }

                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " closing connection...");
                    conn->close();
                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " finished");
                }
                catch (const std::exception &ex)
                {
                    errorCount += opsPerThread;
                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " connection error: " + std::string(ex.what()));
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

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Multiple threads with individual connections: " + std::to_string(successCount) + " successes, " + std::to_string(errorCount) + " errors");

        // Clean up
        auto cleanupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        cleanupConn->close();

        // We expect at least 95% of operations to succeed
        REQUIRE(successCount.load() >= (numThreads * opsPerThread * 0.95));
    }

    /* SECTION("Connection pool concurrent access") - moved to 20_141_test_mysql_real_connection_pool.cpp
    {
        // Create connection pool
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUri(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(20);
        poolConfig.setMinIdle(2);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setTestOnBorrow(true);

        auto poolResult = cpp_dbc::MySQL::MySQLConnectionPool::create(std::nothrow, poolConfig);
        if (!poolResult.has_value())
        {
            throw poolResult.error();
        }
        auto pool = poolResult.value();

        // Setup: create test table
        auto setupConn = pool->getRelationalDBConnection();
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        setupConn->executeUpdate("CREATE TABLE thread_test (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE)");
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
                        auto pstmt = conn->prepareStatement("INSERT INTO thread_test (id, name, value) VALUES (?, ?, ?)");
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
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread " + std::to_string(i) + " error: " + std::string(e.what()));
                    }
                } });
        }

        for (auto &t : threads)
        {
            t.join();
        }

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Connection pool concurrent access: " + std::to_string(successCount) + " successes, " + std::to_string(errorCount) + " errors");

        // Clean up
        auto cleanupConn = pool->getRelationalDBConnection();
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        cleanupConn->returnToPool();

        REQUIRE(successCount > 0);
    } */

    /* SECTION("Concurrent read operations with connection pool") - moved to 20_141_test_mysql_real_connection_pool.cpp
    {
        // Create connection pool
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUri(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(20);
        poolConfig.setMinIdle(2);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setTestOnBorrow(true);

        auto poolResult = cpp_dbc::MySQL::MySQLConnectionPool::create(std::nothrow, poolConfig);
        if (!poolResult.has_value())
        {
            throw poolResult.error();
        }
        auto pool = poolResult.value();

        // Setup: create and populate test table
        auto setupConn = pool->getRelationalDBConnection();
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        setupConn->executeUpdate("CREATE TABLE thread_test (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE)");

        // Insert test data
        for (int i = 0; i < 100; i++)
        {
            auto pstmt = setupConn->prepareStatement("INSERT INTO thread_test (id, name, value) VALUES (?, ?, ?)");
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
                        
                        auto pstmt = conn->prepareStatement("SELECT * FROM thread_test WHERE id = ?");
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

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Concurrent read operations: " + std::to_string(readCount) + " reads, " + std::to_string(errorCount) + " errors");

        // Clean up
        auto cleanupConn = pool->getRelationalDBConnection();
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        cleanupConn->returnToPool();

        // Most reads should succeed
        REQUIRE(readCount > numThreads * readsPerThread * 0.95); // At least 95% success rate
    } */

    /* SECTION("High concurrency stress test") - moved to 20_141_test_mysql_real_connection_pool.cpp
    {
        // Create connection pool for stress test
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUri(connStr);
        poolConfig.setUsername(username);
        poolConfig.setPassword(password);
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(20);
        poolConfig.setMinIdle(2);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setTestOnBorrow(true);

        auto poolResult = cpp_dbc::MySQL::MySQLConnectionPool::create(std::nothrow, poolConfig);
        if (!poolResult.has_value())
        {
            throw poolResult.error();
        }
        auto pool = poolResult.value();

        // Create test table
        auto setupConn = pool->getRelationalDBConnection();
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_stress_test");
        setupConn->executeUpdate("CREATE TABLE thread_stress_test (id INT PRIMARY KEY AUTO_INCREMENT, thread_id INT, op_id INT, data VARCHAR(255))");
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
                                    "INSERT INTO thread_stress_test (thread_id, op_id, data) VALUES (?, ?, ?)");
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
                                conn->executeUpdate("UPDATE thread_stress_test SET data = 'updated' WHERE thread_id = " + std::to_string(i) + " LIMIT 1");
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

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "High concurrency stress test completed in " + std::to_string(duration) + " ms");
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "  Inserts: " + std::to_string(insertCount.load()));
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "  Selects: " + std::to_string(selectCount.load()));
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "  Updates: " + std::to_string(updateCount.load()));
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "  Errors: " + std::to_string(errorCount.load()));
        if (duration > 0)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "  Operations per second: " + std::to_string((insertCount + selectCount + updateCount) * 1000.0 / static_cast<double>(duration)));
        }

        // Clean up
        auto cleanupConn = pool->getRelationalDBConnection();
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_stress_test");
        cleanupConn->returnToPool();

        // Most operations should succeed
        int totalOps = insertCount + selectCount + updateCount;
        REQUIRE(totalOps > numThreads * opsPerThread * 0.95); // At least 95% success rate
    } */

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
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Connection error: " + std::string(e.what()));
                    }
                } });
        }

        for (auto &t : threads)
        {
            t.join();
        }

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Rapid connection test: " + std::to_string(successCount) + " successes, " + std::to_string(errorCount) + " errors");

        REQUIRE(successCount > numThreads * connectionsPerThread * 0.95); // At least 95% success rate
    }
}

#else
TEST_CASE("MySQL Thread-Safety Tests (skipped)", "[20_111_02_mysql_real_thread_safe]")
{
    SKIP("MySQL support is not enabled or thread-safety is disabled");
}
#endif // DB_DRIVER_THREAD_SAFE && USE_MYSQL