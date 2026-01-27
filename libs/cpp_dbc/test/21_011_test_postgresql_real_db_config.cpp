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

 @file 21_011_test_postgresql_real_db_config.cpp
 @brief Tests for PostgreSQL database configuration handling

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

#if USE_POSTGRESQL

// Test case to verify PostgreSQL database configurations
TEST_CASE("PostgreSQL database configurations", "[21_011_01_postgresql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify PostgreSQL databases")
    {
        auto postgresqlDatabases = configManager.getDatabasesByType("postgresql");

        // Check that we have the expected number of PostgreSQL databases
        REQUIRE(postgresqlDatabases.size() == 3);

        // Check that all databases have the correct type
        for (const auto &db : postgresqlDatabases)
        {
            REQUIRE(db.getType() == "postgresql");
        }

        // Check that we have the expected database names
        std::vector<std::string> dbNames;
        for (const auto &db : postgresqlDatabases)
        {
            dbNames.push_back(db.getName());
        }

        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "dev_postgresql") != dbNames.end());
        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "test_postgresql") != dbNames.end());
        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "prod_postgresql") != dbNames.end());
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify specific PostgreSQL database configuration
TEST_CASE("Specific PostgreSQL database configuration", "[21_011_02_postgresql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify prod_postgresql configuration")
    {
        auto prodPostgreSQLOpt = configManager.getDatabaseByName("prod_postgresql");

        // Check that the database was found
        REQUIRE(prodPostgreSQLOpt.has_value());

        // Get the reference to the database config
        const auto &prodPostgreSQL = prodPostgreSQLOpt->get();

        // Check connection parameters
        REQUIRE(prodPostgreSQL.getType() == "postgresql");
        REQUIRE(prodPostgreSQL.getHost() == "db.example.com");
        REQUIRE(prodPostgreSQL.getPort() == 5432);
        REQUIRE(prodPostgreSQL.getDatabase() == "Test01DB");
        REQUIRE(prodPostgreSQL.getUsername() == "root");
        REQUIRE(prodPostgreSQL.getPassword() == "dsystems");

        // Check options
        REQUIRE(prodPostgreSQL.getOption("connect_timeout") == "10");
        REQUIRE(prodPostgreSQL.getOption("application_name") == "cpp_dbc_prod");
        REQUIRE(prodPostgreSQL.getOption("client_encoding") == "UTF8");
        REQUIRE(prodPostgreSQL.getOption("sslmode") == "require");
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify PostgreSQL test queries
TEST_CASE("PostgreSQL test queries", "[21_011_03_postgresql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify PostgreSQL test queries")
    {
        const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();
        auto pgQueries = testQueries.getQueriesForType("postgresql");

        // Check that all expected queries exist
        REQUIRE(pgQueries["create_table"].find("CREATE TABLE") != std::string::npos);
        REQUIRE(pgQueries["insert_data"].find("INSERT INTO") != std::string::npos);
        REQUIRE(pgQueries["select_data"].find("SELECT") != std::string::npos);
        REQUIRE(pgQueries["drop_table"].find("DROP TABLE") != std::string::npos);

        // Check PostgreSQL parameter style ($n placeholders)
        REQUIRE(pgQueries["insert_data"].find("$1") != std::string::npos);
        REQUIRE(pgQueries["select_data"].find("$1") != std::string::npos);
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test database configurations for different environments
TEST_CASE("Select PostgreSQL database for dev environment", "[21_011_04_postgresql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "dev_postgresql";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    REQUIRE(dbConfig.getType() == "postgresql");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();
    REQUIRE(connStr.find("cpp_dbc:postgresql://") == 0);
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

TEST_CASE("Select PostgreSQL database for test environment", "[21_011_05_postgresql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "test_postgresql";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    REQUIRE(dbConfig.getType() == "postgresql");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();
    REQUIRE(connStr.find("cpp_dbc:postgresql://") == 0);
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

TEST_CASE("Select PostgreSQL database for prod environment", "[21_011_06_postgresql_real_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());
    std::string dbName = "prod_postgresql";
    auto dbConfigOpt = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfigOpt.has_value());

    // Get the reference to the database config
    const auto &dbConfig = dbConfigOpt->get();

    REQUIRE(dbConfig.getType() == "postgresql");

    // Create connection string
    std::string connStr = dbConfig.createConnectionString();
    REQUIRE(connStr.find("cpp_dbc:postgresql://") == 0);
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

#endif // USE_POSTGRESQL
