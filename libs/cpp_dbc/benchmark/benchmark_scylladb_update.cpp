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
 * @file benchmark_scylladb_update.cpp
 * @brief Benchmarks for ScyllaDB UPDATE operations
 */

#include "benchmark_common.hpp"

#if USE_SCYLLADB

/**
 * @brief Benchmarks individual UPDATE operations on a small (10-row) ScyllaDB table.
 *
 * Sets up a table populated with SMALL_SIZE rows (outside timing), then measures the cost of executing
 * a separate UPDATE statement for each row in the table on every benchmark iteration. Cleans up the
 * connection after the benchmark and reports items processed as iterations × SMALL_SIZE.
 *
 * @param state Benchmark state provided by Google Benchmark; used to drive iterations and report metrics.
 */
static void BM_ScyllaDB_Update_Small_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_update_small_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    for (auto _ : state)
    {
        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            auto result = conn->executeUpdate(
                "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                "', value = " + std::to_string(i * 2.5) +
                ", description = '" + common_benchmark_helpers::generateRandomString(60) +
                "' WHERE id = " + std::to_string(i));
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Update_Small_Individual)->Iterations(1000);

/**
 * @brief Measures prepared UPDATE operations on a small dataset.
 *
 * Sets up a table with SMALL_SIZE rows outside of measured timing, pauses timing to prepare
 * a single parameterized UPDATE statement, then resumes timing and executes SMALL_SIZE
 * prepared updates per iteration. If the ScyllaDB connection cannot be established, the
 * benchmark is skipped.
 *
 * @param state Google Benchmark state used to control iterations and timing.
 */
static void BM_ScyllaDB_Update_Small_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_update_small_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");
        state.ResumeTiming(); // Resume timing for the actual operations

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            pstmt->setString(1, "Updated Name " + std::to_string(i));
            pstmt->setDouble(2, i * 2.5);
            pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
            pstmt->setInt(4, i);
            auto result = pstmt->executeUpdate();
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Update_Small_Prepared)->Iterations(1000);

/**
 * @brief Measures ScyllaDB batched UPDATE performance on a small dataset.
 *
 * Sets up a ScyllaDB connection and a table populated with common_benchmark_helpers::SMALL_SIZE rows,
 * then repeatedly prepares a batch containing SMALL_SIZE UPDATE statements (preparation done while timing
 * is paused) and measures the time to execute the batch. Cleans up the connection after the benchmark
 * and records items processed as iterations × SMALL_SIZE.
 *
 * @param state Benchmark state provided by Google Benchmark.
 */
static void BM_ScyllaDB_Update_Small_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_update_small_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing batch
        auto pstmt = conn->prepareStatement(
            "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            pstmt->setString(1, "Batch Updated " + std::to_string(i));
            pstmt->setDouble(2, i * 2.5);
            pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
            pstmt->setInt(4, i);
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

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Update_Small_Batch)->Iterations(1000);

/**
 * @brief Measures individual UPDATE operations on a medium (100-row) ScyllaDB table.
 *
 * Sets up a table populated with 100 rows outside of the timed section, then in each benchmark
 * iteration issues separate UPDATE statements for each row. Records items processed as
 * iterations × 100 and performs cleanup outside of timing.
 *
 * @param state Benchmark state provided by Google Benchmark.
 */
static void BM_ScyllaDB_Update_Medium_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_update_medium_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    for (auto _ : state)
    {
        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            auto result = conn->executeUpdate(
                "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                "', value = " + std::to_string(i * 2.5) +
                ", description = '" + common_benchmark_helpers::generateRandomString(60) +
                "' WHERE id = " + std::to_string(i));
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_ScyllaDB_Update_Medium_Individual)->Iterations(1000);

/**
 * @brief Benchmarks UPDATE performance using a prepared statement against a medium-sized table.
 *
 * Sets up a ScyllaDB connection and table populated with MEDIUM_SIZE rows, then for each benchmark
 * iteration prepares an UPDATE statement (preparation excluded from timing) and executes MEDIUM_SIZE
 * updates using bound parameters. Records total items processed as iterations × MEDIUM_SIZE.
 *
 * @param state Benchmark state used to control iterations and timing.
 */
static void BM_ScyllaDB_Update_Medium_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_update_medium_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");
        state.ResumeTiming(); // Resume timing for the actual operations

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            pstmt->setString(1, "Updated Name " + std::to_string(i));
            pstmt->setDouble(2, i * 2.5);
            pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
            pstmt->setInt(4, i);
            auto result = pstmt->executeUpdate();
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_ScyllaDB_Update_Medium_Prepared)->Iterations(1000);

/**
 * @brief Measures batched UPDATE performance on a medium-sized ScyllaDB table.
 *
 * Prepares a batch of UPDATE statements for MEDIUM_SIZE rows while timing is paused,
 * executes the batch while timing is active, and records the result. If a ScyllaDB
 * connection cannot be established, the benchmark is skipped. After completion the
 * connection is closed and the number of items processed is set to iterations × MEDIUM_SIZE.
 *
 * @param state Benchmark state used to control iterations, timing, and to report results.
 */
