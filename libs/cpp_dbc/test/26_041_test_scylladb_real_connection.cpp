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
 * @file 26_041_test_scylladb_real_connection.cpp
 * @brief Tests for ScyllaDB database operations with real connections
 */

#include <string>
#include <fstream>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>

#include "26_001_test_scylladb_real_common.hpp"

// Test case to verify ScyllaDB connection
TEST_CASE("ScyllaDB connection test", "[26_041_01_scylladb_real_connection]")
{
#if USE_SCYLLADB
    // Skip this test if ScyllaDB support is not enabled
    SECTION("Test ScyllaDB connection")
    {
        // Get ScyllaDB configuration
        auto dbConfig = scylla_test_helpers::getScyllaConfig();

        // Extract connection parameters
        std::string username = dbConfig.getUsername();
        std::string password = dbConfig.getPassword();

        // Create connection string
        std::string type = dbConfig.getType();
        std::string host = dbConfig.getHost();
        int port = dbConfig.getPort();
        // Connect to cluster first, without keyspace
        std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port);

        // Register the ScyllaDB driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

        try
        {
            // Attempt to connect to ScyllaDB
            std::cout << "Attempting to connect to ScyllaDB with connection string: " << connStr << std::endl;
            std::cout << "Username: " << username << ", Password: " << password << std::endl;

            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            REQUIRE(conn != nullptr);

            // Execute a simple query to verify the connection (using system table which always exists)
            auto resultSet = conn->executeQuery("SELECT release_version FROM system.local");
            REQUIRE(resultSet->next());
            // Verify we got something back
            REQUIRE_FALSE(resultSet->getString("release_version").empty());

            // Close the connection
            conn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::string errorMsg = e.what_s();
            std::cout << "ScyllaDB connection error: " << errorMsg << std::endl;

            // If connection fails, fail the test
            FAIL("Failed to connect to ScyllaDB: " + errorMsg);
        }
    }

    SECTION("Test connection with keyspace")
    {
        // Get ScyllaDB configuration
        auto dbConfig = scylla_test_helpers::getScyllaConfig();

        // Extract connection parameters
        std::string username = dbConfig.getUsername();
        std::string password = dbConfig.getPassword();
        std::string host = dbConfig.getHost();
        int port = dbConfig.getPort();
        std::string keyspace = dbConfig.getDatabase();

        // Create connection string with keyspace
        std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;

        // Register the ScyllaDB driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

        try
        {
            // Create keyspace if it doesn't exist
            {
                // Connect without keyspace first
                std::string baseConnStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port);
                auto setupConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                    cpp_dbc::DriverManager::getDBConnection(baseConnStr, username, password));

                // Create keyspace if not exists
                std::string createKeyspaceQuery = dbConfig.getOption("query__create_keyspace",
                                                                     "CREATE KEYSPACE IF NOT EXISTS " + keyspace + " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
                setupConn->executeUpdate(createKeyspaceQuery);
                setupConn->close();
            }

            // Now connect with keyspace
            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            REQUIRE(conn != nullptr);

            // Create a test table
            conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".connection_test");
            conn->executeUpdate(
                "CREATE TABLE " + keyspace + ".connection_test ("
                                             "id int PRIMARY KEY, "
                                             "name text"
                                             ")");

            // Insert a test row
            auto stmt = conn->prepareStatement(
                "INSERT INTO " + keyspace + ".connection_test (id, name) VALUES (?, ?)");
            stmt->setInt(1, 1);
            stmt->setString(2, "Connection Test");
            auto result = stmt->executeUpdate();
            REQUIRE(result == 1);

            // Retrieve the test row
            auto rs = conn->executeQuery("SELECT * FROM " + keyspace + ".connection_test WHERE id = 1");
            REQUIRE(rs->next());
            REQUIRE(rs->getInt("id") == 1);
            REQUIRE(rs->getString("name") == "Connection Test");

            // Clean up
            conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".connection_test");
            conn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::string errorMsg = e.what_s();
            std::cout << "ScyllaDB connection with keyspace error: " << errorMsg << std::endl;
            FAIL("Failed to connect to ScyllaDB with keyspace: " + errorMsg);
        }
    }

    SECTION("Test connection properties")
    {
        // Get ScyllaDB configuration
        auto dbConfig = scylla_test_helpers::getScyllaConfig();

        // Extract connection parameters
        std::string username = dbConfig.getUsername();
        std::string password = dbConfig.getPassword();
        std::string host = dbConfig.getHost();
        int port = dbConfig.getPort();

        // Create connection string
        std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port);

        // Register the ScyllaDB driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

        try
        {
            // Connect to ScyllaDB
            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            REQUIRE(conn != nullptr);

            // Test connection properties
            REQUIRE_FALSE(conn->isClosed());

            // Test metadata
            auto url = conn->getURL();
            REQUIRE_FALSE(url.empty());
            REQUIRE(url.find("scylladb") != std::string::npos);

            // Close the connection
            conn->close();
            REQUIRE(conn->isClosed());
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::string errorMsg = e.what_s();
            std::cout << "ScyllaDB connection properties error: " << errorMsg << std::endl;
            FAIL("Failed to test ScyllaDB connection properties: " + errorMsg);
        }
    }
#else
    // Skip this test if ScyllaDB support is not enabled
    SKIP("ScyllaDB support is not enabled");
#endif
}
