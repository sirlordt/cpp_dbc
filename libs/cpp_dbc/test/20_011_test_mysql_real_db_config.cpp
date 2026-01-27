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

 @file 20_011_test_mysql_real_db_config.cpp
 @brief Tests for MySQL database configuration handling

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

#if USE_MYSQL

// Test case to verify MySQL database configurations
TEST_CASE("MySQL database configurations", "[20_011_01_mysql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify MySQL databases")
    {
        auto mysqlDatabases = configManager.getDatabasesByType("mysql");

        // Check that we have the expected number of MySQL databases
        REQUIRE(mysqlDatabases.size() == 3);

        // Check that all databases have the correct type
        for (const auto &db : mysqlDatabases)
        {
            REQUIRE(db.getType() == "mysql");
        }

        // Check that we have the expected database names
        std::vector<std::string> dbNames;
        for (const auto &db : mysqlDatabases)
        {
            dbNames.push_back(db.getName());
        }

        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "dev_mysql") != dbNames.end());
        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "test_mysql") != dbNames.end());
        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "prod_mysql") != dbNames.end());
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify specific MySQL database configuration
TEST_CASE("Specific MySQL database configuration", "[20_011_02_mysql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify dev_mysql configuration")
    {
        auto devMySQLOpt = configManager.getDatabaseByName("dev_mysql");

        // Check that the database was found
        REQUIRE(devMySQLOpt.has_value());

        // Get the reference to the database config
        const auto &devMySQL = devMySQLOpt->get();

        // Check connection parameters
        REQUIRE(devMySQL.getType() == "mysql");
        REQUIRE(devMySQL.getHost() == "localhost");
        REQUIRE(devMySQL.getPort() == 3306);
        REQUIRE(devMySQL.getDatabase() == "Test01DB");
        REQUIRE(devMySQL.getUsername() == "root");
        REQUIRE(devMySQL.getPassword() == "dsystems");

        // Check options
        REQUIRE(devMySQL.getOption("connect_timeout") == "5");
        REQUIRE(devMySQL.getOption("read_timeout") == "10");
        REQUIRE(devMySQL.getOption("write_timeout") == "10");
        REQUIRE(devMySQL.getOption("charset") == "utf8mb4");
        REQUIRE(devMySQL.getOption("auto_reconnect") == "true");
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify MySQL test queries
TEST_CASE("MySQL test queries", "[20_011_03_mysql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify MySQL test queries")
    {
        const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();
        auto mysqlQueries = testQueries.getQueriesForType("mysql");

        // Check that all expected queries exist
        REQUIRE(mysqlQueries["create_table"].find("CREATE TABLE") != std::string::npos);
        REQUIRE(mysqlQueries["insert_data"].find("INSERT INTO") != std::string::npos);
        REQUIRE(mysqlQueries["select_data"].find("SELECT") != std::string::npos);
        REQUIRE(mysqlQueries["drop_table"].find("DROP TABLE") != std::string::npos);

        // Check MySQL parameter style (? placeholders)
        REQUIRE(mysqlQueries["insert_data"].find("?") != std::string::npos);
        REQUIRE(mysqlQueries["select_data"].find("?") != std::string::npos);
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test database configurations for different environments
TEST_CASE("Select MySQL database for dev environment", "[20_011_04_mysql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "dev_mysql";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    // Check that the database has the correct type
    REQUIRE(dbConfig.getType() == "mysql");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();

    // Verify the connection string format
    REQUIRE(connStr.find("cpp_dbc:mysql://") == 0);

    // Verify that we can access the credentials
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    REQUIRE(username == "root");
    REQUIRE(password == "dsystems");
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

TEST_CASE("Select MySQL database for test environment", "[20_011_05_mysql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "test_mysql";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    REQUIRE(dbConfig.getType() == "mysql");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();
    REQUIRE(connStr.find("cpp_dbc:mysql://") == 0);
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

TEST_CASE("Select MySQL database for prod environment", "[20_011_06_mysql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "prod_mysql";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    REQUIRE(dbConfig.getType() == "mysql");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();
    REQUIRE(connStr.find("cpp_dbc:mysql://") == 0);
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

#endif // USE_MYSQL
