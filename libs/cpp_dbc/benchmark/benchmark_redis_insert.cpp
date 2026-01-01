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
 * @file benchmark_redis_insert.cpp
 * @brief Benchmarks for Redis INSERT/SET operations
 */

#include "benchmark_common.hpp"

#if USE_REDIS

// Small dataset (10 entries)
static void BM_Redis_Set_String_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_set_string_small";

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
        // Clean any existing keys before each iteration
        redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            std::string key = keyPrefix + ":" + std::to_string(i);
            std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);

            bool result = conn->setString(key, value);
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
BENCHMARK(BM_Redis_Set_String_Small);

static void BM_Redis_Set_String_With_TTL_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_set_string_ttl_small";
    const int ttl = 300; // 5 minutes TTL

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
        // Clean any existing keys before each iteration
        redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            std::string key = keyPrefix + ":" + std::to_string(i);
            std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);

            bool result = conn->setString(key, value, ttl);
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
BENCHMARK(BM_Redis_Set_String_With_TTL_Small);

static void BM_Redis_Hash_Set_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_hash_small";

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
        // Use a single hash key for all fields
        std::string hashKey = keyPrefix + ":hash";

        // Clean existing hash before each iteration
        redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            std::string field = "field:" + std::to_string(i);
            std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);

            bool result = conn->hashSet(hashKey, field, value);
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
BENCHMARK(BM_Redis_Hash_Set_Small);

static void BM_Redis_List_Push_Small(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_list_small";

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
        // Use a single list key
        std::string listKey = keyPrefix + ":list";

        // Clean existing list before each iteration
        redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);

            int64_t result = conn->listPushRight(listKey, value);
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
BENCHMARK(BM_Redis_List_Push_Small);

// Medium dataset (100 entries)
static void BM_Redis_Set_String_Medium(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_set_string_medium";

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
        // Clean any existing keys before each iteration
        redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            std::string key = keyPrefix + ":" + std::to_string(i);
            std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);

            bool result = conn->setString(key, value);
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
BENCHMARK(BM_Redis_Set_String_Medium);

static void BM_Redis_Hash_Set_Medium(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_hash_medium";

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
        // Use a single hash key for all fields
        std::string hashKey = keyPrefix + ":hash";

        // Clean existing hash before each iteration
        redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            std::string field = "field:" + std::to_string(i);
            std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);

            bool result = conn->hashSet(hashKey, field, value);
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
BENCHMARK(BM_Redis_Hash_Set_Medium);

// Large dataset (1000 entries)
static void BM_Redis_Set_String_Large(benchmark::State &state)
{
    const std::string keyPrefix = "benchmark_redis_set_string_large";

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
        // Clean any existing keys before each iteration
        redis_benchmark_helpers::cleanupRedisKeys(conn, keyPrefix);

        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
        {
            std::string key = keyPrefix + ":" + std::to_string(i);
            std::string value = "Value-" + std::to_string(i) + "-" + common_benchmark_helpers::generateRandomString(20);

            bool result = conn->setString(key, value);
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
BENCHMARK(BM_Redis_Set_String_Large);

#else
// Register empty benchmark when Redis is disabled
static void BM_Redis_Insert_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("Redis support is not enabled");
        break;
    }
}
BENCHMARK(BM_Redis_Insert_Disabled);
#endif