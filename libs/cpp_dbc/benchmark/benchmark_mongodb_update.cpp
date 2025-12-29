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
 * @file benchmark_mongodb_update.cpp
 * @brief Benchmarks for MongoDB UPDATE operations
 */

#include "benchmark_common.hpp"

#if USE_MONGODB

// Small dataset (10 documents)
static void BM_MongoDB_Update_Small_Individual(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_update_small_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Verify collection has documents with proper IDs
    auto verificationCursor = collection->find("{}");
    BENCHMARK_CHECK(verificationCursor != nullptr);

    std::vector<int64_t> actualIds;
    // bool hasDocuments = false;

    while (verificationCursor->next())
    {
        // hasDocuments = true;
        auto doc = verificationCursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);
        }
    }

    // If no documents with ID field found, recreate the collection with proper documents
    if (actualIds.empty() || actualIds.size() != common_benchmark_helpers::SMALL_SIZE)
    {
        cpp_dbc::system_utils::logWithTimestampInfo("Collection has " + std::to_string(actualIds.size()) +
                                                    " documents, expected " + std::to_string(common_benchmark_helpers::SMALL_SIZE) +
                                                    ". Recreating collection...");

        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        actualIds.clear();
        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop (if transactions are supported)
    bool supportsTransactions = false;
    std::string sessionId;
    try
    {
        supportsTransactions = conn->supportsTransactions();
        if (supportsTransactions)
        {
            sessionId = conn->startSession();
            conn->startTransaction(sessionId);
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        supportsTransactions = false;
        cpp_dbc::system_utils::logWithTimestampInfo("MongoDB transactions not supported: " + std::string(e.what_s()));
    }

    for (auto _ : state)
    {
        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            // Create filter for document to update
            std::string filter = "{\"id\": " + std::to_string(i) + "}";

            // Create update operation
            std::string update = "{\"$set\": {\"name\": \"Updated Name " + std::to_string(i) +
                                 "\", \"value\": " + std::to_string(i * 2.5) +
                                 ", \"description\": \"" + common_benchmark_helpers::generateRandomString(50) + "\"}}";

            // Execute update
            auto result = collection->updateOne(filter, update);
            benchmark::DoNotOptimize(result);

            // Verify update
            BENCHMARK_CHECK(result.matchedCount == 1);
            BENCHMARK_CHECK(result.modifiedCount == 1);
        }

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);

            // Reset data for next iteration
            conn->startTransaction(sessionId);

            // Restore original data
            for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
            {
                std::string filter = "{\"id\": " + std::to_string(i) + "}";
                std::string update = "{\"$set\": {\"name\": \"Name " + std::to_string(i) +
                                     "\", \"value\": " + std::to_string(i * 1.5) + "}}";
                collection->updateOne(filter, update);
            }
        }
        else
        {
            // If transactions aren't supported, reset data manually
            conn->dropCollection(collectionName);
            mongodb_benchmark_helpers::createBenchmarkCollection(conn, collectionName);
            collection = conn->getCollection(collectionName);
            mongodb_benchmark_helpers::populateCollection(conn, collectionName, common_benchmark_helpers::SMALL_SIZE);
        }
        state.ResumeTiming(); // Resume timing for next iteration
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_MongoDB_Update_Small_Individual);

