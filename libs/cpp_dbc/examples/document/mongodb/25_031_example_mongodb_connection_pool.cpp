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
 * @file document_connection_pool_example.cpp
 * @brief Example demonstrating MongoDB connection pooling
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - Creating a MongoDB connection pool
 * - Multi-threaded concurrent access
 * - Pool statistics monitoring
 *
 * Usage:
 *   ./document_connection_pool_example [--config=<path>] [--db=<name>] [--help]
 *
 * Examples:
 *   ./document_connection_pool_example                    # Uses dev_mongodb from default config
 *   ./document_connection_pool_example --db=test_mongodb  # Uses test_mongodb configuration
 */

#include "../../common/example_common.hpp"
#include <cpp_dbc/core/document/document_db_connection_pool.hpp>
#include <thread>
#include <vector>
#include <future>
#include <mutex>

#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#endif

using namespace cpp_dbc::examples;

std::mutex consoleMutex;

#if USE_MONGODB
std::string createTestDocument(int id, const std::string &name, double value)
{
    return "{\"id\": " + std::to_string(id) +
           ", \"name\": \"" + name + "\"" +
           ", \"value\": " + std::to_string(value) + "}";
}

void testPoolConnection(std::shared_ptr<cpp_dbc::DocumentDBConnectionPool> pool, int threadId, const std::string &collectionName)
{
    try
    {
        auto conn = pool->getDocumentDBConnection();

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Got connection from pool");
        }

        auto collection = conn->getCollection(collectionName);

        // Insert a document
        std::string docJson = createTestDocument(
            threadId + 100,
            "Thread Document " + std::to_string(threadId),
            threadId * 10.5);

        auto result = collection->insertOne(docJson);

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Inserted document ID=" + result.insertedId);
        }

        // Find the document
        auto doc = collection->findOne("{\"id\": " + std::to_string(threadId + 100) + "}");
        if (doc)
        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Found document, value=" + std::to_string(doc->getDouble("value")));
        }

        // Small delay to simulate work
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            logData("Thread " + std::to_string(threadId) + ": Returning connection to pool");
        }

        conn->close();
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
    log("cpp_dbc MongoDB Connection Pool Example");
    log("========================================");
    log("");

#if !USE_MONGODB
    logError("MongoDB support is not enabled");
    logInfo("Build with -DUSE_MONGODB=ON to enable MongoDB support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,mongodb");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("document_connection_pool_example", "mongodb");
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
    auto dbResult = getDbConfig(configManager, args.dbName, "mongodb");

    // Check for real error
    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return 1;
    }

    // Check if config was found
    if (!dbResult.value())
    {
        logError("MongoDB configuration not found");
        return 1;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) +
          "/" + dbConfig.getDatabase() + ")");

    try
    {
        // ===== Driver Registration =====
        log("");
        log("--- Driver Registration ---");

        logStep("Registering MongoDB driver...");
        auto driver = std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>();
        cpp_dbc::DriverManager::registerDriver(driver);
        logOk("MongoDB driver registered");

        // ===== Pool Creation =====
        log("");
        log("--- Pool Creation ---");

        logStep("Creating MongoDB connection pool...");

        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.withDatabaseConfig(dbConfig);  // Automatically sets URL, user, password, AND options
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(10);
        poolConfig.setValidationQuery("{\"ping\": 1}");

        auto pool = cpp_dbc::MongoDB::MongoDBConnectionPool::create(poolConfig);

        logOk("Connection pool created");
        logData("Active connections: " + std::to_string(pool->getActiveDBConnectionCount()));
        logData("Idle connections: " + std::to_string(pool->getIdleDBConnectionCount()));
        logData("Total connections: " + std::to_string(pool->getTotalDBConnectionCount()));

        // ===== Collection Setup =====
        log("");
        log("--- Collection Setup ---");

        const std::string testCollectionName = "connection_pool_example";

        logStep("Setting up test collection...");
        {
            auto conn = pool->getDocumentDBConnection();
            if (conn->collectionExists(testCollectionName))
            {
                conn->dropCollection(testCollectionName);
            }
            conn->createCollection(testCollectionName);
            conn->close();
        }
        logOk("Collection '" + testCollectionName + "' ready");

        // ===== Basic Operations =====
        log("");
        log("--- Basic Operations ---");

        logStep("Performing basic document operations...");
        {
            auto conn = pool->getDocumentDBConnection();
            auto collection = conn->getCollection(testCollectionName);

            std::string docJson = createTestDocument(1, "Test Document", 42.5);
            auto insertResult = collection->insertOne(docJson);
            logData("Inserted document ID: " + insertResult.insertedId);

            auto doc = collection->findById(insertResult.insertedId);
            if (doc)
            {
                logData("Found document: " + doc->toJson());
            }

            auto updateResult = collection->updateOne(
                "{\"id\": 1}",
                "{\"$set\": {\"value\": 99.9}}");
            logData("Updated " + std::to_string(updateResult.modifiedCount) + " document(s)");

            conn->close();
        }
        logOk("Basic operations completed");

        // ===== Multi-threaded Access =====
        log("");
        log("--- Multi-threaded Access ---");

        const int numThreads = 5;
        logStep("Starting " + std::to_string(numThreads) + " threads...");

        std::vector<std::future<void>> futures;
        for (int i = 0; i < numThreads; ++i)
        {
            futures.push_back(std::async(std::launch::async, testPoolConnection, pool, i, testCollectionName));
        }

        logInfo("Waiting for all threads to complete...");

        for (auto &future : futures)
        {
            future.wait();
        }
        logOk("All threads completed");

        // ===== Verify Results =====
        log("");
        log("--- Verify Results ---");

        logStep("Counting documents...");
        {
            auto conn = pool->getDocumentDBConnection();
            auto collection = conn->getCollection(testCollectionName);
            auto count = collection->countDocuments();
            logData("Total documents in collection: " + std::to_string(count));
            conn->close();
        }
        logOk("Results verified");

        // ===== Pool Statistics =====
        log("");
        log("--- Pool Statistics ---");

        logData("Active connections: " + std::to_string(pool->getActiveDBConnectionCount()));
        logData("Idle connections: " + std::to_string(pool->getIdleDBConnectionCount()));
        logData("Total connections: " + std::to_string(pool->getTotalDBConnectionCount()));
        logOk("Statistics retrieved");

        // ===== Cleanup =====
        log("");
        log("--- Cleanup ---");

        logStep("Dropping test collection...");
        {
            auto conn = pool->getDocumentDBConnection();
            if (conn->collectionExists(testCollectionName))
            {
                conn->dropCollection(testCollectionName);
            }
            conn->close();
        }
        logOk("Collection dropped");

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
