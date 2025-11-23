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
// Using canConnectToSQLite from benchmark_common.hpp

TEST_CASE("SQLite INSERT Benchmark", "[benchmark][sqlite][insert]")
{
    // Skip these tests if we can't connect to SQLite
    if (!benchmark_helpers::canConnectToSQLite())
    {
        SKIP("Cannot connect to SQLite database");
        return;
    }

    // Load the YAML configuration
    std::string config_path = getConfigFilePath();
    YAML::Node config = YAML::LoadFile(config_path);

    // Find the dev_sqlite configuration
    YAML::Node dbConfig;
    for (size_t i = 0; i < config["databases"].size(); i++)
    {
        YAML::Node db = config["databases"][i];
        if (db["name"].as<std::string>() == "dev_sqlite")
        {
            dbConfig = YAML::Node(db);
            break;
        }
    }

    // Create connection parameters
    std::string type = dbConfig["type"].as<std::string>();
    std::string database = dbConfig["database"].as<std::string>();

    std::string connStr = "cpp_dbc:" + type + "://" + database;

    // Register the SQLite driver
    cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDriver>());

    // Get a connection
    auto conn = cpp_dbc::DriverManager::getConnection(connStr, "", "");

    // Table name for benchmarks
    const std::string tableName = "benchmark_sqlite_insert";

    SECTION("INSERT 10 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 10 rows - Individual inserts")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            for (int i = 1; i <= benchmark_helpers::SMALL_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                // auto result =
                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }
            return benchmark_helpers::SMALL_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 10 rows - Prepared statement")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            auto pstmt = conn->prepareStatement(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");

            for (int i = 1; i <= benchmark_helpers::SMALL_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                pstmt->setInt(1, uniqueId);
                pstmt->setString(2, "Name " + std::to_string(i));
                pstmt->setDouble(3, i * 1.5);
                pstmt->setString(4, benchmark_helpers::generateRandomString(50));
                pstmt->executeUpdate();
            }
            return benchmark_helpers::SMALL_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 10 rows - Transaction")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            conn->executeUpdate("BEGIN TRANSACTION");

            for (int i = 1; i <= benchmark_helpers::SMALL_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }

            conn->executeUpdate("COMMIT");
            return benchmark_helpers::SMALL_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("INSERT 100 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 100 rows - Individual inserts")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            for (int i = 1; i <= benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                // auto result =
                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }
            return benchmark_helpers::MEDIUM_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 100 rows - Prepared statement")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            auto pstmt = conn->prepareStatement(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");

            for (int i = 1; i <= benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                pstmt->setInt(1, uniqueId);
                pstmt->setString(2, "Name " + std::to_string(i));
                pstmt->setDouble(3, i * 1.5);
                pstmt->setString(4, benchmark_helpers::generateRandomString(50));
                pstmt->executeUpdate();
            }
            return benchmark_helpers::MEDIUM_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 100 rows - Transaction")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            conn->executeUpdate("BEGIN TRANSACTION");

            for (int i = 1; i <= benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }

            conn->executeUpdate("COMMIT");
            return benchmark_helpers::MEDIUM_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("INSERT 1000 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 1000 rows - Individual inserts")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            for (int i = 1; i <= benchmark_helpers::LARGE_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                // auto result =
                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }
            return benchmark_helpers::LARGE_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 1000 rows - Prepared statement")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            auto pstmt = conn->prepareStatement(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");

            for (int i = 1; i <= benchmark_helpers::LARGE_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                pstmt->setInt(1, uniqueId);
                pstmt->setString(2, "Name " + std::to_string(i));
                pstmt->setDouble(3, i * 1.5);
                pstmt->setString(4, benchmark_helpers::generateRandomString(50));
                pstmt->executeUpdate();
            }
            return benchmark_helpers::LARGE_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 1000 rows - Transaction")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            conn->executeUpdate("BEGIN TRANSACTION");

            for (int i = 1; i <= benchmark_helpers::LARGE_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }

            conn->executeUpdate("COMMIT");
            return benchmark_helpers::LARGE_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("INSERT 10000 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 10000 rows - Individual inserts")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            for (int i = 1; i <= benchmark_helpers::XLARGE_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                // auto result =
                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }
            return benchmark_helpers::XLARGE_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 10000 rows - Prepared statement")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            auto pstmt = conn->prepareStatement(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");

            for (int i = 1; i <= benchmark_helpers::XLARGE_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                pstmt->setInt(1, uniqueId);
                pstmt->setString(2, "Name " + std::to_string(i));
                pstmt->setDouble(3, i * 1.5);
                pstmt->setString(4, benchmark_helpers::generateRandomString(50));
                pstmt->executeUpdate();
            }
            return benchmark_helpers::XLARGE_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("SQLite INSERT 10000 rows - Transaction")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            conn->executeUpdate("BEGIN TRANSACTION");

            for (int i = 1; i <= benchmark_helpers::XLARGE_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }

            conn->executeUpdate("COMMIT");
            return benchmark_helpers::XLARGE_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    // Close the connection
    conn->close();
}
#else
// Skip tests if SQLite support is not enabled
TEST_CASE("SQLite INSERT Benchmark (skipped)", "[benchmark][sqlite][insert]")
{
    SKIP("SQLite support is not enabled");
}
#endif