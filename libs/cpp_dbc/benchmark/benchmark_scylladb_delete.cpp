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
 * @file benchmark_scylladb_delete.cpp
 * @brief Benchmarks for ScyllaDB DELETE operations
 */

#include "benchmark_common.hpp"

#if USE_SCYLLADB

/**
 * @brief Benchmarks individual DELETE operations against a ScyllaDB table using the small dataset.
 *
 * Establishes a ScyllaDB connection and test table with SMALL_SIZE rows (setup outside measurement), then
 * measures repeated iterations where each iteration deletes rows one-by-one by id. On connection failure the
 * benchmark is skipped. After measurement the connection is closed and the benchmark reports items processed
 * as iterations multiplied by SMALL_SIZE.
 *
 * @param state Benchmark state provided by Google Benchmark (controls iteration loop and receives counters).
 */
static void BM_ScyllaDB_Delete_Small_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_delete_small_ind";

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
                "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Delete_Small_Individual)->Iterations(1000);

/**
 * @brief Benchmarks deleting SMALL_SIZE rows using a prepared parameterized DELETE executed per id.
 *
 * Prepares a parameterized DELETE statement outside the timed section and, during each iteration, binds and executes it for ids 1..SMALL_SIZE.
 *
 * @param state Benchmark state controlling iterations and measurement; items processed are set to iterations() * SMALL_SIZE.
 */
static void BM_ScyllaDB_Delete_Small_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_delete_small_prep";

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
            "DELETE FROM " + tableName + " WHERE id = ?");
        state.ResumeTiming(); // Resume timing for actual operations

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            pstmt->setInt(1, i);
            auto result = pstmt->executeUpdate();
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Delete_Small_Prepared)->Iterations(1000);

/**
 * @brief Benchmarks deleting a small number of rows using a single DELETE with an IN clause.
 *
 * Constructs a DELETE statement with an IN(...) containing all IDs for the small dataset and executes it once per benchmark iteration while measurement is active; setup (creating connection and test data) and cleanup (closing the connection) occur outside the timed region.
 *
 * @param state Benchmark state controlling iterations; may be skipped with an error if a ScyllaDB connection cannot be established. The benchmark sets the items processed to iterations * SMALL_SIZE.
 */
static void BM_ScyllaDB_Delete_Small_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_delete_small_batch";

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
        // Build IN clause with all IDs
        std::string ids = "(";
        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            if (i > 1)
                ids += ",";
            ids += std::to_string(i);
        }
        ids += ")";

        auto result = conn->executeUpdate(
            "DELETE FROM " + tableName + " WHERE id IN " + ids);
        benchmark::DoNotOptimize(result);
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Delete_Small_Batch)->Iterations(1000);

/**
 * @brief Measures performance of deleting MEDIUM_SIZE rows from ScyllaDB one-by-one.
 *
 * Sets up the table "benchmark_scylladb_delete_medium_ind" with MEDIUM_SIZE test rows before measurement, deletes each row individually by id during each benchmark iteration, and closes the connection after measurement. Records items processed as iterations * MEDIUM_SIZE.
 */
static void BM_ScyllaDB_Delete_Medium_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_delete_medium_ind";

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
                "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_ScyllaDB_Delete_Medium_Individual)->Iterations(1000);

/**
 * @brief Measures deletion performance using a prepared DELETE for a medium-sized dataset.
 *
 * Runs repeated iterations that prepare a parameterized DELETE statement (preparation time is excluded
 * from the measured region) and then executes that prepared statement once per row id from 1 to MEDIUM_SIZE.
 *
 * @param state Benchmark state provided by Google Benchmark.
 *
 * @note Setup (connection and test data population) and cleanup (connection close) occur outside the timed region.
 * @note Records items processed as iterations * MEDIUM_SIZE.
 */
static void BM_ScyllaDB_Delete_Medium_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_delete_medium_prep";

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
            "DELETE FROM " + tableName + " WHERE id = ?");
        state.ResumeTiming(); // Resume timing for the actual operations

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            pstmt->setInt(1, i);
            auto result = pstmt->executeUpdate();
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_ScyllaDB_Delete_Medium_Prepared)->Iterations(1000);

