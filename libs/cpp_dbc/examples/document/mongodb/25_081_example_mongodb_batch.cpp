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
 * @file 25_081_example_mongodb_batch.cpp
 * @brief MongoDB-specific example demonstrating batch/bulk operations
 *
 * This example demonstrates:
 * - Bulk insert with insertMany()
 * - Bulk update with updateMany()
 * - Bulk delete with deleteMany()
 * - Performance comparison: individual vs batch operations
 * - Ordered vs unordered batch operations
 *
 * Usage:
 *   ./25_081_example_mongodb_batch [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - MongoDB support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <cpp_dbc/core/document/document_db_connection.hpp>
#include <chrono>
#include <vector>

#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_MONGODB
void demonstrateInsertMany(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("--- Bulk Insert with insertMany() ---");
    logInfo("Inserting multiple documents in a single operation");

    auto collection = conn->getCollection("batch_test_products");

    // Clean up first
    logStep("Cleaning up existing documents...");
    collection->deleteMany("{}");
    logOk("Collection cleared");

    // Create documents to insert
    logStep("Creating documents for batch insert...");
    std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> documents;

    for (int i = 1; i <= 10; ++i)
    {
        std::string json = R"({"name": "Product )" + std::to_string(i) +
                           R"(", "price": )" + std::to_string(i * 10.0) +
                           R"(, "category": ")" + (i % 2 == 0 ? "electronics" : "clothing") +
                           R"(", "stock": )" + std::to_string(i * 5) + "}";
        documents.push_back(conn->createDocument(json));
    }
    logData("Created " + std::to_string(documents.size()) + " documents");

    logStep("Inserting documents with insertMany()...");
    auto result = collection->insertMany(documents);
    logData("Inserted count: " + std::to_string(result.insertedCount));
    logData("Acknowledged: " + std::string(result.acknowledged ? "true" : "false"));
    logOk("Bulk insert completed");

    // Verify
    logStep("Verifying inserted documents...");
    uint64_t count = collection->countDocuments("{}");
    logData("Document count: " + std::to_string(count));
    logOk("Verification completed");
}

void demonstrateUpdateMany(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("--- Bulk Update with updateMany() ---");
    logInfo("Updating multiple documents matching a filter");

    auto collection = conn->getCollection("batch_test_products");

    // Update all electronics products
    logStep("Updating all electronics products (add discount field)...");
    auto result = collection->updateMany(
        R"({"category": "electronics"})",
        R"({"$set": {"discount": 0.15, "on_sale": true}})");

    logData("Matched count: " + std::to_string(result.matchedCount));
    logData("Modified count: " + std::to_string(result.modifiedCount));
    logOk("Bulk update completed");

    // Verify updates
    logStep("Verifying updated documents...");
    auto cursor = collection->find(R"({"category": "electronics"})");
    int count = 0;
    while (cursor->hasNext())
    {
        auto doc = cursor->nextDocument();
        logData("Product: " + doc->getString("name") +
                ", discount: " + std::to_string(doc->getDouble("discount")));
        ++count;
    }
    logData("Total updated: " + std::to_string(count));
    logOk("Verification completed");

    // Update with increment
    logMsg("");
    logStep("Incrementing stock for all products...");
    result = collection->updateMany(
        "{}",
        R"({"$inc": {"stock": 10}})");
    logData("Matched count: " + std::to_string(result.matchedCount));
    logData("Modified count: " + std::to_string(result.modifiedCount));
    logOk("Stock increment completed");
}

void demonstrateDeleteMany(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("--- Bulk Delete with deleteMany() ---");
    logInfo("Deleting multiple documents matching a filter");

    auto collection = conn->getCollection("batch_test_products");

    // Count before delete
    logStep("Counting documents before delete...");
    uint64_t beforeCount = collection->countDocuments("{}");
    logData("Documents before: " + std::to_string(beforeCount));

    // Delete all clothing products
    logStep("Deleting all clothing products...");
    auto result = collection->deleteMany(R"({"category": "clothing"})");
    logData("Deleted count: " + std::to_string(result.deletedCount));
    logOk("Bulk delete completed");

    // Count after delete
    logStep("Counting documents after delete...");
    uint64_t afterCount = collection->countDocuments("{}");
    logData("Documents after: " + std::to_string(afterCount));
    logData("Documents removed: " + std::to_string(beforeCount - afterCount));
    logOk("Verification completed");
}

void demonstratePerformanceComparison(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("--- Performance Comparison ---");
    logInfo("Comparing individual inserts vs batch insert");

    auto collection = conn->getCollection("batch_perf_test");

    // Clean up
    collection->deleteMany("{}");

    const int numDocs = 100;

    // Individual inserts
    logStep("Individual inserts (" + std::to_string(numDocs) + " documents)...");
    auto startIndividual = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < numDocs; ++i)
    {
        std::string json = R"({"index": )" + std::to_string(i) +
                           R"(, "value": "individual_)" + std::to_string(i) + R"("})";
        collection->insertOne(json);
    }

    auto endIndividual = std::chrono::high_resolution_clock::now();
    auto durationIndividual = std::chrono::duration_cast<std::chrono::milliseconds>(
        endIndividual - startIndividual);
    logData("Individual inserts time: " + std::to_string(durationIndividual.count()) + " ms");

    // Clean up for batch test
    collection->deleteMany("{}");

    // Batch insert
    logStep("Batch insert (" + std::to_string(numDocs) + " documents)...");
    std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> documents;
    for (int i = 0; i < numDocs; ++i)
    {
        std::string json = R"({"index": )" + std::to_string(i) +
                           R"(, "value": "batch_)" + std::to_string(i) + R"("})";
        documents.push_back(conn->createDocument(json));
    }

    auto startBatch = std::chrono::high_resolution_clock::now();
    collection->insertMany(documents);
    auto endBatch = std::chrono::high_resolution_clock::now();
    auto durationBatch = std::chrono::duration_cast<std::chrono::milliseconds>(
        endBatch - startBatch);
    logData("Batch insert time: " + std::to_string(durationBatch.count()) + " ms");

    // Calculate speedup
    if (durationBatch.count() > 0)
    {
        double speedup = static_cast<double>(durationIndividual.count()) / static_cast<double>(durationBatch.count());
        logData("Speedup factor: " + std::to_string(speedup) + "x");
    }
    logOk("Performance comparison completed");

    // Clean up
    collection->deleteMany("{}");
}

