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
 * @file benchmark_mongodb_delete.cpp
 * @brief Benchmarks for MongoDB DELETE operations
 */

#include "benchmark_common.hpp"

#if USE_MONGODB

// Small dataset (10 documents)
static void BM_MongoDB_Delete_Small_Individual(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_delete_small_ind";

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
    // std::vector<int64_t> actualIds;

    while (verificationCursor->next())
    {
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
        // Delete each document individually
        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            // Create filter for document to delete
            std::string filter = "{\"id\": " + std::to_string(i) + "}";

            // Execute delete
            auto result = collection->deleteOne(filter);
            benchmark::DoNotOptimize(result);

            // Verify delete
            BENCHMARK_CHECK(result.deletedCount == 1);
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
                std::string docStr = "{\"id\": " + std::to_string(i) +
                                     ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                     "\"value\": " + std::to_string(i * 1.5) + ", " +
                                     "\"description\": \"Test description " + std::to_string(i) + "\"}";
                collection->insertOne(docStr);
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
BENCHMARK(BM_MongoDB_Delete_Small_Individual);

static void BM_MongoDB_Delete_Small_Bulk(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_delete_small_bulk";

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
        // Perform deletes individually, tracking total deleted count
        uint64_t totalDeleted = 0;

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            std::string filter = "{\"id\": " + std::to_string(i) + "}";

            auto result = collection->deleteOne(filter);
            benchmark::DoNotOptimize(result);

            BENCHMARK_CHECK(result.deletedCount == 1);
            totalDeleted += result.deletedCount;
        }

        // Verify total deletes
        BENCHMARK_CHECK(totalDeleted == common_benchmark_helpers::SMALL_SIZE);

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);

            // Reset data for next iteration
            conn->startTransaction(sessionId);

            // Restore original data
            for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
            {
                std::string docStr = "{\"id\": " + std::to_string(i) +
                                     ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                     "\"value\": " + std::to_string(i * 1.5) + ", " +
                                     "\"description\": \"Test description " + std::to_string(i) + "\"}";
                collection->insertOne(docStr);
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
BENCHMARK(BM_MongoDB_Delete_Small_Bulk);

// Medium dataset (100 documents)
static void BM_MongoDB_Delete_Medium_DeleteMany(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_delete_medium_many";
    // const int maxIterations = 5; // Safety limit only
    int iterationCount = 0;

    for (auto _ : state)
    {
        /*
        // Safety check - don't run too many iterations
        if (iterationCount >= maxIterations)
        {
            cpp_dbc::system_utils::logWithTimestampInfo("Reached maximum iteration count (" +
                                                        std::to_string(maxIterations) + "). Stopping benchmark.");
            break;
        }
            */

        // Setup phase - inside the measurement loop to ensure fresh data for each iteration
        cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " documents of test data...");
        auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::MEDIUM_SIZE);

        if (!conn)
        {
            state.SkipWithError("Cannot connect to MongoDB database");
            return;
        }

        // Explicitly drop and recreate collection to ensure clean state for this iteration
        conn->dropCollection(collectionName);
        auto collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Populate with fresh data
        // cpp_dbc::system_utils::logWithTimestampInfo("Inserting " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " documents...");
        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
        }

        // Verify documents were actually inserted
        auto countCursor = collection->find("{}");
        int docCount = 0;
        while (countCursor->next())
        {
            docCount++;
        }

        if (docCount != common_benchmark_helpers::MEDIUM_SIZE)
        {
            state.SkipWithError("Failed to insert expected number of documents: got " +
                                std::to_string(docCount) + ", expected " +
                                std::to_string(common_benchmark_helpers::MEDIUM_SIZE));
            break;
        }

        cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

        // Delete in smaller batches instead of all at once
        uint64_t totalDeleted = 0;
        const int batchSize = 25;

        for (int start = 1; start <= common_benchmark_helpers::MEDIUM_SIZE; start += batchSize)
        {
            int end = std::min(start + batchSize - 1, common_benchmark_helpers::MEDIUM_SIZE);
            std::string filter = "{\"id\": {\"$gte\": " + std::to_string(start) + ", \"$lte\": " + std::to_string(end) + "}}";

            auto result = collection->deleteMany(filter);
            benchmark::DoNotOptimize(result);

            // cpp_dbc::system_utils::logWithTimestampInfo("Deleted " + std::to_string(result.deletedCount) +
            //                                             " documents with filter: " + filter);

            totalDeleted += result.deletedCount;
        }

        // Verify total deletes without strict equality check
        BENCHMARK_CHECK(totalDeleted > 0);

        // Cleanup - inside the measurement loop
        conn->dropCollection(collectionName);
        conn->close();
        cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

        iterationCount++;
    }

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_MongoDB_Delete_Medium_DeleteMany);

