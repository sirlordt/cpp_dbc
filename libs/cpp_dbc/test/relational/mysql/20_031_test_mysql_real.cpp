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

 @file 20_031_test_mysql_real.cpp
 @brief Tests for MySQL database operations with real connections

*/

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "20_001_test_mysql_real_common.hpp"

// Helper function to get the path to the test_db_connections.yml file
std::string getConfigFilePath();

#if USE_MYSQL
// Test case for real MySQL connection
TEST_CASE("Real MySQL connection tests", "[20_031_01_mysql_real]")
{
    // Skip these tests if we can't connect to MySQL
    if (!mysql_test_helpers::canConnectToMySQL())
    {
        SKIP("Cannot connect to MySQL database");
        return;
    }

    // Get MySQL configuration using the centralized function
    auto dbConfig = mysql_test_helpers::getMySQLConfig("dev_mysql");

    // Extract connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Get test queries from the config options
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE)");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_table (id, name, value) VALUES (?, ?, ?)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_table WHERE id = ?");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE IF EXISTS test_table");

    SECTION("Basic MySQL operations")
    {
        // Register the MySQL driver (safe registration)
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        auto result = conn->executeUpdate(createTableQuery);
        REQUIRE(result == 0); // CREATE TABLE should return 0 affected rows

        // Insert data using a prepared statement
        auto pstmt = conn->prepareStatement(insertDataQuery);
        REQUIRE(pstmt != nullptr);

        // Insert multiple rows
        for (int i = 1; i <= 10; i++)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Test Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            auto insertResult = pstmt->executeUpdate();
            REQUIRE(insertResult == 1); // Each insert should affect 1 row
        }

        // Select data using a prepared statement
        auto selectStmt = conn->prepareStatement(selectDataQuery);
        REQUIRE(selectStmt != nullptr);

        // Select a specific row
        selectStmt->setInt(1, 5);
        auto rs = selectStmt->executeQuery();
        REQUIRE(rs != nullptr);

        // Verify the result
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 5);
        REQUIRE(rs->getString("name") == "Test Name 5");
        REQUIRE_FALSE(rs->next()); // Should only be one row

        // Select all rows using direct query
        rs = conn->executeQuery("SELECT * FROM test_table ORDER BY id");
        REQUIRE(rs != nullptr);

        // Verify all rows
        int count = 0;
        while (rs->next())
        {
            count++;
            REQUIRE(rs->getInt("id") == count);
            REQUIRE(rs->getString("name") == "Test Name " + std::to_string(count));
        }
        REQUIRE(count == 10);

        // Update data
        auto updateResult = conn->executeUpdate("UPDATE test_table SET name = 'Updated Name' WHERE id = 3");
        REQUIRE(updateResult == 1); // Should affect 1 row

        // Verify the update
        rs = conn->executeQuery("SELECT * FROM test_table WHERE id = 3");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("name") == "Updated Name");

        // Delete data
        auto deleteResult = conn->executeUpdate("DELETE FROM test_table WHERE id > 5");
        REQUIRE(deleteResult == 5); // Should delete 5 rows (6-10)

        // Verify the delete
        rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_table");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("count") == 5); // Should be 5 rows left

        // Drop the test table
        result = conn->executeUpdate(dropTableQuery);
        REQUIRE(result == 0); // DROP TABLE should return 0 affected rows

        // Close the connection
        conn->close();
    }

    // Connection pool and transaction management tests moved to 20_141_test_mysql_real_connection_pool.cpp

    SECTION("MySQL metadata retrieval")
    {
        // Register the MySQL driver (safe registration)
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

        // Create a test table with various data types
        conn->executeUpdate("DROP TABLE IF EXISTS test_types");
        conn->executeUpdate(
            "CREATE TABLE test_types ("
            "id INT PRIMARY KEY, "
            "int_col INT, "
            "double_col DOUBLE, "
            "varchar_col VARCHAR(100), "
            "text_col TEXT, "
            "date_col DATE, "
            "datetime_col DATETIME, "
            "bool_col BOOLEAN"
            ")");

        // Insert test data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_types VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setInt(2, 42);
        pstmt->setDouble(3, 3.14159);
        pstmt->setString(4, "Hello, World!");
        pstmt->setString(5, "This is a longer text field with more content.");
        pstmt->setDate(6, "2023-01-15");
        pstmt->setTimestamp(7, "2023-01-15 14:30:00");
        pstmt->setBoolean(8, true);

        pstmt->executeUpdate();

        // Test retrieving different data types
        auto rs = conn->executeQuery("SELECT * FROM test_types");
        REQUIRE(rs->next());

        // Test each data type
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getInt("int_col") == 42);
        REQUIRE((rs->getDouble("double_col") > 3.14 && rs->getDouble("double_col") < 3.15));
        REQUIRE(rs->getString("varchar_col") == "Hello, World!");
        REQUIRE(rs->getString("text_col") == "This is a longer text field with more content.");
        REQUIRE(rs->getDate("date_col") == "2023-01-15");
        REQUIRE(rs->getTimestamp("datetime_col") == "2023-01-15 14:30:00");
        REQUIRE(rs->getBoolean("bool_col") == true);

        // Test column metadata
        auto columnNames = rs->getColumnNames();
        REQUIRE(columnNames.size() == 8);
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "id") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "int_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "double_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "varchar_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "text_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "date_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "datetime_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "bool_col") != columnNames.end());

        // Test NULL values
        conn->executeUpdate("UPDATE test_types SET int_col = NULL, varchar_col = NULL WHERE id = 1");
        rs = conn->executeQuery("SELECT * FROM test_types");
        REQUIRE(rs->next());
        REQUIRE(rs->isNull("int_col"));
        REQUIRE(rs->isNull("varchar_col"));

        // Clean up
        conn->executeUpdate("DROP TABLE test_types");

        // Close the connection
        conn->close();
    }

    SECTION("MySQL TIME type test")
    {
        // Register the MySQL driver (safe registration)
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

        // Create a test table with TIME column
        conn->executeUpdate("DROP TABLE IF EXISTS test_time_types");
        conn->executeUpdate(
            "CREATE TABLE test_time_types ("
            "id INT PRIMARY KEY, "
            "time_col TIME, "
            "description VARCHAR(100)"
            ")");

        // Insert test data using setTime for TIME columns
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_time_types VALUES (?, ?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setTime(2, "14:30:00"); // TIME using setTime method
        pstmt->setString(3, "Afternoon meeting");
        pstmt->executeUpdate();

        pstmt->setInt(1, 2);
        pstmt->setTime(2, "08:15:30"); // TIME with seconds
        pstmt->setString(3, "Morning routine");
        pstmt->executeUpdate();

        pstmt->setInt(1, 3);
        pstmt->setTime(2, "23:59:59"); // Late night
        pstmt->setString(3, "End of day");
        pstmt->executeUpdate();

        // Test retrieving TIME values using getTime()
        auto rs = conn->executeQuery("SELECT * FROM test_time_types ORDER BY id");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getTime("time_col") == "14:30:00");
        REQUIRE(rs->getString("description") == "Afternoon meeting");

        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 2);
        REQUIRE(rs->getTime("time_col") == "08:15:30");
        REQUIRE(rs->getString("description") == "Morning routine");

        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 3);
        REQUIRE(rs->getTime("time_col") == "23:59:59");
        REQUIRE(rs->getString("description") == "End of day");

        REQUIRE_FALSE(rs->next());

        // Clean up
        conn->executeUpdate("DROP TABLE test_time_types");

        // Close the connection
        conn->close();
    }

    // Stress test moved to 20_141_test_mysql_real_connection_pool.cpp
}
#else
// Skip tests if MySQL support is not enabled
TEST_CASE("Real MySQL connection tests (skipped)", "[20_031_02_mysql_real]")
{
    SKIP("MySQL support is not enabled");
}
#endif