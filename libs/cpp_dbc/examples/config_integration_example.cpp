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

 @file config_integration_example.cpp
 @brief Example of using the integrated configuration and connection classes

*/

#include "cpp_dbc/cpp_dbc.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/connection_pool.hpp"
#include <iostream>
#include <memory>

#ifdef USE_CPP_YAML
#include "cpp_dbc/config/yaml_config_loader.hpp"
#endif

int main(int argc, char *argv[])
{
    // Create a configuration manager
    cpp_dbc::config::DatabaseConfigManager configManager;

    // Load configuration
    if (argc > 1)
    {
        std::string configFile = argv[1];

#ifdef USE_CPP_YAML
        try
        {
            // Load configuration from YAML file
            std::cout << "Loading configuration from YAML file: " << configFile << std::endl;
            configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(configFile);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error loading configuration: " << e.what() << std::endl;
            return 1;
        }
#else
        std::cerr << "YAML support is not enabled. Cannot load configuration from file." << std::endl;
        return 1;
#endif
    }
    else
    {
        // Create configuration programmatically
        std::cout << "Creating configuration programmatically" << std::endl;

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
        cpp_dbc::config::ConnectionPoolConfig poolConfig;
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
        configManager.addConnectionPoolConfig(poolConfig);
    }

    std::cout << "\n=== Example 1: Creating a connection directly from DatabaseConfig ===" << std::endl;
    auto dbConfigOpt = configManager.getDatabaseByName("dev_mysql");
    if (dbConfigOpt)
    {
        try
        {
            // Method 1: Create a connection directly from DatabaseConfig
            std::cout << "Creating connection using DatabaseConfig::createConnection()" << std::endl;
            const auto &dbConfig = dbConfigOpt->get();
            auto conn = dbConfig.createConnection();
            std::cout << "Connection created successfully" << std::endl;

            // Use the connection
            std::cout << "Executing query: SELECT 1" << std::endl;
            auto resultSet = conn->executeQuery("SELECT 1");
            std::cout << "Query executed successfully" << std::endl;

            // Close the connection
            conn->close();
            std::cout << "Connection closed" << std::endl;
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::cerr << e.what_s() << std::endl;
        }
    }

    std::cout << "\n=== Example 2: Creating a connection from DriverManager with DatabaseConfig ===" << std::endl;
    if (dbConfigOpt)
    {
        try
        {
            // Method 2: Create a connection from DriverManager with DatabaseConfig
            std::cout << "Creating connection using DriverManager::getConnection(dbConfig)" << std::endl;
            const auto &dbConfig = dbConfigOpt->get();
            auto conn = cpp_dbc::DriverManager::getConnection(dbConfig);
            std::cout << "Connection created successfully" << std::endl;

            // Close the connection
            conn->close();
            std::cout << "Connection closed" << std::endl;
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::cerr << e.what_s() << std::endl;
        }
    }

    std::cout << "\n=== Example 3: Creating a connection from DriverManager with DatabaseConfigManager ===" << std::endl;
    try
    {
        // Method 3: Create a connection from DriverManager with DatabaseConfigManager
        std::cout << "Creating connection using DriverManager::getConnection(configManager, \"dev_mysql\")" << std::endl;
        auto conn = cpp_dbc::DriverManager::getConnection(configManager, "dev_mysql");
        std::cout << "Connection created successfully" << std::endl;

        // Close the connection
        conn->close();
        std::cout << "Connection closed" << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << e.what_s() << std::endl;
    }

    std::cout << "\n=== Example 4: Creating a connection from DatabaseConfigManager ===" << std::endl;
    try
    {
        // Method 4: Create a connection from DatabaseConfigManager
        std::cout << "Creating connection using configManager.createConnection(\"dev_mysql\")" << std::endl;
        auto conn = configManager.createConnection("dev_mysql");
        if (conn)
        {
            std::cout << "Connection created successfully" << std::endl;

            // Close the connection
            conn->close();
            std::cout << "Connection closed" << std::endl;
        }
        else
        {
            std::cerr << "Failed to create connection: Database configuration not found" << std::endl;
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << e.what_s() << std::endl;
    }

    std::cout << "\n=== Example 5: Creating a connection pool ===" << std::endl;
    try
    {
        // Method 5: Create a connection pool
        std::cout << "Creating connection pool using configManager.createConnectionPool(\"dev_mysql\", \"default\")" << std::endl;
        auto pool = configManager.createConnectionPool("dev_mysql", "default");
        if (pool)
        {
            std::cout << "Connection pool created successfully" << std::endl;

            // Get a connection from the pool
            std::cout << "Getting connection from pool" << std::endl;
            auto conn = pool->getConnection();
            std::cout << "Connection obtained from pool" << std::endl;

            // Use the connection
            std::cout << "Executing query: SELECT 1" << std::endl;
            auto resultSet = conn->executeQuery("SELECT 1");
            std::cout << "Query executed successfully" << std::endl;

            // Return the connection to the pool
            std::cout << "Returning connection to pool" << std::endl;
            conn->close();
            std::cout << "Connection returned to pool" << std::endl;

            // Close the pool
            std::cout << "Closing connection pool" << std::endl;
            pool->close();
            std::cout << "Connection pool closed" << std::endl;
        }
        else
        {
            std::cerr << "Failed to create connection pool: Configuration not found" << std::endl;
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << e.what_s() << std::endl;
    }

    return 0;
}