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

 @file 10_001_test_database_config.cpp
 @brief Tests for configuration handling

*/

#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <string>
#include <map>
#include <vector>
#include <memory>

// Test case for DBConnectionOptions
TEST_CASE("DBConnectionOptions tests", "[10_001_01_database_config]")
{
    SECTION("Default constructor creates empty options")
    {
        cpp_dbc::config::DBConnectionOptions options;
        REQUIRE(options.getAllOptions().empty());
    }

    SECTION("Set and get options")
    {
        cpp_dbc::config::DBConnectionOptions options;

        // Set some options
        options.setOption("connect_timeout", "5");
        options.setOption("charset", "utf8mb4");
        options.setOption("auto_reconnect", "true");

        // Check that options were set correctly
        REQUIRE(options.getOption("connect_timeout") == "5");
        REQUIRE(options.getOption("charset") == "utf8mb4");
        REQUIRE(options.getOption("auto_reconnect") == "true");

        // Check default value for non-existent option
        REQUIRE(options.getOption("non_existent") == "");
        REQUIRE(options.getOption("non_existent", "default") == "default");

        // Check hasOption
        REQUIRE(options.hasOption("connect_timeout"));
        REQUIRE(options.hasOption("charset"));
        REQUIRE(options.hasOption("auto_reconnect"));
        REQUIRE_FALSE(options.hasOption("non_existent"));

        // Check getAllOptions
        auto allOptions = options.getAllOptions();
        REQUIRE(allOptions.size() == 3);
        REQUIRE(allOptions.at("connect_timeout") == "5");
        REQUIRE(allOptions.at("charset") == "utf8mb4");
        REQUIRE(allOptions.at("auto_reconnect") == "true");
    }

    SECTION("Overwrite existing option")
    {
        cpp_dbc::config::DBConnectionOptions options;

        // Set an option
        options.setOption("connect_timeout", "5");
        REQUIRE(options.getOption("connect_timeout") == "5");

        // Overwrite the option
        options.setOption("connect_timeout", "10");
        REQUIRE(options.getOption("connect_timeout") == "10");
    }
}

// Test case for DatabaseConfig
TEST_CASE("DatabaseConfig tests", "[10_001_02_database_config]")
{
    SECTION("Default constructor creates empty config")
    {
        cpp_dbc::config::DatabaseConfig config;
        REQUIRE(config.getName().empty());
        REQUIRE(config.getType().empty());
        REQUIRE(config.getHost().empty());
        REQUIRE(config.getPort() == 0);
        REQUIRE(config.getDatabase().empty());
        REQUIRE(config.getUsername().empty());
        REQUIRE(config.getPassword().empty());
    }

    SECTION("Constructor with parameters")
    {
        cpp_dbc::config::DatabaseConfig config(
            "test_db",
            "mysql",
            "localhost",
            3306,
            "testdb",
            "root",
            "password");

        REQUIRE(config.getName() == "test_db");
        REQUIRE(config.getType() == "mysql");
        REQUIRE(config.getHost() == "localhost");
        REQUIRE(config.getPort() == 3306);
        REQUIRE(config.getDatabase() == "testdb");
        REQUIRE(config.getUsername() == "root");
        REQUIRE(config.getPassword() == "password");
    }

    SECTION("Setters and getters")
    {
        cpp_dbc::config::DatabaseConfig config;

        config.setName("setter_test");
        config.setType("postgresql");
        config.setHost("db.example.com");
        config.setPort(5432);
        config.setDatabase("postgres");
        config.setUsername("postgres");
        config.setPassword("postgres");

        REQUIRE(config.getName() == "setter_test");
        REQUIRE(config.getType() == "postgresql");
        REQUIRE(config.getHost() == "db.example.com");
        REQUIRE(config.getPort() == 5432);
        REQUIRE(config.getDatabase() == "postgres");
        REQUIRE(config.getUsername() == "postgres");
        REQUIRE(config.getPassword() == "postgres");
    }

    SECTION("Connection options")
    {
        cpp_dbc::config::DatabaseConfig config(
            "test_db",
            "mysql",
            "localhost",
            3306,
            "testdb",
            "root",
            "password");

        // Set some options
        config.setOption("connect_timeout", "5");
        config.setOption("charset", "utf8mb4");

        // Check that options were set correctly
        REQUIRE(config.getOption("connect_timeout") == "5");
        REQUIRE(config.getOption("charset") == "utf8mb4");

        // Check default value for non-existent option
        REQUIRE(config.getOption("non_existent") == "");
        REQUIRE(config.getOption("non_existent", "default") == "default");
    }

    SECTION("Create connection string")
    {
        cpp_dbc::config::DatabaseConfig config(
            "test_db",
            "mysql",
            "localhost",
            3306,
            "testdb",
            "root",
            "password");

        std::string connStr = config.createConnectionString();
        REQUIRE(connStr == "cpp_dbc:mysql://localhost:3306/testdb");

        // Change some parameters and check the connection string again
        config.setType("postgresql");
        config.setHost("db.example.com");
        config.setPort(5432);
        config.setDatabase("postgres");

        connStr = config.createConnectionString();
        REQUIRE(connStr == "cpp_dbc:postgresql://db.example.com:5432/postgres");

        // Change parameters for ScyllaDB
        config.setType("scylladb");
        config.setHost("localhost");
        config.setPort(9042);
        config.setDatabase("keyspace");

        connStr = config.createConnectionString();
        REQUIRE(connStr == "cpp_dbc:scylladb://localhost:9042/keyspace");
    }
}

