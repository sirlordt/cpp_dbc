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

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#include "test_sqlite_common.hpp"

// Helper function to get the path to the test_db_connections.yml file
// Using common_test_helpers namespace for helper functions

// Test case for SQLite real database operations
TEST_CASE("SQLite real database operations", "[sqlite_real]")
{
#if USE_SQLITE
    // Skip this test if SQLite support is not enabled
    SECTION("Test SQLite real database operations")
    {
        // Get SQLite configuration using the helper function
        auto dbConfig = sqlite_test_helpers::getSQLiteConfig("test_sqlite");

        // Get connection string from the database config
        std::string connStr = dbConfig.createConnectionString();

        // Test connection

        // Register the SQLite driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

        try
        {
            // Attempt to connect to SQLite
            std::cout << "Attempting to connect to SQLite with connection string: " << connStr << std::endl;

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

            // Rollback the transaction
            conn->rollback();

            //  Verify the rows are back
            countResult = countStmt->executeQuery();
            REQUIRE(countResult->next());
            REQUIRE(countResult->getInt("count") == 100);

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
            conn->executeUpdate("DROP TABLE IF EXISTS test_table");

            // Close the connection
            conn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::string errorMsg = e.what_s();
            std::cout << "SQLite real database error: " << errorMsg << std::endl;
            FAIL("SQLite real database test failed: " + std::string(e.what_s()));
        }
    }
#else
    // Skip this test if SQLite support is not enabled
    SKIP("SQLite support is not enabled");
#endif
}