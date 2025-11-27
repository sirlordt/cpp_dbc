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

 @file test_db_config.cpp
 @brief Tests for configuration handling

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

#include "test_main.hpp"

// Test case to verify that the database configuration file can be loaded
TEST_CASE("Database configuration loading", "[db_config]")
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
        REQUIRE(configManager.getConnectionPoolConfig("default").has_value());
        REQUIRE(!configManager.getTestQueries().getQueriesForType("mysql").empty());

        // Verify that we have at least one database configuration
        REQUIRE(configManager.getAllDatabases().size() > 0);
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify database configurations
TEST_CASE("Database configurations", "[db_config]")
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
        REQUIRE(allDatabases.size() == 9); // 3 MySQL + 3 PostgreSQL + 3 SQLite

        // Check that each database has the required fields
        for (const auto &db : allDatabases)
        {
            std::string type = db.getType();

            // Common fields for all database types
            REQUIRE(!db.getName().empty());
            REQUIRE(!db.getType().empty());
            REQUIRE(!db.getDatabase().empty());

            // Host and port are only required for MySQL and PostgreSQL
            if (type == "mysql" || type == "postgresql")
            {
                REQUIRE(!db.getHost().empty());
                REQUIRE(db.getPort() > 0);
                REQUIRE(!db.getUsername().empty());
                REQUIRE(!db.getPassword().empty());
            }
        }
    }

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

// Test case to verify specific database configurations
TEST_CASE("Specific database configurations", "[db_config]")
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

    SECTION("Verify dev_postgresql configuration")
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

    SECTION("Verify non-existent database")
    {
        auto nonExistentDBOpt = configManager.getDatabaseByName("non_existent_db");

        // Check that the database was not found
        REQUIRE(!nonExistentDBOpt.has_value());
    }
#endif // defined(USE_CPP_YAML) && USE_CPP_YAML == 1
}

// Test case to verify specific SQLite database configurations
TEST_CASE("Specific SQLite database configurations", "[db_config]")
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

// Test case to verify connection pool configurations
TEST_CASE("Connection pool configurations", "[db_config]")
{
#if !defined(USE_CPP_YAML) || USE_CPP_YAML != 1
    SKIP("YAML support is disabled");
#else
    // Use the cpp_dbc::config::YamlConfigLoader to load the configuration
    cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(common_test_helpers::getConfigFilePath());

    SECTION("Verify default pool configuration")
    {
        auto defaultPoolOpt = configManager.getConnectionPoolConfig("default");

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
        auto highPerfPoolOpt = configManager.getConnectionPoolConfig("high_performance");

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

// Test case to verify test queries
TEST_CASE("Test queries", "[db_config]")
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

// Example of how to use the configuration to create connection strings
TEST_CASE("Create connection strings from configuration", "[db_config]")
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

        // In a real application, you would use these connection strings with DriverManager:
        // for (const auto& [dbName, connStr] : connectionStrings) {
        //     auto dbConfigOpt = configManager.getDatabaseByName(dbName);
        //     if (dbConfigOpt.has_value()) {
        //         const auto& dbConfig = dbConfigOpt->get();
        //         auto conn = cpp_dbc::DriverManager::getConnection(
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

// Test database configurations for different environments and types
TEST_CASE("Select MySQL database for dev environment", "[db_config]")
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

TEST_CASE("Select MySQL database for test environment", "[db_config]")
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

TEST_CASE("Select MySQL database for prod environment", "[db_config]")
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

TEST_CASE("Select PostgreSQL database for dev environment", "[db_config]")
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

TEST_CASE("Select PostgreSQL database for test environment", "[db_config]")
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

TEST_CASE("Select PostgreSQL database for prod environment", "[db_config]")
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

TEST_CASE("Select SQLite database for dev environment", "[db_config]")
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

TEST_CASE("Select SQLite database for test environment", "[db_config]")
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

TEST_CASE("Select SQLite database for prod environment", "[db_config]")
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