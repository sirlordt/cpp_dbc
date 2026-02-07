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
 * @file config_integration_example.cpp
 * @brief Example demonstrating integrated configuration and connection classes
 *
 * This example demonstrates:
 * - Multiple ways to create connections from configuration
 * - Connection pool creation from configuration
 * - DatabaseConfig, DatabaseConfigManager, and DriverManager integration
 *
 * Usage:
 *   ./config_integration_example [<config_file>]
 *
 * Examples:
 *   ./config_integration_example                      # Creates config programmatically
 *   ./config_integration_example example_config.yml   # Loads config from YAML file
 */

#include "example_common.hpp"
#include "cpp_dbc/cpp_dbc.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/core/relational/relational_db_connection_pool.hpp"

#ifdef USE_CPP_YAML
#include "cpp_dbc/config/yaml_config_loader.hpp"
#endif

using namespace cpp_dbc::examples;

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc Configuration Integration Example");
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

        // Create a MySQL database configuration
        cpp_dbc::config::DatabaseConfig mysqlConfig;
        mysqlConfig.setName("dev_mysql");
        mysqlConfig.setType("mysql");
        mysqlConfig.setHost("localhost");
        mysqlConfig.setPort(3306);
        mysqlConfig.setDatabase("TestDB");
        mysqlConfig.setUsername("root");
        mysqlConfig.setPassword("password");
        mysqlConfig.setOption("connect_timeout", "5");
        mysqlConfig.setOption("read_timeout", "10");
        mysqlConfig.setOption("charset", "utf8mb4");
        configManager.addDatabaseConfig(mysqlConfig);

        // Create a connection pool configuration
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.setName("default");
        poolConfig.setInitialSize(5);
        poolConfig.setMaxSize(20);
        poolConfig.setMinIdle(3);
        poolConfig.setConnectionTimeout(5000);
        poolConfig.setIdleTimeout(60000);
        poolConfig.setValidationInterval(30000);
        poolConfig.setMaxLifetimeMillis(1800000);
        poolConfig.setTestOnBorrow(true);
        poolConfig.setTestOnReturn(false);
        poolConfig.setValidationQuery("SELECT 1");
        configManager.addDBConnectionPoolConfig(poolConfig);

        logOk("Configuration created programmatically");
    }

    auto dbConfigOpt = configManager.getDatabaseByName("dev_mysql");

    // Example 1: Creating a connection directly from DatabaseConfig
    log("");
    log("--- Example 1: DatabaseConfig::createDBConnection() ---");

    if (dbConfigOpt)
    {
        try
        {
            logStep("Creating connection using DatabaseConfig::createDBConnection()...");
            const auto &dbConfig = dbConfigOpt->get();
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(dbConfig.createDBConnection());
            logOk("Connection created successfully");

            logStep("Executing query: SELECT 1...");
            auto resultSet = conn->executeQuery("SELECT 1");
            logOk("Query executed successfully");

            logStep("Closing connection...");
            conn->close();
            logOk("Connection closed");
        }
        catch (const cpp_dbc::DBException &e)
        {
            logError(e.what_s());
        }
    }
    else
    {
        logInfo("dev_mysql configuration not found");
    }

    // Example 2: Creating a connection from DriverManager with DatabaseConfig
    log("");
    log("--- Example 2: DriverManager::getDBConnection(dbConfig) ---");

    if (dbConfigOpt)
    {
        try
        {
            logStep("Creating connection using DriverManager::getDBConnection(dbConfig)...");
            const auto &dbConfig = dbConfigOpt->get();
            auto conn = cpp_dbc::DriverManager::getDBConnection(dbConfig);
            logOk("Connection created successfully");

            logStep("Closing connection...");
            conn->close();
            logOk("Connection closed");
        }
        catch (const cpp_dbc::DBException &e)
        {
            logError(e.what_s());
        }
    }

    // Example 3: Creating a connection from DriverManager with DatabaseConfigManager
    log("");
    log("--- Example 3: DriverManager::getDBConnection(configManager, name) ---");

    try
    {
        logStep("Creating connection using DriverManager::getDBConnection(configManager, \"dev_mysql\")...");
        auto conn = cpp_dbc::DriverManager::getDBConnection(configManager, "dev_mysql");
        logOk("Connection created successfully");

        logStep("Closing connection...");
        conn->close();
        logOk("Connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError(e.what_s());
    }

    // Example 4: Creating a connection from DatabaseConfigManager
    log("");
    log("--- Example 4: configManager.createDBConnection(name) ---");

    try
    {
        logStep("Creating connection using configManager.createDBConnection(\"dev_mysql\")...");
        auto conn = configManager.createDBConnection("dev_mysql");
        if (conn)
        {
            logOk("Connection created successfully");

            logStep("Closing connection...");
            conn->close();
            logOk("Connection closed");
        }
        else
        {
            logError("Failed to create connection: Database configuration not found");
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError(e.what_s());
    }

    // Example 5: Creating a connection pool
    log("");
    log("--- Example 5: configManager.createDBConnectionPool() ---");

    try
    {
        logStep("Creating connection pool using configManager.createDBConnectionPool(\"dev_mysql\", \"default\")...");
        auto pool = configManager.createDBConnectionPool("dev_mysql", "default");
        if (pool)
        {
            logOk("Connection pool created successfully");

            logStep("Getting connection from pool...");
            auto conn = pool->getRelationalDBConnection();
            logOk("Connection obtained from pool");

            logStep("Executing query: SELECT 1...");
            auto resultSet = conn->executeQuery("SELECT 1");
            logOk("Query executed successfully");

            logStep("Returning connection to pool...");
            conn->close();
            logOk("Connection returned to pool");

            logStep("Closing connection pool...");
            pool->close();
            logOk("Connection pool closed");
        }
        else
        {
            logError("Failed to create connection pool: Configuration not found");
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError(e.what_s());
    }

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return 0;
}
