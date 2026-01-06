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

 @file test_firebird_real_json.cpp
 @brief Tests for Firebird JSON data storage (as text)

 Note: Firebird does not have native JSON data type support like MySQL or PostgreSQL.
 JSON data is stored as VARCHAR or BLOB SUB_TYPE TEXT and must be parsed/validated
 by the application layer. This test file demonstrates storing and retrieving
 JSON-formatted strings in Firebird.

*/

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <fstream>
#include <random>
#include <sstream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "test_firebird_common.hpp"

#if USE_FIREBIRD
// Test case for Firebird JSON data storage (as text)
TEST_CASE("Firebird JSON data storage", "[firebird_real_json]")
{
    // Skip these tests if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    // Get Firebird configuration using the centralized function
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");

    // Extract connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    SECTION("Basic JSON storage operations")
    {
        // Register the Firebird driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with text column for JSON
        // Firebird uses BLOB SUB_TYPE TEXT for large text data
        try
        {
            conn->executeUpdate("DROP TABLE test_json_types");
        }
        catch (...)
        {
        }
        conn->executeUpdate(
            "CREATE TABLE test_json_types ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "json_data BLOB SUB_TYPE TEXT"
            ")");

        // Insert data using a prepared statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_types (id, json_data) VALUES (?, ?)");

        // Simple JSON object
        pstmt->setInt(1, 1);
        pstmt->setString(2, "{\"name\": \"John\", \"age\": 30, \"city\": \"New York\"}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // JSON array
        pstmt->setInt(1, 2);
        pstmt->setString(2, "[1, 2, 3, 4, 5]");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Nested JSON object
        pstmt->setInt(1, 3);
        pstmt->setString(2, "{\"person\": {\"name\": \"Alice\", \"age\": 25}, \"active\": true}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Complex JSON with arrays and nested objects
        pstmt->setInt(1, 4);
        pstmt->setString(2, "{\"people\": [{\"name\": \"Bob\", \"age\": 40}, {\"name\": \"Carol\", \"age\": 35}], \"location\": {\"city\": \"Boston\", \"state\": \"MA\"}}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Test retrieving JSON data
        auto rs = conn->executeQuery("SELECT * FROM test_json_types WHERE id = 1");
        REQUIRE(rs->next());
        std::string jsonData = rs->getString("JSON_DATA");

        // The exact format should be preserved
        REQUIRE(jsonData.find("John") != std::string::npos);
        REQUIRE(jsonData.find("30") != std::string::npos);
        REQUIRE(jsonData.find("New York") != std::string::npos);
        rs->close();

        // Test retrieving JSON array
        rs = conn->executeQuery("SELECT * FROM test_json_types WHERE id = 2");
        REQUIRE(rs->next());
        jsonData = rs->getString("JSON_DATA");
        REQUIRE(jsonData.find("[1, 2, 3, 4, 5]") != std::string::npos);
        rs->close();

        // Test retrieving nested JSON
        rs = conn->executeQuery("SELECT * FROM test_json_types WHERE id = 3");
        REQUIRE(rs->next());
        jsonData = rs->getString("JSON_DATA");
        REQUIRE(jsonData.find("Alice") != std::string::npos);
        REQUIRE(jsonData.find("25") != std::string::npos);
        REQUIRE(jsonData.find("true") != std::string::npos);
        rs->close();

        // Close prepared statement before dropping table (Firebird requires this)
        pstmt->close();

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_types");
        conn->close();
    }

    SECTION("JSON storage with VARCHAR column")
    {
        // Register the Firebird driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with VARCHAR column for smaller JSON
        try
        {
            conn->executeUpdate("DROP TABLE test_json_varchar");
        }
        catch (...)
        {
        }
        conn->executeUpdate(
            "CREATE TABLE test_json_varchar ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "json_data VARCHAR(4000)"
            ")");

        // Insert test data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_varchar (id, json_data) VALUES (?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setString(2, "{\"name\": \"John\", \"age\": 30, \"city\": \"New York\"}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Test retrieving JSON data
        auto rs = conn->executeQuery("SELECT * FROM test_json_varchar WHERE id = 1");
        REQUIRE(rs->next());
        std::string jsonData = rs->getString("JSON_DATA");

        REQUIRE(jsonData.find("John") != std::string::npos);
        REQUIRE(jsonData.find("30") != std::string::npos);
        REQUIRE(jsonData.find("New York") != std::string::npos);

        // Close result set before dropping table (Firebird requires this)
        rs->close();

        // Close prepared statement before dropping table (Firebird requires this)
        pstmt->close();

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_varchar");
        conn->close();
    }

    SECTION("JSON search using LIKE (text-based)")
    {
        // Register the Firebird driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with text column for JSON
        try
        {
            conn->executeUpdate("DROP TABLE test_json_search");
        }
        catch (...)
        {
        }
        conn->executeUpdate(
            "CREATE TABLE test_json_search ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "json_data BLOB SUB_TYPE TEXT"
            ")");

        // Insert multiple records with different JSON data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_search (id, json_data) VALUES (?, ?)");

        // Products data
        pstmt->setInt(1, 1);
        pstmt->setString(2, "{\"product\": \"Laptop\", \"price\": 1200, \"tags\": [\"electronics\", \"computer\"]}");
        REQUIRE(pstmt->executeUpdate() == 1);

        pstmt->setInt(1, 2);
        pstmt->setString(2, "{\"product\": \"Smartphone\", \"price\": 800, \"tags\": [\"electronics\", \"mobile\"]}");
        REQUIRE(pstmt->executeUpdate() == 1);

        pstmt->setInt(1, 3);
        pstmt->setString(2, "{\"product\": \"Headphones\", \"price\": 200, \"tags\": [\"electronics\", \"audio\"]}");
        REQUIRE(pstmt->executeUpdate() == 1);

        pstmt->setInt(1, 4);
        pstmt->setString(2, "{\"product\": \"T-Shirt\", \"price\": 25, \"tags\": [\"clothing\", \"casual\"]}");
        REQUIRE(pstmt->executeUpdate() == 1);

        pstmt->setInt(1, 5);
        pstmt->setString(2, "{\"product\": \"Book\", \"price\": 15, \"tags\": [\"media\", \"education\"]}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Test text-based search using LIKE
        // Note: This is a simple text search, not a proper JSON query
        auto rs = conn->executeQuery("SELECT id FROM test_json_search WHERE json_data LIKE '%electronics%'");
        std::vector<int> electronicsIds;
        while (rs->next())
        {
            electronicsIds.push_back(rs->getInt("ID"));
        }
        REQUIRE(electronicsIds.size() == 3); // Should find 3 products with "electronics" tag

        // Test search for specific product
        rs = conn->executeQuery("SELECT id, json_data FROM test_json_search WHERE json_data LIKE '%Laptop%'");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("ID") == 1);
        std::string jsonData = rs->getString("JSON_DATA");
        REQUIRE(jsonData.find("Laptop") != std::string::npos);
        rs->close();

        // Test search for clothing
        rs = conn->executeQuery("SELECT id FROM test_json_search WHERE json_data LIKE '%clothing%'");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("ID") == 4);
        rs->close();

        // Close prepared statement before dropping table (Firebird requires this)
        pstmt->close();

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_search");
        conn->close();
    }

    SECTION("JSON aggregation and transformation")
    {
        // Register the Firebird driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with JSON column
        try
        {
            conn->executeUpdate("DROP TABLE test_json_aggregation");
        }
        catch (...)
        {
        }
        conn->executeUpdate(
            "CREATE TABLE test_json_aggregation ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "category VARCHAR(50), "
            "json_data BLOB SUB_TYPE TEXT"
            ")");

        // Insert test data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_aggregation (id, category, json_data) VALUES (?, ?, ?)");

        // Electronics category
        pstmt->setInt(1, 1);
        pstmt->setString(2, "electronics");
        pstmt->setString(3, "{\"name\": \"Laptop\", \"price\": 1200, \"stock\": 10}");
        REQUIRE(pstmt->executeUpdate() == 1);

        pstmt->setInt(1, 2);
        pstmt->setString(2, "electronics");
        pstmt->setString(3, "{\"name\": \"Smartphone\", \"price\": 800, \"stock\": 15}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Clothing category
        pstmt->setInt(1, 3);
        pstmt->setString(2, "clothing");
        pstmt->setString(3, "{\"name\": \"T-Shirt\", \"price\": 20, \"stock\": 100}");
        REQUIRE(pstmt->executeUpdate() == 1);

        pstmt->setInt(1, 4);
        pstmt->setString(2, "clothing");
        pstmt->setString(3, "{\"name\": \"Jeans\", \"price\": 50, \"stock\": 75}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Books category
        pstmt->setInt(1, 5);
        pstmt->setString(2, "books");
        pstmt->setString(3, "{\"name\": \"Novel\", \"price\": 15, \"stock\": 50}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Test grouping by category
        auto rs = conn->executeQuery(
            "SELECT category, COUNT(*) as item_count FROM test_json_aggregation GROUP BY category ORDER BY category");

        REQUIRE(rs->next());
        REQUIRE(rs->getString("CATEGORY") == "books");
        REQUIRE(rs->getInt("ITEM_COUNT") == 1);

        REQUIRE(rs->next());
        REQUIRE(rs->getString("CATEGORY") == "clothing");
        REQUIRE(rs->getInt("ITEM_COUNT") == 2);

        REQUIRE(rs->next());
        REQUIRE(rs->getString("CATEGORY") == "electronics");
        REQUIRE(rs->getInt("ITEM_COUNT") == 2);

        // Close result set before next query
        rs->close();

        // Test retrieving all items in a category
        rs = conn->executeQuery(
            "SELECT id, json_data FROM test_json_aggregation WHERE category = 'electronics' ORDER BY id");

        REQUIRE(rs->next());
        REQUIRE(rs->getInt("ID") == 1);
        std::string jsonData = rs->getString("JSON_DATA");
        REQUIRE(jsonData.find("Laptop") != std::string::npos);

        REQUIRE(rs->next());
        REQUIRE(rs->getInt("ID") == 2);
        jsonData = rs->getString("JSON_DATA");
        REQUIRE(jsonData.find("Smartphone") != std::string::npos);
        rs->close();

        // Close prepared statement before dropping table (Firebird requires this)
        pstmt->close();

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_aggregation");
        conn->close();
    }

    SECTION("JSON validation (application-level)")
    {
        // Register the Firebird driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with text column for JSON
        try
        {
            conn->executeUpdate("DROP TABLE test_json_validation");
        }
        catch (...)
        {
        }
        conn->executeUpdate(
            "CREATE TABLE test_json_validation ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "json_data BLOB SUB_TYPE TEXT"
            ")");

        // Test valid JSON insertion
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_validation (id, json_data) VALUES (?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setString(2, "{\"valid\": true}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Note: Firebird does NOT validate JSON - it stores any text
        // Invalid JSON will be stored without error (unlike MySQL's JSON type)
        pstmt->setInt(1, 2);
        pstmt->setString(2, "{invalid: json}"); // This will be stored as-is
        REQUIRE(pstmt->executeUpdate() == 1);

        // Verify both records were stored
        auto rs = conn->executeQuery("SELECT COUNT(*) as cnt FROM test_json_validation");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("CNT") == 2);

        // Close result set before next query
        rs->close();

        // Retrieve and verify the invalid JSON was stored as-is
        rs = conn->executeQuery("SELECT json_data FROM test_json_validation WHERE id = 2");
        REQUIRE(rs->next());
        std::string invalidJson = rs->getString("JSON_DATA");
        REQUIRE(invalidJson == "{invalid: json}");
        rs->close();

        // Close prepared statement before dropping table (Firebird requires this)
        pstmt->close();

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_validation");
        conn->close();
    }

    SECTION("JSON performance with large datasets")
    {
        // Register the Firebird driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with text column for JSON
        try
        {
            conn->executeUpdate("DROP TABLE test_json_performance");
        }
        catch (...)
        {
        }
        conn->executeUpdate(
            "CREATE TABLE test_json_performance ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "json_data BLOB SUB_TYPE TEXT"
            ")");

        // Insert multiple records with random JSON data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_performance (id, json_data) VALUES (?, ?)");

        const int numRecords = 100;
        for (int i = 1; i <= numRecords; i++)
        {
            pstmt->setInt(1, i);

            // Every 10th record will have a specific price for testing
            if (i % 10 == 0)
            {
                std::string jsonData = "{\"name\": \"Product" + std::to_string(i) + "\", \"price\": " + std::to_string(i * 10) +
                                       ", \"data\": " + common_test_helpers::generateRandomJson(2, 3) + "}";
                pstmt->setString(2, jsonData);
            }
            else
            {
                std::string jsonData = "{\"name\": \"Product" + std::to_string(i) + "\", \"price\": " + std::to_string(i * 5) +
                                       ", \"data\": " + common_test_helpers::generateRandomJson(2, 3) + "}";
                pstmt->setString(2, jsonData);
            }

            REQUIRE(pstmt->executeUpdate() == 1);
        }

        // Test text-based search performance
        auto startTime = std::chrono::steady_clock::now();
        auto rs = conn->executeQuery("SELECT id, json_data FROM test_json_performance WHERE json_data LIKE '%Product50%'");
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << "Firebird JSON text search execution time: " << duration << " ms" << std::endl;

        // Should find at least one record
        REQUIRE(rs->next());

        // Close result set before next query
        rs->close();

        // Test count query
        rs = conn->executeQuery("SELECT COUNT(*) as cnt FROM test_json_performance");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("CNT") == numRecords);
        rs->close();

        // Close prepared statement before dropping table (Firebird requires this)
        pstmt->close();

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_performance");
        conn->close();
    }
}
#endif // USE_FIREBIRD