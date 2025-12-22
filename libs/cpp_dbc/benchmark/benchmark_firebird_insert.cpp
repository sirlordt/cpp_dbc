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
 * @file benchmark_firebird_insert.cpp
 * @brief Benchmarks for Firebird INSERT operations
 */

#include "benchmark_common.hpp"

#if USE_FIREBIRD

// Small dataset (10 rows)
static void BM_Firebird_Insert_Small_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_insert_small_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

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
                "INSERT INTO " + tableName + " (id, name, num_value, description, created_at) VALUES (" +
                std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                std::to_string(i * 1.5) + ", '" + common_benchmark_helpers::generateRandomString(50) +
                "', CURRENT_TIMESTAMP)");
            benchmark::DoNotOptimize(result);
        }
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

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Firebird_Insert_Small_Individual);

static void BM_Firebird_Insert_Small_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_insert_small_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, num_value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");
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

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_Firebird_Insert_Small_Prepared);

// Medium dataset (100 rows)
static void BM_Firebird_Insert_Medium_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_insert_medium_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            auto result = conn->executeUpdate(
                "INSERT INTO " + tableName + " (id, name, num_value, description, created_at) VALUES (" +
                std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                std::to_string(i * 1.5) + ", '" + common_benchmark_helpers::generateRandomString(50) +
                "', CURRENT_TIMESTAMP)");
            benchmark::DoNotOptimize(result);
        }

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
BENCHMARK(BM_Firebird_Insert_Medium_Individual);

static void BM_Firebird_Insert_Medium_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_insert_medium_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, num_value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");
        state.ResumeTiming(); // Resume timing for actual operations

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
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
BENCHMARK(BM_Firebird_Insert_Medium_Prepared);

// Large dataset (1000 rows) - only prepared statement for better performance
static void BM_Firebird_Insert_Large_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_insert_large_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, num_value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");
        state.ResumeTiming(); // Resume timing for actual operations

        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
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
BENCHMARK(BM_Firebird_Insert_Large_Prepared);

// XLarge dataset (10000 rows) - only prepared statement for better performance
static void BM_Firebird_Insert_XLarge_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_firebird_insert_xlarge_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up Firebird connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::XLARGE_SIZE) + " rows of test data...");
    auto conn = firebird_benchmark_helpers::setupFirebirdConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to Firebird database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
    static int runCounter = 0;

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, num_value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");
        state.ResumeTiming(); // Resume timing for actual operations

        for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
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
BENCHMARK(BM_Firebird_Insert_XLarge_Prepared);

#else
// Register empty benchmark when Firebird is disabled
static void BM_Firebird_Insert_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("Firebird support is not enabled");
        break;
    }
}
BENCHMARK(BM_Firebird_Insert_Disabled);
#endif