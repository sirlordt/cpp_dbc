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
 * @file 26_031_example_scylladb_connection_pool.cpp
 * @brief Example demonstrating ScyllaDB connection pooling
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - Creating a ScyllaDB connection pool
 * - Multi-threaded concurrent access
 * - Pool statistics monitoring
 *
 * Usage:
 *   ./scylladb_connection_pool_example [--config=<path>] [--db=<name>] [--help]
 *
 * Examples:
 *   ./scylladb_connection_pool_example                    # Uses dev_scylla from default config
 *   ./scylladb_connection_pool_example --db=test_scylla   # Uses test_scylla configuration
 */

#include "../../common/example_common.hpp"
#include <cpp_dbc/core/columnar/columnar_db_connection_pool.hpp>
#include <thread>
#include <vector>
#include <future>
#include <mutex>
#include <chrono>

#if USE_SCYLLADB
#include <cpp_dbc/drivers/columnar/driver_scylladb.hpp>
#endif

using namespace cpp_dbc::examples;

std::mutex consoleMutex;

#if USE_SCYLLADB
void testPoolConnection(std::shared_ptr<cpp_dbc::ColumnarDBConnectionPool> pool, int threadId, const std::string &keyspace)
{
    try
    {
        auto conn = pool->getColumnarDBConnection();

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Got connection from pool");
        }

        std::string table = keyspace + ".thread_table_" + std::to_string(threadId);

        // Create table for this thread
        conn->executeUpdate("DROP TABLE IF EXISTS " + table);
        conn->executeUpdate(
            "CREATE TABLE " + table + " ("
                                      "id int PRIMARY KEY, "
                                      "thread_id int, "
                                      "name text, "
                                      "value double"
                                      ")");

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Created table " + table);
        }

        // Insert rows
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + table + " (id, thread_id, name, value) VALUES (?, ?, ?, ?)");

        for (int i = 1; i <= 3; ++i)
        {
            pstmt->setInt(1, i);
            pstmt->setInt(2, threadId);
            pstmt->setString(3, "Item " + std::to_string(i) + " from thread " + std::to_string(threadId));
            pstmt->setDouble(4, i * 1.5 + threadId);
            pstmt->executeUpdate();
        }

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Inserted 3 rows");
        }

        // Query the data
        auto rs = conn->executeQuery("SELECT * FROM " + table);
        int rowCount = 0;
        while (rs->next())
        {
            ++rowCount;
        }

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Queried " + std::to_string(rowCount) + " rows");
        }

        // Cleanup
        conn->executeUpdate("DROP TABLE " + table);

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Dropped table, returning connection");
        }

        conn->close();
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        logError("Thread " + std::to_string(threadId) + " error: " + e.what_s());
    }
}

