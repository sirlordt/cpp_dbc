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
 * @file benchmark_firebird_update.cpp
 * @brief Benchmarks for Firebird UPDATE operations
 */

#include "benchmark_common.hpp"

#if USE_FIREBIRD

// Small dataset (10 rows)
static void BM_Firebird_Update_Small_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_update_small_ind";

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
                "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                "', num_value = " + std::to_string(i * 2.5) +
                ", description = '" + common_benchmark_helpers::generateRandomString(60) +
                "' WHERE id = " + std::to_string(i));
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
BENCHMARK(BM_Firebird_Update_Small_Individual);

static void BM_Firebird_Update_Small_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_update_small_prep";

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
            "UPDATE " + tableName + " SET name = ?, num_value = ?, description = ? WHERE id = ?");
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
BENCHMARK(BM_Firebird_Update_Small_Prepared);

// Medium dataset (100 rows)
static void BM_Firebird_Update_Medium_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_update_medium_ind";

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
                "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                "', num_value = " + std::to_string(i * 2.5) +
                ", description = '" + common_benchmark_helpers::generateRandomString(60) +
                "' WHERE id = " + std::to_string(i));
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
BENCHMARK(BM_Firebird_Update_Medium_Individual);

static void BM_Firebird_Update_Medium_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_update_medium_prep";

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
            "UPDATE " + tableName + " SET name = ?, num_value = ?, description = ? WHERE id = ?");
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
BENCHMARK(BM_Firebird_Update_Medium_Prepared);

// Large dataset (1000 rows)
static void BM_Firebird_Update_Large_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_update_large_ind";

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
        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
        {
            auto result = conn->executeUpdate(
                "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                "', num_value = " + std::to_string(i * 2.5) +
                ", description = '" + common_benchmark_helpers::generateRandomString(60) +
                "' WHERE id = " + std::to_string(i));
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
BENCHMARK(BM_Firebird_Update_Large_Individual);

static void BM_Firebird_Update_Large_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_update_large_prep";

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
            "UPDATE " + tableName + " SET name = ?, num_value = ?, description = ? WHERE id = ?");
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
BENCHMARK(BM_Firebird_Update_Large_Prepared);

// Extra Large dataset (10000 rows) - only prepared statement for better performance
static void BM_Firebird_Update_XLarge_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_update_xlarge_prep";

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
        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "UPDATE " + tableName + " SET name = ?, num_value = ?, description = ? WHERE id = ?");
        state.ResumeTiming(); // Resume timing for actual operations

        for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
        {
            pstmt->setString(1, "Updated Name " + std::to_string(i));
            pstmt->setDouble(2, i * 2.5);
            pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
            pstmt->setInt(4, i);
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

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::XLARGE_SIZE);
}
BENCHMARK(BM_Firebird_Update_XLarge_Prepared);

#else
// Register empty benchmark when Firebird is disabled
static void BM_Firebird_Update_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("Firebird support is not enabled");
        break;
    }
}
BENCHMARK(BM_Firebird_Update_Disabled);
#endif