/**
 * @brief Benchmarks deleting MEDIUM_SIZE rows from a ScyllaDB table using a single batch DELETE with an IN-clause.
 *
 * Sets up a ScyllaDB connection and populates the table "benchmark_scylladb_delete_medium_batch" with MEDIUM_SIZE test rows outside the timed region; for each benchmark iteration issues one DELETE ... WHERE id IN (...) that removes all test rows, then closes the connection after benchmarking. Reports items processed as iterations * MEDIUM_SIZE.
 *
 * @param state Benchmark state provided by Google Benchmark used to control iterations and record metrics.
 */
static void BM_ScyllaDB_Delete_Medium_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_delete_medium_batch";

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
        // Build IN clause with all IDs
        std::string ids = "(";
        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            if (i > 1)
                ids += ",";
            ids += std::to_string(i);
        }
        ids += ")";

        auto result = conn->executeUpdate(
            "DELETE FROM " + tableName + " WHERE id IN " + ids);
        benchmark::DoNotOptimize(result);
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_ScyllaDB_Delete_Medium_Batch)->Iterations(1000);

// Large dataset (1000 rows) - DISABLED due to ScyllaDB IN clause size limit
// ScyllaDB throws exception: "size of partition-key IN list or partition-key cartesian
// product of IN list 1000 is greater than maximum 100" (error code 772E10871903)
// when trying to use IN clause with 1000 IDs in a single DELETE statement.
// The default limit is 100 items in the IN clause.
// Chunking the deletes would change the benchmark semantics (measuring multiple
// DELETE statements instead of a single batch delete), so this benchmark is
// disabled rather than modified.
/**
 * @brief Benchmarks deleting a large dataset using a prepared DELETE executed per id.
 *
 * Runs iterations that execute a prepared DELETE statement for each id from 1 to common_benchmark_helpers::LARGE_SIZE,
 * excluding statement preparation from the measured timing. The benchmark sets the total items processed to
 * iterations * LARGE_SIZE and performs setup/teardown outside the measured region.
 *
 * @param state Benchmark state controlling iterations and timing.
 */

static void BM_ScyllaDB_Delete_Large_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_delete_large_prep";

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
            "DELETE FROM " + tableName + " WHERE id = ?");
        state.ResumeTiming(); // Resume timing for the actual operations

        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
        {
            pstmt->setInt(1, i);
            auto result = pstmt->executeUpdate();
            benchmark::DoNotOptimize(result);
        }
    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_ScyllaDB_Delete_Large_Prepared)->Iterations(100);

// XLarge dataset (10000 rows) - DISABLED due to ScyllaDB IN clause size limit
// ScyllaDB throws exception: "size of partition-key IN list or partition-key cartesian
// product of IN list 10000 is greater than maximum 100" (error code 772E10871903)
// when trying to use IN clause with 10000 IDs in a single DELETE statement.
// The default limit is 100 items in the IN clause.
// Chunking the deletes would change the benchmark semantics (measuring multiple
// DELETE statements instead of a single batch delete), so this benchmark is
// disabled rather than modified.
/*
static void BM_ScyllaDB_Delete_XLarge_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_delete_xlarge_batch";

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
        // Build IN clause with all IDs
        std::string ids = "(";
        for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
        {
            if (i > 1) ids += ",";
            ids += std::to_string(i);
        }
        ids += ")";

        auto result = conn->executeUpdate(
            "DELETE FROM " + tableName + " WHERE id IN " + ids);
        benchmark::DoNotOptimize(result);

    }
    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::XLARGE_SIZE);
}
BENCHMARK(BM_ScyllaDB_Delete_XLarge_Batch)->Iterations(1000);
*/

#else
/**
 * @brief Benchmark callback that immediately skips when ScyllaDB support is not enabled.
 *
 * Signals the benchmark framework to skip this benchmark with an explanatory error message.
 *
 * @param state Benchmark state used to control iterations and report the skip.
 */
static void BM_ScyllaDB_Delete_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("ScyllaDB support is not enabled");
        break;
    }
}
BENCHMARK(BM_ScyllaDB_Delete_Disabled);
#endif