void demonstrateOrderedVsUnordered(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("--- Ordered vs Unordered Operations ---");
    logInfo("Ordered stops on first error, unordered continues");

    // Clean up and create unique index
    logStep("Setting up collection with unique index...");

    // Drop collection if exists to ensure clean state
    if (conn->collectionExists("batch_ordered_test"))
    {
        conn->dropCollection("batch_ordered_test");
    }

    // Recreate collection and add unique index
    conn->createCollection("batch_ordered_test");
    auto collection = conn->getCollection("batch_ordered_test");
    collection->createIndex(R"({"email": 1})", R"({"unique": true})");
    logOk("Unique index created on 'email' field");

    // Ordered insert (default behavior - stops on first error)
    logMsg("");
    logStep("Demonstrating ORDERED insert (default)...");
    logInfo("Ordered inserts stop on first duplicate");

    std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> orderedDocs;
    orderedDocs.push_back(conn->createDocument(R"({"email": "user1@test.com", "name": "User 1"})"));
    orderedDocs.push_back(conn->createDocument(R"({"email": "user1@test.com", "name": "Duplicate"})"));
    orderedDocs.push_back(conn->createDocument(R"({"email": "user2@test.com", "name": "User 2"})"));

    try
    {
        cpp_dbc::DocumentWriteOptions orderedOpts;
        orderedOpts.ordered = true;
        auto result = collection->insertMany(orderedDocs, orderedOpts);
        logData("Inserted: " + std::to_string(result.insertedCount));
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Ordered insert failed (expected): " + e.what_s());
    }

    uint64_t orderedCount = collection->countDocuments("{}");
    logData("Documents after ordered insert: " + std::to_string(orderedCount));
    logInfo("Only first document was inserted before error stopped the operation");

    // Clean up for unordered test
    collection->deleteMany("{}");

    // Unordered insert (continues on errors)
    logMsg("");
    logStep("Demonstrating UNORDERED insert...");
    logInfo("Unordered inserts continue despite errors");

    std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> unorderedDocs;
    unorderedDocs.push_back(conn->createDocument(R"({"email": "userA@test.com", "name": "User A"})"));
    unorderedDocs.push_back(conn->createDocument(R"({"email": "userA@test.com", "name": "Duplicate A"})"));
    unorderedDocs.push_back(conn->createDocument(R"({"email": "userB@test.com", "name": "User B"})"));
    unorderedDocs.push_back(conn->createDocument(R"({"email": "userC@test.com", "name": "User C"})"));

    try
    {
        cpp_dbc::DocumentWriteOptions unorderedOpts;
        unorderedOpts.ordered = false;
        auto result = collection->insertMany(unorderedDocs, unorderedOpts);
        logData("Inserted: " + std::to_string(result.insertedCount));
    }
    catch (const cpp_dbc::DBException &e)
    {
        logInfo("Unordered insert reported error but may have inserted some docs");
    }

    uint64_t unorderedCount = collection->countDocuments("{}");
    logData("Documents after unordered insert: " + std::to_string(unorderedCount));
    logInfo("Three valid documents were inserted despite one duplicate error");

    // Cleanup
    collection->deleteMany("{}");
    collection->dropAllIndexes();
}

void cleanup(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("--- Cleanup ---");
    logStep("Dropping test collections...");

    try
    {
        conn->dropCollection("batch_test_products");
        conn->dropCollection("batch_perf_test");
        conn->dropCollection("batch_ordered_test");
        logOk("Test collections dropped");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logInfo("Some collections may not exist: " + e.what_s());
    }
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc MongoDB Batch Operations Example");
    logMsg("========================================");
    logMsg("");

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
        printHelp("25_081_example_mongodb_batch", "mongodb");
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

    logStep("Getting MongoDB database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "mongodb");

    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!dbResult.value())
    {
        logError("MongoDB configuration not found");
        return EXIT_ERROR_;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) +
          "/" + dbConfig.getDatabase() + ")");

    logStep("Registering MongoDB driver...");
    registerDriver("mongodb");
    logOk("Driver registered");

    try
    {
        logStep("Connecting to MongoDB...");
        auto connBase = dbConfig.createDBConnection();
        auto conn = std::dynamic_pointer_cast<cpp_dbc::DocumentDBConnection>(connBase);

        if (!conn)
        {
            logError("Failed to cast connection to DocumentDBConnection");
            return EXIT_ERROR_;
        }
        logOk("Connected to MongoDB");

        demonstrateInsertMany(conn);
        demonstrateUpdateMany(conn);
        demonstrateDeleteMany(conn);
        demonstrateOrderedVsUnordered(conn);
        demonstratePerformanceComparison(conn);
        cleanup(conn);

        logMsg("");
        logStep("Closing connection...");
        conn->close();
        logOk("Connection closed");
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
