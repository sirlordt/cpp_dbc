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
 * @file benchmark_mysql_delete.cpp
 * @brief Benchmarks for MySQL DELETE operations
 */

#include "benchmark_common.hpp"

#if USE_MYSQL
// Using canConnectToMySQL from benchmark_common.hpp

TEST_CASE("MySQL DELETE Benchmark", "[benchmark][mysql][delete]")
{
    // Skip these tests if we can't connect to MySQL
    if (!benchmark_helpers::canConnectToMySQL())
    {
        SKIP("Cannot connect to MySQL database");
        return;
    }

    // Load the YAML configuration
    std::string config_path = getConfigFilePath();
    YAML::Node config = YAML::LoadFile(config_path);

    // Find the dev_mysql configuration
    YAML::Node dbConfig;
    for (size_t i = 0; i < config["databases"].size(); i++)
    {
        YAML::Node db = config["databases"][i];
        if (db["name"].as<std::string>() == "dev_mysql")
        {
            dbConfig = YAML::Node(db);
            break;
        }
    }

    // Create connection parameters
    std::string type = dbConfig["type"].as<std::string>();
    std::string host = dbConfig["host"].as<std::string>();
    int port = dbConfig["port"].as<int>();
    std::string database = dbConfig["database"].as<std::string>();
    std::string username = dbConfig["username"].as<std::string>();
    std::string password = dbConfig["password"].as<std::string>();

    std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

    // Register the MySQL driver
    cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDriver>());

    // Get a connection
    auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

    // Table name for benchmarks
    const std::string tableName = "benchmark_mysql_delete";

    SECTION("DELETE 10 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 10 rows
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::SMALL_SIZE);

        BENCHMARK("MySQL DELETE 10 rows - Individual deletes")
        {
            for (int i = 1; i <= benchmark_helpers::SMALL_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            }
            return benchmark_helpers::SMALL_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::SMALL_SIZE);

        BENCHMARK("MySQL DELETE 10 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "DELETE FROM " + tableName + " WHERE id = ?");

            for (int i = 1; i <= benchmark_helpers::SMALL_SIZE; ++i)
            {
                pstmt->setInt(1, i);
                pstmt->executeUpdate();
            }
            return benchmark_helpers::SMALL_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::SMALL_SIZE);

        BENCHMARK("MySQL DELETE 10 rows - Batch delete")
        {
            auto result = conn->executeUpdate(
                "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
                std::to_string(benchmark_helpers::SMALL_SIZE));
            return result;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("DELETE 100 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 100 rows
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("MySQL DELETE 100 rows - Individual deletes")
        {
            for (int i = 1; i <= benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            }
            return benchmark_helpers::MEDIUM_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("MySQL DELETE 100 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "DELETE FROM " + tableName + " WHERE id = ?");

            for (int i = 1; i <= benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                pstmt->setInt(1, i);
                pstmt->executeUpdate();
            }
            return benchmark_helpers::MEDIUM_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("MySQL DELETE 100 rows - Batch delete")
        {
            auto result = conn->executeUpdate(
                "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
                std::to_string(benchmark_helpers::MEDIUM_SIZE));
            return result;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("DELETE 1000 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 1000 rows
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::LARGE_SIZE);

        BENCHMARK("MySQL DELETE 1000 rows - Individual deletes")
        {
            for (int i = 1; i <= benchmark_helpers::LARGE_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            }
            return benchmark_helpers::LARGE_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::LARGE_SIZE);

        BENCHMARK("MySQL DELETE 1000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "DELETE FROM " + tableName + " WHERE id = ?");

            for (int i = 1; i <= benchmark_helpers::LARGE_SIZE; ++i)
            {
                pstmt->setInt(1, i);
                pstmt->executeUpdate();
            }
            return benchmark_helpers::LARGE_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::LARGE_SIZE);

        BENCHMARK("MySQL DELETE 1000 rows - Batch delete")
        {
            auto result = conn->executeUpdate(
                "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
                std::to_string(benchmark_helpers::LARGE_SIZE));
            return result;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("DELETE 10000 rows")
    {
        // Create benchmark table
        benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 10000 rows
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("MySQL DELETE 10000 rows - Individual deletes")
        {
            for (int i = 1; i <= benchmark_helpers::XLARGE_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            }
            return benchmark_helpers::XLARGE_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("MySQL DELETE 10000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "DELETE FROM " + tableName + " WHERE id = ?");

            for (int i = 1; i <= benchmark_helpers::XLARGE_SIZE; ++i)
            {
                pstmt->setInt(1, i);
                pstmt->executeUpdate();
            }
            return benchmark_helpers::XLARGE_SIZE;
        };

        // Repopulate table for next benchmark
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
        benchmark_helpers::createBenchmarkTable(conn, tableName);
        benchmark_helpers::populateTable(conn, tableName, benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("MySQL DELETE 10000 rows - Batch delete")
        {
            auto result = conn->executeUpdate(
                "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
                std::to_string(benchmark_helpers::XLARGE_SIZE));
            return result;
        };

        // Clean up
        benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    // Close the connection
    conn->close();
}
#else
// Skip tests if MySQL support is not enabled
TEST_CASE("MySQL DELETE Benchmark (skipped)", "[benchmark][mysql][delete]")
{
    SKIP("MySQL support is not enabled");
}
#endif