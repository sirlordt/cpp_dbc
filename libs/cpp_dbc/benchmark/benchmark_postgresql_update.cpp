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
 * @file benchmark_postgresql_update.cpp
 * @brief Benchmarks for PostgreSQL UPDATE operations
 */

#include "benchmark_common.hpp"

#if USE_POSTGRESQL

TEST_CASE("PostgreSQL UPDATE Benchmark", "[benchmark][postgresql][update]")
{
    // Skip these tests if we can't connect to PostgreSQL
    if (!postgresql_benchmark_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

    // Get database configuration using the centralized helper
    auto dbConfig = postgresql_benchmark_helpers::getPostgreSQLConfig("dev_postgresql");

    // Get connection parameters
    std::string connStr = dbConfig.createConnectionString();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    // Register the PostgreSQL driver and get a connection
    cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());
    auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

    // Table name for benchmarks
    const std::string tableName = "benchmark_postgresql_update";

    SECTION("UPDATE 10 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 10 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::SMALL_SIZE);

        BENCHMARK("PostgreSQL UPDATE 10 rows - Individual updates")
        {
            for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + common_benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }
            return common_benchmark_helpers::SMALL_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::SMALL_SIZE);

        BENCHMARK("PostgreSQL UPDATE 10 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "UPDATE " + tableName + " SET name = $1, value = $2, description = $3 WHERE id = $4");

            for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
            {
                pstmt->setString(1, "Updated Name " + std::to_string(i));
                pstmt->setDouble(2, i * 2.5);
                pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
                pstmt->setInt(4, i);
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::SMALL_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("UPDATE 100 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 100 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("PostgreSQL UPDATE 100 rows - Individual updates")
        {
            for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + common_benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }
            return common_benchmark_helpers::MEDIUM_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("PostgreSQL UPDATE 100 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "UPDATE " + tableName + " SET name = $1, value = $2, description = $3 WHERE id = $4");

            for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                pstmt->setString(1, "Updated Name " + std::to_string(i));
                pstmt->setDouble(2, i * 2.5);
                pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
                pstmt->setInt(4, i);
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::MEDIUM_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("UPDATE 1000 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 1000 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::LARGE_SIZE);

        BENCHMARK("PostgreSQL UPDATE 1000 rows - Individual updates")
        {
            for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
            {
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + common_benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }
            return common_benchmark_helpers::LARGE_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::LARGE_SIZE);

        BENCHMARK("PostgreSQL UPDATE 1000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "UPDATE " + tableName + " SET name = $1, value = $2, description = $3 WHERE id = $4");

            for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
            {
                pstmt->setString(1, "Updated Name " + std::to_string(i));
                pstmt->setDouble(2, i * 2.5);
                pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
                pstmt->setInt(4, i);
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::LARGE_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("UPDATE 10000 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        // Populate table with 10000 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("PostgreSQL UPDATE 10000 rows - Individual updates")
        {
            for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
            {
                // auto result =
                conn->executeUpdate(
                    "UPDATE " + tableName + " SET name = 'Updated Name " + std::to_string(i) +
                    "', value = " + std::to_string(i * 2.5) +
                    ", description = '" + common_benchmark_helpers::generateRandomString(60) +
                    "' WHERE id = " + std::to_string(i));
            }
            return common_benchmark_helpers::XLARGE_SIZE;
        };

        // Repopulate table for next benchmark
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("PostgreSQL UPDATE 10000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement(
                "UPDATE " + tableName + " SET name = $1, value = $2, description = $3 WHERE id = $4");

            for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
            {
                pstmt->setString(1, "Updated Name " + std::to_string(i));
                pstmt->setDouble(2, i * 2.5);
                pstmt->setString(3, common_benchmark_helpers::generateRandomString(60));
                pstmt->setInt(4, i);
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::XLARGE_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    // Close the connection
    conn->close();
}
#else
// Skip tests if PostgreSQL support is not enabled
TEST_CASE("PostgreSQL UPDATE Benchmark (skipped)", "[benchmark][postgresql][update]")
{
    SKIP("PostgreSQL support is not enabled");
}
#endif