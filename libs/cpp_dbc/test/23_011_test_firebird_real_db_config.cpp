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

 @file test_firebird_db_config.cpp
 @brief Tests for Firebird database configuration handling

*/

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <filesystem>
#include <unistd.h>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "10_000_test_main.hpp"

#if USE_FIREBIRD

// Test case to verify Firebird database configurations
TEST_CASE("Firebird database configurations", "[23_011_01_firebird_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify Firebird databases")
    {
        auto firebirdDatabases = configManager.getDatabasesByType("firebird");

        // Check that we have the expected number of Firebird databases
        REQUIRE(firebirdDatabases.size() == 2);

        // Check that all databases have the correct type
        for (const auto &db : firebirdDatabases)
        {
            REQUIRE(db.getType() == "firebird");
        }

        // Check that we have the expected database names
        std::vector<std::string> dbNames;
        for (const auto &db : firebirdDatabases)
        {
            dbNames.push_back(db.getName());
        }

        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "dev_firebird") != dbNames.end());
        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "test_firebird") != dbNames.end());
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify specific Firebird database configuration
TEST_CASE("Specific Firebird database configuration", "[23_011_02_firebird_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify dev_firebird configuration")
    {
        auto devFirebirdOpt = configManager.getDatabaseByName("dev_firebird");

        // Check that the database was found
        REQUIRE(devFirebirdOpt.has_value());

        // Get the reference to the database config
        const auto &devFirebird = devFirebirdOpt->get();

        // Check connection parameters
        REQUIRE(devFirebird.getType() == "firebird");
        REQUIRE(!devFirebird.getHost().empty());
        REQUIRE(devFirebird.getPort() > 0);
        REQUIRE(!devFirebird.getDatabase().empty());
        REQUIRE(!devFirebird.getUsername().empty());
        REQUIRE(!devFirebird.getPassword().empty());
    }

    SECTION("Verify test_firebird configuration")
    {
        auto testFirebirdOpt = configManager.getDatabaseByName("test_firebird");

        // Check that the database was found
        REQUIRE(testFirebirdOpt.has_value());

        // Get the reference to the database config
        const auto &testFirebird = testFirebirdOpt->get();

        // Check connection parameters
        REQUIRE(testFirebird.getType() == "firebird");
        REQUIRE(!testFirebird.getHost().empty());
        REQUIRE(testFirebird.getPort() > 0);
        REQUIRE(!testFirebird.getDatabase().empty());
        REQUIRE(!testFirebird.getUsername().empty());
        REQUIRE(!testFirebird.getPassword().empty());
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test database configurations for different environments
TEST_CASE("Select Firebird database for dev environment", "[23_011_03_firebird_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "dev_firebird";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    // Check that the database has the correct type
    REQUIRE(dbConfig.getType() == "firebird");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();

    // Verify the connection string format
    REQUIRE(connStr.find("cpp_dbc:firebird://") == 0);
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

TEST_CASE("Select Firebird database for test environment", "[23_011_04_firebird_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "test_firebird";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    REQUIRE(dbConfig.getType() == "firebird");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();
    REQUIRE(connStr.find("cpp_dbc:firebird://") == 0);
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

#endif // USE_FIREBIRD