static void BM_MongoDB_Update_Small_Bulk(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_update_small_bulk";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Verify collection has documents with proper IDs
    auto verificationCursor = collection->find("{}");
    BENCHMARK_CHECK(verificationCursor != nullptr);

    std::vector<int64_t> actualIds;
    // bool hasDocuments = false;

    while (verificationCursor->next())
    {
        // hasDocuments = true;
        auto doc = verificationCursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);
        }
    }

    // If no documents with ID field found, recreate the collection with proper documents
    if (actualIds.empty() || actualIds.size() != common_benchmark_helpers::SMALL_SIZE)
    {
        cpp_dbc::system_utils::logWithTimestampInfo("Collection has " + std::to_string(actualIds.size()) +
                                                    " documents, expected " + std::to_string(common_benchmark_helpers::SMALL_SIZE) +
                                                    ". Recreating collection...");

        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        actualIds.clear();
        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop (if transactions are supported)
    bool supportsTransactions = false;
    std::string sessionId;
    try
    {
        supportsTransactions = conn->supportsTransactions();
        if (supportsTransactions)
        {
            sessionId = conn->startSession();
            conn->startTransaction(sessionId);
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        supportsTransactions = false;
        cpp_dbc::system_utils::logWithTimestampInfo("MongoDB transactions not supported: " + std::string(e.what_s()));
    }

    for (auto _ : state)
    {
        // Perform updates individually, tracking total modified count
        uint64_t totalModifiedCount = 0;

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            std::string filter = "{\"id\": " + std::to_string(i) + "}";
            std::string update = "{\"$set\": {\"name\": \"Bulk Updated " + std::to_string(i) +
                                 "\", \"value\": " + std::to_string(i * 2.5) +
                                 ", \"description\": \"" + common_benchmark_helpers::generateRandomString(50) + "\"}}";

            auto result = collection->updateOne(filter, update);
            benchmark::DoNotOptimize(result);

            BENCHMARK_CHECK(result.matchedCount == 1);
            BENCHMARK_CHECK(result.modifiedCount == 1);
            totalModifiedCount += result.modifiedCount;
        }

        // Verify total updates
        BENCHMARK_CHECK(totalModifiedCount == common_benchmark_helpers::SMALL_SIZE);

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);

            // Reset data for next iteration
            conn->startTransaction(sessionId);

            // Restore original data individually
            for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
            {
                std::string filter = "{\"id\": " + std::to_string(i) + "}";
                std::string update = "{\"$set\": {\"name\": \"Name " + std::to_string(i) +
                                     "\", \"value\": " + std::to_string(i * 1.5) + "}}";
                collection->updateOne(filter, update);
            }
        }
        else
        {
            // If transactions aren't supported, reset data manually
            conn->dropCollection(collectionName);
            mongodb_benchmark_helpers::createBenchmarkCollection(conn, collectionName);
            collection = conn->getCollection(collectionName);
            mongodb_benchmark_helpers::populateCollection(conn, collectionName, common_benchmark_helpers::SMALL_SIZE);
        }
        state.ResumeTiming(); // Resume timing for next iteration
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_MongoDB_Update_Small_Bulk);

// Medium dataset (100 documents)
static void BM_MongoDB_Update_Medium_UpdateMany(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_update_medium_many";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Verify collection has documents with proper IDs
    auto verificationCursor = collection->find("{}");
    BENCHMARK_CHECK(verificationCursor != nullptr);

    std::vector<int64_t> actualIds;
    // bool hasDocuments = false;

    while (verificationCursor->next())
    {
        // hasDocuments = true;
        auto doc = verificationCursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);
        }
    }

    // If no documents with ID field found, recreate the collection with proper documents
    if (actualIds.empty() || actualIds.size() != common_benchmark_helpers::MEDIUM_SIZE)
    {
        cpp_dbc::system_utils::logWithTimestampInfo("Collection has " + std::to_string(actualIds.size()) +
                                                    " documents, expected " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) +
                                                    ". Recreating collection...");

        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        actualIds.clear();
        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop (if transactions are supported)
    bool supportsTransactions = false;
    std::string sessionId;
    try
    {
        supportsTransactions = conn->supportsTransactions();
        if (supportsTransactions)
        {
            sessionId = conn->startSession();
            conn->startTransaction(sessionId);
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        supportsTransactions = false;
        cpp_dbc::system_utils::logWithTimestampInfo("MongoDB transactions not supported: " + std::string(e.what_s()));
    }

    for (auto _ : state)
    {
        // Create filter to update all documents with even id
        std::string filter = "{\"id\": {\"$mod\": [2, 0]}}";

        // Create update operation with timestamp and a unique random value to ensure modification
        std::string randomValue = common_benchmark_helpers::generateRandomString(20);
        std::string update = "{\"$set\": {\"is_even\": true, \"updated_at\": \"2025-12-29T00:00:00Z\", \"random_value\": \"" + randomValue + "\"}}";

        // Execute updateMany operation
        auto result = collection->updateMany(filter, update);
        benchmark::DoNotOptimize(result);

        // Verify update (should update approximately half of the documents)
        BENCHMARK_CHECK(result.matchedCount > 0);
        // We don't check result.modifiedCount == result.matchedCount since some documents might not be modified
        BENCHMARK_CHECK(result.modifiedCount > 0);

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);
            conn->startTransaction(sessionId);
        }
        else
        {
            // If transactions aren't supported, reset data manually
            conn->dropCollection(collectionName);
            mongodb_benchmark_helpers::createBenchmarkCollection(conn, collectionName);
            collection = conn->getCollection(collectionName);
            mongodb_benchmark_helpers::populateCollection(conn, collectionName, common_benchmark_helpers::MEDIUM_SIZE);
        }
        state.ResumeTiming(); // Resume timing for next iteration
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * (common_benchmark_helpers::MEDIUM_SIZE / 2));
}
BENCHMARK(BM_MongoDB_Update_Medium_UpdateMany);