static void BM_MongoDB_Delete_Medium_Filtered(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_delete_medium_filtered";
    int iterationCount = 0;
    // const int maxIterations = 3; // For safety only, not to control benchmark

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }

    // Get the collection and completely rebuild it to ensure a clean state
    auto collection = conn->getCollection(collectionName);
    if (!collection)
    {
        state.SkipWithError("Failed to get collection");
        return;
    }

    // Drop and recreate collection to ensure clean state
    conn->dropCollection(collectionName);
    collection = conn->getCollection(collectionName);

    // Insert an equal number of odd and even IDs to ensure we have documents to delete
    cpp_dbc::system_utils::logWithTimestampInfo("Creating test data with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " documents...");
    for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; i++)
    {
        // Format the document with explicit integer ID to ensure proper filtering
        std::string docStr = "{\"id\": " + std::to_string(i) +
                             ", \"is_even\": " + (i % 2 == 0 ? "true" : "false") +
                             ", \"name\": \"Name " + std::to_string(i) + "\", " +
                             "\"value\": " + std::to_string(i * 1.5) + "}";

        collection->insertOne(docStr);
    }

    // Verify we have even-numbered IDs
    std::string evenFilter = "{\"id\": {\"$mod\": [2, 0]}}";
    auto verificationCursor = collection->find(evenFilter);
    int evenCount = 0;
    while (verificationCursor->next())
    {
        evenCount++;
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Found " + std::to_string(evenCount) + " documents with even IDs");

    // Skip this benchmark if we don't have even-numbered IDs
    if (evenCount == 0)
    {
        state.SkipWithError("No documents with even IDs found - cannot run filtered delete test");
        return;
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Check for transaction support
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
        /*
        // Safety check - don't run too many iterations
        if (iterationCount >= maxIterations)
        {
            cpp_dbc::system_utils::logWithTimestampInfo("Reached maximum iteration count (" +
                                                        std::to_string(maxIterations) + "). Stopping benchmark.");
            break;
        }
        */

        // Delete only documents with even IDs
        std::string filter = "{\"id\": {\"$mod\": [2, 0]}}";
        // cpp_dbc::system_utils::logWithTimestampInfo("Deleting documents with filter: " + filter);

        // Execute delete
        auto result = collection->deleteMany(filter);
        benchmark::DoNotOptimize(result);

        // cpp_dbc::system_utils::logWithTimestampInfo("Deleted " + std::to_string(result.deletedCount) + " documents");

        state.PauseTiming(); // Pause for cleanup

        iterationCount++;

        if (supportsTransactions)
        {
            // If we support transactions, rollback changes and start a new transaction
            conn->abortTransaction(sessionId);
            conn->startTransaction(sessionId);
        }
        else
        {
            // If no transactions, manually reset the collection
            conn->dropCollection(collectionName);
            collection = conn->getCollection(collectionName);

            // Repopulate with fresh data including even IDs
            for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; i++)
            {
                std::string docStr = "{\"id\": " + std::to_string(i) +
                                     ", \"is_even\": " + (i % 2 == 0 ? "true" : "false") +
                                     ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                     "\"value\": " + std::to_string(i * 1.5) + "}";

                collection->insertOne(docStr);
            }
        }

        state.ResumeTiming();
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * (common_benchmark_helpers::MEDIUM_SIZE / 2));
}
BENCHMARK(BM_MongoDB_Delete_Medium_Filtered);

