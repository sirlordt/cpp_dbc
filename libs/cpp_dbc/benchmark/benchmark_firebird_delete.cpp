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
 * @file benchmark_firebird_delete.cpp
 * @brief Benchmarks for Firebird DELETE operations
 */

#include "benchmark_common.hpp"

#if USE_FIREBIRD

// Small dataset (10 rows)
static void BM_Firebird_Delete_Small_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_delete_small_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            auto result = conn->executeUpdate(
                "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            benchmark::DoNotOptimize(result);
        }

        state.PauseTiming(); // Pause for rollback and repopulation
        conn->rollback();
        conn->beginTransaction(); // Begin a new transaction for the next iteration
        state.ResumeTiming();     // Resume timing for next iteration
    }

    // Rollback the final transaction outside the loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Firebird_Delete_Small_Individual);

static void BM_Firebird_Delete_Small_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_delete_small_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

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

        state.PauseTiming(); // Pause for rollback and repopulation
        conn->rollback();
        conn->beginTransaction(); // Begin a new transaction for the next iteration
        state.ResumeTiming();     // Resume timing for next iteration
    }

    // Rollback the final transaction outside the loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Firebird_Delete_Small_Prepared);

static void BM_Firebird_Delete_Small_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_delete_small_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::SMALL_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        auto result = conn->executeUpdate(
            "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
            std::to_string(common_benchmark_helpers::SMALL_SIZE));
        benchmark::DoNotOptimize(result);

        state.PauseTiming(); // Pause for rollback
        conn->rollback();
        conn->beginTransaction(); // Begin a new transaction for the next iteration
        state.ResumeTiming();     // Resume timing for next iteration
    }

    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Firebird_Delete_Small_Batch);

// Medium dataset (100 rows)
static void BM_Firebird_Delete_Medium_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_delete_medium_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            auto result = conn->executeUpdate(
                "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            benchmark::DoNotOptimize(result);
        }

        state.PauseTiming(); // Pause for rollback and repopulation
        conn->rollback();
        conn->beginTransaction(); // Begin a new transaction for the next iteration
        state.ResumeTiming();     // Resume timing for next iteration
    }

    // Rollback the final transaction outside the loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_Firebird_Delete_Medium_Individual);

static void BM_Firebird_Delete_Medium_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_delete_medium_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

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

        state.PauseTiming(); // Pause for rollback and repopulation
        conn->rollback();
        conn->beginTransaction(); // Begin a new transaction for the next iteration
        state.ResumeTiming();     // Resume timing for next iteration
    }

    // Rollback the final transaction outside the loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_Firebird_Delete_Medium_Prepared);

static void BM_Firebird_Delete_Medium_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_delete_medium_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::MEDIUM_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        auto result = conn->executeUpdate(
            "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
            std::to_string(common_benchmark_helpers::MEDIUM_SIZE));
        benchmark::DoNotOptimize(result);

        state.PauseTiming(); // Pause for rollback
        conn->rollback();
        conn->beginTransaction(); // Begin a new transaction for the next iteration
        state.ResumeTiming();     // Resume timing for next iteration
    }

    // Rollback the final transaction outside the loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_Firebird_Delete_Medium_Batch);

// Large dataset (1000 rows) - limit to batch operations for performance
static void BM_Firebird_Delete_Large_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_delete_large_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        auto result = conn->executeUpdate(
            "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
            std::to_string(common_benchmark_helpers::LARGE_SIZE));
        benchmark::DoNotOptimize(result);

        state.PauseTiming(); // Pause for rollback
        conn->rollback();
        conn->beginTransaction(); // Begin a new transaction for the next iteration
        state.ResumeTiming();     // Resume timing for next iteration
    }

    // Rollback the final transaction outside the loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_Firebird_Delete_Large_Batch);

static void BM_Firebird_Delete_Large_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_delete_large_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::LARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

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

        state.PauseTiming(); // Pause for rollback and repopulation
        conn->rollback();
        conn->beginTransaction(); // Begin a new transaction for the next iteration
        state.ResumeTiming();     // Resume timing for next iteration
    }

    // Rollback the final transaction outside the loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_Firebird_Delete_Large_Prepared);

// XLarge dataset (10000 rows) - only batch delete for better performance
static void BM_Firebird_Delete_XLarge_Batch(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_delete_xlarge_batch";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName, common_benchmark_helpers::XLARGE_SIZE);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        auto result = conn->executeUpdate(
            "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
            std::to_string(common_benchmark_helpers::XLARGE_SIZE));
        benchmark::DoNotOptimize(result);

        state.PauseTiming(); // Pause for rollback
        conn->rollback();
        conn->beginTransaction(); // Begin a new transaction for the next iteration
        state.ResumeTiming();     // Resume timing for next iteration
    }

    // Rollback the final transaction outside the loop
    conn->rollback();

    // Cleanup - outside of measurement
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::XLARGE_SIZE);
}
BENCHMARK(BM_Firebird_Delete_XLarge_Batch);

#else
// Register empty benchmark when Firebird is disabled
static void BM_Firebird_Delete_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("Firebird support is not enabled");
        break;
    }
}
BENCHMARK(BM_Firebird_Delete_Disabled);
#endif