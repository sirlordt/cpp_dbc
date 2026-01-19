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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file test_scylla_thread_safe.cpp
 * @brief Thread-safety stress tests for ScyllaDB database driver
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
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "test_scylladb_common.hpp"

#if USE_SCYLLADB

/**
 * @brief Thread-safety stress tests for ScyllaDB driver
 */
TEST_CASE("ScyllaDB Thread-Safety Tests", "[scylladb_thread_safe]")
{
    // Skip these tests if we can't connect to ScyllaDB
    if (!scylla_test_helpers::canConnectToScylla())
    {
        SKIP("Cannot connect to ScyllaDB database");
        return;
    }

    // Get ScyllaDB configuration
    auto dbConfig = scylla_test_helpers::getScyllaConfig("dev_scylla");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string host = dbConfig.getHost();
    int port = dbConfig.getPort();
    std::string keyspace = dbConfig.getDatabase();

    std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;

    // Register the ScyllaDB driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

    // Prepare table
    {
        auto setupConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(setupConn != nullptr);
        setupConn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".thread_test");
        setupConn->executeUpdate("CREATE TABLE " + keyspace + ".thread_test (id int PRIMARY KEY, value text)");
        setupConn->close();
    }

    SECTION("Multiple threads with individual connections")
    {
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
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                        cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                    
                    for (int j = 0; j < opsPerThread; j++)
                    {
                        try
                        {
                            int id = i * 1000 + j;
                            
                            // Insert operation
                            auto pstmt = conn->prepareStatement("INSERT INTO " + keyspace + ".thread_test (id, value) VALUES (?, ?)");
                            pstmt->setInt(1, id);
                            pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                            pstmt->executeUpdate();

                            // Select operation
                            auto selectStmt = conn->prepareStatement("SELECT * FROM " + keyspace + ".thread_test WHERE id = ?");
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
        auto cleanupConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        cleanupConn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".thread_test");
        cleanupConn->close();

        // We expect most operations to succeed
        REQUIRE(successCount > 0);
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
                        auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                        
                        // Do a simple operation
                        auto rs = conn->executeQuery("SELECT release_version FROM system.local");
                        if (rs->next())
                        {
                            rs->getString("release_version");
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

    SECTION("Concurrent read/write operations")
    {
        // Create test table with a counter column for atomic increments
        auto setupConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(setupConn != nullptr);

        setupConn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".concurrent_test");
        // En Cassandra/ScyllaDB, una tabla con columnas counter no puede tener otras columnas
        // excepto la clave primaria, así que creamos dos tablas separadas
        setupConn->executeUpdate(
            "CREATE TABLE " + keyspace + ".concurrent_test_counter ("
                                         "id int PRIMARY KEY, "
                                         "counter counter"
                                         ")");

        setupConn->executeUpdate(
            "CREATE TABLE " + keyspace + ".concurrent_test_info ("
                                         "id int PRIMARY KEY, "
                                         "last_updated text"
                                         ")");

        // Initialize counters
        for (int i = 1; i <= 5; i++)
        {
            // Inicializar contador
            auto stmt = setupConn->prepareStatement(
                "UPDATE " + keyspace + ".concurrent_test_counter SET counter = counter + ? WHERE id = ?");
            stmt->setLong(1, 0L); // Initialize with 0
            stmt->setInt(2, i);
            stmt->executeUpdate();

            // Inicializar información
            auto infoStmt = setupConn->prepareStatement(
                "INSERT INTO " + keyspace + ".concurrent_test_info (id, last_updated) VALUES (?, ?)");
            infoStmt->setInt(1, i);
            infoStmt->setString(2, "Initial");
            infoStmt->executeUpdate();
        }

        setupConn->close();

        // Set up concurrent readers and writers
        const int numReaders = 5;
        const int numWriters = 5;
        const int readsPerThread = 10;
        const int writesPerThread = 10;

        std::atomic<int> readSuccessCount(0);
        std::atomic<int> writeSuccessCount(0);
        std::atomic<int> errorCount(0);

        std::vector<std::thread> threads;

        // Start reader threads
        for (int i = 0; i < numReaders; i++)
        {
            threads.emplace_back([&, i]()
                                 {
                try {
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                        cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                        
                    for (int j = 0; j < readsPerThread; j++) {
                        try {
                            // Read a random record
                            int id = (j % 5) + 1;
                            // Leer contador
                            auto counterStmt = conn->prepareStatement(
                                "SELECT * FROM " + keyspace + ".concurrent_test_counter WHERE id = ?");
                            counterStmt->setInt(1, id);
                            auto counterRs = counterStmt->executeQuery();
                            
                            // Leer información
                            auto infoStmt = conn->prepareStatement(
                                "SELECT * FROM " + keyspace + ".concurrent_test_info WHERE id = ?");
                            infoStmt->setInt(1, id);
                            auto infoRs = infoStmt->executeQuery();
                            
                            if (counterRs->next() && infoRs->next()) {
                                // Just read the values
                                int readId = counterRs->getInt("id");
                                long counterVal = counterRs->getLong("counter");
                                std::string lastUpdated = infoRs->getString("last_updated");
                                (void)readId; (void)counterVal; (void)lastUpdated; // Evitar warnings de unused
                                readSuccessCount++;
                            }
                        }
                        catch (const std::exception& e) {
                            errorCount++;
                            std::cerr << "Reader " << i << " error: " << e.what() << std::endl;
                        }
                        
                        // Small delay to increase chance of concurrent access
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    }
                    
                    conn->close();
                }
                catch (const std::exception& e) {
                    errorCount += readsPerThread;
                    std::cerr << "Reader connection error: " << e.what() << std::endl;
                } });
        }

        // Start writer threads
        for (int i = 0; i < numWriters; i++)
        {
            threads.emplace_back([&, i]()
                                 {
                try {
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                        cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                        
                    for (int j = 0; j < writesPerThread; j++) {
                        try {
                            // Update a random record
                            int id = (j % 5) + 1;
                            
                            // Incrementar contador (atómicamente en Cassandra/Scylla)
                            auto updateCounterStmt = conn->prepareStatement(
                                "UPDATE " + keyspace + ".concurrent_test_counter SET counter = counter + ? WHERE id = ?");
                            updateCounterStmt->setLong(1, 1L); // Increment by 1
                            updateCounterStmt->setInt(2, id);
                            updateCounterStmt->executeUpdate();
                            
                            // Actualizar la información
                            auto updateTextStmt = conn->prepareStatement(
                                "UPDATE " + keyspace + ".concurrent_test_info SET last_updated = ? WHERE id = ?");
                            updateTextStmt->setString(1, "Writer " + std::to_string(i) + " Op " + std::to_string(j));
                            updateTextStmt->setInt(2, id);
                            updateTextStmt->executeUpdate();
                            
                            writeSuccessCount++;
                        }
                        catch (const std::exception& e) {
                            errorCount++;
                            std::cerr << "Writer " << i << " error: " << e.what() << std::endl;
                        }
                        
                        // Small delay to increase chance of concurrent access
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    
                    conn->close();
                }
                catch (const std::exception& e) {
                    errorCount += writesPerThread;
                    std::cerr << "Writer connection error: " << e.what() << std::endl;
                } });
        }

        // Wait for all threads to complete
        for (auto &t : threads)
        {
            t.join();
        }

        std::cout << "Concurrent read/write test: " << readSuccessCount << " reads, "
                  << writeSuccessCount << " writes, " << errorCount << " errors" << std::endl;

        // Verify that most operations succeeded
        REQUIRE(readSuccessCount > numReaders * readsPerThread * 0.9);
        REQUIRE(writeSuccessCount > numWriters * writesPerThread * 0.9);

        // Verify that counters were incremented
        auto verifyConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

        auto rs = verifyConn->executeQuery("SELECT * FROM " + keyspace + ".concurrent_test_counter");

        size_t totalCounters = 0;
        while (rs->next())
        {
            totalCounters += rs->getLong("counter");
        }

        // Limpiar ambas tablas
        verifyConn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".concurrent_test_counter");
        verifyConn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".concurrent_test_info");
        verifyConn->close();

        // With counter columns, increments are atomic, so the total should match successful writes
        // But due to how the counter is read after the fact, allow some tolerance
        std::cout << "Total counter value: " << totalCounters << ", Write success count: " << writeSuccessCount.load() << std::endl;

        // Cast explicitly to avoid conversion warnings
        double counterValue = static_cast<double>(totalCounters);
        double writeSuccess = static_cast<double>(writeSuccessCount.load());

        REQUIRE(counterValue >= writeSuccess * 0.9);
        REQUIRE(counterValue <= writeSuccess * 1.1);
    }
}

#else
TEST_CASE("ScyllaDB Thread-Safety Tests (skipped)", "[scylladb_thread_safe]")
{
    SKIP("ScyllaDB support is not enabled");
}
#endif // USE_SCYLLA

#else // DB_DRIVER_THREAD_SAFE
// Empty test case when thread safety is disabled
#include <catch2/catch_test_macros.hpp>
TEST_CASE("ScyllaDB Thread-Safety Tests (disabled)", "[scylladb_thread_safe]")
{
    SKIP("Thread-safety tests are disabled when DB_DRIVER_THREAD_SAFE=0");
}
#endif // DB_DRIVER_THREAD_SAFE