// Test case for TestQueries
TEST_CASE("TestQueries tests", "[10_001_03_database_config]")
{
    SECTION("Default constructor creates empty queries")
    {
        cpp_dbc::config::TestQueries queries;
        REQUIRE(queries.getConnectionTest().empty());
    }

    SECTION("Set and get connection test query")
    {
        cpp_dbc::config::TestQueries queries;

        // Set connection test query
        queries.setConnectionTest("SELECT 1");

        // Check that the query was set correctly
        REQUIRE(queries.getConnectionTest() == "SELECT 1");
    }

    SECTION("Set and get database-specific queries")
    {
        cpp_dbc::config::TestQueries queries;

        // Set some queries for MySQL
        queries.setQuery("mysql", "create_table", "CREATE TABLE test (id INT)");
        queries.setQuery("mysql", "insert", "INSERT INTO test VALUES (?)");
        queries.setQuery("mysql", "select", "SELECT * FROM test");

        // Set some queries for PostgreSQL
        queries.setQuery("postgresql", "create_table", "CREATE TABLE test (id INTEGER)");
        queries.setQuery("postgresql", "insert", "INSERT INTO test VALUES ($1)");
        queries.setQuery("postgresql", "select", "SELECT * FROM test");

        // Check MySQL queries
        REQUIRE(queries.getQuery("mysql", "create_table") == "CREATE TABLE test (id INT)");
        REQUIRE(queries.getQuery("mysql", "insert") == "INSERT INTO test VALUES (?)");
        REQUIRE(queries.getQuery("mysql", "select") == "SELECT * FROM test");

        // Check PostgreSQL queries
        REQUIRE(queries.getQuery("postgresql", "create_table") == "CREATE TABLE test (id INTEGER)");
        REQUIRE(queries.getQuery("postgresql", "insert") == "INSERT INTO test VALUES ($1)");
        REQUIRE(queries.getQuery("postgresql", "select") == "SELECT * FROM test");

        // Check default value for non-existent query
        REQUIRE(queries.getQuery("mysql", "non_existent") == "");
        REQUIRE(queries.getQuery("mysql", "non_existent", "DEFAULT") == "DEFAULT");
        REQUIRE(queries.getQuery("non_existent", "create_table") == "");

        // Check getQueriesForType
        auto mysqlQueries = queries.getQueriesForType("mysql");
        REQUIRE(mysqlQueries.size() == 3);
        REQUIRE(mysqlQueries.at("create_table") == "CREATE TABLE test (id INT)");
        REQUIRE(mysqlQueries.at("insert") == "INSERT INTO test VALUES (?)");
        REQUIRE(mysqlQueries.at("select") == "SELECT * FROM test");

        auto pgQueries = queries.getQueriesForType("postgresql");
        REQUIRE(pgQueries.size() == 3);
        REQUIRE(pgQueries.at("create_table") == "CREATE TABLE test (id INTEGER)");
        REQUIRE(pgQueries.at("insert") == "INSERT INTO test VALUES ($1)");
        REQUIRE(pgQueries.at("select") == "SELECT * FROM test");

        // Check empty result for non-existent database type
        auto nonExistentQueries = queries.getQueriesForType("non_existent");
        REQUIRE(nonExistentQueries.empty());
    }
}

