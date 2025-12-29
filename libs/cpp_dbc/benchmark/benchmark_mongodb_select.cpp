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
 * @file benchmark_mongodb_select.cpp
 * @brief Benchmarks for MongoDB find/query operations
 */

#include "benchmark_common.hpp"

#if USE_MONGODB

// Small dataset (10 documents)
static void BM_MongoDB_Select_Small_FindOne(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_select_small_find";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Debug: Examine the actual content of the collection
    // cpp_dbc::system_utils::logWithTimestampInfo("Examining collection content...");
    auto cursor = collection->find("{}");
    BENCHMARK_CHECK(cursor != nullptr);

    std::vector<int64_t> actualIds;
    bool hasDocuments = false;

    while (cursor->next())
    {
        hasDocuments = true;
        auto doc = cursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);
            // cpp_dbc::system_utils::logWithTimestampInfo("Found document with ID: " + std::to_string(id));
        }
        else
        {
            // cpp_dbc::system_utils::logWithTimestampInfo("Found document without ID field");
        }
    }

    // If no documents with ID field found, recreate the collection with proper documents
    if (actualIds.empty())
    {
        // cpp_dbc::system_utils::logWithTimestampInfo("No documents with ID field found. Recreating collection...");

        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Test Document " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            // cpp_dbc::system_utils::logWithTimestampInfo("Inserting document: " + docStr);
            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }
    else if (!hasDocuments)
    {
        cpp_dbc::system_utils::logWithTimestampInfo("Collection appears to be empty. Skipping benchmark.");
        state.SkipWithError("Collection is empty");
        return;
    }

    // Use the actual IDs found in the collection
    std::vector<int64_t> validIds = actualIds;

    for (auto _ : state)
    {
        // Skip if no valid IDs
        if (validIds.empty())
        {
            state.SkipWithError("No valid IDs found in collection");
            break;
        }

        // Select a random ID to query from our valid IDs
        int randomIndex = rand() % static_cast<int>(validIds.size());
        auto randomId = validIds[randomIndex];

        // Create query filter with proper JSON format
        std::string filter = "{\"id\": " + std::to_string(randomId) + "}";

        /*
        // Print debug info for first iteration
        if (state.iterations() == 0)
        {
            cpp_dbc::system_utils::logWithTimestampInfo("Querying with filter: " + filter);
        }
        */

        // Execute the query
        auto doc = collection->findOne(filter);
        benchmark::DoNotOptimize(doc);

        // Verify result
        BENCHMARK_CHECK(doc != nullptr);
        BENCHMARK_CHECK(doc->getInt("id") == randomId);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MongoDB_Select_Small_FindOne);

static void BM_MongoDB_Select_Small_FindAll(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_select_small_all";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Verify collection has documents
    auto cursor = collection->find("{}");
    BENCHMARK_CHECK(cursor != nullptr);

    std::vector<int64_t> actualIds;
    bool hasDocuments = false;

    while (cursor->next())
    {
        hasDocuments = true;
        auto doc = cursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);
        }
    }

    // If no documents with ID field found, recreate the collection with proper documents
    if (actualIds.empty())
    {
        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Test Document " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }
    else if (!hasDocuments || actualIds.size() != common_benchmark_helpers::SMALL_SIZE)
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
                                 ", \"name\": \"Test Document " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }

    for (auto _ : state)
    {
        // Find all documents
        auto findCursor = collection->find("{}");
        BENCHMARK_CHECK(findCursor != nullptr);

        // Process all results
        int count = 0;
        while (findCursor->next())
        {
            auto doc = findCursor->current();
            benchmark::DoNotOptimize(doc);
            count++;
        }

        // Verify we got all documents
        BENCHMARK_CHECK(count == common_benchmark_helpers::SMALL_SIZE);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_MongoDB_Select_Small_FindAll);

static void BM_MongoDB_Select_Small_FindProjection(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_select_small_proj";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Verify collection has documents
    auto cursor = collection->find("{}");
    BENCHMARK_CHECK(cursor != nullptr);

    std::vector<int64_t> actualIds;
    bool hasDocuments = false;

    while (cursor->next())
    {
        hasDocuments = true;
        auto doc = cursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);
        }
    }

    // If no documents with ID field found, recreate the collection with proper documents
    if (actualIds.empty())
    {
        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Test Document " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }
    else if (!hasDocuments || actualIds.size() != common_benchmark_helpers::SMALL_SIZE)
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
                                 ", \"name\": \"Test Document " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }

    for (auto _ : state)
    {
        // Find all documents with projection (only return id and name fields)
        auto projectionCursor = collection->find("{}", "{\"id\": 1, \"name\": 1, \"_id\": 0}");
        BENCHMARK_CHECK(projectionCursor != nullptr);

        // Process all results
        int count = 0;
        while (projectionCursor->next())
        {
            auto doc = projectionCursor->current();
            benchmark::DoNotOptimize(doc);

            // Verify projection worked
            BENCHMARK_CHECK(doc->hasField("id"));
            BENCHMARK_CHECK(doc->hasField("name"));
            BENCHMARK_CHECK(!doc->hasField("value"));
            BENCHMARK_CHECK(!doc->hasField("description"));

            count++;
        }

        // Verify we got all documents
        BENCHMARK_CHECK(count == common_benchmark_helpers::SMALL_SIZE);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_MongoDB_Select_Small_FindProjection);