void batchOperations(std::shared_ptr<cpp_dbc::ColumnarDBConnectionPool> pool, const std::string &keyspace)
{
    logMsg("");
    logMsg("--- Batch Operations ---");

    logStep("Performing batch insert...");
    auto conn = pool->getColumnarDBConnection();

    std::string table = keyspace + ".batch_test_table";

    conn->executeUpdate("DROP TABLE IF EXISTS " + table);
    conn->executeUpdate(
        "CREATE TABLE " + table + " ("
                                  "id int PRIMARY KEY, "
                                  "category text, "
                                  "amount double"
                                  ")");

    auto pstmt = conn->prepareStatement(
        "INSERT INTO " + table + " (id, category, amount) VALUES (?, ?, ?)");

    for (int i = 1; i <= 10; ++i)
    {
        pstmt->setInt(1, i);
        pstmt->setString(2, "Category_" + std::to_string(i % 3));
        pstmt->setDouble(3, i * 10.5);
        pstmt->executeUpdate();
    }
    logData("Inserted 10 rows");

    auto rs = conn->executeQuery("SELECT * FROM " + table);
    int count = 0;
    while (rs->next())
    {
        ++count;
    }
    logData("Total rows: " + std::to_string(count));

    conn->executeUpdate("DROP TABLE " + table);
    conn->close();

    logOk("Batch operations completed");
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc ScyllaDB Connection Pool Example");
    logMsg("========================================");
    logMsg("");

#if !USE_SCYLLADB
    logError("ScyllaDB support is not enabled");
    logInfo("Build with -DUSE_SCYLLADB=ON to enable ScyllaDB support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,scylladb");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("scylladb_connection_pool_example", "scylladb");
        return EXIT_OK_;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    // Check for real error (DBException)
    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return EXIT_ERROR_;
    }

    // Check if file was found
    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return EXIT_ERROR_;
    }
    logOk("Configuration loaded successfully");

    auto &configManager = *configResult.value();

    logStep("Getting database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "scylladb");

    // Check for real error
    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return EXIT_ERROR_;
    }

    // Check if config was found
    if (!dbResult.value())
    {
        logError("ScyllaDB configuration not found");
        return EXIT_ERROR_;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) +
          "/" + dbConfig.getDatabase() + ")");

    logStep("Registering ScyllaDB driver...");
    registerDriver("scylladb");
    logOk("Driver registered");

    try
    {
        // ===== Pool Creation =====
        logMsg("");
        logMsg("--- Pool Creation ---");

        logStep("Creating ScyllaDB connection pool...");
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(dbConfig.createConnectionString());
        poolConfig.setUsername(dbConfig.getUsername());
        poolConfig.setPassword(dbConfig.getPassword());
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(10);
        poolConfig.setValidationQuery("SELECT now() FROM system.local");

        auto pool = cpp_dbc::ColumnarDBConnectionPool::create(poolConfig);

        logOk("Connection pool created");
        logData("Active connections: " + std::to_string(pool->getActiveDBConnectionCount()));
        logData("Idle connections: " + std::to_string(pool->getIdleDBConnectionCount()));
        logData("Total connections: " + std::to_string(pool->getTotalDBConnectionCount()));

        // ===== Keyspace Setup =====
        logMsg("");
        logMsg("--- Keyspace Setup ---");

        const std::string keyspace = "test_pool_keyspace";

        logStep("Creating keyspace...");
        {
            auto conn = pool->getColumnarDBConnection();
            conn->executeUpdate(
                "CREATE KEYSPACE IF NOT EXISTS " + keyspace +
                " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
            conn->close();
        }
        logOk("Keyspace '" + keyspace + "' ready");

        // Small delay to allow schema metadata to propagate across cluster nodes
        // This is necessary for ScyllaDB/Cassandra eventual consistency
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // ===== Multi-threaded Access =====
        logMsg("");
        logMsg("--- Multi-threaded Access ---");

        const int numThreads = 4;
        logStep("Starting " + std::to_string(numThreads) + " threads...");

        std::vector<std::future<void>> futures;
        for (int i = 0; i < numThreads; ++i)
        {
            futures.push_back(std::async(std::launch::async, testPoolConnection, pool, i, keyspace));
        }

        logInfo("Waiting for all threads to complete...");

        for (auto &future : futures)
        {
            future.wait();
        }
        logOk("All threads completed");

        // ===== Batch Operations =====
        batchOperations(pool, keyspace);

        // ===== Pool Statistics =====
        logMsg("");
        logMsg("--- Pool Statistics ---");

        logData("Active connections: " + std::to_string(pool->getActiveDBConnectionCount()));
        logData("Idle connections: " + std::to_string(pool->getIdleDBConnectionCount()));
        logData("Total connections: " + std::to_string(pool->getTotalDBConnectionCount()));
        logOk("Statistics retrieved");

        // ===== Stress Test =====
        logMsg("");
        logMsg("--- Stress Test ---");

        logStep("Rapidly acquiring and releasing connections...");
        for (int i = 0; i < 10; ++i)
        {
            auto conn = pool->getColumnarDBConnection();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            conn->close();
        }
        logOk("Stress test completed (10 rapid acquire/release cycles)");

        // ===== Cleanup =====
        logMsg("");
        logMsg("--- Cleanup ---");

        logStep("Dropping keyspace...");
        {
            auto conn = pool->getColumnarDBConnection();
            conn->executeUpdate("DROP KEYSPACE IF EXISTS " + keyspace);
            conn->close();
        }
        logOk("Keyspace dropped");

        logStep("Closing connection pool...");
        pool->close();
        logOk("Connection pool closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        return EXIT_ERROR_;
    }
    catch (const std::exception &e)
    {
        logError("Error: " + std::string(e.what()));
        return EXIT_ERROR_;
    }

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return EXIT_OK_;
#endif
}
