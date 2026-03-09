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

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "26_001_test_scylladb_real_common.hpp"

#if USE_SCYLLADB
// Test case to verify ScyllaDB connection
TEST_CASE("ScyllaDB connection test", "[26_041_01_scylladb_real_connection]")
{
    // Get ScyllaDB configuration
    auto dbConfig = scylla_test_helpers::getScyllaConfig();

    // Extract connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    // Build connection string without keyspace
    std::string type = dbConfig.getType();
    std::string host = dbConfig.getHost();
    int port = dbConfig.getPort();
    std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port);

    // Register the ScyllaDB driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

    SECTION("Test ScyllaDB connection without keyspace")
    {
        try
        {
            // Attempt to connect to ScyllaDB
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to ScyllaDB with connection string: " + connStr);
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Username: " + username + ", Password: " + password);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
            REQUIRE(conn != nullptr);

            // Execute a simple query to verify the connection (system table always exists)
            auto resultSet = conn->executeQuery("SELECT release_version FROM system.local");
            REQUIRE(resultSet->next());
            REQUIRE_FALSE(resultSet->getString("release_version").empty());

            // Verify connection state and URL
            CHECK_FALSE(conn->isClosed());

            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Connection URL: " + conn->getURL());

            auto url = conn->getURL();
            REQUIRE_FALSE(url.empty());
            REQUIRE(url.find(type) != std::string::npos);

            CHECK(conn->getURL() == connStr);

            // Close the connection
            conn->close();
            CHECK(conn->isClosed());
        }
        catch (const cpp_dbc::DBException &ex)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "ScyllaDB connection error: " + std::string(ex.what_s()));
            WARN("ScyllaDB connection failed: " + std::string(ex.what_s()));
            WARN("This is expected if ScyllaDB is not installed or the server is unavailable");
            WARN("The test is still considered successful for CI purposes");
        }
    }

    SECTION("Test ScyllaDB connection with keyspace")
    {
        std::string keyspace = dbConfig.getDatabase();
        std::string connStrKeyspace = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;

        try
        {
            // Create keyspace if it doesn't exist
            {
                // Connect without keyspace first
                auto setupConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                    cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

                std::string createKeyspaceQuery = dbConfig.getOption("query__create_keyspace",
                                                                     "CREATE KEYSPACE IF NOT EXISTS " + keyspace + " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
                setupConn->executeUpdate(createKeyspaceQuery);
                setupConn->close();
            }

            // Now connect with keyspace
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to ScyllaDB with keyspace: " + connStrKeyspace);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStrKeyspace, username, password));
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
            CHECK(conn->isClosed());
        }
        catch (const cpp_dbc::DBException &ex)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "ScyllaDB connection with keyspace error: " + std::string(ex.what_s()));
            WARN("ScyllaDB connection with keyspace failed: " + std::string(ex.what_s()));
            WARN("This is expected if ScyllaDB is not installed or the keyspace cannot be created");
            WARN("The test is still considered successful for CI purposes");
        }
    }
}
TEST_CASE("ScyllaDB getServerVersion and getServerInfo", "[26_041_03_scylladb_real_connection]")
{
    auto dbConfig = scylla_test_helpers::getScyllaConfig();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

    SECTION("getServerVersion returns non-empty version string")
    {
        try
        {
            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
            REQUIRE(conn != nullptr);

            auto version = conn->getServerVersion();
            CHECK_FALSE(version.empty());
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "ScyllaDB server version: " + version);

            conn->close();
        }
        catch (const cpp_dbc::DBException &ex)
        {
            WARN("ScyllaDB getServerVersion test skipped: " + std::string(ex.what_s()));
        }
    }

    SECTION("getServerInfo returns map with ServerVersion key")
    {
        try
        {
            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
            REQUIRE(conn != nullptr);

            auto info = conn->getServerInfo();
            CHECK_FALSE(info.empty());
            CHECK(info.count("ServerVersion") == 1);
            CHECK_FALSE(info.at("ServerVersion").empty());

            for (const auto &[key, value] : info)
            {
                cpp_dbc::system_utils::logWithTimesMillis("TEST", "  ScyllaDB ServerInfo [" + key + "] = " + value);
            }

            // Verify some ScyllaDB-specific keys
            CHECK(info.count("ClusterName") == 1);
            CHECK(info.count("DataCenter") == 1);

            conn->close();
        }
        catch (const cpp_dbc::DBException &ex)
        {
            WARN("ScyllaDB getServerInfo test skipped: " + std::string(ex.what_s()));
        }
    }

    SECTION("getServerVersion matches ServerVersion in getServerInfo")
    {
        try
        {
            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
            REQUIRE(conn != nullptr);

            auto version = conn->getServerVersion();
            auto info = conn->getServerInfo();

            CHECK(version == info.at("ServerVersion"));

            conn->close();
        }
        catch (const cpp_dbc::DBException &ex)
        {
            WARN("ScyllaDB version consistency test skipped: " + std::string(ex.what_s()));
        }
    }
}
TEST_CASE("ScyllaDB getDriverVersion", "[26_041_04_scylladb_real_connection]")
{
    auto driver = std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>();

    SECTION("getDriverVersion returns non-empty version string")
    {
        auto version = driver->getDriverVersion();
        CHECK_FALSE(version.empty());
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "ScyllaDB driver version: " + version);
    }
}

#else
// Skip this test if ScyllaDB support is not enabled
TEST_CASE("ScyllaDB connection test (skipped)", "[26_041_02_scylladb_real_connection]")
{
    SKIP("ScyllaDB support is not enabled");
}
#endif
