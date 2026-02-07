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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file config_example.cpp
 * @brief Example demonstrating database configuration classes
 *
 * This example demonstrates:
 * - Creating configuration programmatically
 * - Loading configuration from YAML file
 * - Accessing database, pool, and test query configurations
 *
 * Usage:
 *   ./config_example [<config_file>]
 *
 * Examples:
 *   ./config_example                          # Creates config programmatically
 *   ./config_example example_config.yml       # Loads config from YAML file
 */

#include "example_common.hpp"
#include <cpp_dbc/config/database_config.hpp>

#ifdef USE_CPP_YAML
#include <cpp_dbc/config/yaml_config_loader.hpp>
#endif

using namespace cpp_dbc::examples;

// Function to create a configuration programmatically
cpp_dbc::config::DatabaseConfigManager createConfigProgrammatically()
{
    cpp_dbc::config::DatabaseConfigManager configManager;

    // Create a MySQL database configuration
    cpp_dbc::config::DatabaseConfig mysqlConfig;
    mysqlConfig.setName("dev_mysql");
    mysqlConfig.setType("mysql");
    mysqlConfig.setHost("localhost");
    mysqlConfig.setPort(3306);
    mysqlConfig.setDatabase("TestDB");
    mysqlConfig.setUsername("root");
    mysqlConfig.setPassword("password");

    // Set some options
    mysqlConfig.setOption("connect_timeout", "5");
    mysqlConfig.setOption("read_timeout", "10");
    mysqlConfig.setOption("charset", "utf8mb4");

    // Add the configuration to the manager
    configManager.addDatabaseConfig(mysqlConfig);

    // Create a PostgreSQL database configuration
    cpp_dbc::config::DatabaseConfig postgresConfig;
    postgresConfig.setName("dev_postgresql");
    postgresConfig.setType("postgresql");
    postgresConfig.setHost("localhost");
    postgresConfig.setPort(5432);
    postgresConfig.setDatabase("TestDB");
    postgresConfig.setUsername("postgres");
    postgresConfig.setPassword("password");

    // Set some options
    postgresConfig.setOption("connect_timeout", "5");
    postgresConfig.setOption("application_name", "cpp_dbc_example");
    postgresConfig.setOption("client_encoding", "UTF8");

    // Add the configuration to the manager
    configManager.addDatabaseConfig(postgresConfig);

    // Create a connection pool configuration
    cpp_dbc::config::DBConnectionPoolConfig poolConfig;
    poolConfig.setName("default");
    poolConfig.setInitialSize(5);
    poolConfig.setMaxSize(20);
    poolConfig.setConnectionTimeout(5000);
    poolConfig.setIdleTimeout(60000);
    poolConfig.setValidationInterval(30000);

    // Add the pool configuration to the manager
    configManager.addDBConnectionPoolConfig(poolConfig);

    // Create test queries
    cpp_dbc::config::TestQueries queries;
    queries.setConnectionTest("SELECT 1");
    queries.setQuery("mysql", "get_users", "SELECT * FROM users");
    queries.setQuery("postgresql", "get_users", "SELECT * FROM users");

    // Set the test queries in the manager
    configManager.setTestQueries(queries);

    return configManager;
}

// Function to print database configurations
void printDatabaseConfigs(const cpp_dbc::config::DatabaseConfigManager &configManager)
{
    log("");
    log("--- Database Configurations ---");

    for (const auto &dbConfig : configManager.getAllDatabases())
    {
        logData("Name: " + dbConfig.getName());
        logData("Type: " + dbConfig.getType());
        logData("Host: " + dbConfig.getHost());
        logData("Port: " + std::to_string(dbConfig.getPort()));
        logData("Database: " + dbConfig.getDatabase());
        logData("Username: " + dbConfig.getUsername());
        logData("Password: " + dbConfig.getPassword());

        logData("Options:");
        for (const auto &option : dbConfig.getOptions())
        {
            logData("  " + option.first + ": " + option.second);
        }

        logData("Connection String: " + dbConfig.createConnectionString());
        log("");
    }
    logOk("Database configurations printed");
}

// Function to print connection pool configurations
void printConnectionPoolConfigs(const cpp_dbc::config::DatabaseConfigManager &configManager)
{
    log("");
    log("--- Connection Pool Configurations ---");

    auto poolConfigOpt = configManager.getDBConnectionPoolConfig("default");
    if (poolConfigOpt)
    {
        const auto &poolConfig = poolConfigOpt->get();
        logData("Name: " + poolConfig.getName());
        logData("Initial Size: " + std::to_string(poolConfig.getInitialSize()));
        logData("Max Size: " + std::to_string(poolConfig.getMaxSize()));
        logData("Connection Timeout: " + std::to_string(poolConfig.getConnectionTimeout()) + " ms");
        logData("Idle Timeout: " + std::to_string(poolConfig.getIdleTimeout()) + " ms");
        logData("Validation Interval: " + std::to_string(poolConfig.getValidationInterval()) + " ms");
        logOk("Pool configuration printed");
    }
    else
    {
        logInfo("No 'default' pool configuration found");
    }
}

// Function to print test queries
void printTestQueries(const cpp_dbc::config::DatabaseConfigManager &configManager)
{
    log("");
    log("--- Test Queries ---");

    const auto &testQueries = configManager.getTestQueries();

    logData("Connection Test: " + testQueries.getConnectionTest());
    log("");

    logData("MySQL Queries:");
    for (const auto &query : testQueries.getQueriesForType("mysql"))
    {
        logData("  " + query.first + ": " + query.second);
    }

    logData("PostgreSQL Queries:");
    for (const auto &query : testQueries.getQueriesForType("postgresql"))
    {
        logData("  " + query.first + ": " + query.second);
    }
    logOk("Test queries printed");
}

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc Configuration Example");
    log("========================================");
    log("");

    // Create a configuration manager
    cpp_dbc::config::DatabaseConfigManager configManager;

    // Load configuration
    if (argc > 1)
    {
        std::string configFile = argv[1];

#ifdef USE_CPP_YAML
        try
        {
            logStep("Loading configuration from YAML file: " + configFile);
            configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(configFile);
            logOk("Configuration loaded from YAML");
        }
        catch (const std::exception &e)
        {
            logError("Error loading configuration: " + std::string(e.what()));
            return 1;
        }
#else
        logError("YAML support is not enabled. Cannot load configuration from file.");
        return 1;
#endif
    }
    else
    {
        logStep("Creating configuration programmatically...");
        configManager = createConfigProgrammatically();
        logOk("Configuration created programmatically");
    }

    // Print configurations
    printDatabaseConfigs(configManager);
    printConnectionPoolConfigs(configManager);
    printTestQueries(configManager);

    // Example of using the configuration to create a connection
    log("");
    log("--- Connection Creation Example ---");

    auto dbConfigOpt = configManager.getDatabaseByName("dev_mysql");
    if (dbConfigOpt)
    {
        const auto &dbConfig = dbConfigOpt->get();
        logStep("Creating connection to MySQL database:");
        logData("Connection String: " + dbConfig.createConnectionString());
        logData("Username: " + dbConfig.getUsername());
        logData("Password: " + dbConfig.getPassword());
        logInfo("In a real application, use createDBConnection() to create the connection");
    }
    else
    {
        logInfo("dev_mysql configuration not found");
    }

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return 0;
}
