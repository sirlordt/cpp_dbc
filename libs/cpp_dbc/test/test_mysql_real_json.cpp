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

 @file test_mysql_real_json.cpp
 @brief Tests for MySQL JSON data type

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

#include "test_mysql_common.hpp"

#if USE_MYSQL
// Test case for MySQL JSON data type
TEST_CASE("MySQL JSON data type", "[mysql_real_json]")
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

    SECTION("Basic JSON operations")
    {
        // Register the MySQL driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with JSON column
        conn->executeUpdate("DROP TABLE IF EXISTS test_json_types");
        conn->executeUpdate(
            "CREATE TABLE test_json_types ("
            "id INT PRIMARY KEY, "
            "json_data JSON"
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
        std::string jsonData = rs->getString("json_data");

        // The exact format might vary, but the content should be the same
        REQUIRE(jsonData.find("John") != std::string::npos);
        REQUIRE(jsonData.find("30") != std::string::npos);
        REQUIRE(jsonData.find("New York") != std::string::npos);

        // Test JSON path expressions (MySQL 5.7+)
        rs = conn->executeQuery("SELECT JSON_EXTRACT(json_data, '$.name') as name FROM test_json_types WHERE id = 1");
        REQUIRE(rs->next());
        std::string name = rs->getString("name");
        REQUIRE((name == "\"John\"" || name == "John")); // Different MySQL versions might return with or without quotes

        // Test JSON array access
        rs = conn->executeQuery("SELECT JSON_EXTRACT(json_data, '$[1]') as second_element FROM test_json_types WHERE id = 2");
        REQUIRE(rs->next());
        std::string secondElement = rs->getString("second_element");
        REQUIRE((secondElement == "2" || secondElement == "\"2\""));

        // Test JSON_CONTAINS function
        rs = conn->executeQuery("SELECT JSON_CONTAINS(json_data, '[1]', '$') as contains_1 FROM test_json_types WHERE id = 2");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("contains_1") == 1);

        // Test JSON_CONTAINS_PATH function
        rs = conn->executeQuery("SELECT JSON_CONTAINS_PATH(json_data, 'one', '$.person.name') as has_person_name FROM test_json_types WHERE id = 3");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("has_person_name") == 1);

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_types");
        conn->close();
    }

    SECTION("JSON modification functions")
    {
        // Register the MySQL driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with JSON column
        conn->executeUpdate("DROP TABLE IF EXISTS test_json_modification");
        conn->executeUpdate(
            "CREATE TABLE test_json_modification ("
            "id INT PRIMARY KEY, "
            "data JSON"
            ")");

        // Insert test data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_modification (id, data) VALUES (?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setString(2, "{\"name\": \"John\", \"age\": 30, \"city\": \"New York\"}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Test JSON_SET function
        auto rs = conn->executeQuery("SELECT JSON_SET(data, '$.age', 35) as updated_data FROM test_json_modification WHERE id = 1");
        REQUIRE(rs->next());
        std::string updatedData = rs->getString("updated_data");
        REQUIRE((updatedData.find("\"age\":35") != std::string::npos || updatedData.find("\"age\": 35") != std::string::npos));

        // Update the record with JSON_SET
        conn->executeUpdate("UPDATE test_json_modification SET data = JSON_SET(data, '$.age', 35) WHERE id = 1");

        // Verify the update
        rs = conn->executeQuery("SELECT JSON_EXTRACT(data, '$.age') as age FROM test_json_modification WHERE id = 1");
        REQUIRE(rs->next());
        std::string age = rs->getString("age");
        REQUIRE((age == "35" || age == "\"35\""));

        // Test JSON_INSERT function
        conn->executeUpdate("UPDATE test_json_modification SET data = JSON_INSERT(data, '$.hobbies', JSON_ARRAY('reading', 'swimming')) WHERE id = 1");

        // Verify the insert
        rs = conn->executeQuery("SELECT JSON_EXTRACT(data, '$.hobbies') as hobbies FROM test_json_modification WHERE id = 1");
        REQUIRE(rs->next());
        std::string hobbies = rs->getString("hobbies");
        REQUIRE(hobbies.find("reading") != std::string::npos);
        REQUIRE(hobbies.find("swimming") != std::string::npos);

        // Test JSON_MERGE_PATCH function (MySQL 8.0+)
        try
        {
            conn->executeUpdate("UPDATE test_json_modification SET data = JSON_MERGE_PATCH(data, '{\"email\": \"john@example.com\"}') WHERE id = 1");

            // Verify the merge
            rs = conn->executeQuery("SELECT JSON_EXTRACT(data, '$.email') as email FROM test_json_modification WHERE id = 1");
            REQUIRE(rs->next());
            std::string email = rs->getString("email");
            REQUIRE((email == "\"john@example.com\"" || email == "john@example.com"));
        }
        catch (const cpp_dbc::DBException &e)
        {
            // JSON_MERGE_PATCH might not be available in older MySQL versions
            std::cout << "JSON_MERGE_PATCH test skipped: " << e.what_s() << std::endl;
        }

        // Test JSON_REMOVE function
        conn->executeUpdate("UPDATE test_json_modification SET data = JSON_REMOVE(data, '$.city') WHERE id = 1");

        // Verify the deletion
        rs = conn->executeQuery("SELECT JSON_CONTAINS_PATH(data, 'one', '$.city') as has_city FROM test_json_modification WHERE id = 1");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("has_city") == 0);

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_modification");
        conn->close();
    }

    SECTION("JSON search and filtering")
    {
        // Register the MySQL driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with JSON column
        conn->executeUpdate("DROP TABLE IF EXISTS test_json_search");
        conn->executeUpdate(
            "CREATE TABLE test_json_search ("
            "id INT PRIMARY KEY, "
            "data JSON"
            ")");

        // Insert multiple records with different JSON data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_search (id, data) VALUES (?, ?)");

        // Products data
        pstmt->setInt(1, 1);
        pstmt->setString(2, "{\"product\": \"Laptop\", \"price\": 1200, \"tags\": [\"electronics\", \"computer\"], \"stock\": {\"warehouse1\": 10, \"warehouse2\": 5}}");
        REQUIRE(pstmt->executeUpdate() == 1);

        pstmt->setInt(1, 2);
        pstmt->setString(2, "{\"product\": \"Smartphone\", \"price\": 800, \"tags\": [\"electronics\", \"mobile\"], \"stock\": {\"warehouse1\": 15, \"warehouse2\": 8}}");
        REQUIRE(pstmt->executeUpdate() == 1);

        pstmt->setInt(1, 3);
        pstmt->setString(2, "{\"product\": \"Headphones\", \"price\": 200, \"tags\": [\"electronics\", \"audio\"], \"stock\": {\"warehouse1\": 30, \"warehouse2\": 20}}");
        REQUIRE(pstmt->executeUpdate() == 1);

        pstmt->setInt(1, 4);
        pstmt->setString(2, "{\"product\": \"T-Shirt\", \"price\": 25, \"tags\": [\"clothing\", \"casual\"], \"stock\": {\"warehouse1\": 100, \"warehouse2\": 80}}");
        REQUIRE(pstmt->executeUpdate() == 1);

        pstmt->setInt(1, 5);
        pstmt->setString(2, "{\"product\": \"Book\", \"price\": 15, \"tags\": [\"media\", \"education\"], \"stock\": {\"warehouse1\": 50, \"warehouse2\": 40}}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Test JSON_SEARCH function
        auto rs = conn->executeQuery("SELECT id, JSON_SEARCH(data, 'one', 'electronics') as path FROM test_json_search");
        std::vector<int> electronicsIds;
        while (rs->next())
        {
            if (!rs->isNull("path"))
            {
                electronicsIds.push_back(rs->getInt("id"));
            }
        }
        REQUIRE(electronicsIds.size() == 3); // Should find 3 products with "electronics" tag

        // Test filtering with JSON_EXTRACT
        rs = conn->executeQuery("SELECT id, JSON_EXTRACT(data, '$.product') as product FROM test_json_search WHERE JSON_EXTRACT(data, '$.price') > 500 ORDER BY JSON_EXTRACT(data, '$.price') DESC");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 1);
        std::string product = rs->getString("product");
        REQUIRE((product == "\"Laptop\"" || product == "Laptop"));

        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 2);
        product = rs->getString("product");
        REQUIRE((product == "\"Smartphone\"" || product == "Smartphone"));

        // Test JSON_CONTAINS for array elements
        rs = conn->executeQuery("SELECT id FROM test_json_search WHERE JSON_CONTAINS(JSON_EXTRACT(data, '$.tags'), '\"clothing\"')");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 4);

        // Test complex JSON path expressions
        rs = conn->executeQuery("SELECT id, JSON_EXTRACT(data, '$.stock.warehouse1') as warehouse1_stock FROM test_json_search WHERE JSON_EXTRACT(data, '$.stock.warehouse1') > 20 ORDER BY JSON_EXTRACT(data, '$.stock.warehouse1') DESC");

        std::vector<std::pair<int, int>> warehouseStocks;
        while (rs->next())
        {
            int id = rs->getInt("id");
            std::string stockStr = rs->getString("warehouse1_stock");
            int stock = std::stoi(stockStr.find('\"') != std::string::npos ? stockStr.substr(1, stockStr.length() - 2) : stockStr);
            warehouseStocks.push_back({id, stock});
        }

        REQUIRE(warehouseStocks.size() == 3);
        REQUIRE(warehouseStocks[0].first == 4); // T-Shirt with 100 stock
        REQUIRE(warehouseStocks[0].second == 100);

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_search");
        conn->close();
    }

    SECTION("JSON aggregation and transformation")
    {
        // Register the MySQL driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with JSON column
        conn->executeUpdate("DROP TABLE IF EXISTS test_json_aggregation");
        conn->executeUpdate(
            "CREATE TABLE test_json_aggregation ("
            "id INT PRIMARY KEY, "
            "category VARCHAR(50), "
            "data JSON"
            ")");

        // Insert test data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_aggregation (id, category, data) VALUES (?, ?, ?)");

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

        // Test JSON_ARRAYAGG function (MySQL 5.7.22+)
        try
        {
            auto rs = conn->executeQuery(
                "SELECT category, JSON_ARRAYAGG(data) as items FROM test_json_aggregation GROUP BY category ORDER BY category");

            REQUIRE(rs->next());
            REQUIRE(rs->getString("category") == "books");
            std::string booksJson = rs->getString("items");
            REQUIRE(booksJson.find("Novel") != std::string::npos);

            REQUIRE(rs->next());
            REQUIRE(rs->getString("category") == "clothing");
            std::string clothingJson = rs->getString("items");
            REQUIRE(clothingJson.find("T-Shirt") != std::string::npos);
            REQUIRE(clothingJson.find("Jeans") != std::string::npos);

            REQUIRE(rs->next());
            REQUIRE(rs->getString("category") == "electronics");
            std::string electronicsJson = rs->getString("items");
            REQUIRE(electronicsJson.find("Laptop") != std::string::npos);
            REQUIRE(electronicsJson.find("Smartphone") != std::string::npos);
        }
        catch (const cpp_dbc::DBException &e)
        {
            // JSON_ARRAYAGG might not be available in older MySQL versions
            std::cout << "JSON_ARRAYAGG test skipped: " << e.what_s() << std::endl;
        }

        // Test JSON_OBJECT function
        auto rs = conn->executeQuery(
            "SELECT JSON_OBJECT('id', id, 'name', JSON_EXTRACT(data, '$.name'), 'price', JSON_EXTRACT(data, '$.price')) as product_json "
            "FROM test_json_aggregation WHERE id = 1");
        REQUIRE(rs->next());
        std::string productJson = rs->getString("product_json");
        REQUIRE((productJson.find("\"id\":1") != std::string::npos || productJson.find("\"id\": 1") != std::string::npos));
        REQUIRE(productJson.find("Laptop") != std::string::npos);
        REQUIRE(productJson.find("1200") != std::string::npos);

        // Test JSON_TABLE function (MySQL 8.0+)
        try
        {
            rs = conn->executeQuery(
                "SELECT jt.* FROM test_json_aggregation, "
                "JSON_TABLE(data, '$' COLUMNS("
                "  name VARCHAR(100) PATH '$.name', "
                "  price DECIMAL(10,2) PATH '$.price'"
                ")) AS jt "
                "WHERE category = 'electronics' ORDER BY price DESC");

            REQUIRE(rs->next());
            REQUIRE(rs->getString("name") == "Laptop");
            REQUIRE(std::abs(rs->getDouble("price") - 1200.0) < 0.001);

            REQUIRE(rs->next());
            REQUIRE(rs->getString("name") == "Smartphone");
            REQUIRE(std::abs(rs->getDouble("price") - 800.0) < 0.001);
        }
        catch (const cpp_dbc::DBException &e)
        {
            // JSON_TABLE might not be available in older MySQL versions
            std::cout << "JSON_TABLE test skipped: " << e.what_s() << std::endl;
        }

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_aggregation");
        conn->close();
    }

    SECTION("JSON validation and error handling")
    {
        // Register the MySQL driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with JSON column
        conn->executeUpdate("DROP TABLE IF EXISTS test_json_validation");
        conn->executeUpdate(
            "CREATE TABLE test_json_validation ("
            "id INT PRIMARY KEY, "
            "data JSON"
            ")");

        // Test valid JSON insertion
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_validation (id, data) VALUES (?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setString(2, "{\"valid\": true}");
        REQUIRE(pstmt->executeUpdate() == 1);

        // Test invalid JSON insertion (should throw an exception)
        pstmt->setInt(1, 2);
        pstmt->setString(2, "{invalid: json}"); // Missing quotes around key
        REQUIRE_THROWS_AS(pstmt->executeUpdate(), cpp_dbc::DBException);

        // Test JSON_VALID function
        auto rs = conn->executeQuery("SELECT JSON_VALID('{\"valid\": true}') as is_valid_1, JSON_VALID('{invalid: json}') as is_valid_2");
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("is_valid_1") == 1);
        REQUIRE(rs->getInt("is_valid_2") == 0);

        // Test JSON_TYPE function
        rs = conn->executeQuery("SELECT JSON_TYPE('{\"key\": \"value\"}') as type_object, JSON_TYPE('[1, 2, 3]') as type_array, JSON_TYPE('\"string\"') as type_string, JSON_TYPE('42') as type_number");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("type_object") == "OBJECT");
        REQUIRE(rs->getString("type_array") == "ARRAY");
        REQUIRE(rs->getString("type_string") == "STRING");
        REQUIRE((rs->getString("type_number") == "INTEGER" || rs->getString("type_number") == "NUMBER"));

        // Test error handling with JSON_EXTRACT on invalid path
        rs = conn->executeQuery("SELECT JSON_EXTRACT('{\"key\": \"value\"}', '$.nonexistent') as nonexistent");
        REQUIRE(rs->next());
        REQUIRE(rs->isNull("nonexistent"));

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_validation");
        conn->close();
    }

    SECTION("JSON performance with large datasets")
    {
        // Register the MySQL driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create a test table with JSON column and index
        conn->executeUpdate("DROP TABLE IF EXISTS test_json_performance");
        conn->executeUpdate(
            "CREATE TABLE test_json_performance ("
            "id INT PRIMARY KEY, "
            "data JSON, "
            "INDEX idx_price ((CAST(JSON_EXTRACT(data, '$.price') AS DECIMAL(10,2))))"
            ")");

        // Insert multiple records with random JSON data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_json_performance (id, data) VALUES (?, ?)");

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

        // Test index-based search
        auto startTime = std::chrono::steady_clock::now();
        auto rs = conn->executeQuery("SELECT id, JSON_EXTRACT(data, '$.price') as price FROM test_json_performance WHERE JSON_EXTRACT(data, '$.price') > 500 ORDER BY JSON_EXTRACT(data, '$.price')");
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << "JSON query execution time: " << duration << " ms" << std::endl;

        std::vector<int> highPriceIds;
        while (rs->next())
        {
            highPriceIds.push_back(rs->getInt("id"));
        }

        // Should find products with price > 500
        REQUIRE(!highPriceIds.empty());

        // Test JSON path query performance
        startTime = std::chrono::steady_clock::now();
        rs = conn->executeQuery("SELECT id, JSON_EXTRACT(data, '$.name') as name FROM test_json_performance LIMIT 10");

        // Clean up
        conn->executeUpdate("DROP TABLE test_json_performance");
        conn->close();
    }
}
#endif // USE_MYSQL