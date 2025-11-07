/**
 * @file config_example.cpp
 * @brief Example of using the database configuration classes
 */

#include <cpp_dbc/config/database_config.hpp>
#include <iostream>
#include <memory>

// Include the YAML loader if YAML support is enabled
#ifdef USE_CPP_YAML
#include <cpp_dbc/config/yaml_config_loader.hpp>
#endif

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
    cpp_dbc::config::ConnectionPoolConfig poolConfig;
    poolConfig.setName("default");
    poolConfig.setInitialSize(5);
    poolConfig.setMaxSize(20);
    poolConfig.setConnectionTimeout(5000);
    poolConfig.setIdleTimeout(60000);
    poolConfig.setValidationInterval(30000);

    // Add the pool configuration to the manager
    configManager.addConnectionPoolConfig(poolConfig);

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
    std::cout << "Database Configurations:" << std::endl;
    std::cout << "=======================" << std::endl;

    for (const auto &dbConfig : configManager.getAllDatabases())
    {
        std::cout << "Name: " << dbConfig.getName() << std::endl;
        std::cout << "Type: " << dbConfig.getType() << std::endl;
        std::cout << "Host: " << dbConfig.getHost() << std::endl;
        std::cout << "Port: " << dbConfig.getPort() << std::endl;
        std::cout << "Database: " << dbConfig.getDatabase() << std::endl;
        std::cout << "Username: " << dbConfig.getUsername() << std::endl;
        std::cout << "Password: " << dbConfig.getPassword() << std::endl;

        std::cout << "Options:" << std::endl;
        for (const auto &option : dbConfig.getOptions())
        {
            std::cout << "  " << option.first << ": " << option.second << std::endl;
        }

        std::cout << "Connection String: " << dbConfig.createConnectionString() << std::endl;
        std::cout << std::endl;
    }
}

// Function to print connection pool configurations
void printConnectionPoolConfigs(const cpp_dbc::config::DatabaseConfigManager &configManager)
{
    std::cout << "Connection Pool Configurations:" << std::endl;
    std::cout << "==============================" << std::endl;

    const auto *poolConfig = configManager.getConnectionPoolConfig("default");
    if (poolConfig)
    {
        std::cout << "Name: " << poolConfig->getName() << std::endl;
        std::cout << "Initial Size: " << poolConfig->getInitialSize() << std::endl;
        std::cout << "Max Size: " << poolConfig->getMaxSize() << std::endl;
        std::cout << "Connection Timeout: " << poolConfig->getConnectionTimeout() << " ms" << std::endl;
        std::cout << "Idle Timeout: " << poolConfig->getIdleTimeout() << " ms" << std::endl;
        std::cout << "Validation Interval: " << poolConfig->getValidationInterval() << " ms" << std::endl;
        std::cout << std::endl;
    }
}

// Function to print test queries
void printTestQueries(const cpp_dbc::config::DatabaseConfigManager &configManager)
{
    std::cout << "Test Queries:" << std::endl;
    std::cout << "============" << std::endl;

    const auto &testQueries = configManager.getTestQueries();

    std::cout << "Connection Test: " << testQueries.getConnectionTest() << std::endl;
    std::cout << std::endl;

    std::cout << "MySQL Queries:" << std::endl;
    for (const auto &query : testQueries.getQueriesForType("mysql"))
    {
        std::cout << "  " << query.first << ": " << query.second << std::endl;
    }
    std::cout << std::endl;

    std::cout << "PostgreSQL Queries:" << std::endl;
    for (const auto &query : testQueries.getQueriesForType("postgresql"))
    {
        std::cout << "  " << query.first << ": " << query.second << std::endl;
    }
    std::cout << std::endl;
}

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
        configManager = createConfigProgrammatically();
    }

    // Print configurations
    printDatabaseConfigs(configManager);
    printConnectionPoolConfigs(configManager);
    printTestQueries(configManager);

    // Example of using the configuration to create a connection
    std::cout << "Example of using the configuration to create a connection:" << std::endl;
    std::cout << "=======================================================" << std::endl;

    const auto *dbConfig = configManager.getDatabaseByName("dev_mysql");
    if (dbConfig)
    {
        std::cout << "Creating connection to MySQL database:" << std::endl;
        std::cout << "Connection String: " << dbConfig->createConnectionString() << std::endl;
        std::cout << "Username: " << dbConfig->getUsername() << std::endl;
        std::cout << "Password: " << dbConfig->getPassword() << std::endl;

        // In a real application, you would use the connection string to create a connection
        // For example:
        // auto conn = cpp_dbc::DriverManager::getConnection(
        //     dbConfig->createConnectionString(),
        //     dbConfig->getUsername(),
        //     dbConfig->getPassword()
        // );
    }

    return 0;
}