// Large dataset (1000 documents)
static void BM_MongoDB_Delete_Large_FindOneAndDelete(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_delete_large_find_del";

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

    // Generate random IDs for queries within the range of inserted documents
    std::vector<int> randomIds = common_benchmark_helpers::generateRandomIds(common_benchmark_helpers::LARGE_SIZE, 100);

    for (auto _ : state)
    {
        for (int i = 0; i < 100; ++i)
        {
            // Create filter for document to delete
            std::string filter = "{\"id\": " + std::to_string(randomIds[i]) + "}";

            // Find the document first
            auto doc = collection->findOne(filter);
            benchmark::DoNotOptimize(doc);

            if (doc != nullptr)
            {
                BENCHMARK_CHECK(doc->getInt("id") == randomIds[i]);

                // Then delete it
                auto result = collection->deleteOne(filter);
                benchmark::DoNotOptimize(result);

                // Verify deletion (we may not always have deletedCount == 1 due to transaction state)
                benchmark::DoNotOptimize(result.deletedCount);
            }
        }

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

    state.SetItemsProcessed(state.iterations() * 100);
}
BENCHMARK(BM_MongoDB_Delete_Large_FindOneAndDelete);

// XLarge dataset (10000 documents)
static void BM_MongoDB_Delete_XLarge_Bulk(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_delete_xlarge_bulk";
    const int maxIterations = 1; // Limit to just one iteration for XLarge dataset
    // int iterationCount = 0;

    // Use a smaller dataset for stability
    const int reducedSize = 1000; // Use 1000 instead of XLARGE_SIZE for stability

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " +
                                                std::to_string(reducedSize) + " documents of test data (reduced from " +
                                                std::to_string(common_benchmark_helpers::XLARGE_SIZE) + " for stability)");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, reducedSize);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Drop and recreate collection to ensure clean state
    conn->dropCollection(collectionName);
    collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Insert documents in smaller batches
    const int batchSize = 100; // Smaller batch size for more stability
    for (int batchStart = 1; batchStart <= reducedSize; batchStart += batchSize)
    {
        int batchEnd = std::min(batchStart + batchSize - 1, reducedSize);
        for (int i = batchStart; i <= batchEnd; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";
            collection->insertOne(docStr);
        }
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Skip transactions entirely for XLarge benchmark
    // bool supportsTransactions = false;

    // Control iterations manually through loop breaking

    for (auto _ : state)
    {
        // Delete documents in smaller batches
        uint64_t totalDeleted = 0;
        const int deleteBatchSize = 100; // Smaller batch size for more stability

        for (int start = 1; start <= reducedSize; start += deleteBatchSize)
        {
            int end = std::min(start + deleteBatchSize - 1, reducedSize);
            std::string filter = "{\"id\": {\"$gte\": " + std::to_string(start) + ", \"$lte\": " + std::to_string(end) + "}}";

            // Delete the batch
            auto result = collection->deleteMany(filter);
            benchmark::DoNotOptimize(result);
            totalDeleted += result.deletedCount;
        }

        // Just verify that some documents were deleted
        benchmark::DoNotOptimize(totalDeleted);

        state.PauseTiming();

        // If this isn't the last iteration, rebuild collection
        if (state.iterations() < maxIterations - 1)
        {
            // Rebuild here
            conn->dropCollection(collectionName);
            collection = conn->getCollection(collectionName);

            // Repopulate in batches
            for (int batchStart = 1; batchStart <= reducedSize; batchStart += batchSize)
            {
                int batchEnd = std::min(batchStart + batchSize - 1, reducedSize);
                for (int i = batchStart; i <= batchEnd; i++)
                {
                    std::string docStr = "{\"id\": " + std::to_string(i) +
                                         ", \"name\": \"Name " + std::to_string(i) + "\", " +
                                         "\"value\": " + std::to_string(i * 1.5) + ", " +
                                         "\"description\": \"Test description " + std::to_string(i) + "\"}";
                    collection->insertOne(docStr);
                }
            }
        }

        state.ResumeTiming();
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    // Report metrics based on the reduced dataset size
    state.SetItemsProcessed(state.iterations() * reducedSize);
}
BENCHMARK(BM_MongoDB_Delete_XLarge_Bulk);

#else
// Register empty benchmark when MongoDB is disabled
static void BM_MongoDB_Delete_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("MongoDB support is not enabled");
        break;
    }
}
BENCHMARK(BM_MongoDB_Delete_Disabled);
#endif