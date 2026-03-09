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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file test_sqlite_real.cpp
 @brief Tests for SQLite database operations

*/

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <chrono>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/pool/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "22_001_test_sqlite_real_common.hpp"

// Helper function to get the path to the test_db_connections.yml file
// Using common_test_helpers namespace for helper functions

// Test case for SQLite real database operations
#if USE_SQLITE
TEST_CASE("SQLite real database operations", "[22_031_01_sqlite_real]")
{
    // Get SQLite configuration to check if cleanup is needed
    auto dbConfig = sqlite_test_helpers::getSQLiteConfig("test_sqlite");

    // CRITICAL: Cleanup SQLite database files before test to prevent "readonly database" errors
    // under Helgrind/Valgrind when running multiple iterations (WAL mode issue)
    // Only cleanup if using file-based database (not :memory:)
    if (dbConfig.getDatabase() != ":memory:")
    {
        sqlite_test_helpers::cleanupSQLiteTestFiles(dbConfig.getDatabase());
    }

    // Get connection string from the database config (reuse dbConfig from outer scope)
    std::string connStr = dbConfig.createConnectionString();

    // Test connection

    // Register the SQLite driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

    // Skip this test if SQLite support is not enabled
    SECTION("Test SQLite real database operations")
    {

        try
        {
            // Attempt to connect to SQLite
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to SQLite with connection string: " + connStr);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));

            // Clean up any existing test table
            conn->executeUpdate("DROP TABLE IF EXISTS test_table");

            // Create a test table
            conn->executeUpdate("CREATE TABLE test_table (id INTEGER PRIMARY KEY, name TEXT, value REAL, is_active INTEGER)");

            // Test batch insert using prepared statement
            auto insertStmt = conn->prepareStatement("INSERT INTO test_table (id, name, value, is_active) VALUES (?, ?, ?, ?)");

            // Insert 100 rows
            for (int i = 1; i <= 100; i++)
            {
                insertStmt->setInt(1, i);
                insertStmt->setString(2, "Name " + std::to_string(i));
                insertStmt->setDouble(3, i * 1.5);
                insertStmt->setBoolean(4, i % 2 == 0);
                auto rowsAffected = insertStmt->executeUpdate();
                REQUIRE(rowsAffected == 1);
            }

            // Test query with filtering
            auto queryStmt = conn->prepareStatement("SELECT * FROM test_table WHERE is_active = ? AND value > ?");
            queryStmt->setBoolean(1, true);
            queryStmt->setDouble(2, 50.0);

            auto resultSet = queryStmt->executeQuery();

            // Count the results
            int count = 0;
            std::vector<int> ids;
            while (resultSet->next())
            {
                count++;
                ids.push_back(resultSet->getInt("id"));

                // Verify the row data
                REQUIRE(resultSet->getString("name") == "Name " + std::to_string(resultSet->getInt("id")));
                REQUIRE(resultSet->getDouble("value") == Catch::Approx(resultSet->getInt("id") * 1.5));
                REQUIRE(resultSet->getBoolean("is_active") == true);
            }

            // We should have all even numbers from 34 to 100 (34, 36, 38, ..., 100)
            // That's (100 - 34) / 2 + 1 = 34 rows
            REQUIRE(count == 34);

            // Test transaction support
            // conn->setAutoCommit(false);
            conn->beginTransaction();

            // Delete half the rows
            auto deleteStmt = conn->prepareStatement("DELETE FROM test_table WHERE id <= ?");
            deleteStmt->setInt(1, 50);
            auto deletedRows = deleteStmt->executeUpdate();
            REQUIRE(deletedRows == 50);

            // Verify rows are deleted in this transaction
            auto countStmt = conn->prepareStatement("SELECT COUNT(*) as count FROM test_table");
            auto countResult = countStmt->executeQuery();
            REQUIRE(countResult->next());
            REQUIRE(countResult->getInt("count") == 50);

            // RESOURCE MANAGEMENT FIX (2026-02-15): Close result set before reusing variable
            // to prevent resource leaks and file locks
            countResult->close();

            // Rollback the transaction
            conn->rollback();

            //  Verify the rows are back
            countResult = countStmt->executeQuery();
            REQUIRE(countResult->next());
            REQUIRE(countResult->getInt("count") == 100);

            // RESOURCE MANAGEMENT FIX (2026-02-15): Close result set before reusing variable
            countResult->close();

            // Now delete and commit
            conn->beginTransaction();
            deletedRows = deleteStmt->executeUpdate();
            REQUIRE(deletedRows == 50);
            conn->commit();

            // Verify the deletion is permanent
            countResult = countStmt->executeQuery();
            REQUIRE(countResult->next());
            REQUIRE(countResult->getInt("count") == 50);

            // Test NULL handling
            auto nullStmt = conn->prepareStatement("INSERT INTO test_table (id, name, value, is_active) VALUES (?, ?, ?, ?)");
            nullStmt->setInt(1, 101);
            nullStmt->setString(2, "Null Test");
            nullStmt->setNull(3, cpp_dbc::Types::DOUBLE);
            nullStmt->setNull(4, cpp_dbc::Types::BOOLEAN);
            nullStmt->executeUpdate();

            // Verify NULL values
            auto nullQueryStmt = conn->prepareStatement("SELECT * FROM test_table WHERE id = ?");
            nullQueryStmt->setInt(1, 101);
            auto nullResult = nullQueryStmt->executeQuery();
            REQUIRE(nullResult->next());
            REQUIRE(nullResult->getString("name") == "Null Test");
            REQUIRE(nullResult->isNull(3)); // value column
            REQUIRE(nullResult->isNull(4)); // is_active column

            // Close all statements and result sets before dropping the table
            nullResult->close();
            nullQueryStmt->close();
            nullStmt->close();
            countResult->close();
            countStmt->close();
            deleteStmt->close();
            queryStmt->close();
            insertStmt->close();

            // Clean up
            // conn->executeUpdate("DROP TABLE IF EXISTS test_table");

            // Close the connection
            conn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::string errorMsg = std::string(e.what_s());
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "SQLite real database error: " + errorMsg);
            FAIL("SQLite real database test failed: " + std::string(e.what_s()));
        }
    }

    SECTION("SQLite date and time types test")
    {

        try
        {
            // Connect to SQLite
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));

            // Create a test table with date/time columns
            // SQLite stores dates as TEXT in ISO8601 format: "YYYY-MM-DD" or "YYYY-MM-DD HH:MM:SS"
            conn->executeUpdate("DROP TABLE IF EXISTS test_datetime_types");
            conn->executeUpdate(
                "CREATE TABLE test_datetime_types ("
                "id INTEGER PRIMARY KEY, "
                "date_col TEXT, "     // DATE stored as TEXT
                "datetime_col TEXT, " // DATETIME stored as TEXT
                "time_col TEXT, "     // TIME stored as TEXT
                "description TEXT"
                ")");

            // Insert test data using specialized methods
            auto pstmt = conn->prepareStatement(
                "INSERT INTO test_datetime_types VALUES (?, ?, ?, ?, ?)");

            // Test 1: Full date and time values
            pstmt->setInt(1, 1);
            pstmt->setDate(2, "2023-01-15");               // Using setDate()
            pstmt->setTimestamp(3, "2023-01-15 14:30:00"); // Using setTimestamp()
            pstmt->setString(4, "14:30:00");               // TIME uses setString()
            pstmt->setString(5, "Afternoon meeting");
            pstmt->executeUpdate();

            // Test 2: Morning time
            pstmt->setInt(1, 2);
            pstmt->setDate(2, "2023-06-20");
            pstmt->setTimestamp(3, "2023-06-20 08:15:30");
            pstmt->setString(4, "08:15:30");
            pstmt->setString(5, "Morning routine");
            pstmt->executeUpdate();

            // Test 3: Late night
            pstmt->setInt(1, 3);
            pstmt->setDate(2, "2023-12-31");
            pstmt->setTimestamp(3, "2023-12-31 23:59:59");
            pstmt->setString(4, "23:59:59");
            pstmt->setString(5, "End of year");
            pstmt->executeUpdate();

            // Test 4: NULL values for date/time
            pstmt->setInt(1, 4);
            pstmt->setNull(2, cpp_dbc::Types::VARCHAR); // NULL date
            pstmt->setNull(3, cpp_dbc::Types::VARCHAR); // NULL datetime
            pstmt->setNull(4, cpp_dbc::Types::VARCHAR); // NULL time
            pstmt->setString(5, "NULL test");
            pstmt->executeUpdate();

            // Close prepared statement
            pstmt->close();

            // Test retrieving date/time values using getDate(), getTimestamp(), and getTime()
            auto rs = conn->executeQuery("SELECT * FROM test_datetime_types ORDER BY id");

            // Verify first row
            REQUIRE(rs->next());
            REQUIRE(rs->getInt("id") == 1);
            REQUIRE(rs->getDate("date_col") == "2023-01-15");
            REQUIRE(rs->getTimestamp("datetime_col") == "2023-01-15 14:30:00");
            REQUIRE(rs->getTime("time_col") == "14:30:00");
            REQUIRE(rs->getString("description") == "Afternoon meeting");

            // Verify second row
            REQUIRE(rs->next());
            REQUIRE(rs->getInt("id") == 2);
            REQUIRE(rs->getDate("date_col") == "2023-06-20");
            REQUIRE(rs->getTimestamp("datetime_col") == "2023-06-20 08:15:30");
            REQUIRE(rs->getTime("time_col") == "08:15:30");
            REQUIRE(rs->getString("description") == "Morning routine");

            // Verify third row
            REQUIRE(rs->next());
            REQUIRE(rs->getInt("id") == 3);
            REQUIRE(rs->getDate("date_col") == "2023-12-31");
            REQUIRE(rs->getTimestamp("datetime_col") == "2023-12-31 23:59:59");
            REQUIRE(rs->getTime("time_col") == "23:59:59");
            REQUIRE(rs->getString("description") == "End of year");

            // Verify fourth row (NULL values)
            REQUIRE(rs->next());
            REQUIRE(rs->getInt("id") == 4);
            REQUIRE(rs->isNull("date_col"));
            REQUIRE(rs->isNull("datetime_col"));
            REQUIRE(rs->isNull("time_col"));
            REQUIRE(rs->getString("description") == "NULL test");

            REQUIRE_FALSE(rs->next());

            // Test date-based query using prepared statement
            auto queryStmt = conn->prepareStatement(
                "SELECT * FROM test_datetime_types WHERE date_col = ?");
            queryStmt->setDate(1, "2023-06-20");
            auto queryRs = queryStmt->executeQuery();

            REQUIRE(queryRs->next());
            REQUIRE(queryRs->getInt("id") == 2);
            REQUIRE_FALSE(queryRs->next());

            // Test datetime-based query
            auto datetimeQuery = conn->prepareStatement(
                "SELECT * FROM test_datetime_types WHERE datetime_col = ?");
            datetimeQuery->setTimestamp(1, "2023-12-31 23:59:59");
            auto datetimeRs = datetimeQuery->executeQuery();

            REQUIRE(datetimeRs->next());
            REQUIRE(datetimeRs->getInt("id") == 3);
            REQUIRE_FALSE(datetimeRs->next());

            // Clean up
            datetimeRs->close();
            datetimeQuery->close();
            queryRs->close();
            queryStmt->close();
            rs->close();

            // conn->executeUpdate("DROP TABLE test_datetime_types");
            conn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::string errorMsg = std::string(e.what_s());
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "SQLite date/time test error: " + errorMsg);
            FAIL("SQLite date/time test failed: " + std::string(e.what_s()));
        }
    }

    /* SECTION("SQLite connection pool") - moved to 22_141_test_sqlite_real_connection_pool.cpp
    {
        cpp_dbc::system_utils::logWithTimesMillis("SECTION", "SQLite connection pool");

        // Create a connection pool configuration with shorter timeouts for tests
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setUri(connStr);
        poolConfig.setUsername("");
        poolConfig.setPassword("");
        poolConfig.setInitialSize(3);
        poolConfig.setMaxSize(5);
        poolConfig.setMinIdle(1);
        poolConfig.setConnectionTimeout(10000);
        poolConfig.setValidationInterval(500);
        poolConfig.setIdleTimeout(5000);
        poolConfig.setMaxLifetimeMillis(10000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(true);

        // Create a connection pool using factory method
        auto poolResult = cpp_dbc::SQLite::SQLiteConnectionPool::create(std::nothrow, poolConfig);
        if (!poolResult.has_value())
        {
            throw poolResult.error();
        }
        auto pool = poolResult.value();

        // Create a test table
        auto conn = pool->getRelationalDBConnection();
        conn->executeUpdate("DROP TABLE IF EXISTS test_table");
        conn->executeUpdate("CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT, value REAL)");
        conn->returnToPool();

        // Test multiple connections in parallel
        const int numThreads = 20;
        const int opsPerThread = 50;

        std::atomic<int> successCount(0);
        std::vector<std::thread> threads;

        const std::string insertQuery = "INSERT INTO test_table (id, name, value) VALUES (?, ?, ?)";

        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([&pool, i, opsPerThread, insertQuery, &successCount]()
                                          {
                for (int j = 0; j < opsPerThread; j++) {
                    try {
                        auto conn_thread = pool->getRelationalDBConnection();

                        int id = i * 100 + j;
                        auto pstmt = conn_thread->prepareStatement(insertQuery);
                        pstmt->setInt(1, id);
                        pstmt->setString(2, "Thread " + std::to_string(i) + " Op " + std::to_string(j));
                        pstmt->setDouble(3, id * 1.5);
                        pstmt->executeUpdate();

                        conn_thread->returnToPool();
                        successCount++;
                    }
                    catch (const std::exception& e) {
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Thread operation failed: " + std::string(e.what()));
                    }
                } }));
        }

        for (auto &t : threads)
        {
            t.join();
        }

        REQUIRE(successCount >= (numThreads * opsPerThread * 0.95));

        // Verify the data
        conn = pool->getRelationalDBConnection();
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_table");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("count") == numThreads * opsPerThread);
        rs->close();

        // Clean up
        conn->executeUpdate("DROP TABLE IF EXISTS test_table");
        conn->returnToPool();

        // Close the pool
        pool->close();
    } */

    SECTION("SQLite metadata retrieval")
    {
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));

        conn->executeUpdate("DROP TABLE IF EXISTS test_types");
        conn->executeUpdate(
            "CREATE TABLE test_types ("
            "id INTEGER PRIMARY KEY, "
            "int_col INTEGER, "
            "double_col REAL, "
            "varchar_col TEXT, "
            "date_col TEXT, "
            "bool_col INTEGER"
            ")");

        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_types (id, int_col, double_col, varchar_col, date_col, bool_col) "
            "VALUES (?, ?, ?, ?, ?, ?)");
        pstmt->setInt(1, 1);
        pstmt->setInt(2, 42);
        pstmt->setDouble(3, 3.14159);
        pstmt->setString(4, "Hello SQLite");
        pstmt->setDate(5, "2023-01-15");
        pstmt->setBoolean(6, true);
        auto result = pstmt->executeUpdate();
        REQUIRE(result == 1);
        pstmt->close();

        auto rs = conn->executeQuery("SELECT * FROM test_types");
        REQUIRE(rs->next());

        // Verify column count and names
        REQUIRE(rs->getColumnCount() == 6);
        auto columnNames = rs->getColumnNames();
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "id") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "int_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "double_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "varchar_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "date_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "bool_col") != columnNames.end());

        // Verify values
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getInt("int_col") == 42);
        REQUIRE(rs->getDouble("double_col") == Catch::Approx(3.14159));
        REQUIRE(rs->getString("varchar_col") == "Hello SQLite");
        REQUIRE(rs->getDate("date_col") == "2023-01-15");
        REQUIRE(rs->getBoolean("bool_col") == true);
        rs->close();

        // Test NULL values
        conn->executeUpdate("UPDATE test_types SET int_col = NULL, varchar_col = NULL WHERE id = 1");
        rs = conn->executeQuery("SELECT * FROM test_types WHERE id = 1");
        REQUIRE(rs->next());
        REQUIRE(rs->isNull("int_col"));
        REQUIRE(rs->isNull("varchar_col"));
        rs->close();

        // Clean up
        conn->executeUpdate("DROP TABLE IF EXISTS test_types");
        conn->close();
    }

    // Stress test (concurrent pool load) moved to 22_141_test_sqlite_real_connection_pool.cpp
    // ("Connection pool under load" SECTION).
}
#else
// Skip tests if SQLite support is not enabled
TEST_CASE("SQLite real database operations (skipped)", "[22_031_02_sqlite_real]")
{
    SKIP("SQLite support is not enabled");
}
#endif