// Medium dataset (100 documents)
static void BM_MongoDB_Select_Medium_FindFiltered(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_select_medium_filter";

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

    // Verify collection has documents with proper values
    auto verificationCursor = collection->find("{}");
    BENCHMARK_CHECK(verificationCursor != nullptr);

    std::vector<int64_t> actualIds;
    // Variable removed to fix unused warning
    bool hasDocsWithValueOver50 = false;

    while (verificationCursor->next())
    {
        // This assignment was removed as the variable was unused
        auto doc = verificationCursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);

            if (doc->hasField("value") && doc->getDouble("value") > 50.0)
            {
                hasDocsWithValueOver50 = true;
            }
        }
    }

    // If no documents with values > 50 found, recreate the collection with proper documents
    if (!hasDocsWithValueOver50 || actualIds.size() != common_benchmark_helpers::MEDIUM_SIZE)
    {
        cpp_dbc::system_utils::logWithTimestampInfo("Collection has " + std::to_string(actualIds.size()) +
                                                    " documents, expected " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) +
                                                    " with values > 50. Recreating collection...");

        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs and values
        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Test Document " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
        }
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    for (auto _ : state)
    {
        // Find documents with filter (value > 50)
        auto cursor = collection->find("{\"value\": {\"$gt\": 50}}");
        BENCHMARK_CHECK(cursor != nullptr);

        // Process all results
        int count = 0;
        while (cursor->next())
        {
            auto doc = cursor->current();
            benchmark::DoNotOptimize(doc);

            // Verify filter worked
            BENCHMARK_CHECK(doc->getDouble("value") > 50.0);
            count++;
        }

        // Verify we got expected number of documents (should be around half of medium size)
        BENCHMARK_CHECK(count > 0);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MongoDB_Select_Medium_FindFiltered);

static void BM_MongoDB_Select_Medium_CountDocuments(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_select_medium_count";

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

    // Verify the collection has the right number of documents
    auto initialCount = collection->countDocuments("{}");

    // If the count is wrong, recreate the collection
    if (initialCount != static_cast<uint64_t>(common_benchmark_helpers::MEDIUM_SIZE))
    {
        cpp_dbc::system_utils::logWithTimestampInfo("Collection has " + std::to_string(initialCount) +
                                                    " documents, expected " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) +
                                                    ". Recreating collection...");

        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Test Document " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
        }

        // Verify the collection now has the right number of documents
        initialCount = collection->countDocuments("{}");
        BENCHMARK_CHECK(initialCount == static_cast<uint64_t>(common_benchmark_helpers::MEDIUM_SIZE));
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    for (auto _ : state)
    {
        // Count all documents
        auto count = collection->countDocuments("{}");
        benchmark::DoNotOptimize(count);

        // Verify count
        BENCHMARK_CHECK(count == static_cast<uint64_t>(common_benchmark_helpers::MEDIUM_SIZE));
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MongoDB_Select_Medium_CountDocuments);

// Large dataset (1000 documents)
static void BM_MongoDB_Select_Large_Aggregation(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_select_large_agg";

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
    // Variable removed to fix unused warning

    while (verificationCursor->next())
    {
        // This assignment was removed as the variable was unused
        auto doc = verificationCursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);
        }
    }

    // If no documents or wrong number of documents, recreate the collection
    if (actualIds.size() != common_benchmark_helpers::LARGE_SIZE)
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
        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Test Document " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
        }
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Define an aggregation pipeline for group and sort
    std::string pipeline = R"([
        {"$group": {"_id": {"$mod": ["$id", 10]}, "total": {"$sum": "$value"}, "avg": {"$avg": "$value"}, "count": {"$sum": 1}}},
        {"$sort": {"_id": 1}}
    ])";

    for (auto _ : state)
    {
        // Execute aggregation pipeline
        auto cursor = collection->aggregate(pipeline);
        BENCHMARK_CHECK(cursor != nullptr);

        // Process all results
        int count = 0;
        while (cursor->next())
        {
            auto doc = cursor->current();
            benchmark::DoNotOptimize(doc);
            count++;
        }

        // Verify we got expected number of groups (should be 10 groups for mod 10)
        BENCHMARK_CHECK(count == 10);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MongoDB_Select_Large_Aggregation);

