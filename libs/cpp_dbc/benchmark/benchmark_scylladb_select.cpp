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
 * @file benchmark_scylladb_select.cpp
 * @brief Benchmarks for ScyllaDB SELECT operations
 */

#include "benchmark_common.hpp"

#if USE_SCYLLADB

/**
 * @brief Benchmarks selecting all columns from a small ScyllaDB table.
 *
 * Establishes a connection and a test table populated with SMALL_SIZE rows and skips the benchmark if the connection cannot be made.
 * For each benchmark iteration executes "SELECT *" against the table and counts the returned rows. After the benchmark closes the connection and records total items processed as iterations * SMALL_SIZE.
 */
static void BM_ScyllaDB_Select_Small_AllColumns(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_select_small_all";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT * FROM " + tableName);
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Select_Small_AllColumns)->Iterations(1000);

/**
 * @brief Benchmarks selecting the `id` column from a small ScyllaDB table and measures query/result iteration cost.
 *
 * This benchmark sets up a small test table outside the timed section, executes `SELECT id FROM <table>` each iteration,
 * iterates the result set to count returned rows, and performs cleanup outside the timed section.
 *
 * @note Items processed are recorded as `state.iterations() * common_benchmark_helpers::SMALL_SIZE`.
 */
static void BM_ScyllaDB_Select_Small_SingleColumn(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_select_small_single";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT id FROM " + tableName);
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_ScyllaDB_Select_Small_SingleColumn)->Iterations(1000);

/**
 * @brief Benchmarks selecting all columns with a WHERE clause restricting id to 5 on a small test table.
 *
 * Executes the query `SELECT * FROM <table> WHERE id <= 5 ALLOW FILTERING` once per iteration and materializes the result set by counting returned rows.
 *
 * @note Each benchmark iteration processes 5 rows; the benchmark records total items processed as iterations * 5.
 */
static void BM_ScyllaDB_Select_Small_WhereClause(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_select_small_where";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 5 ALLOW FILTERING");
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 5); // Only 5 rows processed per iteration
}
BENCHMARK(BM_ScyllaDB_Select_Small_WhereClause)->Iterations(1000);

/**
 * @brief Benchmarks execution of a prepared SELECT statement against a small test table.
 *
 * Measures the time to execute the prepared query "SELECT * FROM <table> WHERE id > ? ALLOW FILTERING"
 * with the parameter bound to 5. Statement preparation is excluded from the measured time.
 *
 * @param state Benchmark state provided by Google Benchmark.
 */
static void BM_ScyllaDB_Select_Small_PreparedStatement(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_select_small_prepared";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop

    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > ? ALLOW FILTERING");
        pstmt->setInt(1, 5);
        state.ResumeTiming(); // Resume timing for query execution

        auto rs = pstmt->executeQuery();
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 5); // 10 - 5 = 5 rows per iteration
}
BENCHMARK(BM_ScyllaDB_Select_Small_PreparedStatement)->Iterations(1000);

/**
 * @brief Benchmarks selecting all columns from a medium-sized test table in ScyllaDB.
 *
 * Establishes a ScyllaDB connection and verifies the test table exists outside the measurement,
 * executes "SELECT * FROM <table>" once per iteration and counts returned rows, then closes the connection.
 *
 * @param state Benchmark state used for iterations and reporting; the benchmark will call SkipWithError on failure to connect.
 * @note Records items processed as `state.iterations() * common_benchmark_helpers::MEDIUM_SIZE`.
 */
static void BM_ScyllaDB_Select_Medium_AllColumns(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_select_medium_all";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT * FROM " + tableName);
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_ScyllaDB_Select_Medium_AllColumns)->Iterations(1000);

/**
 * @brief Benchmarks selecting a single column (`id`) from a medium-sized ScyllaDB table.
 *
 * Executes repeated `SELECT id FROM <table>` queries against a prepared medium dataset,
 * counts rows returned per iteration, and records total items processed as
 * `state.iterations() * common_benchmark_helpers::MEDIUM_SIZE`.
 */