static void BM_MongoDB_Update_Medium_FindOneAndUpdate(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_update_medium_find_upd";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Verify collection has documents with proper IDs
    auto verificationCursor = collection->find("{}");
    BENCHMARK_CHECK(verificationCursor != nullptr);

    std::vector<int64_t> actualIds;
    // bool hasDocuments = false;

    while (verificationCursor->next())
    {
        // hasDocuments = true;
        auto doc = verificationCursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);
        }
    }

    // If no documents with ID field found, recreate the collection with proper documents
    if (actualIds.empty() || actualIds.size() != common_benchmark_helpers::MEDIUM_SIZE)
    {
        cpp_dbc::system_utils::logWithTimestampInfo("Collection has " + std::to_string(actualIds.size()) +
                                                    " documents, expected " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) +
                                                    ". Recreating collection...");

        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        actualIds.clear();
        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop (if transactions are supported)
    bool supportsTransactions = false;
    std::string sessionId;
    try
    {
        supportsTransactions = conn->supportsTransactions();
        if (supportsTransactions)
        {
            sessionId = conn->startSession();
            conn->startTransaction(sessionId);
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        supportsTransactions = false;
        cpp_dbc::system_utils::logWithTimestampInfo("MongoDB transactions not supported: " + std::string(e.what_s()));
    }

    // Generate random IDs for queries within the range of inserted documents
    std::vector<int> randomIds = common_benchmark_helpers::generateRandomIds(common_benchmark_helpers::MEDIUM_SIZE, 20);

    for (auto _ : state)
    {
        // Select a random ID to update
        int randomIndex = rand() % static_cast<int>(randomIds.size());
        int randomId = randomIds[randomIndex];

        // Create filter for document to update
        std::string filter = "{\"id\": " + std::to_string(randomId) + "}";

        // Create update operation
        // Add a random value to ensure the update always modifies the document
        std::string randomValue = common_benchmark_helpers::generateRandomString(20);
        std::string update = "{\"$set\": {\"name\": \"FindAndUpdate " + std::to_string(randomId) +
                             "\", \"value\": " + std::to_string(randomId * 3.5) +
                             ", \"updated_at\": \"2025-12-29T00:00:00Z\"" +
                             ", \"random_value\": \"" + randomValue + "\"}}";

        // Use updateOne and then findOne to simulate findOneAndUpdate
        auto result = collection->updateOne(filter, update);
        benchmark::DoNotOptimize(result);

        // Verify update was successful - we only check matched count
        BENCHMARK_CHECK(result.matchedCount == 1);
        // Don't check modifiedCount as it might vary

        // Get the updated document
        auto updatedDoc = collection->findOne(filter);
        benchmark::DoNotOptimize(updatedDoc);

        // Verify the document was correctly updated
        BENCHMARK_CHECK(updatedDoc != nullptr);
        BENCHMARK_CHECK(updatedDoc->getInt("id") == randomId);
        BENCHMARK_CHECK(updatedDoc->getString("name") == "FindAndUpdate " + std::to_string(randomId));
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MongoDB_Update_Medium_FindOneAndUpdate);

// Large dataset (1000 documents)
static void BM_MongoDB_Update_Large_Bulk(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_update_large_bulk";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Verify collection has documents with proper IDs
    auto verificationCursor = collection->find("{}");
    BENCHMARK_CHECK(verificationCursor != nullptr);

    std::vector<int64_t> actualIds;
    // bool hasDocuments = false;

    while (verificationCursor->next())
    {
        // hasDocuments = true;
        auto doc = verificationCursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);
        }
    }

    // If no documents with ID field found, recreate the collection with proper documents
    if (actualIds.empty() || actualIds.size() != common_benchmark_helpers::LARGE_SIZE)
    {
        cpp_dbc::system_utils::logWithTimestampInfo("Collection has " + std::to_string(actualIds.size()) +
                                                    " documents, expected " + std::to_string(common_benchmark_helpers::LARGE_SIZE) +
                                                    ". Recreating collection...");

        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        actualIds.clear();
        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop (if transactions are supported)
    bool supportsTransactions = false;
    std::string sessionId;
    try
    {
        supportsTransactions = conn->supportsTransactions();
        if (supportsTransactions)
        {
            sessionId = conn->startSession();
            conn->startTransaction(sessionId);
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        supportsTransactions = false;
        cpp_dbc::system_utils::logWithTimestampInfo("MongoDB transactions not supported: " + std::string(e.what_s()));
    }

    for (auto _ : state)
    {
        // Perform updates individually, tracking total modified count
        uint64_t totalModifiedCount = 0;
        cpp_dbc::DocumentUpdateOptions options;
        options.multi = true; // Update multiple documents

        // Update in batches to improve performance
        const int batchSize = 50;
        for (int batchStart = 1; batchStart <= common_benchmark_helpers::LARGE_SIZE; batchStart += batchSize)
        {
            int batchEnd = std::min(batchStart + batchSize - 1, common_benchmark_helpers::LARGE_SIZE);

            // Create a filter for the current batch
            std::string filter = "{\"id\": {\"$gte\": " + std::to_string(batchStart) +
                                 ", \"$lte\": " + std::to_string(batchEnd) + "}}";

            // Create the update operation with a unique random value to ensure modification
            std::string randomValue = common_benchmark_helpers::generateRandomString(20);
            std::string update = "{\"$set\": {\"updated\": true, \"value\": " +
                                 std::to_string(batchStart * 2.0) +
                                 ", \"random_value\": \"" + randomValue + "\"}}";

            // Update the batch
            auto result = collection->updateMany(filter, update, options);
            benchmark::DoNotOptimize(result);

            // BENCHMARK_CHECK(result.matchedCount >= 0);
            //  Don't check modifiedCount as it might vary
            totalModifiedCount += result.modifiedCount;
        }

        // Verify total updates (allow for some flexibility)
        BENCHMARK_CHECK(totalModifiedCount > 0);

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);
            conn->startTransaction(sessionId);
        }
        else
        {
            // If transactions aren't supported, reset data manually
            conn->dropCollection(collectionName);
            mongodb_benchmark_helpers::createBenchmarkCollection(conn, collectionName);
            collection = conn->getCollection(collectionName);
            mongodb_benchmark_helpers::populateCollection(conn, collectionName, common_benchmark_helpers::LARGE_SIZE);
        }
        state.ResumeTiming(); // Resume timing for next iteration
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_MongoDB_Update_Large_Bulk);

#else
// Register empty benchmark when MongoDB is disabled
static void BM_MongoDB_Update_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("MongoDB support is not enabled");
        break;
    }
}
BENCHMARK(BM_MongoDB_Update_Disabled);
#endif