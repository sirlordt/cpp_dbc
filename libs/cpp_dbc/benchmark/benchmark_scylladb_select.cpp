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

// Small dataset (10 rows)
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

// Medium dataset (100 rows)
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

// Large dataset (1000 rows) - fewer benchmarks for efficiency
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
// Register empty benchmark when ScyllaDB is disabled
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
