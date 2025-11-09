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

#include <catch2/catch_test_macros.hpp>
#include <yaml-cpp/yaml.h>
#include <cpp_dbc/cpp_dbc.hpp>
#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <filesystem>
#include <unistd.h>

#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>

std::string getConfigFilePath();

// Helper function to create a connection string based on database type
std::string createConnectionString(const YAML::Node &dbConfig)
{
    std::string type = dbConfig["type"].as<std::string>();
    std::string database = dbConfig["database"].as<std::string>();

    // SQLite connection strings are different - they don't have host/port
    if (type == "sqlite")
    {
        return "cpp_dbc:" + type + "://" + database;
    }
    else
    {
        // MySQL and PostgreSQL connection strings
        std::string host = dbConfig["host"].as<std::string>();
        int port = dbConfig["port"].as<int>();
        return "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;
    }
}

// Helper class to manage database configurations
class DatabaseConfigManager
{
private:
    YAML::Node config;

public:
    DatabaseConfigManager(const std::string &configPath)
    {
        config = YAML::LoadFile(configPath);
    }

    // Get all database configurations
    std::vector<YAML::Node> getAllDatabases() const
    {
        std::vector<YAML::Node> result;
        for (size_t i = 0; i < config["databases"].size(); i++)
        {
            YAML::Node db = config["databases"][i];
            result.push_back(YAML::Node(db));
        }
        return result;
    }

    // Get databases of a specific type
    std::vector<YAML::Node> getDatabasesByType(const std::string &type) const
    {
        std::vector<YAML::Node> result;
        for (size_t i = 0; i < config["databases"].size(); i++)
        {
            YAML::Node db = config["databases"][i];
            if (db["type"].as<std::string>() == type)
            {
                result.push_back(YAML::Node(db));
            }
        }
        return result;
    }

    // Get a database by name
    YAML::Node getDatabaseByName(const std::string &name) const
    {
        for (size_t i = 0; i < config["databases"].size(); i++)
        {
            YAML::Node db = config["databases"][i];
            if (db["name"].as<std::string>() == name)
            {
                return YAML::Node(db);
            }
        }
        // Return an undefined node if not found
        // Using a temporary variable to ensure it's properly undefined
        YAML::Node undefinedNode;
        return undefinedNode;
    }

    // Get connection pool configuration
    YAML::Node getConnectionPoolConfig(const std::string &name = "default") const
    {
        return config["connection_pool"][name];
    }

    // Get test queries
    YAML::Node getTestQueries() const
    {
        return config["test_queries"];
    }

    // Get test queries for a specific database type
    YAML::Node getTestQueriesForType(const std::string &type) const
    {
        return config["test_queries"][type];
    }
};

// Test case to verify that the database configuration file can be loaded
TEST_CASE("Database configuration loading", "[db_config]")
{
    SECTION("Load database configuration file")
    {
        // Get the path to the configuration file
        std::string config_path = getConfigFilePath();

        // Check if the file exists
        std::ifstream file(config_path);
        REQUIRE(file.good());
        file.close();

        // Load the YAML file
        YAML::Node config = YAML::LoadFile(config_path);

        // Verify that the configuration contains the expected sections
        REQUIRE(config["databases"]);
        REQUIRE(config["connection_pool"]);
        REQUIRE(config["test_queries"]);

        // Verify that databases is a sequence
        REQUIRE(config["databases"].IsSequence());

        // Verify that we have at least one database configuration
        REQUIRE(config["databases"].size() > 0);
    }
}

