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
 * @file benchmark_postgresql_delete.cpp
 * @brief Benchmarks for PostgreSQL DELETE operations
 */

#include "benchmark_common.hpp"

#if USE_POSTGRESQL
// Using canConnectToPostgreSQL from benchmark_common.hpp

TEST_CASE("PostgreSQL DELETE Benchmark", "[benchmark][postgresql][delete]")
{
    // Skip these tests if we can't connect to PostgreSQL
    if (!postgresql_benchmark_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

    // Get database configuration using the centralized helper function
    auto dbConfig = postgresql_benchmark_helpers::getPostgreSQLConfig("dev_postgresql");

    // Get connection parameters
    std::string connStr = dbConfig.createConnectionString();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    // Register the PostgreSQL driver
    cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

    // Get a connection
    auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

    // Table name for benchmarks
    const std::string tableName = "benchmark_postgresql_delete";

    SECTION("DELETE 10 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 10 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::SMALL_SIZE);

        BENCHMARK("PostgreSQL DELETE 10 rows - Individual deletes")
        {
            for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            }
            return common_benchmark_helpers::SMALL_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::SMALL_SIZE);

        BENCHMARK("PostgreSQL DELETE 10 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "DELETE FROM " + tableName + " WHERE id = $1");

            for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
            {
                pstmt->setInt(1, i);
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::SMALL_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::SMALL_SIZE);

        BENCHMARK("PostgreSQL DELETE 10 rows - Batch delete")
        {
            auto result = conn->executeUpdate(
                "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
                std::to_string(common_benchmark_helpers::SMALL_SIZE));
            return result;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("DELETE 100 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 100 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("PostgreSQL DELETE 100 rows - Individual deletes")
        {
            for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            }
            return common_benchmark_helpers::MEDIUM_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("PostgreSQL DELETE 100 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "DELETE FROM " + tableName + " WHERE id = $1");

            for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                pstmt->setInt(1, i);
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::MEDIUM_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("PostgreSQL DELETE 100 rows - Batch delete")
        {
            auto result = conn->executeUpdate(
                "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
                std::to_string(common_benchmark_helpers::MEDIUM_SIZE));
            return result;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("DELETE 1000 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 1000 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::LARGE_SIZE);

        BENCHMARK("PostgreSQL DELETE 1000 rows - Individual deletes")
        {
            for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            }
            return common_benchmark_helpers::LARGE_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::LARGE_SIZE);

        BENCHMARK("PostgreSQL DELETE 1000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "DELETE FROM " + tableName + " WHERE id = $1");

            for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
            {
                pstmt->setInt(1, i);
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::LARGE_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::LARGE_SIZE);

        BENCHMARK("PostgreSQL DELETE 1000 rows - Batch delete")
        {
            auto result = conn->executeUpdate(
                "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
                std::to_string(common_benchmark_helpers::LARGE_SIZE));
            return result;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("DELETE 10000 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 10000 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("PostgreSQL DELETE 10000 rows - Individual deletes")
        {
            for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "DELETE FROM " + tableName + " WHERE id = " + std::to_string(i));
            }
            return common_benchmark_helpers::XLARGE_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("PostgreSQL DELETE 10000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "DELETE FROM " + tableName + " WHERE id = $1");

            for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
            {
                pstmt->setInt(1, i);
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::XLARGE_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("PostgreSQL DELETE 10000 rows - Batch delete")
        {
            auto result = conn->executeUpdate(
                "DELETE FROM " + tableName + " WHERE id BETWEEN 1 AND " +
                std::to_string(common_benchmark_helpers::XLARGE_SIZE));
            return result;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    // Close the connection
    conn->close();
}
#else
// Skip tests if PostgreSQL support is not enabled
TEST_CASE("PostgreSQL DELETE Benchmark (skipped)", "[benchmark][postgresql][delete]")
{
    SKIP("PostgreSQL support is not enabled");
}
#endif