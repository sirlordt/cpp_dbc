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

 @file 21_041_test_postgresql_real_connection.cpp
 @brief Tests for PostgreSQL database operations with real connections

*/

#include <string>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "21_001_test_postgresql_real_common.hpp"

#if USE_POSTGRESQL
// Test case to verify PostgreSQL connection
TEST_CASE("PostgreSQL connection test", "[21_041_01_postgresql_real_connection]")
{
    if (!postgresql_test_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

    auto dbConfig = postgresql_test_helpers::getPostgreSQLConfig("dev_postgresql");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());

    SECTION("Test PostgreSQL connection")
    {
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to PostgreSQL with connection string: " + connStr);

        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Execute a simple query to verify the connection
        auto resultSet = conn->executeQuery("SELECT 1 as test_value");
        REQUIRE(resultSet->next());
        REQUIRE(resultSet->getInt("test_value") == 1);

        // Verify connection state and URI
        CHECK_FALSE(conn->isClosed());

        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Connection URI: " + conn->getURI());

        CHECK(conn->getURI() == connStr);

        // Close the connection
        conn->close();
        CHECK(conn->isClosed());
    }
}
TEST_CASE("PostgreSQL getServerVersion and getServerInfo", "[21_041_03_postgresql_real_connection]")
{
    if (!postgresql_test_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

    auto dbConfig = postgresql_test_helpers::getPostgreSQLConfig("dev_postgresql");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());

    SECTION("getServerVersion returns non-empty version string")
    {
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        auto version = conn->getServerVersion();
        CHECK_FALSE(version.empty());
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "PostgreSQL server version: " + version);

        conn->close();
    }

    SECTION("getServerInfo returns map with ServerVersion key")
    {
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        auto info = conn->getServerInfo();
        CHECK_FALSE(info.empty());
        CHECK(info.count("ServerVersion") == 1);
        CHECK_FALSE(info.at("ServerVersion").empty());

        for (const auto &[key, value] : info)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "  PostgreSQL ServerInfo [" + key + "] = " + value);
        }

        // Verify some PostgreSQL-specific keys
        CHECK(info.count("ProtocolVersion") == 1);
        CHECK(info.count("ServerEncoding") == 1);

        conn->close();
    }

    SECTION("getServerVersion matches ServerVersion in getServerInfo")
    {
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        auto version = conn->getServerVersion();
        auto info = conn->getServerInfo();

        CHECK(version == info.at("ServerVersion"));

        conn->close();
    }
}
TEST_CASE("PostgreSQL getDriverVersion", "[21_041_04_postgresql_real_connection]")
{
    auto driver = std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>();

    SECTION("getDriverVersion returns non-empty version string")
    {
        auto version = driver->getDriverVersion();
        CHECK_FALSE(version.empty());
        cpp_dbc::system_utils::logWithTimesMillis("TEST", "PostgreSQL driver version: " + version);
    }
}

#else
// Skip this test if PostgreSQL support is not enabled
TEST_CASE("PostgreSQL connection test (skipped)", "[21_041_02_postgresql_real_connection]")
{
    SKIP("PostgreSQL support is not enabled");
}
#endif