// Test case to verify database configurations
TEST_CASE("Database configurations", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());

    SECTION("Verify all databases")
    {
        auto allDatabases = configManager.getAllDatabases();

        // Check that we have the expected number of databases
        REQUIRE(allDatabases.size() == 9); // 3 MySQL + 3 PostgreSQL + 3 SQLite

        // Check that each database has the required fields
        for (const auto &db : allDatabases)
        {
            std::string type = db["type"].as<std::string>();

            // Common fields for all database types
            REQUIRE(db["name"]);
            REQUIRE(db["type"]);
            REQUIRE(db["database"]);
            REQUIRE(db["options"]);

            // Host and port are only required for MySQL and PostgreSQL
            if (type == "mysql" || type == "postgresql")
            {
                REQUIRE(db["host"]);
                REQUIRE(db["port"]);
                REQUIRE(db["username"]);
                REQUIRE(db["password"]);
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
            REQUIRE(db["type"].as<std::string>() == "mysql");
        }

        // Check that we have the expected database names
        std::vector<std::string> dbNames;
        for (const auto &db : mysqlDatabases)
        {
            dbNames.push_back(db["name"].as<std::string>());
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
            REQUIRE(db["type"].as<std::string>() == "postgresql");
        }

        // Check that we have the expected database names
        std::vector<std::string> dbNames;
        for (const auto &db : postgresqlDatabases)
        {
            dbNames.push_back(db["name"].as<std::string>());
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
            REQUIRE(db["type"].as<std::string>() == "sqlite");
        }

        // Check that we have the expected database names
        std::vector<std::string> dbNames;
        for (const auto &db : sqliteDatabases)
        {
            dbNames.push_back(db["name"].as<std::string>());
        }

        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "dev_sqlite") != dbNames.end());
        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "test_sqlite") != dbNames.end());
        REQUIRE(std::find(dbNames.begin(), dbNames.end(), "prod_sqlite") != dbNames.end());
    }
}

// Test case to verify specific database configurations
TEST_CASE("Specific database configurations", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());

    SECTION("Verify dev_mysql configuration")
    {
        YAML::Node devMySQL = configManager.getDatabaseByName("dev_mysql");

        // Check that the database was found
        REQUIRE(devMySQL.IsDefined());

        // Check connection parameters
        REQUIRE(devMySQL["type"].as<std::string>() == "mysql");
        REQUIRE(devMySQL["host"].as<std::string>() == "localhost");
        REQUIRE(devMySQL["port"].as<int>() == 3306);
        REQUIRE(devMySQL["database"].as<std::string>() == "Test01DB");
        REQUIRE(devMySQL["username"].as<std::string>() == "root");
        REQUIRE(devMySQL["password"].as<std::string>() == "dsystems");

        // Check options
        YAML::Node options = devMySQL["options"];
        REQUIRE(options["connect_timeout"].as<int>() == 5);
        REQUIRE(options["read_timeout"].as<int>() == 10);
        REQUIRE(options["write_timeout"].as<int>() == 10);
        REQUIRE(options["charset"].as<std::string>() == "utf8mb4");
        REQUIRE(options["auto_reconnect"].as<bool>() == true);
    }

    SECTION("Verify dev_postgresql configuration")
    {
        YAML::Node prodPostgreSQL = configManager.getDatabaseByName("prod_postgresql");

        // Check that the database was found
        REQUIRE(prodPostgreSQL.IsDefined());

        // Check connection parameters
        REQUIRE(prodPostgreSQL["type"].as<std::string>() == "postgresql");
        REQUIRE(prodPostgreSQL["host"].as<std::string>() == "db.example.com");
        REQUIRE(prodPostgreSQL["port"].as<int>() == 5432);
        REQUIRE(prodPostgreSQL["database"].as<std::string>() == "Test01DB");
        REQUIRE(prodPostgreSQL["username"].as<std::string>() == "root");
        REQUIRE(prodPostgreSQL["password"].as<std::string>() == "dsystems");

        // Check options
        YAML::Node options = prodPostgreSQL["options"];
        REQUIRE(options["connect_timeout"].as<int>() == 10);
        REQUIRE(options["application_name"].as<std::string>() == "cpp_dbc_prod");
        REQUIRE(options["client_encoding"].as<std::string>() == "UTF8");
        REQUIRE(options["sslmode"].as<std::string>() == "require");
    }

    SECTION("Verify non-existent database")
    {
        YAML::Node nonExistentDB = configManager.getDatabaseByName("non_existent_db");

        // Check that the database does not have a name field
        // This is a more reliable way to check if the database exists
        REQUIRE_FALSE(nonExistentDB["name"].IsDefined());
    }
}

