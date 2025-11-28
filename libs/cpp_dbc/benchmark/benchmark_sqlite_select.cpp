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
 * @file benchmark_sqlite_select.cpp
 * @brief Benchmarks for SQLite SELECT operations
 */

#include "benchmark_common.hpp"

#if USE_SQLITE

TEST_CASE("SQLite SELECT Benchmark", "[benchmark][sqlite][select]")
{
    // Skip these tests if we can't connect to SQLite
    if (!sqlite_benchmark_helpers::canConnectToSQLite())
    {
        SKIP("Cannot connect to SQLite database");
        return;
    }

    // Get connection string using the centralized helper
    std::string connStr = sqlite_benchmark_helpers::getSQLiteConnectionString();

    // Register the SQLite driver and get a connection
    cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDriver>());
    auto conn = cpp_dbc::DriverManager::getConnection(connStr, "", "");

    // Table name for benchmarks
    const std::string tableName = "benchmark_sqlite_select";

    // Create benchmark table
    common_benchmark_helpers::createBenchmarkTable(conn, tableName);

    SECTION("SELECT 10 rows")
    {
        // Populate table with 10 rows
        common_benchmark_helpers::populateTable(conn, tableName, common_benchmark_helpers::SMALL_SIZE);

        BENCHMARK("SQLite SELECT 10 rows - All columns")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 10 rows - Single column")
        {
            auto rs = conn->executeQuery("SELECT id FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 10 rows - With WHERE clause")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 5");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 10 rows - With ORDER BY")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " ORDER BY name");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 10 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > ?");
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

        BENCHMARK("SQLite SELECT 100 rows - All columns")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 100 rows - Single column")
        {
            auto rs = conn->executeQuery("SELECT id FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 100 rows - With WHERE clause")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 50");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 100 rows - With ORDER BY")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " ORDER BY name");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 100 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > ?");
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

        BENCHMARK("SQLite SELECT 1000 rows - All columns")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 1000 rows - Single column")
        {
            auto rs = conn->executeQuery("SELECT id FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 1000 rows - With WHERE clause")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 500");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 1000 rows - With ORDER BY")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " ORDER BY name");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 1000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > ?");
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

        BENCHMARK("SQLite SELECT 10000 rows - All columns")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 10000 rows - Single column")
        {
            auto rs = conn->executeQuery("SELECT id FROM " + tableName);
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 10000 rows - With WHERE clause")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " WHERE id <= 5000");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 10000 rows - With ORDER BY")
        {
            auto rs = conn->executeQuery("SELECT * FROM " + tableName + " ORDER BY name");
            int count = 0;
            while (rs->next())
            {
                count++;
            }
            return count;
        };

        BENCHMARK("SQLite SELECT 10000 rows - Prepared statement")
        {
            auto pstmt = conn->prepareStatement("SELECT * FROM " + tableName + " WHERE id > ?");
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
// Skip tests if SQLite support is not enabled
TEST_CASE("SQLite SELECT Benchmark (skipped)", "[benchmark][sqlite][select]")
{
    SKIP("SQLite support is not enabled");
}
#endif