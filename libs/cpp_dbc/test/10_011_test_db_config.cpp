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

 @file test_db_config_common.cpp
 @brief Common tests for database configuration handling

*/

#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "10_000_test_main.hpp"

// Test case to verify that the database configuration file can be loaded
TEST_CASE("Database configuration loading", "[10_011_01_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    SECTION("Load database configuration file")
    {
        // Get the path to the configuration file
        std::string config_path = common_test_helpers::getConfigFilePath();

        // Check if the file exists
        std::ifstream file(config_path);
        REQUIRE(file.good());
        file.close();

        // Load the YAML file using YamlConfigLoader
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Verify that the configuration contains the expected sections
        REQUIRE(configManager.getAllDatabases().size() > 0);
        REQUIRE(configManager.getDBConnectionPoolConfig("default").has_value());
        REQUIRE(!configManager.getTestQueries().getQueriesForType("mysql").empty());

        // Verify that we have at least one database configuration
        REQUIRE(configManager.getAllDatabases().size() > 0);
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify all database configurations
TEST_CASE("Database configurations - verify all", "[10_011_02_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify all databases")
    {
        const auto &allDatabases = configManager.getAllDatabases();

        // Check that we have the expected number of databases
        REQUIRE(allDatabases.size() == 17); // 3 MySQL + 3 PostgreSQL + 3 SQLite + 2 Firebird + 2 MongoDB + 2 Redis + 2 ScyllaDB

        // Define sets for different engine categories
        const std::set<std::string> networkEngines = {"mysql", "postgresql", "firebird", "scylladb", "mongodb", "redis"};
        const std::set<std::string> credentialEngines = {"mysql", "postgresql", "firebird", "scylladb"};

        // Check that each database has the required fields
        for (const auto &db : allDatabases)
        {
            std::string type = db.getType();

            // Common fields for all database types
            REQUIRE(!db.getName().empty());
            REQUIRE(!db.getType().empty());
            REQUIRE(!db.getDatabase().empty());

            // Host and port are required for network-based engines
            if (networkEngines.count(type) > 0)
            {
                REQUIRE(!db.getHost().empty());
                REQUIRE(db.getPort() > 0);
            }

            // Username and password are required for engines that need credentials
            if (credentialEngines.count(type) > 0)
            {
                REQUIRE(!db.getUsername().empty());
                REQUIRE(!db.getPassword().empty());
            }
        }
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify non-existent database
TEST_CASE("Non-existent database configuration", "[10_011_03_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify non-existent database")
    {
        auto nonExistentDBOpt = configManager.getDatabaseByName("non_existent_db");

        // Check that the database was not found
        REQUIRE(!nonExistentDBOpt.has_value());
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify connection pool configurations
TEST_CASE("Connection pool configurations", "[10_011_04_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify default pool configuration")
    {
        auto defaultPoolOpt = configManager.getDBConnectionPoolConfig("default");

        // Check that the pool was found
        REQUIRE(defaultPoolOpt.has_value());

        // Get the reference to the pool config
        const auto &defaultPool = defaultPoolOpt->get();

        // Check pool parameters
        REQUIRE(defaultPool.getInitialSize() == 5);
        REQUIRE(defaultPool.getMaxSize() == 10);
        REQUIRE(defaultPool.getConnectionTimeout() == 5000);
        REQUIRE(defaultPool.getIdleTimeout() == 60000);
        REQUIRE(defaultPool.getValidationInterval() == 30000);
    }

    SECTION("Verify high_performance pool configuration")
    {
        auto highPerfPoolOpt = configManager.getDBConnectionPoolConfig("high_performance");

        // Check that the pool was found
        REQUIRE(highPerfPoolOpt.has_value());

        // Get the reference to the pool config
        const auto &highPerfPool = highPerfPoolOpt->get();

        // Check pool parameters
        REQUIRE(highPerfPool.getInitialSize() == 10);
        REQUIRE(highPerfPool.getMaxSize() == 50);
        REQUIRE(highPerfPool.getConnectionTimeout() == 3000);
        REQUIRE(highPerfPool.getIdleTimeout() == 30000);
        REQUIRE(highPerfPool.getValidationInterval() == 15000);
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify common test query
TEST_CASE("Common test queries", "[10_011_05_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify common test query")
    {
        const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();

        // Check that the connection test query exists
        REQUIRE(testQueries.getConnectionTest() == "SELECT 1");
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Example of how to use the configuration to create connection strings
TEST_CASE("Create connection strings from configuration", "[10_011_06_db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Create connection strings for all databases")
    {
        const auto &allDatabases = configManager.getAllDatabases();

        // Create a map to store connection strings for each database
        std::map<std::string, std::string> connectionStrings;

        // Iterate through all database configurations
        for (const auto &db : allDatabases)
        {
            std::string dbName = db.getName();

            // Create connection string using the db's createConnectionString method
            connectionStrings[dbName] = db.createConnectionString();
        }

        // Verify connection strings for MySQL databases
        REQUIRE(connectionStrings["dev_mysql"] == "cpp_dbc:mysql://localhost:3306/Test01DB");
        REQUIRE(connectionStrings["test_mysql"] == "cpp_dbc:mysql://localhost:3306/Test01DB");
        REQUIRE(connectionStrings["prod_mysql"] == "cpp_dbc:mysql://db.example.com:3306/Test01DB");

        // Verify connection strings for PostgreSQL databases
        REQUIRE(connectionStrings["dev_postgresql"] == "cpp_dbc:postgresql://localhost:5432/Test01DB");
        REQUIRE(connectionStrings["test_postgresql"] == "cpp_dbc:postgresql://localhost:5432/Test01DB");
        REQUIRE(connectionStrings["prod_postgresql"] == "cpp_dbc:postgresql://db.example.com:5432/Test01DB");

        // Verify connection strings for SQLite databases
        // SQLite connection strings are different as they don't have host/port
        REQUIRE(connectionStrings["dev_sqlite"] == "cpp_dbc:sqlite://:memory:");
        REQUIRE(connectionStrings["test_sqlite"] == "cpp_dbc:sqlite://test_sqlite.db");
        REQUIRE(connectionStrings["prod_sqlite"] == "cpp_dbc:sqlite:///path/to/production.db");

        // Verify connection strings for ScyllaDB databases
        REQUIRE(connectionStrings["dev_scylla"] == "cpp_dbc:scylladb://localhost:9042/dev_keyspace");

        // In a real application, you would use these connection strings with DriverManager:
        // for (const auto& [dbName, connStr] : connectionStrings) {
        //     auto dbConfigOpt = configManager.getDatabaseByName(dbName);
        //     if (dbConfigOpt.has_value()) {
        //         const auto& dbConfig = dbConfigOpt->get();
        //         auto conn = cpp_dbc::DriverManager::getDBConnection(
        //             connStr,
        //             dbConfig.getUsername(),
        //             dbConfig.getPassword()
        //         );
        //         // Use the connection...
        //     }
        // }
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}