// Test case to verify specific SQLite database configurations
TEST_CASE("Specific SQLite database configurations", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());

    SECTION("Verify dev_sqlite configuration")
    {
        YAML::Node devSQLite = configManager.getDatabaseByName("dev_sqlite");

        // Check that the database was found
        REQUIRE(devSQLite.IsDefined());

        // Check connection parameters
        REQUIRE(devSQLite["type"].as<std::string>() == "sqlite");
        REQUIRE(devSQLite["database"].as<std::string>() == ":memory:");

        // Check options
        YAML::Node options = devSQLite["options"];
        REQUIRE(options["foreign_keys"].as<bool>() == true);
        REQUIRE(options["journal_mode"].as<std::string>() == "WAL");
    }

    SECTION("Verify test_sqlite configuration")
    {
        YAML::Node testSQLite = configManager.getDatabaseByName("test_sqlite");

        // Check that the database was found
        REQUIRE(testSQLite.IsDefined());

        // Check connection parameters
        REQUIRE(testSQLite["type"].as<std::string>() == "sqlite");
        REQUIRE(testSQLite["database"].as<std::string>() == "test_sqlite.db");

        // Check options
        YAML::Node options = testSQLite["options"];
        REQUIRE(options["foreign_keys"].as<bool>() == true);
        REQUIRE(options["journal_mode"].as<std::string>() == "WAL");
    }

    SECTION("Verify prod_sqlite configuration")
    {
        YAML::Node prodSQLite = configManager.getDatabaseByName("prod_sqlite");

        // Check that the database was found
        REQUIRE(prodSQLite.IsDefined());

        // Check connection parameters
        REQUIRE(prodSQLite["type"].as<std::string>() == "sqlite");
        REQUIRE(prodSQLite["database"].as<std::string>() == "/path/to/production.db");

        // Check options
        YAML::Node options = prodSQLite["options"];
        REQUIRE(options["foreign_keys"].as<bool>() == true);
        REQUIRE(options["journal_mode"].as<std::string>() == "WAL");
        REQUIRE(options["synchronous"].as<std::string>() == "FULL");
    }
}

// Test case to verify connection pool configurations
TEST_CASE("Connection pool configurations", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());

    SECTION("Verify default pool configuration")
    {
        YAML::Node defaultPool = configManager.getConnectionPoolConfig("default");

        // Check that the pool was found
        REQUIRE(defaultPool.IsDefined());

        // Check pool parameters
        REQUIRE(defaultPool["initial_size"].as<int>() == 5);
        REQUIRE(defaultPool["max_size"].as<int>() == 10);
        REQUIRE(defaultPool["connection_timeout"].as<int>() == 5000);
        REQUIRE(defaultPool["idle_timeout"].as<int>() == 60000);
        REQUIRE(defaultPool["validation_interval"].as<int>() == 30000);
    }

    SECTION("Verify high_performance pool configuration")
    {
        YAML::Node highPerfPool = configManager.getConnectionPoolConfig("high_performance");

        // Check that the pool was found
        REQUIRE(highPerfPool.IsDefined());

        // Check pool parameters
        REQUIRE(highPerfPool["initial_size"].as<int>() == 10);
        REQUIRE(highPerfPool["max_size"].as<int>() == 50);
        REQUIRE(highPerfPool["connection_timeout"].as<int>() == 3000);
        REQUIRE(highPerfPool["idle_timeout"].as<int>() == 30000);
        REQUIRE(highPerfPool["validation_interval"].as<int>() == 15000);
    }
}