static void BM_ScyllaDB_Select_Medium_SingleColumn(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_select_medium_single";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT id FROM " + tableName);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_ScyllaDB_Select_Medium_SingleColumn)->Iterations(1000);

/**
 * @brief Benchmarks executing a SELECT with a WHERE clause against a medium-size ScyllaDB table.
 *
 * Connects to a medium-size test table, and for each benchmark iteration executes
 * "SELECT * FROM <table> WHERE id <= 50 ALLOW FILTERING", materializes and counts
 * the returned rows. If the connection cannot be established the benchmark is skipped.
 *
 * @param state Benchmark state object driving iteration and timing; used to set items processed.
 */
static void BM_ScyllaDB_Select_Medium_WhereClause(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_select_medium_where";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 50 ALLOW FILTERING");
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 50); // Only 50 rows processed per iteration
}
BENCHMARK(BM_ScyllaDB_Select_Medium_WhereClause)->Iterations(1000);

/**
 * Benchmarks selecting the `id` column from a large ScyllaDB test table and reports total items processed.
 *
 * Establishes a ScyllaDB connection and runs repeated executions of `SELECT id FROM <table>` while counting rows
 * returned; the benchmark is skipped if the connection cannot be established. After completion the connection is closed
 * and items processed are recorded as `state.iterations() * LARGE_SIZE`.
 */
static void BM_ScyllaDB_Select_Large_SingleColumn(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_select_large_single";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT id FROM " + tableName);
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_ScyllaDB_Select_Large_SingleColumn)->Iterations(1000);

/**
 * @brief Benchmarks selecting rows with a WHERE clause from a large ScyllaDB table.
 *
 * Executes "SELECT * FROM <table> WHERE id <= 500 ALLOW FILTERING" each iteration, counts the rows returned,
 * and records total items processed as state.iterations() * 500. If the ScyllaDB connection cannot be established the benchmark is skipped.
 */
static void BM_ScyllaDB_Select_Large_WhereClause(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_select_large_where";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 500 ALLOW FILTERING");
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 500); // Only 500 rows processed per iteration
}
BENCHMARK(BM_ScyllaDB_Select_Large_WhereClause)->Iterations(1000);

/**
 * @brief Benchmarks execution of a prepared SELECT query against the large test table.
 *
 * Measures the time to execute a prepared statement "SELECT * FROM <table> WHERE id > ? ALLOW FILTERING"
 * with the parameter bound to 500, and iterates the result set to count rows returned.
 * Statement preparation is excluded from the measurement; only query execution and result iteration are timed.
 *
 * @param state Benchmark state provided by the benchmarking framework.
 */
static void BM_ScyllaDB_Select_Large_PreparedStatement(benchmark::State &state)
{
    const std::string tableName = "benchmark_scylladb_select_large_prepared";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up ScyllaDB connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = scylladb_benchmark_helpers::setupScyllaDBConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to ScyllaDB database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop

    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > ? ALLOW FILTERING");
        pstmt->setInt(1, 500);
        state.ResumeTiming(); // Resume timing for query execution

        auto rs = pstmt->executeQuery();
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 500); // 1000 - 500 = 500 rows per iteration
}
BENCHMARK(BM_ScyllaDB_Select_Large_PreparedStatement)->Iterations(1000);

#else
/**
 * @brief Marks the benchmark as skipped when ScyllaDB support is not enabled.
 *
 * Signals the benchmark framework to skip this benchmark and records the
 * error message "ScyllaDB support is not enabled".
 *
 * @param state Benchmark state used to signal the skip and control iterations.
 */
static void BM_ScyllaDB_Select_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("ScyllaDB support is not enabled");
        break;
    }
}
BENCHMARK(BM_ScyllaDB_Select_Disabled);
#endif