// XLarge dataset (10000 documents)
static void BM_MongoDB_Select_XLarge_FindWithLimit(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_select_xlarge_limit";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::XLARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

    // Verify the collection has the right number of documents
    auto initialCount = collection->countDocuments("{}");

    // If the count is wrong, recreate the collection
    if (initialCount != static_cast<uint64_t>(common_benchmark_helpers::XLARGE_SIZE))
    {
        cpp_dbc::system_utils::logWithTimestampInfo("Collection has " + std::to_string(initialCount) +
                                                    " documents, expected " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) +
                                                    ". Recreating collection...");

        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Test Document " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
        }

        // Verify the collection now has the right number of documents
        initialCount = collection->countDocuments("{}");
        BENCHMARK_CHECK(initialCount == static_cast<uint64_t>(common_benchmark_helpers::XLARGE_SIZE));
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Define a manual limit for processing (don't use MongoDB's limit functionality)
    const int manualLimit = 10;

    for (auto _ : state)
    {
        // Use a simple find without limit options
        std::string filter = "{}";
        auto cursor = collection->find(filter);
        BENCHMARK_CHECK(cursor != nullptr);

        // Process only the first manualLimit results
        int count = 0;
        while (cursor->next() && count < manualLimit)
        {
            auto doc = cursor->current();
            benchmark::DoNotOptimize(doc);
            count++;
        }

        // Verify we processed the expected number of documents
        // This should always succeed since we manually control the limit
        BENCHMARK_CHECK(count == manualLimit);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * manualLimit);
}
BENCHMARK(BM_MongoDB_Select_XLarge_FindWithLimit);

static void BM_MongoDB_Select_XLarge_IndexedQuery(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_select_xlarge_idx";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName, common_benchmark_helpers::XLARGE_SIZE);

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
    // Variable removed to fix unused warning

    while (verificationCursor->next())
    {
        // This assignment was removed as the variable was unused
        auto doc = verificationCursor->current();
        if (doc->hasField("id"))
        {
            auto id = doc->getInt("id");
            actualIds.push_back(id);
        }
    }

    // If no documents or wrong number of documents, recreate the collection
    if (actualIds.size() != common_benchmark_helpers::XLARGE_SIZE)
    {
        cpp_dbc::system_utils::logWithTimestampInfo("Collection has " + std::to_string(actualIds.size()) +
                                                    " documents, expected " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) +
                                                    ". Recreating collection...");

        // Drop the existing collection
        conn->dropCollection(collectionName);

        // Get a fresh collection
        collection = conn->getCollection(collectionName);
        BENCHMARK_CHECK(collection != nullptr);

        // Manually insert documents with known IDs
        for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; i++)
        {
            std::string docStr = "{\"id\": " + std::to_string(i) +
                                 ", \"name\": \"Test Document " + std::to_string(i) + "\", " +
                                 "\"value\": " + std::to_string(i * 1.5) + ", " +
                                 "\"description\": \"Test description " + std::to_string(i) + "\"}";

            collection->insertOne(docStr);
            actualIds.push_back(i);
        }
    }

    // Create an index on the id field
    collection->createIndex("{\"id\": 1}");
    cpp_dbc::system_utils::logWithTimestampInfo("Index created. Starting benchmark...");

    // Generate random IDs for queries from the actual IDs in the collection
    std::vector<int> randomIds;
    randomIds.reserve(100);

    if (actualIds.size() > 100)
    {
        std::sample(actualIds.begin(), actualIds.end(), std::back_inserter(randomIds), 100, std::mt19937{std::random_device{}()});
    }
    else
    {
        for (auto id : actualIds)
        {
            randomIds.push_back(static_cast<int>(id));
        }
    }

    for (auto _ : state)
    {
        // Select a random ID to query
        int randomIndex = rand() % static_cast<int>(randomIds.size());
        int randomId = randomIds[randomIndex];

        // Create query filter
        std::string filter = "{\"id\": " + std::to_string(randomId) + "}";

        // Execute the query
        auto doc = collection->findOne(filter);
        benchmark::DoNotOptimize(doc);

        // Verify result
        BENCHMARK_CHECK(doc != nullptr);
        BENCHMARK_CHECK(doc->getInt("id") == randomId);
    }

    // Cleanup - outside of measurement
    conn->dropCollection(collectionName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MongoDB_Select_XLarge_IndexedQuery);

#else
// Register empty benchmark when MongoDB is disabled
static void BM_MongoDB_Select_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("MongoDB support is not enabled");
        break;
    }
}
BENCHMARK(BM_MongoDB_Select_Disabled);
#endif