// Test case for DatabaseConfigManager
TEST_CASE("DatabaseConfigManager tests", "[10_001_04_database_config]")
{
    SECTION("Default constructor creates empty manager")
    {
        cpp_dbc::config::DatabaseConfigManager manager;
        REQUIRE(manager.getAllDatabases().empty());
    }

    SECTION("Add and retrieve database configurations")
    {
        cpp_dbc::config::DatabaseConfigManager manager;

        // Create some database configurations
        cpp_dbc::config::DatabaseConfig mysqlConfig(
            "mysql_db",
            "mysql",
            "localhost",
            3306,
            "testdb",
            "root",
            "password");

        cpp_dbc::config::DatabaseConfig pgConfig(
            "pg_db",
            "postgresql",
            "localhost",
            5432,
            "postgres",
            "postgres",
            "postgres");

        // Add the configurations to the manager
        manager.addDatabaseConfig(mysqlConfig);
        manager.addDatabaseConfig(pgConfig);

        // Check getAllDatabases
        auto allDatabases = manager.getAllDatabases();
        REQUIRE(allDatabases.size() == 2);
        REQUIRE(allDatabases[0].getName() == "mysql_db");
        REQUIRE(allDatabases[1].getName() == "pg_db");

        // Check getDatabasesByType
        auto mysqlDatabases = manager.getDatabasesByType("mysql");
        REQUIRE(mysqlDatabases.size() == 1);
        REQUIRE(mysqlDatabases[0].getName() == "mysql_db");

        auto pgDatabases = manager.getDatabasesByType("postgresql");
        REQUIRE(pgDatabases.size() == 1);
        REQUIRE(pgDatabases[0].getName() == "pg_db");

        auto nonExistentDatabases = manager.getDatabasesByType("non_existent");
        REQUIRE(nonExistentDatabases.empty());

        // Check getDatabaseByName
        auto mysqlDbOpt = manager.getDatabaseByName("mysql_db");
        REQUIRE(mysqlDbOpt.has_value());
        const auto &mysqlDb = mysqlDbOpt->get();
        REQUIRE(mysqlDb.getName() == "mysql_db");
        REQUIRE(mysqlDb.getType() == "mysql");

        auto pgDbOpt = manager.getDatabaseByName("pg_db");
        REQUIRE(pgDbOpt.has_value());
        const auto &pgDb = pgDbOpt->get();
        REQUIRE(pgDb.getName() == "pg_db");
        REQUIRE(pgDb.getType() == "postgresql");

        auto nonExistentDbOpt = manager.getDatabaseByName("non_existent");
        REQUIRE_FALSE(nonExistentDbOpt.has_value());
    }

    SECTION("Add and retrieve connection pool configurations")
    {
        cpp_dbc::config::DatabaseConfigManager manager;

        // Create some connection pool configurations
        cpp_dbc::config::DBConnectionPoolConfig defaultPoolConfig;
        defaultPoolConfig.setName("default");
        defaultPoolConfig.setInitialSize(5);
        defaultPoolConfig.setMaxSize(10);

        cpp_dbc::config::DBConnectionPoolConfig highPerfPoolConfig;
        highPerfPoolConfig.setName("high_performance");
        highPerfPoolConfig.setInitialSize(10);
        highPerfPoolConfig.setMaxSize(50);

        // Add the configurations to the manager
        manager.addDBConnectionPoolConfig(defaultPoolConfig);
        manager.addDBConnectionPoolConfig(highPerfPoolConfig);

        // Check getConnectionPoolConfig
        auto defaultPoolOpt = manager.getDBConnectionPoolConfig("default");
        REQUIRE(defaultPoolOpt.has_value());
        const auto &defaultPool = defaultPoolOpt->get();
        REQUIRE(defaultPool.getName() == "default");
        REQUIRE(defaultPool.getInitialSize() == 5);
        REQUIRE(defaultPool.getMaxSize() == 10);

        auto highPerfPoolOpt = manager.getDBConnectionPoolConfig("high_performance");
        REQUIRE(highPerfPoolOpt.has_value());
        const auto &highPerfPool = highPerfPoolOpt->get();
        REQUIRE(highPerfPool.getName() == "high_performance");
        REQUIRE(highPerfPool.getInitialSize() == 10);
        REQUIRE(highPerfPool.getMaxSize() == 50);

        auto nonExistentPoolOpt = manager.getDBConnectionPoolConfig("non_existent");
        REQUIRE_FALSE(nonExistentPoolOpt.has_value());

        // Check default parameter
        auto defaultPoolOpt2 = manager.getDBConnectionPoolConfig();
        REQUIRE(defaultPoolOpt2.has_value());
        const auto &defaultPool2 = defaultPoolOpt2->get();
        REQUIRE(defaultPool2.getName() == "default");
    }

    SECTION("Set and get test queries")
    {
        cpp_dbc::config::DatabaseConfigManager manager;

        // Create test queries
        cpp_dbc::config::TestQueries queries;
        queries.setConnectionTest("SELECT 1");
        queries.setQuery("mysql", "create_table", "CREATE TABLE test (id INT)");
        queries.setQuery("postgresql", "create_table", "CREATE TABLE test (id INTEGER)");

        // Set the test queries in the manager
        manager.setTestQueries(queries);

        // Check getTestQueries
        auto retrievedQueries = manager.getTestQueries();
        REQUIRE(retrievedQueries.getConnectionTest() == "SELECT 1");
        REQUIRE(retrievedQueries.getQuery("mysql", "create_table") == "CREATE TABLE test (id INT)");
        REQUIRE(retrievedQueries.getQuery("postgresql", "create_table") == "CREATE TABLE test (id INTEGER)");
    }
}