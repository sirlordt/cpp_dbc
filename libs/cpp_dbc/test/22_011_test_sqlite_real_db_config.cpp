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

 @file 22_011_test_sqlite_real_db_config.cpp
 @brief Tests for SQLite database configuration handling

*/

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "10_000_test_main.hpp"

#if USE_SQLITE

// Test case to verify SQLite database configurations
TEST_CASE("SQLite database configurations", "[22_011_01_sqlite_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify SQLite databases")
    {
        auto sqliteDatabases = configManager.getDatabasesByType("sqlite");

        // Check that we have the expected number of SQLite databases
        REQUIRE(sqliteDatabases.size() == 3);

        // Check that all databases have the correct type
        for (const auto &db : sqliteDatabases)
        {
            REQUIRE(db.getType() == "sqlite");
        }

        // Check that we have the expected database names
        std::vector<std::string> dbNames;
        for (const auto &db : sqliteDatabases)
        {
            dbNames.push_back(db.getName());
        }

        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "dev_sqlite") != dbNames.end());
        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "test_sqlite") != dbNames.end());
        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "prod_sqlite") != dbNames.end());
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify specific SQLite database configurations
TEST_CASE("Specific SQLite database configurations", "[22_011_02_sqlite_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify dev_sqlite configuration")
    {
        auto devSQLiteOpt = configManager.getDatabaseByName("dev_sqlite");

        // Check that the database was found
        REQUIRE(devSQLiteOpt.has_value());

        // Get the reference to the database config
        const auto &devSQLite = devSQLiteOpt->get();

        // Check connection parameters
        REQUIRE(devSQLite.getType() == "sqlite");
        REQUIRE(devSQLite.getDatabase() == ":memory:");

        // Check options
        REQUIRE(devSQLite.getOption("foreign_keys") == "true");
        REQUIRE(devSQLite.getOption("journal_mode") == "WAL");
    }

    SECTION("Verify test_sqlite configuration")
    {
        auto testSQLiteOpt = configManager.getDatabaseByName("test_sqlite");

        // Check that the database was found
        REQUIRE(testSQLiteOpt.has_value());

        // Get the reference to the database config
        const auto &testSQLite = testSQLiteOpt->get();

        // Check connection parameters
        REQUIRE(testSQLite.getType() == "sqlite");
        REQUIRE(testSQLite.getDatabase() == "test_sqlite.db");

        // Check options
        REQUIRE(testSQLite.getOption("foreign_keys") == "true");
        REQUIRE(testSQLite.getOption("journal_mode") == "WAL");
    }

    SECTION("Verify prod_sqlite configuration")
    {
        auto prodSQLiteOpt = configManager.getDatabaseByName("prod_sqlite");

        // Check that the database was found
        REQUIRE(prodSQLiteOpt.has_value());

        // Get the reference to the database config
        const auto &prodSQLite = prodSQLiteOpt->get();

        // Check connection parameters
        REQUIRE(prodSQLite.getType() == "sqlite");
        REQUIRE(prodSQLite.getDatabase() == "/path/to/production.db");

        // Check options
        REQUIRE(prodSQLite.getOption("foreign_keys") == "true");
        REQUIRE(prodSQLite.getOption("journal_mode") == "WAL");
        REQUIRE(prodSQLite.getOption("synchronous") == "FULL");
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify SQLite test queries
TEST_CASE("SQLite test queries", "[22_011_03_sqlite_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify SQLite test queries")
    {
        const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();
        auto sqliteQueries = testQueries.getQueriesForType("sqlite");

        // Check that all expected queries exist
        REQUIRE(sqliteQueries["create_table"].find("CREATE TABLE") != std::string::npos);
        REQUIRE(sqliteQueries["insert_data"].find("INSERT INTO") != std::string::npos);
        REQUIRE(sqliteQueries["select_data"].find("SELECT") != std::string::npos);
        REQUIRE(sqliteQueries["drop_table"].find("DROP TABLE") != std::string::npos);

        // Check SQLite parameter style (? placeholders)
        REQUIRE(sqliteQueries["insert_data"].find("?") != std::string::npos);
        REQUIRE(sqliteQueries["select_data"].find("?") != std::string::npos);
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test database configurations for different environments
TEST_CASE("Select SQLite database for dev environment", "[22_011_04_sqlite_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "dev_sqlite";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    REQUIRE(dbConfig.getType() == "sqlite");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();
    REQUIRE(connStr == "cpp_dbc:sqlite://:memory:");
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

TEST_CASE("Select SQLite database for test environment", "[22_011_05_sqlite_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "test_sqlite";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    REQUIRE(dbConfig.getType() == "sqlite");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();
    REQUIRE(connStr == "cpp_dbc:sqlite://test_sqlite.db");
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

TEST_CASE("Select SQLite database for prod environment", "[22_011_06_sqlite_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "prod_sqlite";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    REQUIRE(dbConfig.getType() == "sqlite");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();
    REQUIRE(connStr == "cpp_dbc:sqlite:///path/to/production.db");
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

#endif // USE_SQLITE
