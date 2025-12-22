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
 * @file benchmark_firebird_select.cpp
 * @brief Benchmarks for Firebird SELECT operations
 */

#include "benchmark_common.hpp"

#if USE_FIREBIRD

// Small dataset (10 rows)
static void BM_Firebird_Select_Small_AllColumns(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_select_small_all";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    // This is not strictly necessary for SELECT operations but ensures consistency
    conn->beginTransaction();

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
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Firebird_Select_Small_AllColumns);

static void BM_Firebird_Select_Small_SingleColumn(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_select_small_single";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

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
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Firebird_Select_Small_SingleColumn);

static void BM_Firebird_Select_Small_WhereClause(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_select_small_where";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 5");
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 5); // Only 5 rows processed per iteration
}
BENCHMARK(BM_Firebird_Select_Small_WhereClause);

static void BM_Firebird_Select_Small_OrderBy(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_select_small_order";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT * FROM " + tableName + " ORDER BY name");
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Firebird_Select_Small_OrderBy);

static void BM_Firebird_Select_Small_PreparedStatement(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_select_small_prepared";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > ?");
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
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 5); // 10 - 5 = 5 rows per iteration
}
BENCHMARK(BM_Firebird_Select_Small_PreparedStatement);

// Medium dataset (100 rows)
static void BM_Firebird_Select_Medium_AllColumns(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_select_medium_all";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

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
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_Firebird_Select_Medium_AllColumns);

static void BM_Firebird_Select_Medium_SingleColumn(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_select_medium_single";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

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
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_Firebird_Select_Medium_SingleColumn);

// Large dataset (1000 rows) - fewer benchmarks for efficiency
static void BM_Firebird_Select_Large_SingleColumn(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_select_large_single";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }

    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

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
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_Firebird_Select_Large_SingleColumn);

static void BM_Firebird_Select_Large_WhereClause(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_select_large_where";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 500");
        benchmark::DoNotOptimize(rs);
        int count = 0;
        while (rs->next())
        {
            count++;
        }
        benchmark::DoNotOptimize(count);
    }

    // Rollback the transaction after the benchmark loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 500); // Only 500 rows processed per iteration
}
BENCHMARK(BM_Firebird_Select_Large_WhereClause);

#else
// Register empty benchmark when Firebird is disabled
static void BM_Firebird_Select_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("Firebird support is not enabled");
        break;
    }
}
BENCHMARK(BM_Firebird_Select_Disabled);
#endif