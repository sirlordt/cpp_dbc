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
 * @file 26_051_test_scylladb_real_json.cpp
 * @brief Tests for ScyllaDB JSON operations (storing JSON as text)
 */

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <map>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "26_001_test_scylladb_real_common.hpp"

#if USE_SCYLLADB
// Test case for ScyllaDB JSON operations
TEST_CASE("ScyllaDB JSON operations", "[26_051_01_scylladb_real_json]")
{
    // Skip these tests if we can't connect to ScyllaDB
    if (!scylla_test_helpers::canConnectToScylla())
    {
        SKIP("Cannot connect to ScyllaDB database");
        return;
    }

    // Get ScyllaDB configuration
    auto dbConfig = scylla_test_helpers::getScyllaConfig("dev_scylla");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string host = dbConfig.getHost();
    int port = dbConfig.getPort();
    std::string keyspace = dbConfig.getDatabase();
    std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;

    // Register the ScyllaDB driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

    // Get a connection
    auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
        cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
    REQUIRE(conn != nullptr);

    // Create test table with TEXT column to store JSON
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_json");
    conn->executeUpdate(
        "CREATE TABLE " + keyspace + ".test_json ("
                                     "id int PRIMARY KEY, "
                                     "json_data text"
                                     ")");

    SECTION("Basic JSON storage")
    {
        // Insert data using PreparedStatement
        auto stmt = conn->prepareStatement(
            "INSERT INTO " + keyspace + ".test_json (id, json_data) VALUES (?, ?)");

        // Simple JSON object
        stmt->setInt(1, 1);
        std::string json1 = "{\"name\": \"John\", \"age\": 30, \"city\": \"New York\"}";
        stmt->setString(2, json1);
        stmt->executeUpdate();

        // JSON array
        stmt->setInt(1, 2);
        std::string json2 = "[1, 2, 3, 4, 5]";
        stmt->setString(2, json2);
        stmt->executeUpdate();

        // Retrieve data
        auto rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_json WHERE id = 1");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("json_data") == json1);

        rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_json WHERE id = 2");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("json_data") == json2);
    }

    SECTION("Complex JSON operations")
    {
        // Insert data using PreparedStatement
        auto stmt = conn->prepareStatement(
            "INSERT INTO " + keyspace + ".test_json (id, json_data) VALUES (?, ?)");

        // Nested JSON object
        stmt->setInt(1, 3);
        std::string json3 = "{\"person\": {\"name\": \"Alice\", \"age\": 25, \"address\": {\"street\": \"123 Main St\", \"city\": \"Boston\", \"zip\": \"02108\"}}, \"active\": true}";
        stmt->setString(2, json3);
        stmt->executeUpdate();

        // JSON with array of objects
        stmt->setInt(1, 4);
        std::string json4 = "{\"employees\": [{\"name\": \"Bob\", \"age\": 30}, {\"name\": \"Carol\", \"age\": 35}, {\"name\": \"Dave\", \"age\": 40}]}";
        stmt->setString(2, json4);
        stmt->executeUpdate();

        // Retrieve and verify nested JSON
        auto rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_json WHERE id = 3");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("json_data") == json3);

        // Retrieve and verify JSON with array of objects
        rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_json WHERE id = 4");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("json_data") == json4);
    }

    SECTION("JSON update operations")
    {
        // Insert initial JSON
        auto stmt = conn->prepareStatement(
            "INSERT INTO " + keyspace + ".test_json (id, json_data) VALUES (?, ?)");

        stmt->setInt(1, 5);
        std::string initialJson = "{\"user\": {\"name\": \"John\", \"email\": \"john@example.com\"}}";
        stmt->setString(2, initialJson);
        stmt->executeUpdate();

        // Verify initial JSON
        auto rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_json WHERE id = 5");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("json_data") == initialJson);

        // Update JSON
        std::string updatedJson = "{\"user\": {\"name\": \"John\", \"email\": \"john@example.com\", \"phone\": \"555-1234\"}}";
        auto updateStmt = conn->prepareStatement(
            "UPDATE " + keyspace + ".test_json SET json_data = ? WHERE id = ?");
        updateStmt->setString(1, updatedJson);
        updateStmt->setInt(2, 5);
        updateStmt->executeUpdate();

        // Verify updated JSON
        rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_json WHERE id = 5");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("json_data") == updatedJson);
    }

    SECTION("Multiple JSON documents")
    {
        // Create a batch of JSON documents
        std::vector<std::string> jsonDocuments = {
            "{\"id\": \"doc1\", \"type\": \"article\", \"title\": \"Introduction to ScyllaDB\"}",
            "{\"id\": \"doc2\", \"type\": \"article\", \"title\": \"JSON in Databases\"}",
            "{\"id\": \"doc3\", \"type\": \"tutorial\", \"title\": \"Working with JSON\"}",
            "{\"id\": \"doc4\", \"type\": \"tutorial\", \"title\": \"Advanced ScyllaDB\"}",
            "{\"id\": \"doc5\", \"type\": \"reference\", \"title\": \"ScyllaDB API Reference\"}"};

        // Insert all documents
        auto stmt = conn->prepareStatement(
            "INSERT INTO " + keyspace + ".test_json (id, json_data) VALUES (?, ?)");

        for (size_t i = 0; i < jsonDocuments.size(); i++)
        {
            stmt->setInt(1, static_cast<int>(10 + i));
            stmt->setString(2, jsonDocuments[i]);
            stmt->executeUpdate();
        }

        // Retrieve all documents
        auto rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_json WHERE id IN (10, 11, 12, 13, 14)");

        // Create a map to track which documents we've found
        std::map<int, bool> foundDocuments;
        for (int i = 0; i < static_cast<int>(jsonDocuments.size()); i++)
        {
            foundDocuments[10 + i] = false;
        }

        size_t count = 0;
        while (rs->next() && count < jsonDocuments.size())
        {
            int id = rs->getInt("id");
            // Verify the ID is in our expected range
            REQUIRE(id >= 10);
            REQUIRE(id < 10 + static_cast<int>(jsonDocuments.size()));

            // Mark this document as found
            foundDocuments[id] = true;

            // Verify the JSON content matches what we expect
            int index = id - 10;
            REQUIRE(rs->getString("json_data") == jsonDocuments[index]);

            count++;
        }

        // Verify we found all documents
        REQUIRE(count == static_cast<size_t>(jsonDocuments.size()));
        for (const auto &pair : foundDocuments)
        {
            REQUIRE(pair.second == true);
        }
    }

    // Clean up
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_json");
    conn->close();
}
#else
TEST_CASE("ScyllaDB JSON operations (skipped)", "[26_051_02_scylladb_real_json]")
{
    SKIP("ScyllaDB support is not enabled");
}
#endif
