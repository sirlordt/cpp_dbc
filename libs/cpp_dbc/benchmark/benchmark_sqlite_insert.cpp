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
 * @file benchmark_sqlite_insert.cpp
 * @brief Benchmarks for SQLite INSERT operations
 */

#include "benchmark_common.hpp"

#if USE_SQLITE

// Small dataset (10 rows)
static void BM_SQLite_Insert_Small_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_sqlite_insert_small_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up SQLite connection and table '" + tableName + "' for benchmark...");
    auto conn = sqlite_benchmark_helpers::setupSQLiteConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to SQLite database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Use a static counter to ensure unique IDs across benchmark runs
    static int runCounter = 0;

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        int runId = ++runCounter;

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            auto result = conn->executeUpdate(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
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
    // std::cout << "Benchmark complete. Cleaning up table '" << tableName << "'..." << std::endl;
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_SQLite_Insert_Small_Individual);

static void BM_SQLite_Insert_Small_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_sqlite_insert_small_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up SQLite connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = sqlite_benchmark_helpers::setupSQLiteConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to SQLite database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Use a static counter to ensure unique IDs across benchmark runs
    static int runCounter = 0;

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");
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
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_SQLite_Insert_Small_Prepared);

// Commented out because all benchmarks are transactional
/*
static void BM_SQLite_Insert_Small_Transaction(benchmark::State &state)
{
    const std::string tableName = "benchmark_sqlite_insert_small_trans";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up SQLite connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::SMALL_SIZE) + " rows of test data...");
    auto conn = sqlite_benchmark_helpers::setupSQLiteConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to SQLite database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Use a static counter to ensure unique IDs across benchmark runs
    static int runCounter = 0;

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        int runId = ++runCounter;

        for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            auto result = conn->executeUpdate(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
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
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::SMALL_SIZE);
}
BENCHMARK(BM_SQLite_Insert_Small_Transaction);
*/

// Medium dataset (100 rows)
static void BM_SQLite_Insert_Medium_Individual(benchmark::State &state)
{
    const std::string tableName = "benchmark_sqlite_insert_medium_ind";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up SQLite connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = sqlite_benchmark_helpers::setupSQLiteConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to SQLite database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Use a static counter to ensure unique IDs across benchmark runs
    static int runCounter = 0;

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        int runId = ++runCounter;

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            auto result = conn->executeUpdate(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
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
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_SQLite_Insert_Medium_Individual);

static void BM_SQLite_Insert_Medium_Prepared(benchmark::State &state)
{
    const std::string tableName = "benchmark_sqlite_insert_medium_prep";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up SQLite connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = sqlite_benchmark_helpers::setupSQLiteConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to SQLite database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Use a static counter to ensure unique IDs across benchmark runs
    static int runCounter = 0;

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");
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
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_SQLite_Insert_Medium_Prepared);

// Commented out because all benchmarks are transactional
/*
static void BM_SQLite_Insert_Medium_Transaction(benchmark::State &state)
{
    const std::string tableName = "benchmark_sqlite_insert_medium_trans";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up SQLite connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::MEDIUM_SIZE) + " rows of test data...");
    auto conn = sqlite_benchmark_helpers::setupSQLiteConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to SQLite database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Use a static counter to ensure unique IDs across benchmark runs
    static int runCounter = 0;

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        int runId = ++runCounter;

        for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            auto result = conn->executeUpdate(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
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
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::MEDIUM_SIZE);
}
BENCHMARK(BM_SQLite_Insert_Medium_Transaction);
*/

// Large dataset (1000 rows)
// Commented out because all benchmarks are transactional
/*
static void BM_SQLite_Insert_Large_Transaction(benchmark::State &state)
{
    const std::string tableName = "benchmark_sqlite_insert_large_trans";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up SQLite connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = sqlite_benchmark_helpers::setupSQLiteConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to SQLite database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Use a static counter to ensure unique IDs across benchmark runs
    static int runCounter = 0;

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        int runId = ++runCounter;

        for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
        {
            // Create a unique ID by combining the run counter and the loop counter
            int uniqueId = runId * 10000 + i;

            auto result = conn->executeUpdate(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
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
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_SQLite_Insert_Large_Transaction);
*/

// Only use prepared statements with transactions for large datasets
// Commented out because all benchmarks are transactional
/*
static void BM_SQLite_Insert_Large_Prepared_Transaction(benchmark::State &state)
{
    const std::string tableName = "benchmark_sqlite_insert_large_prep_trans";

    // Setup phase - outside of measurement
    cpp_dbc::system_utils::logWithTimestampInfo("Setting up SQLite connection and table '" + tableName + "' with " + std::to_string(common_benchmark_helpers::LARGE_SIZE) + " rows of test data...");
    auto conn = sqlite_benchmark_helpers::setupSQLiteConnection(tableName);

    if (!conn)
    {
        state.SkipWithError("Cannot connect to SQLite database");
        return;
    }
    cpp_dbc::system_utils::logWithTimestampInfo("Setup complete. Starting benchmark...");

    // Use a static counter to ensure unique IDs across benchmark runs
    static int runCounter = 0;

    // Begin initial transaction outside of timing loop
    conn->beginTransaction();

    for (auto _ : state)
    {
        int runId = ++runCounter;

        state.PauseTiming(); // Pause while preparing statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");
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
    // cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete. Cleaning up table '" + tableName + "'...");
    // common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
    cpp_dbc::system_utils::logWithTimestampInfo("Benchmark complete.");

    state.SetItemsProcessed(state.iterations() * common_benchmark_helpers::LARGE_SIZE);
}
BENCHMARK(BM_SQLite_Insert_Large_Prepared_Transaction);
*/

#else
// Register empty benchmark when SQLite is disabled
static void BM_SQLite_Insert_Disabled(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.SkipWithError("SQLite support is not enabled");
        break;
    }
}
BENCHMARK(BM_SQLite_Insert_Disabled);
#endif