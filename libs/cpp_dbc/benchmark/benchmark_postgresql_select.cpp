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
 * @file benchmark_postgresql_select.cpp
 * @brief Benchmarks for PostgreSQL SELECT operations
 */

#include "benchmark_common.hpp"

#if USE_POSTGRESQL

// Helper function to setup PostgreSQL connection with data is moved to postgresql_benchmark_helpers
// namespace in benchmark_common.hpp/cpp for consistency

// Small dataset (10 rows)
static void BM_PostgreSQL_Select_Small_AllColumns(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_small_all";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_PostgreSQL_Select_Small_AllColumns);

static void BM_PostgreSQL_Select_Small_SingleColumn(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_small_single";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_PostgreSQL_Select_Small_SingleColumn);

static void BM_PostgreSQL_Select_Small_Where(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_small_where";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 5); // We query 5 rows
}
BENCHMARK(BM_PostgreSQL_Select_Small_Where);

static void BM_PostgreSQL_Select_Small_OrderBy(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_small_order";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_PostgreSQL_Select_Small_OrderBy);

static void BM_PostgreSQL_Select_Small_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_small_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > $1");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 5); // We query ~5 rows
}
BENCHMARK(BM_PostgreSQL_Select_Small_Prepared);

// Medium dataset (100 rows)
static void BM_PostgreSQL_Select_Medium_AllColumns(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_medium_all";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_PostgreSQL_Select_Medium_AllColumns);

static void BM_PostgreSQL_Select_Medium_SingleColumn(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_medium_single";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_PostgreSQL_Select_Medium_SingleColumn);

static void BM_PostgreSQL_Select_Medium_Where(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_medium_where";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 50");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 50); // We query 50 rows
}
BENCHMARK(BM_PostgreSQL_Select_Medium_Where);

static void BM_PostgreSQL_Select_Medium_OrderBy(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_medium_order";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_PostgreSQL_Select_Medium_OrderBy);

static void BM_PostgreSQL_Select_Medium_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_medium_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin a transaction before the benchmark loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > $1");
        pstmt->setInt(1, 50);
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 50); // We query ~50 rows
}
BENCHMARK(BM_PostgreSQL_Select_Medium_Prepared);

// Large dataset (1000 rows) - limit some tests for performance
static void BM_PostgreSQL_Select_Large_AllColumns(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_large_all";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_PostgreSQL_Select_Large_AllColumns);

static void BM_PostgreSQL_Select_Large_SingleColumn(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_large_single";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_PostgreSQL_Select_Large_SingleColumn);

static void BM_PostgreSQL_Select_Large_Where(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_large_where";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 500); // We query 500 rows
}
BENCHMARK(BM_PostgreSQL_Select_Large_Where);

// XLarge dataset (10000 rows) - only a subset of operations
static void BM_PostgreSQL_Select_XLarge_SingleColumn(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_xlarge_single";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::XLARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::XLARGE_SIZE);
}
BENCHMARK(BM_PostgreSQL_Select_XLarge_SingleColumn);

static void BM_PostgreSQL_Select_XLarge_Where(benchmark::State &state)
{
    const std::string tableName = "benchmark_postgresql_select_xlarge_where";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up PostgreSQL connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) + " rows of test data...");
    auto conn = postgresql_benchmark_helpers::setupPostgreSQLConnection(tableName, common_benchmark_helpers::XLARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to PostgreSQL database");
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
    // Cleanup - outside of measurement
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * 500); // We query 500 rows
}
BENCHMARK(BM_PostgreSQL_Select_XLarge_Where);

#else
// Register empty benchmark when PostgreSQL is disabled
static void BM_PostgreSQL_Select_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("PostgreSQL support is not enabled");
        break;
    }
}
BENCHMARK(BM_PostgreSQL_Select_Disabled);
#endif