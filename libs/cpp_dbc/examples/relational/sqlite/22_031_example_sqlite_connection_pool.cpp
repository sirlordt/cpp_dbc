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
 * @file 22_031_example_sqlite_connection_pool.cpp
 * @brief SQLite-specific example demonstrating connection pooling
 *
 * This example demonstrates:
 * - Creating a SQLite connection pool
 * - Concurrent multi-threaded database access
 * - Pool statistics monitoring
 * - Proper pool lifecycle management
 *
 * Note: SQLite has specific considerations for multi-threaded access.
 * This example uses WAL mode for better concurrent performance.
 *
 * Usage:
 *   ./22_031_example_sqlite_connection_pool [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - SQLite support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include "cpp_dbc/core/relational/relational_db_connection_pool.hpp"
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <mutex>

#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_SQLITE
std::mutex consoleMutex;

// Function to simulate a database operation
void performDatabaseOperation(cpp_dbc::RelationalDBConnectionPool &pool, int threadId)
{
    try
    {
        // Simulate random delay before requesting connection
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));

        // Get connection from pool
        auto conn = pool.getRelationalDBConnection();

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Got connection from pool");
        }

        // Simulate database operation
        auto resultSet = conn->executeQuery("SELECT 1 as test_value");

        if (resultSet->next())
        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Query returned: " +
                    std::to_string(resultSet->getInt("test_value")));
        }

        // Simulate more work with the connection
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 150));

        // Connection will be returned to pool when it goes out of scope
        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Returning connection to pool");
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        logError("Thread " + std::to_string(threadId) + " error: " + e.what_s());
    }
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc SQLite Connection Pool Example");
    logMsg("========================================");
    logMsg("");

#if !USE_SQLITE
    logError("SQLite support is not enabled");
    logInfo("Build with -DUSE_SQLITE=ON to enable SQLite support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,sqlite");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    // Seed the random number generator
    srand(static_cast<unsigned int>(time(nullptr)));

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("22_031_example_sqlite_connection_pool", "sqlite");
        return EXIT_OK_;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return EXIT_ERROR_;
    }
    logOk("Configuration loaded successfully");

    auto &configManager = *configResult.value();

    logStep("Getting database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "sqlite");

    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!dbResult.value())
    {
        logError("SQLite configuration not found");
        return EXIT_ERROR_;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" + dbConfig.getDatabase() + ")");

    logStep("Registering SQLite driver...");
    registerDriver("sqlite");
    logOk("Driver registered");

    try
    {
        // ===== Pool Configuration =====
        logMsg("");
        logMsg("--- Pool Configuration ---");

        logStep("Configuring connection pool...");
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUrl(dbConfig.createConnectionString());
        poolConfig.setUsername(dbConfig.getUsername());
        poolConfig.setPassword(dbConfig.getPassword());
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(10);
        poolConfig.setValidationQuery("SELECT 1");

        logInfo("Initial size: 3");
        logInfo("Max size: 10");
        logInfo("Validation query: SELECT 1");
        logOk("Pool configuration ready");

        // ===== Create Pool =====
        logMsg("");
        logMsg("--- Pool Creation ---");

        logStep("Creating SQLite connection pool...");
        auto pool = cpp_dbc::SQLite::SQLiteConnectionPool::create(poolConfig);

        if (!pool)
        {
            logError("Failed to create SQLite connection pool");
            return EXIT_ERROR_;
        }

        logOk("Connection pool created");
        logData("Initial idle connections: " + std::to_string(pool->getIdleDBConnectionCount()));

        // Enable WAL mode for better concurrent access
        logMsg("");
        logMsg("--- Enabling WAL Mode ---");
        logStep("Setting journal_mode to WAL for better concurrency...");
        {
            auto conn = pool->getRelationalDBConnection();
            conn->executeUpdate("PRAGMA journal_mode=WAL");
            logOk("WAL mode enabled");
        }

        // ===== Multi-threaded Access =====
        logMsg("");
        logMsg("--- Multi-threaded Access ---");

        const int numThreads = 6;
        logStep("Starting " + std::to_string(numThreads) + " threads...");
        logInfo("Note: SQLite handles concurrency differently than server databases");

        std::vector<std::thread> threads;
        for (int i = 0; i < numThreads; ++i)
        {
            threads.push_back(std::thread(performDatabaseOperation, std::ref(*pool), i));
        }

        logInfo("Waiting for all threads to complete...");

        for (auto &thread : threads)
        {
            thread.join();
        }
        logOk("All threads completed");

        // ===== Pool Statistics =====
        logMsg("");
        logMsg("--- Pool Statistics ---");

        logData("Active connections: " + std::to_string(pool->getActiveDBConnectionCount()));
        logData("Idle connections: " + std::to_string(pool->getIdleDBConnectionCount()));
        logData("Total connections: " + std::to_string(pool->getTotalDBConnectionCount()));
        logOk("Statistics retrieved");

        // ===== Cleanup =====
        logMsg("");
        logMsg("--- Cleanup ---");

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
