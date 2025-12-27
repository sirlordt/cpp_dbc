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

 @file test_sqlite_thread_safe.cpp
 @brief Thread-safety stress tests for SQLite database driver

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
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>

#include "test_sqlite_common.hpp"

#if USE_SQLITE

/**
 * @brief Thread-safety stress tests for SQLite driver
 *
 * These tests verify that the SQLite driver is thread-safe by:
 * 1. Multiple threads each with their own connection to a file-based database
 * 2. Connection pool under high concurrency
 * 3. Rapid connection open/close from multiple threads
 * 4. Stress testing with high concurrency using connection pool
 *
 * Note: SQLite has specific threading modes (single-thread, multi-thread, serialized)
 * These tests assume SQLite is compiled with SQLITE_THREADSAFE=1 (serialized mode)
 *
 * Each thread should have its own connection for proper thread-safety.
 * Sharing a single connection across threads is not recommended even with
 * thread-safe drivers, as it can lead to logical issues with transactions
 * and statement state.
 */
TEST_CASE("SQLite Thread-Safety Tests", "[sqlite_thread_safe]")
{
    // Get SQLite configuration using the helper function
    auto dbConfig = sqlite_test_helpers::getSQLiteConfig("test_sqlite");
    std::string connStr = dbConfig.createConnectionString();

    // Register the SQLite driver
    cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

    SECTION("Multiple threads with individual connections")
    {
        // Setup: create test table using a single connection
        auto setupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));
        REQUIRE(setupConn != nullptr);
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        setupConn->executeUpdate("CREATE TABLE thread_test (id INTEGER PRIMARY KEY, value TEXT)");
        setupConn->close();

        const int numThreads = 5;
        const int opsPerThread = 10;
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
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));
                    
                    // Enable WAL mode for better concurrency
                    conn->executeUpdate("PRAGMA journal_mode=WAL");
                    
                    for (int j = 0; j < opsPerThread; j++)
                    {
                        try
                        {
                            int id = i * 1000 + j;
                            
                            // Insert operation with retry for SQLITE_BUSY
                            bool inserted = false;
                            for (int retry = 0; retry < 5 && !inserted; retry++)
                            {
                                try
                                {
                                    auto pstmt = conn->prepareStatement("INSERT INTO thread_test (id, value) VALUES (?, ?)");
                                    pstmt->setInt(1, id);
                                    pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                                    pstmt->executeUpdate();
                                    inserted = true;
                                }
                                catch (const std::exception& e)
                                {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(10 * (retry + 1)));
                                }
                            }

                            if (inserted)
                            {
                                // Select operation
                                auto selectStmt = conn->prepareStatement("SELECT * FROM thread_test WHERE id = ?");
                                selectStmt->setInt(1, id);
                                auto rs = selectStmt->executeQuery();
                                
                                if (rs->next())
                                {
                                    successCount++;
                                }
                            }
                            else
                            {
                                errorCount++;
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
        auto cleanupConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));
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
        poolConfig.setUsername("");
        poolConfig.setPassword("");
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(10);
        poolConfig.setMinIdle(1);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setValidationQuery("SELECT 1");
        // SQLite only supports SERIALIZABLE isolation level
        poolConfig.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

        auto pool = std::make_shared<cpp_dbc::SQLite::SQLiteConnectionPool>(poolConfig);

        // Setup: create test table
        auto setupConn = pool->getDBConnection();
        setupConn->executeUpdate("PRAGMA journal_mode=WAL");
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        setupConn->executeUpdate("CREATE TABLE thread_test (id INTEGER PRIMARY KEY, name TEXT, value REAL)");
        setupConn->returnToPool();

        const int numThreads = 5;
        const int opsPerThread = 10;
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
                        auto conn = pool->getDBConnection();
                        int id = idCounter.fetch_add(1);
                        
                        // Insert with prepared statement and retry
                        bool success = false;
                        for (int retry = 0; retry < 5 && !success; retry++)
                        {
                            try
                            {
                                auto pstmt = conn->prepareStatement("INSERT INTO thread_test (id, name, value) VALUES (?, ?, ?)");
                                pstmt->setInt(1, id);
                                pstmt->setString(2, "Name " + std::to_string(id));
                                pstmt->setDouble(3, id * 1.5);
                                pstmt->executeUpdate();
                                success = true;
                            }
                            catch (const std::exception& e)
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(10 * (retry + 1)));
                            }
                        }
                        
                        conn->returnToPool();
                        
                        if (success)
                        {
                            successCount++;
                        }
                        else
                        {
                            errorCount++;
                        }
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
        auto cleanupConn = pool->getDBConnection();
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        cleanupConn->returnToPool();

        REQUIRE(successCount > 0);
    }

    SECTION("Concurrent read operations with connection pool")
    {
        // Create connection pool
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername("");
        poolConfig.setPassword("");
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(10);
        poolConfig.setMinIdle(1);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setValidationQuery("SELECT 1");
        // SQLite only supports SERIALIZABLE isolation level
        poolConfig.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

        auto pool = std::make_shared<cpp_dbc::SQLite::SQLiteConnectionPool>(poolConfig);

        // Setup: create and populate test table
        auto setupConn = pool->getDBConnection();
        setupConn->executeUpdate("PRAGMA journal_mode=WAL");
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        setupConn->executeUpdate("CREATE TABLE thread_test (id INTEGER PRIMARY KEY, name TEXT, value REAL)");

        // Insert test data
        for (int i = 0; i < 50; i++)
        {
            auto pstmt = setupConn->prepareStatement("INSERT INTO thread_test (id, name, value) VALUES (?, ?, ?)");
            pstmt->setInt(1, i);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->executeUpdate();
        }
        setupConn->returnToPool();

        const int numThreads = 5;
        const int readsPerThread = 20;
        std::atomic<int> readCount(0);
        std::atomic<int> errorCount(0);
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++)
        {
            threads.emplace_back([&, i]()
                                 {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> idDist(0, 49);

                for (int j = 0; j < readsPerThread; j++)
                {
                    try
                    {
                        auto conn = pool->getDBConnection();
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

        std::cout << "Concurrent read operations: " << readCount << " reads, " << errorCount << " errors" << std::endl;

        // Clean up
        auto cleanupConn = pool->getDBConnection();
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_test");
        cleanupConn->returnToPool();

        // Most reads should succeed
        REQUIRE(readCount > numThreads * readsPerThread * 0.8);
    }

    SECTION("High concurrency stress test")
    {
        // Create connection pool for stress test
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(connStr);
        poolConfig.setUsername("");
        poolConfig.setPassword("");
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(10);
        poolConfig.setMinIdle(1);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(1000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setValidationQuery("SELECT 1");
        // SQLite only supports SERIALIZABLE isolation level
        poolConfig.setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

        auto pool = std::make_shared<cpp_dbc::SQLite::SQLiteConnectionPool>(poolConfig);

        // Create test table
        auto setupConn = pool->getDBConnection();
        setupConn->executeUpdate("PRAGMA journal_mode=WAL");
        setupConn->executeUpdate("DROP TABLE IF EXISTS thread_stress_test");
        setupConn->executeUpdate("CREATE TABLE thread_stress_test (id INTEGER PRIMARY KEY AUTOINCREMENT, thread_id INTEGER, op_id INTEGER, data TEXT)");
        setupConn->returnToPool();

        const int numThreads = 10;
        const int opsPerThread = 20;
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
                        auto conn = pool->getDBConnection();
                        int op = opDist(gen);

                        bool success = false;
                        for (int retry = 0; retry < 5 && !success; retry++)
                        {
                            try
                            {
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
                                        success = true;
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
                                        success = true;
                                        break;
                                    }
                                    case 2: // Update
                                    {
                                        conn->executeUpdate("UPDATE thread_stress_test SET data = 'updated' WHERE thread_id = " + std::to_string(i) + " AND id IN (SELECT id FROM thread_stress_test WHERE thread_id = " + std::to_string(i) + " LIMIT 1)");
                                        updateCount++;
                                        success = true;
                                        break;
                                    }
                                }
                            }
                            catch (const std::exception& e)
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(10 * (retry + 1)));
                            }
                        }

                        conn->returnToPool();
                        
                        if (!success)
                        {
                            errorCount++;
                        }
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
        auto cleanupConn = pool->getDBConnection();
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS thread_stress_test");
        cleanupConn->returnToPool();

        // Most operations should succeed (SQLite may have more contention)
        int totalOps = insertCount + selectCount + updateCount;
        REQUIRE(totalOps > numThreads * opsPerThread * 0.5); // At least 50% success rate for SQLite
    }

    SECTION("Rapid connection open/close stress test")
    {
        const int numThreads = 5;
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
                        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));
                        
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
TEST_CASE("SQLite Thread-Safety Tests (skipped)", "[sqlite_thread_safe]")
{
    SKIP("SQLite support is not enabled");
}
#endif // USE_SQLITE

#else // DB_DRIVER_THREAD_SAFE
// Empty test case when thread safety is disabled
#include <catch2/catch_test_macros.hpp>
TEST_CASE("SQLite Thread-Safety Tests (disabled)", "[sqlite_thread_safe]")
{
    SKIP("Thread-safety tests are disabled when DB_DRIVER_THREAD_SAFE=0");
}
#endif // DB_DRIVER_THREAD_SAFE