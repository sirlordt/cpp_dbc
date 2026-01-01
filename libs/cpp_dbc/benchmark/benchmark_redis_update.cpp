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
 * @file benchmark_redis_update.cpp
 * @brief Benchmarks for Redis UPDATE operations
 */

#include "benchmark_common.hpp"

#if USE_REDIS

// Small dataset (10 entries)
static void BM_Redis_Update_String_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_update_string_small";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection and populating with " +
                                                std::to_string(common_benchmark_helpers::SMALL_SIZE) + " keys for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix, common_benchmark_helpers::SMALL_SIZE);

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
        for (size_t i = 0; i < keys.size(); ++i)
        {
            std::string newValue = "UpdatedValue-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);
            bool result = conn->setString(keys[i], newValue);
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * keys.size());
}
BENCHMARK(BM_Redis_Update_String_Small);

static void BM_Redis_Increment_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_increment_small";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Initialize counter keys
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        std::string key = keyPrefix + ":counter:" + std::to_string(i);
        // Set initial value to 0
        conn->setString(key, "0");
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Generate key list for benchmarking
    std::vector<std::string> keys;
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        keys.push_back(keyPrefix + ":counter:" + std::to_string(i));
    }

    for (auto _ : state)
    {
        for (const auto &key : keys)
        {
            int64_t result = conn->increment(key, 1);
            benchmark::DoNotOptimize(result);
        }

        // Reset counters after each full iteration
        state.PauseTiming();
        for (const auto &key : keys)
        {
            conn->setString(key, "0");
        }
        state.ResumeTiming();
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * keys.size());
}
BENCHMARK(BM_Redis_Increment_Small);

static void BM_Redis_Decrement_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_decrement_small";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Initialize counter keys
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        std::string key = keyPrefix + ":counter:" + std::to_string(i);
        // Set initial value to 1000 (so we can decrement without going negative)
        conn->setString(key, "1000");
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Generate key list for benchmarking
    std::vector<std::string> keys;
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        keys.push_back(keyPrefix + ":counter:" + std::to_string(i));
    }

    for (auto _ : state)
    {
        for (const auto &key : keys)
        {
            int64_t result = conn->decrement(key, 1);
            benchmark::DoNotOptimize(result);
        }

        // Reset counters after each full iteration
        state.PauseTiming();
        for (const auto &key : keys)
        {
            conn->setString(key, "1000");
        }
        state.ResumeTiming();
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * keys.size());
}
BENCHMARK(BM_Redis_Decrement_Small);

static void BM_Redis_Update_Hash_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_update_hash_small";
    std::string hashKey = keyPrefix + ":hash";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Populate hash with initial test data
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        std::string field = "field:" + std::to_string(i);
        std::string value = "InitialValue-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);
        conn->hashSet(hashKey, field, value);
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
        for (size_t i = 0; i < fields.size(); ++i)
        {
            std::string newValue = "UpdatedValue-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);
            bool result = conn->hashSet(hashKey, fields[i], newValue);
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * fields.size());
}
BENCHMARK(BM_Redis_Update_Hash_Small);

static void BM_Redis_Update_SortedSet_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_update_sortedset_small";
    std::string zsetKey = keyPrefix + ":zset";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Populate sorted set with initial test data
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        std::string member = "member:" + std::to_string(i);
        double score = i * 1.0;
        conn->sortedSetAdd(zsetKey, score, member);
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
        for (size_t i = 0; i < members.size(); ++i)
        {
            // Update score with a new value
            double newScore = (static_cast<double>(i) + 1.0) * 10.5;
            bool result = conn->sortedSetAdd(zsetKey, newScore, members[i]);
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * members.size());
}
BENCHMARK(BM_Redis_Update_SortedSet_Small);

// Medium dataset (100 entries)
static void BM_Redis_Update_String_Medium(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_update_string_medium";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection and populating with " +
                                                std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " keys for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Generate key list for benchmarking
    std::vector<std::string> keys;
    for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
    {
        keys.push_back(keyPrefix + ":" + std::to_string(i));
    }

    for (auto _ : state)
    {
        for (size_t i = 0; i < keys.size(); ++i)
        {
            std::string newValue = "UpdatedValue-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);
            bool result = conn->setString(keys[i], newValue);
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * keys.size());
}
BENCHMARK(BM_Redis_Update_String_Medium);

static void BM_Redis_Increment_Medium_Batch(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_increment_medium";
    const int batchSize = 10; // Number of keys to increment in a batch

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Initialize counter keys
    for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
    {
        std::string key = keyPrefix + ":counter:" + std::to_string(i);
        conn->setString(key, "0");
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Generate key lists for batches
    std::vector<std::vector<std::string>> keyBatches;
    for (int batch = 0; batch < common_benchmark_helpers::MEDIUM_SIZE / batchSize; batch++)
    {
        std::vector<std::string> batchKeys;
        for (int i = 1 + (batch * batchSize); i <= (batch + 1) * batchSize; ++i)
        {
            batchKeys.push_back(keyPrefix + ":counter:" + std::to_string(i));
        }
        keyBatches.push_back(batchKeys);
    }

    for (auto _ : state)
    {
        for (const auto &batchKeys : keyBatches)
        {
            for (const auto &key : batchKeys)
            {
                int64_t result = conn->increment(key, 1);
                benchmark::DoNotOptimize(result);
            }
        }

        // Reset counters after each full iteration
        state.PauseTiming();
        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            std::string key = keyPrefix + ":counter:" + std::to_string(i);
            conn->setString(key, "0");
        }
        state.ResumeTiming();
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_Redis_Increment_Medium_Batch);

// Large dataset (1000 entries) - only a single benchmark to avoid excessive runtime
static void BM_Redis_Update_String_Large_Batch(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_update_string_large";
    const int batchSize = 100; // Update values in batches for large dataset

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection and populating with " +
                                                std::to_string(common_benchmark_helpers::LARGE_SIZE) + " keys for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Generate key lists for batches
    std::vector<std::vector<std::string>> keyBatches;
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
        for (const auto &batchKeys : keyBatches)
        {
            for (size_t i = 0; i < batchKeys.size(); ++i)
            {
                std::string newValue = "UpdatedValue-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);
                bool result = conn->setString(batchKeys[i], newValue);
                benchmark::DoNotOptimize(result);
            }
        }
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_Redis_Update_String_Large_Batch);

#else
// Register empty benchmark when Redis is disabled
static void BM_Redis_Update_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("Redis support is not enabled");
        break;
    }
}
BENCHMARK(BM_Redis_Update_Disabled);
#endif