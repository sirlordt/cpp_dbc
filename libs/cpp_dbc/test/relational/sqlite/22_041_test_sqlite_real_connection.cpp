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

 @file 22_041_test_sqlite_real_connection.cpp
 @brief Tests for SQLite database operations with real connections

*/

#include <string>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "22_001_test_sqlite_real_common.hpp"

#if USE_SQLITE
// Test case to verify SQLite connection
TEST_CASE("SQLite connection test", "[22_041_01_sqlite_real_connection]")
{
    // Get SQLite configuration using the helper function
    auto dbConfig = sqlite_test_helpers::getSQLiteConfig("dev_sqlite");

    // Get connection string directly from the database config
    std::string connStr = dbConfig.createConnectionString();

    // Register the SQLite driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

    if (!sqlite_test_helpers::canConnectToSQLite())
    {
        SKIP("Cannot connect to SQLite database");
        return;
    }

    SECTION("Test SQLite connection")
    {
        try
        {
            // Attempt to connect to SQLite
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to SQLite with connection string: " + connStr);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));
            REQUIRE(conn != nullptr);

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

            // Verify connection state and URL
            CHECK_FALSE(conn->isClosed());

            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Connection URL: " + conn->getURL());

            CHECK(conn->getURL() == connStr);

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
            CHECK(conn->isClosed());
        }
        catch (const cpp_dbc::DBException &ex)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "SQLite connection error: " + std::string(ex.what_s()));
            FAIL("SQLite connection test failed: " + std::string(ex.what_s()));
        }
    }
}

// Test case for SQLite in-memory database
TEST_CASE("SQLite in-memory database test", "[22_041_02_sqlite_real_connection]")
{
    // Register the SQLite driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

    SECTION("Test SQLite in-memory database")
    {
        try
        {
            // Connect to an in-memory database
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection("cpp_dbc:sqlite://:memory:", "", ""));
            REQUIRE(conn != nullptr);

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
            CHECK(conn->isClosed());
        }
        catch (const cpp_dbc::DBException &ex)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "SQLite in-memory database error: " + std::string(ex.what_s()));
            FAIL("SQLite in-memory database test failed: " + std::string(ex.what_s()));
        }
    }
}
TEST_CASE("SQLite getServerVersion and getServerInfo", "[22_041_04_sqlite_real_connection]")
{
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

    SECTION("getServerVersion returns non-empty version string")
    {
        try
        {
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection("cpp_dbc:sqlite://:memory:", "", ""));
            REQUIRE(conn != nullptr);

            auto version = conn->getServerVersion();
            CHECK_FALSE(version.empty());
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "SQLite server version: " + version);

            conn->close();
        }
        catch (const cpp_dbc::DBException &ex)
        {
            FAIL("SQLite getServerVersion failed: " + std::string(ex.what_s()));
        }
    }

    SECTION("getServerInfo returns map with ServerVersion key")
    {
        try
        {
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection("cpp_dbc:sqlite://:memory:", "", ""));
            REQUIRE(conn != nullptr);

            auto info = conn->getServerInfo();
            CHECK_FALSE(info.empty());
            CHECK(info.count("ServerVersion") == 1);
            CHECK_FALSE(info.at("ServerVersion").empty());

            for (const auto &[key, value] : info)
            {
                cpp_dbc::system_utils::logWithTimesMillis("TEST", "  SQLite ServerInfo [" + key + "] = " + value);
            }

            // Verify some SQLite-specific keys
            CHECK(info.count("SourceId") == 1);
            CHECK(info.count("ThreadSafe") == 1);

            conn->close();
        }
        catch (const cpp_dbc::DBException &ex)
        {
            FAIL("SQLite getServerInfo failed: " + std::string(ex.what_s()));
        }
    }

    SECTION("getServerVersion matches ServerVersion in getServerInfo")
    {
        try
        {
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
                cpp_dbc::DriverManager::getDBConnection("cpp_dbc:sqlite://:memory:", "", ""));
            REQUIRE(conn != nullptr);

            auto version = conn->getServerVersion();
            auto info = conn->getServerInfo();

            CHECK(version == info.at("ServerVersion"));

            conn->close();
        }
        catch (const cpp_dbc::DBException &ex)
        {
            FAIL("SQLite version consistency test failed: " + std::string(ex.what_s()));
        }
    }
}
TEST_CASE("SQLite getDriverVersion", "[22_041_05_sqlite_real_connection]")
{
    auto driver = std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>();

    SECTION("getDriverVersion returns non-empty version string")
    {
        auto version = driver->getDriverVersion();
        CHECK_FALSE(version.empty());
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "SQLite driver version: " + version);
    }
}

#else
// Skip this test if SQLite support is not enabled
TEST_CASE("SQLite connection test (skipped)", "[22_041_03_sqlite_real_connection]")
{
    SKIP("SQLite support is not enabled");
}
#endif
