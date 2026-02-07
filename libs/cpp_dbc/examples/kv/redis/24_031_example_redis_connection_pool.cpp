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
 * @file kv_connection_pool_example.cpp
 * @brief Example demonstrating Redis connection pooling
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - Creating a Redis connection pool
 * - Multi-threaded concurrent access
 * - Pool statistics monitoring
 *
 * Usage:
 *   ./kv_connection_pool_example [--config=<path>] [--db=<name>] [--help]
 *
 * Examples:
 *   ./kv_connection_pool_example                    # Uses dev_redis from default config
 *   ./kv_connection_pool_example --db=test_redis   # Uses test_redis configuration
 */

#include "../../common/example_common.hpp"
#include "cpp_dbc/core/kv/kv_db_connection_pool.hpp"
#include <thread>
#include <vector>
#include <future>
#include <mutex>

#if USE_REDIS
#include "cpp_dbc/drivers/kv/driver_redis.hpp"
#endif

using namespace cpp_dbc::examples;

std::mutex consoleMutex;

#if USE_REDIS
void testPoolConnection(std::shared_ptr<cpp_dbc::KVDBConnectionPool> pool, int threadId)
{
    try
    {
        auto conn = pool->getKVDBConnection();

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Got connection from pool");
        }

        // Create test keys unique to this thread
        std::string key = "pool_test_key_" + std::to_string(threadId);
        std::string value = "Hello from thread " + std::to_string(threadId);

        // Set and get a value
        conn->setString(key, value);
        std::string retrieved = conn->getString(key);

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": SET/GET verified, value='" + retrieved + "'");
        }

        // Increment a counter
        std::string counterKey = "pool_counter_" + std::to_string(threadId);
        conn->setString(counterKey, "0");
        int64_t finalValue = 0;
        for (int i = 0; i < 3; ++i)
        {
            finalValue = conn->increment(counterKey);
        }

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Counter incremented to " + std::to_string(finalValue));
        }

        // Cleanup
        conn->deleteKey(key);
        conn->deleteKey(counterKey);

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
    log("========================================");
    log("cpp_dbc Redis Connection Pool Example");
    log("========================================");
    log("");

#if !USE_REDIS
    logError("Redis support is not enabled");
    logInfo("Build with -DUSE_REDIS=ON to enable Redis support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,redis");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("kv_connection_pool_example", "redis");
        return 0;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    // Check for real error (DBException)
    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return 1;
    }

    // Check if file was found
    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return 1;
    }
    logOk("Configuration loaded successfully");

    auto &configManager = *configResult.value();

    logStep("Getting database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "redis");

    // Check for real error
    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return 1;
    }

    // Check if config was found
    if (!dbResult.value())
    {
        logError("Redis configuration not found");
        return 1;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) + ")");

    logStep("Registering Redis driver...");
    registerDriver("redis");
    logOk("Driver registered");

    try
    {
        // ===== Pool Creation =====
        log("");
        log("--- Pool Creation ---");

        logStep("Creating Redis connection pool...");
        std::string url = "cpp_dbc:redis://" + dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) + "/" + dbConfig.getDatabase();
        auto pool = cpp_dbc::Redis::RedisConnectionPool::create(
            url,
            dbConfig.getUsername(),
            dbConfig.getPassword());

        logOk("Connection pool created");
        logData("Active connections: " + std::to_string(pool->getActiveDBConnectionCount()));
        logData("Idle connections: " + std::to_string(pool->getIdleDBConnectionCount()));
        logData("Total connections: " + std::to_string(pool->getTotalDBConnectionCount()));

        // ===== Multi-threaded Access =====
        log("");
        log("--- Multi-threaded Access ---");

        const int numThreads = 6;
        logStep("Starting " + std::to_string(numThreads) + " threads...");

        std::vector<std::future<void>> futures;
        for (int i = 0; i < numThreads; ++i)
        {
            futures.push_back(std::async(std::launch::async, testPoolConnection, pool, i));
        }

        logInfo("Waiting for all threads to complete...");

        for (auto &future : futures)
        {
            future.wait();
        }
        logOk("All threads completed");

        // ===== Final Statistics =====
        log("");
        log("--- Pool Statistics ---");

        logData("Active connections: " + std::to_string(pool->getActiveDBConnectionCount()));
        logData("Idle connections: " + std::to_string(pool->getIdleDBConnectionCount()));
        logData("Total connections: " + std::to_string(pool->getTotalDBConnectionCount()));
        logOk("Statistics retrieved");

        // ===== Cleanup =====
        log("");
        log("--- Cleanup ---");

        logStep("Closing connection pool...");
        pool->close();
        logOk("Connection pool closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        return 1;
    }
    catch (const std::exception &e)
    {
        logError("Error: " + std::string(e.what()));
        return 1;
    }

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return 0;
#endif
}
