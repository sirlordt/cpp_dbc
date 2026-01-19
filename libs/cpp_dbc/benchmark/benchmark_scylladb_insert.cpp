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
 * @file benchmark_scylladb_insert.cpp
 * @brief Benchmarks for ScyllaDB INSERT operations
 */

#include "benchmark_common.hpp"

#if USE_SCYLLADB

// Small dataset (10 rows)
static void BM_ScyllaDB_Insert_Small_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_insert_small_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
    static int runCounter = 0;

    // Benchmark measurement begins
    for (auto _ : state)
    {
        int runId = ++runCounter;

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            auto result = conn->executeUpdate(
                "INSERT INTO " + tableName + " (id, name, value, description) VALUES (" +
                std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                std::to_string(i * 1.5) + ", '" + common_benchmark_helpers::generateRandomString(50) + "')");
            benchmark::DoNotOptimize(result);
        }
    }

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Insert_Small_Individual)->Iterations(1000);

static void BM_ScyllaDB_Insert_Small_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_insert_small_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description) VALUES (?, ?, ?, ?)");
        state.ResumeTiming(); // Resume timing for actual operations

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            pstmt->setInt(1, uniqueId);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
            auto result = pstmt->executeUpdate();
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Insert_Small_Prepared)->Iterations(1000);

static void BM_ScyllaDB_Insert_Small_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_insert_small_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Use a unique ID prefix for each benchmark run
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing batch
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description) VALUES (?, ?, ?, ?)");

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            int uniqueId = runId * 10000 + i;
            pstmt->setInt(1, uniqueId);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
            pstmt->addBatch();
        }
        state.ResumeTiming(); // Resume timing for actual batch execution

        auto result = pstmt->executeBatch();
        benchmark::DoNotOptimize(result);
    }

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Insert_Small_Batch)->Iterations(1000);

// Medium dataset (100 rows)
static void BM_ScyllaDB_Insert_Medium_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_insert_medium_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    // Use a unique ID prefix for each benchmark run
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            int uniqueId = runId * 10000 + i;

            auto result = conn->executeUpdate(
                "INSERT INTO " + tableName + " (id, name, value, description) VALUES (" +
                std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                std::to_string(i * 1.5) + ", '" + common_benchmark_helpers::generateRandomString(50) + "')");
            benchmark::DoNotOptimize(result);
        }

        state.ResumeTiming();
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_ScyllaDB_Insert_Medium_Individual)->Iterations(1000);

static void BM_ScyllaDB_Insert_Medium_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_insert_medium_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    // Use a unique ID prefix for each benchmark run
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description) VALUES (?, ?, ?, ?)");
        state.ResumeTiming(); // Resume timing for actual operations

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            int uniqueId = runId * 10000 + i;

            pstmt->setInt(1, uniqueId);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
            auto result = pstmt->executeUpdate();
            benchmark::DoNotOptimize(result);
        }

        state.ResumeTiming();
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_ScyllaDB_Insert_Medium_Prepared)->Iterations(1000);

static void BM_ScyllaDB_Insert_Medium_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_insert_medium_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    // Use a unique ID prefix for each benchmark run
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing batch
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description) VALUES (?, ?, ?, ?)");

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            int uniqueId = runId * 10000 + i;
            pstmt->setInt(1, uniqueId);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
            pstmt->addBatch();
        }
        state.ResumeTiming(); // Resume timing for actual batch execution

        auto result = pstmt->executeBatch();
        benchmark::DoNotOptimize(result);

        state.ResumeTiming();
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_ScyllaDB_Insert_Medium_Batch)->Iterations(1000);

// Large dataset (1000 rows) - only prepared statement and batch for better performance
static void BM_ScyllaDB_Insert_Large_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_insert_large_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    // Use a unique ID prefix for each benchmark run
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description) VALUES (?, ?, ?, ?)");
        state.ResumeTiming(); // Resume timing for actual operations

        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
        {
            int uniqueId = runId * 10000 + i;

            pstmt->setInt(1, uniqueId);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
            auto result = pstmt->executeUpdate();
            benchmark::DoNotOptimize(result);
        }

        state.ResumeTiming();
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_ScyllaDB_Insert_Large_Prepared)->Iterations(100);

static void BM_ScyllaDB_Insert_Large_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_insert_large_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    // Use a unique ID prefix for each benchmark run
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing batch
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description) VALUES (?, ?, ?, ?)");

        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
        {
            int uniqueId = runId * 10000 + i;
            pstmt->setInt(1, uniqueId);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
            pstmt->addBatch();
        }
        state.ResumeTiming(); // Resume timing for actual batch execution

        auto result = pstmt->executeBatch();
        benchmark::DoNotOptimize(result);

        state.ResumeTiming();
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_ScyllaDB_Insert_Large_Batch)->Iterations(1000);

// XLarge dataset (10000 rows) - DISABLED due to ScyllaDB batch size limit
// ScyllaDB throws exception: "Batch too large" (error code 295872350923)
// when trying to batch 10000 INSERT operations in a single executeBatch() call.
// The batch size limit appears to be around 100-1000 items depending on row size.
// Chunking the batch would change the benchmark semantics (measuring multiple
// executeBatch() calls instead of a single large batch), so this benchmark is
// disabled rather than modified.
/*
static void BM_ScyllaDB_Insert_XLarge_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_insert_xlarge_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    // Use a unique ID prefix for each benchmark run
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing batch
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description) VALUES (?, ?, ?, ?)");

        for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
        {
            int uniqueId = runId * 100000 + i;
            pstmt->setInt(1, uniqueId);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
            pstmt->addBatch();
        }
        state.ResumeTiming(); // Resume timing for actual batch execution

        auto result = pstmt->executeBatch();
        benchmark::DoNotOptimize(result);

        state.ResumeTiming();
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::XLARGE_SIZE);
}
BENCHMARK(BM_ScyllaDB_Insert_XLarge_Batch)->Iterations(1000);
*/

#else
// Register empty benchmark when ScyllaDB is disabled
static void BM_ScyllaDB_Insert_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("ScyllaDB support is not enabled");
        break;
    }
}
BENCHMARK(BM_ScyllaDB_Insert_Disabled);
#endif
