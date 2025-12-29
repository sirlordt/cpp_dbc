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
 * @file benchmark_mongodb_insert.cpp
 * @brief Benchmarks for MongoDB INSERT operations
 */

#include "benchmark_common.hpp"

#if USE_MONGODB

// Small dataset (10 documents)
static void BM_MongoDB_Insert_Small_Individual(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_insert_small_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

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

    // Use a unique ID prefix for each benchmark run to avoid duplicate key errors
    static int runCounter = 0;

    // Benchmark measurement begins
    for (auto _ : state)
    {
        int runId = ++runCounter;

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            // Create document with the test data
            std::string doc = mongodb_benchmark_helpers::generateTestDocument(
                uniqueId,
                "Name " + std::to_string(i),
                i * 1.5,
                common_benchmark_helpers::generateRandomString(50));

            auto result = collection->insertOne(doc);
            benchmark::DoNotOptimize(result);
        }

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);
            conn->startTransaction(sessionId); // Begin a new transaction for the next iteration
        }
        else
        {
            // If transactions aren't supported, delete the inserted documents
            auto deleteResult = collection->deleteMany("{}");
            benchmark::DoNotOptimize(deleteResult);
        }
        state.ResumeTiming(); // Resume timing for next iteration
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }
    else
    {
        collection->deleteMany("{}");
    }

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_MongoDB_Insert_Small_Individual);

static void BM_MongoDB_Insert_Small_Bulk(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_insert_small_bulk";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

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

    // Use a unique ID prefix for each benchmark run to avoid duplicate key errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing documents
        std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> docs;

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            // Create document with the test data
            std::string docString = mongodb_benchmark_helpers::generateTestDocument(
                uniqueId,
                "Name " + std::to_string(i),
                i * 1.5,
                common_benchmark_helpers::generateRandomString(50));

            auto doc = conn->createDocument(docString);
            docs.push_back(doc);
        }
        state.ResumeTiming(); // Resume timing for actual operations

        // Insert all documents in a single bulk operation
        auto result = collection->insertMany(docs);
        benchmark::DoNotOptimize(result);

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);
            conn->startTransaction(sessionId); // Begin a new transaction for the next iteration
        }
        else
        {
            // If transactions aren't supported, delete the inserted documents
            auto deleteResult = collection->deleteMany("{}");
            benchmark::DoNotOptimize(deleteResult);
        }
        state.ResumeTiming(); // Resume timing for next iteration
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }
    else
    {
        collection->deleteMany("{}");
    }

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_MongoDB_Insert_Small_Bulk);

// Medium dataset (100 documents)
static void BM_MongoDB_Insert_Medium_Individual(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_insert_medium_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

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

    // Use a unique ID prefix for each benchmark run to avoid duplicate key errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            // Create document with the test data
            std::string doc = mongodb_benchmark_helpers::generateTestDocument(
                uniqueId,
                "Name " + std::to_string(i),
                i * 1.5,
                common_benchmark_helpers::generateRandomString(50));

            auto result = collection->insertOne(doc);
            benchmark::DoNotOptimize(result);
        }

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);
            conn->startTransaction(sessionId); // Begin a new transaction for the next iteration
        }
        else
        {
            // If transactions aren't supported, delete the inserted documents
            auto deleteResult = collection->deleteMany("{}");
            benchmark::DoNotOptimize(deleteResult);
        }
        state.ResumeTiming(); // Resume timing for next iteration
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }
    else
    {
        collection->deleteMany("{}");
    }

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_MongoDB_Insert_Medium_Individual);

