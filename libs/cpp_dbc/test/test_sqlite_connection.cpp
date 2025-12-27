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

 @file test_sqlite_connection.cpp
 @brief Tests for SQLite database operations

*/

#include <string>
#include <fstream>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#include "test_sqlite_common.hpp"

// Helper function to get the path to the test_db_connections.yml file
// Using common_test_helpers namespace for helper functions

// Test case to verify SQLite connection
TEST_CASE("SQLite connection test", "[sqlite_connection]")
{
#if USE_SQLITE
    // Skip this test if SQLite support is not enabled
    SECTION("Test SQLite connection")
    {
        // Get SQLite configuration using the helper function
        auto dbConfig = sqlite_test_helpers::getSQLiteConfig("dev_sqlite");

        // Get connection string directly from the database config
        std::string connStr = dbConfig.createConnectionString();

        // Register the SQLite driver
        cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

        try
        {
            // Attempt to connect to SQLite
            std::cout << "Attempting to connect to SQLite with connection string: " << connStr << std::endl;

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));

            // Execute a simple query to verify the connection
            auto resultSet = conn->executeQuery("SELECT 1 as test_value");
            REQUIRE(resultSet->next());
            REQUIRE(resultSet->getInt("test_value") == 1);

            // Test creating a table
            conn->executeUpdate("CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT)");

            // Test inserting data using a prepared statement
            auto stmt = conn->prepareStatement("INSERT INTO test_table (id, name) VALUES (?, ?)");
            stmt->setInt(1, 1);
            stmt->setString(2, "Test Name");
            auto rowsAffected = stmt->executeUpdate();
            REQUIRE(rowsAffected == 1);

            // Test querying the inserted data
            auto queryStmt = conn->prepareStatement("SELECT * FROM test_table WHERE id = ?");
            queryStmt->setInt(1, 1);
            auto queryResult = queryStmt->executeQuery();
            REQUIRE(queryResult->next());
            REQUIRE(queryResult->getInt("id") == 1);
            REQUIRE(queryResult->getString("name") == "Test Name");

            // Test transaction support
            conn->beginTransaction();
            REQUIRE(conn->getAutoCommit() == false);
            REQUIRE(conn->transactionActive() == true);

            // Insert another row in a transaction
            conn->executeUpdate("INSERT INTO test_table (id, name) VALUES (2, 'Transaction Test')");

            // Rollback the transaction
            conn->rollback();

            // Verify the row was not inserted
            auto verifyStmt = conn->prepareStatement("SELECT COUNT(*) as count FROM test_table WHERE id = ?");
            verifyStmt->setInt(1, 2);
            auto verifyResult = verifyStmt->executeQuery();
            REQUIRE(verifyResult->next());
            REQUIRE(verifyResult->getInt("count") == 0);

            // Insert again and commit
            conn->beginTransaction();
            conn->executeUpdate("INSERT INTO test_table (id, name) VALUES (2, 'Transaction Test')");
            conn->commit();

            // Verify the row was inserted
            verifyResult = verifyStmt->executeQuery();
            REQUIRE(verifyResult->next());
            REQUIRE(verifyResult->getInt("count") == 1);

            // Close all result sets and statements before dropping the table
            verifyResult->close();
            verifyStmt->close();
            queryResult->close();
            queryStmt->close();
            stmt->close();
            resultSet->close();

            // Clean up
            conn->executeUpdate("DROP TABLE IF EXISTS test_table");

            // Close the connection
            conn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::string errorMsg = e.what_s();
            std::cout << "SQLite connection error: " << errorMsg << std::endl;
            FAIL("SQLite connection failed: " + std::string(e.what_s()));
        }
    }
#else
    // Skip this test if SQLite support is not enabled
    SKIP("SQLite support is not enabled");
#endif
}

// Test case for SQLite in-memory database
TEST_CASE("SQLite in-memory database test", "[sqlite_memory]")
{
#if USE_SQLITE
    SECTION("Test SQLite in-memory database")
    {
        // Register the SQLite driver
        cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

        try
        {
            // Connect to an in-memory database
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection("cpp_dbc:sqlite://:memory:", "", ""));

            // Create a table
            conn->executeUpdate("CREATE TABLE test_table (id INTEGER PRIMARY KEY, name TEXT)");

            // Insert data
            auto stmt = conn->prepareStatement("INSERT INTO test_table (id, name) VALUES (?, ?)");
            for (int i = 1; i <= 10; i++)
            {
                stmt->setInt(1, i);
                stmt->setString(2, "Name " + std::to_string(i));
                stmt->executeUpdate();
            }

            // Query data
            auto resultSet = conn->executeQuery("SELECT COUNT(*) as count FROM test_table");
            REQUIRE(resultSet->next());
            REQUIRE(resultSet->getInt("count") == 10);

            // Test transaction isolation (SQLite only supports SERIALIZABLE)
            REQUIRE(conn->getTransactionIsolation() == cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

            // Attempting to set a different isolation level should throw an exception
            REQUIRE_THROWS_AS(
                conn->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED),
                cpp_dbc::DBException);

            // Close all result sets and statements before closing the connection
            resultSet->close();
            stmt->close();

            // Close the connection
            conn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::string errorMsg = e.what_s();
            std::cout << "SQLite in-memory database error: " << errorMsg << std::endl;
            FAIL("SQLite in-memory database test failed: " + std::string(e.what_s()));
        }
    }
#else
    // Skip this test if SQLite support is not enabled
    SKIP("SQLite support is not enabled");
#endif
}