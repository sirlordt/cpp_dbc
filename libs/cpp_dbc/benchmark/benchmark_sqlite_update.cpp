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
 * @file benchmark_sqlite_update.cpp
 * @brief Benchmarks for SQLite UPDATE operations
 */

#include "benchmark_common.hpp"

#if USE_SQLITE
// Using canConnectToSQLite from benchmark_common.hpp

TEST_CASE("SQLite UPDATE Benchmark", "[benchmark][sqlite][update]")
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
    const std::string tableName = "benchmark_sqlite_update";

    SECTION("UPDATE 10 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 10 rows
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::SMALL_SIZE);

        BENCHMARK("SQLite UPDATE 10 rows - Individual updates")
        {
            for (int i = 1; i <= benchmark_helpers::SMALL_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }
            return benchmark_helpers::SMALL_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::SMALL_SIZE);

        BENCHMARK("SQLite UPDATE 10 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");

            for (int i = 1; i <= benchmark_helpers::SMALL_SIZE; ++i)
            {
                pstmt->setString(1, "Updated Name " + std::to_string(i));
                pstmt->setDouble(2, i * 2.5);
                pstmt->setString(3, benchmark_helpers::generateRandomString(60));
                pstmt->setInt(4, i);
                pstmt->executeUpdate();
            }
            return benchmark_helpers::SMALL_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::SMALL_SIZE);

        BENCHMARK("SQLite UPDATE 10 rows - Transaction")
        {
            conn->executeUpdate("BEGIN TRANSACTION");

            for (int i = 1; i <= benchmark_helpers::SMALL_SIZE; ++i)
            {
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }

            conn->executeUpdate("COMMIT");
            return benchmark_helpers::SMALL_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("UPDATE 100 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 100 rows
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("SQLite UPDATE 100 rows - Individual updates")
        {
            for (int i = 1; i <= benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }
            return benchmark_helpers::MEDIUM_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("SQLite UPDATE 100 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");

            for (int i = 1; i <= benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                pstmt->setString(1, "Updated Name " + std::to_string(i));
                pstmt->setDouble(2, i * 2.5);
                pstmt->setString(3, benchmark_helpers::generateRandomString(60));
                pstmt->setInt(4, i);
                pstmt->executeUpdate();
            }
            return benchmark_helpers::MEDIUM_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("SQLite UPDATE 100 rows - Transaction")
        {
            conn->executeUpdate("BEGIN TRANSACTION");

            for (int i = 1; i <= benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }

            conn->executeUpdate("COMMIT");
            return benchmark_helpers::MEDIUM_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("UPDATE 1000 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 1000 rows
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::LARGE_SIZE);

        BENCHMARK("SQLite UPDATE 1000 rows - Individual updates")
        {
            for (int i = 1; i <= benchmark_helpers::LARGE_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }
            return benchmark_helpers::LARGE_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::LARGE_SIZE);

        BENCHMARK("SQLite UPDATE 1000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");

            for (int i = 1; i <= benchmark_helpers::LARGE_SIZE; ++i)
            {
                pstmt->setString(1, "Updated Name " + std::to_string(i));
                pstmt->setDouble(2, i * 2.5);
                pstmt->setString(3, benchmark_helpers::generateRandomString(60));
                pstmt->setInt(4, i);
                pstmt->executeUpdate();
            }
            return benchmark_helpers::LARGE_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::LARGE_SIZE);

        BENCHMARK("SQLite UPDATE 1000 rows - Transaction")
        {
            conn->executeUpdate("BEGIN TRANSACTION");

            for (int i = 1; i <= benchmark_helpers::LARGE_SIZE; ++i)
            {
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }

            conn->executeUpdate("COMMIT");
            return benchmark_helpers::LARGE_SIZE;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("UPDATE 10000 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 10000 rows
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("SQLite UPDATE 10000 rows - Individual updates")
        {
            for (int i = 1; i <= benchmark_helpers::XLARGE_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }
            return benchmark_helpers::XLARGE_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("SQLite UPDATE 10000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "UPDATE " + tableName + " SET name = ?, value = ?, description = ? WHERE id = ?");

            for (int i = 1; i <= benchmark_helpers::XLARGE_SIZE; ++i)
            {
                pstmt->setString(1, "Updated Name " + std::to_string(i));
                pstmt->setDouble(2, i * 2.5);
                pstmt->setString(3, benchmark_helpers::generateRandomString(60));
                pstmt->setInt(4, i);
                pstmt->executeUpdate();
            }
            return benchmark_helpers::XLARGE_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("SQLite UPDATE 10000 rows - Transaction")
        {
            conn->executeUpdate("BEGIN TRANSACTION");

            for (int i = 1; i <= benchmark_helpers::XLARGE_SIZE; ++i)
            {
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
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
TEST_CASE("SQLite UPDATE Benchmark (skipped)", "[benchmark][sqlite][update]")
{
    SKIP("SQLite support is not enabled");
}
#endif