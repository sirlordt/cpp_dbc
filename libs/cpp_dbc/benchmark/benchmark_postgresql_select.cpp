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
 * @file benchmark_postgresql_select.cpp
 * @brief Benchmarks for PostgreSQL SELECT operations
 */

#include "benchmark_common.hpp"

#if USE_POSTGRESQL

TEST_CASE("PostgreSQL SELECT Benchmark", "[benchmark][postgresql][select]")
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
    const std::string tableName = "benchmark_postgresql_select";

    // Create benchmark table
    common_benchmark_helpers::createBenchmarkTable(conn, tableName);

    SECTION("SELECT 10 rows")
    {
        // Populate table with 10 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::SMALL_SIZE);

        BENCHMARK("PostgreSQL SELECT 10 rows - All columns")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 10 rows - Single column")
        {
            auto rs = conn->executeQuery("SELECT id FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 10 rows - With WHERE clause")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 5");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 10 rows - With ORDER BY")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " ORDER BY name");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 10 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > $1");
            pstmt->setInt(1, 5);
            auto rs = pstmt->executeQuery();
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };
    }

    SECTION("SELECT 100 rows")
    {
        // Populate table with 100 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::MEDIUM_SIZE);

        BENCHMARK("PostgreSQL SELECT 100 rows - All columns")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 100 rows - Single column")
        {
            auto rs = conn->executeQuery("SELECT id FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 100 rows - With WHERE clause")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 50");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 100 rows - With ORDER BY")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " ORDER BY name");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 100 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > $1");
            pstmt->setInt(1, 50);
            auto rs = pstmt->executeQuery();
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };
    }

    SECTION("SELECT 1000 rows")
    {
        // Populate table with 1000 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::LARGE_SIZE);

        BENCHMARK("PostgreSQL SELECT 1000 rows - All columns")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 1000 rows - Single column")
        {
            auto rs = conn->executeQuery("SELECT id FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 1000 rows - With WHERE clause")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 500");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 1000 rows - With ORDER BY")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " ORDER BY name");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 1000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > $1");
            pstmt->setInt(1, 500);
            auto rs = pstmt->executeQuery();
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };
    }

    SECTION("SELECT 10000 rows")
    {
        // Populate table with 10000 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::XLARGE_SIZE);

        BENCHMARK("PostgreSQL SELECT 10000 rows - All columns")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 10000 rows - Single column")
        {
            auto rs = conn->executeQuery("SELECT id FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 10000 rows - With WHERE clause")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 5000");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 10000 rows - With ORDER BY")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " ORDER BY name");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("PostgreSQL SELECT 10000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > $1");
            pstmt->setInt(1, 5000);
            auto rs = pstmt->executeQuery();
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };
    }

    // Clean up
    common_benchmark_helpers::dropBenchmarkTable(conn, tableName);
    conn->close();
}
#else
// Skip tests if PostgreSQL support is not enabled
TEST_CASE("PostgreSQL SELECT Benchmark (skipped)", "[benchmark][postgresql][select]")
{
    SKIP("PostgreSQL support is not enabled");
}
#endif