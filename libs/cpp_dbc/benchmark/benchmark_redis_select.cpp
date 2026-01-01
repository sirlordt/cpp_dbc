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
 * @file benchmark_redis_select.cpp
 * @brief Benchmarks for Redis SELECT/GET operations
 */

#include "benchmark_common.hpp"

#if USE_REDIS

// Small dataset (10 entries)
static void BM_Redis_Get_String_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_get_string_small";

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
        for (const auto &key : keys)
        {
            std::string result = conn->getString(key);
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
BENCHMARK(BM_Redis_Get_String_Small);

static void BM_Redis_Exists_Keys_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_exists_small";

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
        for (const auto &key : keys)
        {
            bool result = conn->exists(key);
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
BENCHMARK(BM_Redis_Exists_Keys_Small);

static void BM_Redis_Hash_Get_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_hash_get_small";
    std::string hashKey = keyPrefix + ":hash";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Populate hash with test data
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        std::string field = "field:" + std::to_string(i);
        std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);
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
        for (const auto &field : fields)
        {
            std::string result = conn->hashGet(hashKey, field);
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
BENCHMARK(BM_Redis_Hash_Get_Small);

static void BM_Redis_Hash_GetAll_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_hash_getall_small";
    std::string hashKey = keyPrefix + ":hash";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Populate hash with test data
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        std::string field = "field:" + std::to_string(i);
        std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);
        conn->hashSet(hashKey, field, value);
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    for (auto _ : state)
    {
        auto result = conn->hashGetAll(hashKey);
        benchmark::DoNotOptimize(result);
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Redis_Hash_GetAll_Small);

static void BM_Redis_List_Range_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_list_range_small";
    std::string listKey = keyPrefix + ":list";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Redis connection for benchmark '" + keyPrefix + "'...");
    auto conn = redis_benchmark_helpers::setupRedisConnection(keyPrefix);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Redis database");
        return;
    }

    // Populate list with test data
    for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
    {
        std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);
        conn->listPushRight(listKey, value);
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    for (auto _ : state)
    {
        auto result = conn->listRange(listKey, 0, -1); // Get all items
        benchmark::DoNotOptimize(result);
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Redis_List_Range_Small);

// Medium dataset (100 entries)
static void BM_Redis_Get_String_Medium(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_get_string_medium";

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
        for (const auto &key : keys)
        {
            std::string result = conn->getString(key);
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
BENCHMARK(BM_Redis_Get_String_Medium);

static void BM_Redis_ScanKeys_Medium(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_scan_medium";

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

    for (auto _ : state)
    {
        auto result = conn->scanKeys(keyPrefix + ":*");
        benchmark::DoNotOptimize(result);
    }

    // Cleanup - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up Redis keys with prefix '" + keyPrefix + "'...");
    redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Redis_ScanKeys_Medium);

// Large dataset (1000 entries)
static void BM_Redis_Get_String_Large(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_get_string_large";
    const int batchSize = 100; // Get values in batches for large dataset

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

    // Generate key lists for benchmarking in batches
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
            for (const auto &key : batchKeys)
            {
                std::string result = conn->getString(key);
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
BENCHMARK(BM_Redis_Get_String_Large);

#else
// Register empty benchmark when Redis is disabled
static void BM_Redis_Select_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("Redis support is not enabled");
        break;
    }
}
BENCHMARK(BM_Redis_Select_Disabled);
#endif