static void BM_ScyllaDB_Update_Medium_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_update_medium_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing batch
        auto pstmt = conn->prepareStatement(
            "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            pstmt->setString(1, "Batch Updated " + std::to_string(i));
            pstmt->setDouble(2, i * 2.5);
            pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
            pstmt->setInt(4, i);
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
BENCHMARK(BM_ScyllaDB_Update_Medium_Batch)->Iterations(1000);

/**
 * @brief Benchmarks individual UPDATE operations on a large ScyllaDB table.
 *
 * Runs a benchmark that updates each row in a prepopulated table of size common_benchmark_helpers::LARGE_SIZE (1000)
 * by executing one UPDATE per row inside the measured loop. Setup and teardown (connection establishment and closing)
 * occur outside the timed section. The benchmark records items processed as iterations × LARGE_SIZE.
 *
 * @param state Google Benchmark state controlling the benchmark loop and metrics.
 */
static void BM_ScyllaDB_Update_Large_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_update_large_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    for (auto _ : state)
    {
        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
        {
            auto result = conn->executeUpdate(
                "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                "', value = " + std::to_string(i * 2.5) +
                ", description = '" + common_benchmark_helpers::generateRandomString(60) +
                "' WHERE id = " + std::to_string(i));
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_ScyllaDB_Update_Large_Individual)->Iterations(100);

/**
 * @brief Benchmarks prepared UPDATE statements on a large ScyllaDB table.
 *
 * Updates each row in a table of size LARGE_SIZE using a prepared UPDATE statement.
 * Statement preparation is performed while timing is paused; the timed section covers binding parameters and executing the prepared updates.
 * If a ScyllaDB connection cannot be established, the benchmark is skipped via state.SkipWithError.
 *
 * @param state Google Benchmark state object used to control iterations, timing, and metrics.
 *
 * @note On completion, the function sets the items-processed metric to iterations() * LARGE_SIZE.
 */
static void BM_ScyllaDB_Update_Large_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_update_large_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");
        state.ResumeTiming(); // Resume timing for the actual operations

        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
        {
            pstmt->setString(1, "Updated Name " + std::to_string(i));
            pstmt->setDouble(2, i * 2.5);
            pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
            pstmt->setInt(4, i);
            auto result = pstmt->executeUpdate();
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_ScyllaDB_Update_Large_Prepared)->Iterations(100);

/**
 * @brief Measures batch UPDATE performance on a large dataset.
 *
 * Executes, in each benchmark iteration, a single batched UPDATE containing LARGE_SIZE parameterized updates
 * and records the batch execution performance. If a ScyllaDB connection cannot be established the benchmark
 * is skipped. The connection is closed after the benchmark and the number of processed items is set to
 * iterations × LARGE_SIZE.
 */
static void BM_ScyllaDB_Update_Large_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_update_large_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing batch
        auto pstmt = conn->prepareStatement(
            "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");

        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
        {
            pstmt->setString(1, "Batch Updated " + std::to_string(i));
            pstmt->setDouble(2, i * 2.5);
            pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
            pstmt->setInt(4, i);
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
BENCHMARK(BM_ScyllaDB_Update_Large_Batch)->Iterations(100);

// Extra Large dataset (10000 rows) - DISABLED due to ScyllaDB batch size limit
// ScyllaDB throws exception: "Batch too large" (error code 295872350923)
// when trying to batch 10000 UPDATE operations in a single executeBatch() call.
// The batch size limit appears to be around 100-1000 items depending on row size.
// Chunking the batch would change the benchmark semantics (measuring multiple
// executeBatch() calls instead of a single large batch), so this benchmark is
// disabled rather than modified.
/*
static void BM_ScyllaDB_Update_XLarge_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_update_xlarge_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::XLARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");
    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing batch
        auto pstmt = conn->prepareStatement(
            "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");

        for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
        {
            pstmt->setString(1, "Batch Updated " + std::to_string(i));
            pstmt->setDouble(2, i * 2.5);
            pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
            pstmt->setInt(4, i);
            pstmt->addBatch();
        }
        state.ResumeTiming(); // Resume timing for actual batch execution

        auto result = pstmt->executeBatch();
        benchmark::DoNotOptimize(result);
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::XLARGE_SIZE);
}
BENCHMARK(BM_ScyllaDB_Update_XLarge_Batch)->Iterations(100);
*/

#else
/**
 * @brief Placeholder benchmark that immediately skips execution when ScyllaDB support is not enabled.
 *
 * Signals the benchmark framework to skip this benchmark and reports the error message
 * "ScyllaDB support is not enabled".
 *
 * @param state Benchmark state object used to mark the benchmark as skipped.
 */
static void BM_ScyllaDB_Update_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("ScyllaDB support is not enabled");
        break;
    }
}
BENCHMARK(BM_ScyllaDB_Update_Disabled);
#endif