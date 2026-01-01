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
 * @file benchmark_redis_delete.cpp
 * @brief Benchmarks for Redis DELETE operations
 */

#include "benchmark_common.hpp"

#if USE_REDIS

// Small dataset (10 entries)
static void BM_Redis_Delete_Keys_Small_Individual(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_delete_small_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Generate key list for benchmarking
    std::vector<std::string> keys;
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        keys.push_back(keyPrefix + ":" + std::to_string(i));
    }

    for (auto _ : state)
    {
        // Repopulate keys before each iteration
        state.PauseTiming();
        for (const auto &key : keys)
        {
            conn->setString(key, "DeleteTest-" + common_benchmark_helpers::generateRandomString(20));
        }
        state.ResumeTiming();

        for (const auto &key : keys)
        {
            bool result = conn->deleteKey(key);
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Redis_Delete_Keys_Small_Individual)->Iterations(1000);

static void BM_Redis_Delete_Keys_Small_Batch(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_delete_small_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Generate key list for benchmarking
    std::vector<std::string> keys;
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        keys.push_back(keyPrefix + ":" + std::to_string(i));
    }

    for (auto _ : state)
    {
        // Repopulate keys before each iteration
        state.PauseTiming();
        for (const auto &key : keys)
        {
            conn->setString(key, "DeleteTest-" + common_benchmark_helpers::generateRandomString(20));
        }
        state.ResumeTiming();

        // Delete all keys in a single batch operation
        int64_t result = conn->deleteKeys(keys);
        benchmark::DoNotOptimize(result);
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Redis_Delete_Keys_Small_Batch)->Iterations(1000);

static void BM_Redis_Hash_Delete_Fields_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_hash_delete_small";
    std::string hashKey = keyPrefix + ":hash";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Generate field list for benchmarking
    std::vector<std::string> fields;
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        fields.push_back("field:" + std::to_string(i));
    }

    for (auto _ : state)
    {
        // Repopulate hash before each iteration
        state.PauseTiming();
        for (const auto &field : fields)
        {
            conn->hashSet(hashKey, field, "DeleteTest-" + common_benchmark_helpers::generateRandomString(20));
        }
        state.ResumeTiming();

        for (const auto &field : fields)
        {
            bool result = conn->hashDelete(hashKey, field);
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Redis_Hash_Delete_Fields_Small)->Iterations(1000);

static void BM_Redis_Set_Remove_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_set_remove_small";
    std::string setKey = keyPrefix + ":set";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Generate member list for benchmarking
    std::vector<std::string> members;
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        members.push_back("member:" + std::to_string(i));
    }

    for (auto _ : state)
    {
        // Repopulate set before each iteration
        state.PauseTiming();
        for (const auto &member : members)
        {
            conn->setAdd(setKey, member);
        }
        state.ResumeTiming();

        for (const auto &member : members)
        {
            bool result = conn->setRemove(setKey, member);
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Redis_Set_Remove_Small)->Iterations(1000);

static void BM_Redis_SortedSet_Remove_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_zset_remove_small";
    std::string zsetKey = keyPrefix + ":zset";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Generate member list for benchmarking
    std::vector<std::string> members;
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        members.push_back("member:" + std::to_string(i));
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    for (auto _ : state)
    {
        // Repopulate sorted set before each iteration
        state.PauseTiming();
        for (size_t i = 0; i < members.size(); ++i)
        {
            conn->sortedSetAdd(zsetKey, static_cast<double>(i) * 1.5, members[i]);
        }
        state.ResumeTiming();

        for (const auto &member : members)
        {
            bool result = conn->sortedSetRemove(zsetKey, member);
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Redis_SortedSet_Remove_Small)->Iterations(1000);

// Medium dataset (100 entries)
static void BM_Redis_Delete_Keys_Medium_Individual(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_delete_medium_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Generate key list for benchmarking
    std::vector<std::string> keys;
    keys.reserve(common_benchmark_helpers::MEDIUM_SIZE);
    for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
    {
        keys.push_back(keyPrefix + ":" + std::to_string(i));
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    for (auto _ : state)
    {
        // Repopulate keys before each iteration
        state.PauseTiming();
        // cpp_dbc::system_utils::logWithTimestampInfo("Begin - SetString");
        for (const auto &key : keys)
        {
            conn->setString(key, "DeleteTest-" + common_benchmark_helpers::generateRandomString(20));
        }
        // cpp_dbc::system_utils::logWithTimestampInfo("End - SetString");
        state.ResumeTiming();

        for (const auto &key : keys)
        {
            bool result = conn->deleteKey(key);
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_Redis_Delete_Keys_Medium_Individual)->Iterations(1000);

static void BM_Redis_Delete_Keys_Medium_Batch(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_delete_medium_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);
    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Generate key list for benchmarking
    std::vector<std::string> keys;
    keys.reserve(common_benchmark_helpers::MEDIUM_SIZE);
    for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
    {
        keys.push_back(keyPrefix + ":" + std::to_string(i));
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    for (auto _ : state)
    {
        // Pause timing - setup won't be measured
        state.PauseTiming();

        // Repopulate keys before each iteration
        for (const auto &key : keys)
        {
            conn->setString(key, "DeleteTest-" + common_benchmark_helpers::generateRandomString(20));
        }

        // Resume timing - ONLY deleteKeys() is measured
        state.ResumeTiming();

        int64_t result = conn->deleteKeys(keys);
        benchmark::DoNotOptimize(result);
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
// Set fixed iterations and repetitions to ensure consistent benchmark runs
BENCHMARK(BM_Redis_Delete_Keys_Medium_Batch)->Iterations(1000);

// Large dataset (1000 entries) - limit to batch operations for performance
static void BM_Redis_Delete_Keys_Large_Batch(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_delete_large_batch";
    const int batchSize = 100; // Delete keys in batches for large dataset

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Generate key lists for batches
    std::vector<std::vector<std::string>> keyBatches;
    keyBatches.reserve(common_benchmark_helpers::LARGE_SIZE);
    for (int batch = 0; batch < common_benchmark_helpers::LARGE_SIZE / batchSize; batch++)
    {
        std::vector<std::string> batchKeys;
        for (int i = 1 + (batch * batchSize); i <= (batch + 1) * batchSize; ++i)
        {
            batchKeys.push_back(keyPrefix + ":" + std::to_string(i));
        }
        keyBatches.push_back(batchKeys);
    }

    for (auto _ : state)
    {
        // Repopulate keys in batches before each iteration
        state.PauseTiming();
        for (const auto &batchKeys : keyBatches)
        {
            for (const auto &key : batchKeys)
            {
                conn->setString(key, "DeleteTest-" + common_benchmark_helpers::generateRandomString(20));
            }
        }
        state.ResumeTiming();

        // Delete keys in batches
        for (const auto &batchKeys : keyBatches)
        {
            int64_t result = conn->deleteKeys(batchKeys);
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_Redis_Delete_Keys_Large_Batch)->Iterations(1000);

/*
// This benchmark is disabled because it's too destructive and can hang
// It attempts to flush the entire Redis database, which is potentially dangerous
static void BM_Redis_DeletePattern_XLarge(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_delete_pattern";
    const int keysToPopulate = 100; // Reduced key count for better performance

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    for (auto _ : state)
    {
        // Repopulate keys before each iteration
        state.PauseTiming();

        // Generate a unique token for this iteration to create a consistent pattern
        std::string uniqueToken = common_benchmark_helpers::generateRandomString(8);
        std::string uniquePattern = keyPrefix + "_" + uniqueToken + "_*";

        // Delete any existing keys with this prefix first
        std::vector<std::string> existingKeys = conn->scanKeys(keyPrefix + "_*");
        if (!existingKeys.empty())
        {
            conn->deleteKeys(existingKeys);
        }

        // Create new keys with the same unique token for this iteration
        std::vector<std::string> createdKeys;
        for (int i = 1; i <= keysToPopulate; ++i)
        {
            std::string key = keyPrefix + "_" + uniqueToken + "_" + std::to_string(i);
            conn->setString(key, "PatternTest-" + std::to_string(i));
            createdKeys.push_back(key);
        }

        // Find all keys matching the pattern to verify they were created
        std::vector<std::string> keys = conn->scanKeys(uniquePattern);
        // cpp_dbc::system_utils::logWithTimestampInfo("Populated " + std::to_string(keys.size()) + " keys for pattern deletion");

        state.ResumeTiming();

        // Delete all keys directly using our list of created keys rather than scanning
        int64_t result = conn->deleteKeys(createdKeys);
        benchmark::DoNotOptimize(result);
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * keysToPopulate);
}
BENCHMARK(BM_Redis_DeletePattern_XLarge);
*/

#else
// Register empty benchmark when Redis is disabled
static void BM_Redis_Delete_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("Redis support is not enabled");
        break;
    }
}
BENCHMARK(BM_Redis_Delete_Disabled);
#endif