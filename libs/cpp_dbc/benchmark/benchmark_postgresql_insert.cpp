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
 * @file benchmark_postgresql_insert.cpp
 * @brief Benchmarks for PostgreSQL INSERT operations
 */

#include "benchmark_common.hpp"

#if USE_POSTGRESQL

TEST_CASE("PostgreSQL INSERT Benchmark", "[benchmark][postgresql][insert]")
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
    const std::string tableName = "benchmark_postgresql_insert";

    SECTION("INSERT 10 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("PostgreSQL INSERT 10 rows - Individual inserts")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                // auto result =
                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + common_benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }
            return common_benchmark_helpers::SMALL_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("PostgreSQL INSERT 10 rows - Prepared statement")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            auto pstmt = conn->prepareStatement(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP)");

            for (int i = 1; i <= common_benchmark_helpers::SMALL_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                pstmt->setInt(1, uniqueId);
                pstmt->setString(2, "Name " + std::to_string(i));
                pstmt->setDouble(3, i * 1.5);
                pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::SMALL_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("INSERT 100 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("PostgreSQL INSERT 100 rows - Individual inserts")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                // auto result =
                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + common_benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }
            return common_benchmark_helpers::MEDIUM_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("PostgreSQL INSERT 100 rows - Prepared statement")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            auto pstmt = conn->prepareStatement(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP)");

            for (int i = 1; i <= common_benchmark_helpers::MEDIUM_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                pstmt->setInt(1, uniqueId);
                pstmt->setString(2, "Name " + std::to_string(i));
                pstmt->setDouble(3, i * 1.5);
                pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::MEDIUM_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("INSERT 1000 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("PostgreSQL INSERT 1000 rows - Individual inserts")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                // auto result =
                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + common_benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }
            return common_benchmark_helpers::LARGE_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("PostgreSQL INSERT 1000 rows - Prepared statement")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            auto pstmt = conn->prepareStatement(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP)");

            for (int i = 1; i <= common_benchmark_helpers::LARGE_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                pstmt->setInt(1, uniqueId);
                pstmt->setString(2, "Name " + std::to_string(i));
                pstmt->setDouble(3, i * 1.5);
                pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
                pstmt->executeUpdate();
            }
            return common_benchmark_helpers::LARGE_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    }

    SECTION("INSERT 10000 rows")
    {
        // Create benchmark table
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("PostgreSQL INSERT 10000 rows - Individual inserts")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                // auto result =
                conn->executeUpdate(
                    "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (" +
                    std::to_string(uniqueId) + ", 'Name " + std::to_string(i) + "', " +
                    std::to_string(i * 1.5) + ", '" + common_benchmark_helpers::generateRandomString(50) + "', CURRENT_TIMESTAMP)");
            }
            return common_benchmark_helpers::XLARGE_SIZE;
        };

        // Clean up
        common_benchmark_helpers::dropBenchmarkTable(conn, tableName);

        // Recreate table for next benchmark
        common_benchmark_helpers::createBenchmarkTable(conn, tableName);

        BENCHMARK("PostgreSQL INSERT 10000 rows - Prepared statement")
        {
            // Use a unique ID prefix for each benchmark run to avoid UNIQUE constraint errors
            static int runCounter = 0;
            int runId = ++runCounter;

            auto pstmt = conn->prepareStatement(
                "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES ($1, $2, $3, $4, CURRENT_TIMESTAMP)");

            for (int i = 1; i <= common_benchmark_helpers::XLARGE_SIZE; ++i)
            {
                // Create a unique ID by combining the run counter and the loop counter
                int uniqueId = runId * 10000 + i;

                pstmt->setInt(1, uniqueId);
                pstmt->setString(2, "Name " + std::to_string(i));
                pstmt->setDouble(3, i * 1.5);
                pstmt->setString(4, common_benchmark_helpers::generateRandomString(50));
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
TEST_CASE("PostgreSQL INSERT Benchmark (skipped)", "[benchmark][postgresql][insert]")
{
    SKIP("PostgreSQL support is not enabled");
}
#endif