// Test case to verify test queries
TEST_CASE("Test queries", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());

    SECTION("Verify common test query")
    {
        YAML::Node testQueries = configManager.getTestQueries();

        // Check that the connection test query exists
        REQUIRE(testQueries["connection_test"].as<std::string>() == "SELECT 1");
    }

    SECTION("Verify MySQL test queries")
    {
        YAML::Node mysqlQueries = configManager.getTestQueriesForType("mysql");

        // Check that all expected queries exist
        REQUIRE(mysqlQueries["create_table"].as<std::string>().find("CREATE TABLE") != std::string::npos);
        REQUIRE(mysqlQueries["insert_data"].as<std::string>().find("INSERT INTO") != std::string::npos);
        REQUIRE(mysqlQueries["select_data"].as<std::string>().find("SELECT") != std::string::npos);
        REQUIRE(mysqlQueries["drop_table"].as<std::string>().find("DROP TABLE") != std::string::npos);

        // Check MySQL parameter style (? placeholders)
        REQUIRE(mysqlQueries["insert_data"].as<std::string>().find("?") != std::string::npos);
        REQUIRE(mysqlQueries["select_data"].as<std::string>().find("?") != std::string::npos);
    }

    SECTION("Verify PostgreSQL test queries")
    {
        YAML::Node pgQueries = configManager.getTestQueriesForType("postgresql");

        // Check that all expected queries exist
        REQUIRE(pgQueries["create_table"].as<std::string>().find("CREATE TABLE") != std::string::npos);
        REQUIRE(pgQueries["insert_data"].as<std::string>().find("INSERT INTO") != std::string::npos);
        REQUIRE(pgQueries["select_data"].as<std::string>().find("SELECT") != std::string::npos);
        REQUIRE(pgQueries["drop_table"].as<std::string>().find("DROP TABLE") != std::string::npos);

        // Check PostgreSQL parameter style ($n placeholders)
        REQUIRE(pgQueries["insert_data"].as<std::string>().find("$1") != std::string::npos);
        REQUIRE(pgQueries["select_data"].as<std::string>().find("$1") != std::string::npos);
    }

    SECTION("Verify SQLite test queries")
    {
        YAML::Node sqliteQueries = configManager.getTestQueriesForType("sqlite");

        // Check that all expected queries exist
        REQUIRE(sqliteQueries["create_table"].as<std::string>().find("CREATE TABLE") != std::string::npos);
        REQUIRE(sqliteQueries["insert_data"].as<std::string>().find("INSERT INTO") != std::string::npos);
        REQUIRE(sqliteQueries["select_data"].as<std::string>().find("SELECT") != std::string::npos);
        REQUIRE(sqliteQueries["drop_table"].as<std::string>().find("DROP TABLE") != std::string::npos);

        // Check SQLite parameter style (? placeholders)
        REQUIRE(sqliteQueries["insert_data"].as<std::string>().find("?") != std::string::npos);
        REQUIRE(sqliteQueries["select_data"].as<std::string>().find("?") != std::string::npos);
    }
}

// Example of how to use the configuration to create connection strings
TEST_CASE("Create connection strings from configuration", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());

    SECTION("Create connection strings for all databases")
    {
        auto allDatabases = configManager.getAllDatabases();

        // Create a map to store connection strings for each database
        std::map<std::string, std::string> connectionStrings;

        // Iterate through all database configurations
        for (const auto &db : allDatabases)
        {
            std::string dbName = db["name"].as<std::string>();

            // Create connection string
            connectionStrings[dbName] = createConnectionString(db);
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
        //     YAML::Node db = configManager.getDatabaseByName(dbName);
        //     auto conn = cpp_dbc::DriverManager::getConnection(
        //         connStr,
        //         db["username"].as<std::string>(),
        //         db["password"].as<std::string>()
        //     );
        //     // Use the connection...
        // }
    }
}

