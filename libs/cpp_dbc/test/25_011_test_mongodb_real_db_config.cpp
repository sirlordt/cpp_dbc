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

 @file 25_011_test_mongodb_real_db_config.cpp
 @brief Tests for MongoDB database configuration handling

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

#if USE_MONGODB

// Test case to verify MongoDB database configurations
TEST_CASE("MongoDB database configurations", "[25_011_01_mongodb_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify MongoDB databases")
    {
        auto mongodbDatabases = configManager.getDatabasesByType("mongodb");

        // Check that we have the expected number of MongoDB databases
        REQUIRE(mongodbDatabases.size() == 2);

        // Check that all databases have the correct type
        for (const auto &db : mongodbDatabases)
        {
            REQUIRE(db.getType() == "mongodb");
        }

        // Check that we have the expected database names
        std::vector<std::string> dbNames;
        for (const auto &db : mongodbDatabases)
        {
            dbNames.push_back(db.getName());
        }

        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "dev_mongodb") != dbNames.end());
        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "test_mongodb") != dbNames.end());
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify specific MongoDB database configuration
TEST_CASE("Specific MongoDB database configuration", "[25_011_02_mongodb_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify dev_mongodb configuration")
    {
        auto devMongoDBOpt = configManager.getDatabaseByName("dev_mongodb");

        // Check that the database was found
        REQUIRE(devMongoDBOpt.has_value());

        // Get the reference to the database config
        const auto &devMongoDB = devMongoDBOpt->get();

        // Check connection parameters
        REQUIRE(devMongoDB.getType() == "mongodb");
        REQUIRE(!devMongoDB.getDatabase().empty());
    }

    SECTION("Verify test_mongodb configuration")
    {
        auto testMongoDBOpt = configManager.getDatabaseByName("test_mongodb");

        // Check that the database was found
        REQUIRE(testMongoDBOpt.has_value());

        // Get the reference to the database config
        const auto &testMongoDB = testMongoDBOpt->get();

        // Check connection parameters
        REQUIRE(testMongoDB.getType() == "mongodb");
        REQUIRE(!testMongoDB.getDatabase().empty());
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test database configurations for different environments
TEST_CASE("Select MongoDB database for dev environment", "[25_011_03_mongodb_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "dev_mongodb";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    // Check that the database has the correct type
    REQUIRE(dbConfig.getType() == "mongodb");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();

    // Verify the connection string format
    REQUIRE(connStr.find("cpp_dbc:mongodb://") == 0);
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

TEST_CASE("Select MongoDB database for test environment", "[25_011_04_mongodb_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "test_mongodb";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    REQUIRE(dbConfig.getType() == "mongodb");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();
    REQUIRE(connStr.find("cpp_dbc:mongodb://") == 0);
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

#endif // USE_MONGODB