static void BM_MongoDB_Insert_Medium_Bulk(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_insert_medium_bulk";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

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

    // Use a unique ID prefix for each benchmark run to avoid duplicate key errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing documents
        std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> docs;

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            // Create document with the test data
            std::string docString = mongodb_benchmark_helpers::generateTestDocument(
                uniqueId,
                "Name " + std::to_string(i),
                i * 1.5,
                common_benchmark_helpers::generateRandomString(50));

            auto doc = conn->createDocument(docString);
            docs.push_back(doc);
        }
        state.ResumeTiming(); // Resume timing for actual operations

        // Insert all documents in a single bulk operation
        auto result = collection->insertMany(docs);
        benchmark::DoNotOptimize(result);

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);
            conn->startTransaction(sessionId); // Begin a new transaction for the next iteration
        }
        else
        {
            // If transactions aren't supported, delete the inserted documents
            auto deleteResult = collection->deleteMany("{}");
            benchmark::DoNotOptimize(deleteResult);
        }
        state.ResumeTiming(); // Resume timing for next iteration
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }
    else
    {
        collection->deleteMany("{}");
    }

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_MongoDB_Insert_Medium_Bulk);

// Large dataset (1000 documents) - only bulk insert for better performance
static void BM_MongoDB_Insert_Large_Bulk(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_insert_large_bulk";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

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

    // Use a unique ID prefix for each benchmark run to avoid duplicate key errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing documents
        std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> docs;

        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            // Create document with the test data
            std::string docString = mongodb_benchmark_helpers::generateTestDocument(
                uniqueId,
                "Name " + std::to_string(i),
                i * 1.5,
                common_benchmark_helpers::generateRandomString(50));

            auto doc = conn->createDocument(docString);
            docs.push_back(doc);
        }
        state.ResumeTiming(); // Resume timing for actual operations

        // Insert all documents in a single bulk operation
        auto result = collection->insertMany(docs);
        benchmark::DoNotOptimize(result);

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);
            conn->startTransaction(sessionId); // Begin a new transaction for the next iteration
        }
        else
        {
            // If transactions aren't supported, delete the inserted documents
            auto deleteResult = collection->deleteMany("{}");
            benchmark::DoNotOptimize(deleteResult);
        }
        state.ResumeTiming(); // Resume timing for next iteration
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }
    else
    {
        collection->deleteMany("{}");
    }

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_MongoDB_Insert_Large_Bulk);

// XLarge dataset (10000 documents) - only bulk insert for better performance
static void BM_MongoDB_Insert_XLarge_Bulk(benchmark::State &state)
{
    const std::string collectionName = "benchmark_mongodb_insert_xlarge_bulk";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up MongoDB connection and collection '" + collectionName + "' with " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) + " documents of test data...");
    auto conn = mongodb_benchmark_helpers::setupMongoDBConnection(collectionName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to MongoDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Get the collection
    auto collection = conn->getCollection(collectionName);
    BENCHMARK_CHECK(collection != nullptr);

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

    // Use a unique ID prefix for each benchmark run to avoid duplicate key errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing documents
        std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> docs;

        for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            // Create document with the test data
            std::string docString = mongodb_benchmark_helpers::generateTestDocument(
                uniqueId,
                "Name " + std::to_string(i),
                i * 1.5,
                common_benchmark_helpers::generateRandomString(50));

            auto doc = conn->createDocument(docString);
            docs.push_back(doc);
        }
        state.ResumeTiming(); // Resume timing for actual operations

        // Insert all documents in a single bulk operation
        auto result = collection->insertMany(docs);
        benchmark::DoNotOptimize(result);

        state.PauseTiming(); // Pause for cleanup
        if (supportsTransactions)
        {
            conn->abortTransaction(sessionId);
            conn->startTransaction(sessionId); // Begin a new transaction for the next iteration
        }
        else
        {
            // If transactions aren't supported, delete the inserted documents
            auto deleteResult = collection->deleteMany("{}");
            benchmark::DoNotOptimize(deleteResult);
        }
        state.ResumeTiming(); // Resume timing for next iteration
    }

    // Clean up the final transaction if needed
    if (supportsTransactions)
    {
        conn->abortTransaction(sessionId);
    }
    else
    {
        collection->deleteMany("{}");
    }

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::XLARGE_SIZE);
}
BENCHMARK(BM_MongoDB_Insert_XLarge_Bulk);

#else
// Register empty benchmark when MongoDB is disabled
static void BM_MongoDB_Insert_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("MongoDB support is not enabled");
        break;
    }
}
BENCHMARK(BM_MongoDB_Insert_Disabled);
#endif