// Test database configurations for different environments and types
TEST_CASE("Select MySQL database for dev environment", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());
    std::string dbName = "dev_mysql";
    YAML::Node dbConfig = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfig.IsDefined());

    // Check that the database has the correct type
    REQUIRE(dbConfig["type"].as<std::string>() == "mysql");

    // Create connection string
    std::string connStr = createConnectionString(dbConfig);

    // Verify the connection string format
    REQUIRE(connStr.find("cpp_dbc:mysql://") == 0);

    // Verify that we can access the credentials
    std::string username = dbConfig["username"].as<std::string>();
    std::string password = dbConfig["password"].as<std::string>();

    REQUIRE(username == "root");
    REQUIRE(password == "dsystems");
}

TEST_CASE("Select MySQL database for test environment", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());
    std::string dbName = "test_mysql";
    YAML::Node dbConfig = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfig.IsDefined());
    REQUIRE(dbConfig["type"].as<std::string>() == "mysql");

    // Create connection string
    std::string connStr = createConnectionString(dbConfig);
    REQUIRE(connStr.find("cpp_dbc:mysql://") == 0);
}

TEST_CASE("Select MySQL database for prod environment", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());
    std::string dbName = "prod_mysql";
    YAML::Node dbConfig = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfig.IsDefined());
    REQUIRE(dbConfig["type"].as<std::string>() == "mysql");

    // Create connection string
    std::string connStr = createConnectionString(dbConfig);
    REQUIRE(connStr.find("cpp_dbc:mysql://") == 0);
}

TEST_CASE("Select PostgreSQL database for dev environment", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());
    std::string dbName = "dev_postgresql";
    YAML::Node dbConfig = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfig.IsDefined());
    REQUIRE(dbConfig["type"].as<std::string>() == "postgresql");

    // Create connection string
    std::string connStr = createConnectionString(dbConfig);
    REQUIRE(connStr.find("cpp_dbc:postgresql://") == 0);
}

TEST_CASE("Select PostgreSQL database for test environment", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());
    std::string dbName = "test_postgresql";
    YAML::Node dbConfig = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfig.IsDefined());
    REQUIRE(dbConfig["type"].as<std::string>() == "postgresql");

    // Create connection string
    std::string connStr = createConnectionString(dbConfig);
    REQUIRE(connStr.find("cpp_dbc:postgresql://") == 0);
}

TEST_CASE("Select PostgreSQL database for prod environment", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());
    std::string dbName = "prod_postgresql";
    YAML::Node dbConfig = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfig.IsDefined());
    REQUIRE(dbConfig["type"].as<std::string>() == "postgresql");

    // Create connection string
    std::string connStr = createConnectionString(dbConfig);
    REQUIRE(connStr.find("cpp_dbc:postgresql://") == 0);
}

TEST_CASE("Select SQLite database for dev environment", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());
    std::string dbName = "dev_sqlite";
    YAML::Node dbConfig = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfig.IsDefined());
    REQUIRE(dbConfig["type"].as<std::string>() == "sqlite");

    // Create connection string
    std::string connStr = createConnectionString(dbConfig);
    REQUIRE(connStr == "cpp_dbc:sqlite://:memory:");
}

TEST_CASE("Select SQLite database for test environment", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());
    std::string dbName = "test_sqlite";
    YAML::Node dbConfig = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfig.IsDefined());
    REQUIRE(dbConfig["type"].as<std::string>() == "sqlite");

    // Create connection string
    std::string connStr = createConnectionString(dbConfig);
    REQUIRE(connStr == "cpp_dbc:sqlite://test_sqlite.db");
}

TEST_CASE("Select SQLite database for prod environment", "[db_config]")
{
    DatabaseConfigManager configManager(getConfigFilePath());
    std::string dbName = "prod_sqlite";
    YAML::Node dbConfig = configManager.getDatabaseByName(dbName);

    // Check that the database was found
    REQUIRE(dbConfig.IsDefined());
    REQUIRE(dbConfig["type"].as<std::string>() == "sqlite");

    // Create connection string
    std::string connStr = createConnectionString(dbConfig);
    REQUIRE(connStr == "cpp_dbc:sqlite:///path/to/production.db");
}