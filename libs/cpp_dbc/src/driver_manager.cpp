// driver_manager.cpp
// Implementation of the DriverManager class

#include "cpp_dbc/cpp_dbc.hpp"
#include "cpp_dbc/config/database_config.hpp"

namespace cpp_dbc
{
    // Initialize static variables
    std::map<std::string, std::shared_ptr<Driver>> DriverManager::drivers;

    void DriverManager::registerDriver(const std::string &name, std::shared_ptr<Driver> driver)
    {
        drivers[name] = driver;
    }

    std::shared_ptr<Connection> DriverManager::getConnection(const std::string &url,
                                                             const std::string &user,
                                                             const std::string &password)
    {
        // Parse the URL to determine which driver to use
        // URL format: cpp_dbc:driverName://host:port/database
        size_t prefixPos = url.find("cpp_dbc:");
        if (prefixPos != 0)
        {
            throw SQLException("Invalid URL format. Expected cpp_dbc:driverName://host:port/database");
        }

        size_t driverEndPos = url.find("://", 7);
        if (driverEndPos == std::string::npos)
        {
            throw SQLException("Invalid URL format. Expected cpp_dbc:driverName://host:port/database");
        }

        std::string driverName = url.substr(7, driverEndPos - 7);

        // Find the driver
        auto it = drivers.find(driverName);
        if (it == drivers.end())
        {
            throw SQLException("No suitable driver found for " + url);
        }

        // Use the driver to create a connection
        return it->second->connect(url, user, password);
    }

    std::shared_ptr<Connection> DriverManager::getConnection(const config::DatabaseConfig &dbConfig)
    {
        return getConnection(
            dbConfig.createConnectionString(),
            dbConfig.getUsername(),
            dbConfig.getPassword());
    }

    std::shared_ptr<Connection> DriverManager::getConnection(const config::DatabaseConfigManager &configManager,
                                                             const std::string &configName)
    {
        const auto *dbConfig = configManager.getDatabaseByName(configName);
        if (!dbConfig)
        {
            throw SQLException("Database configuration not found: " + configName);
        }

        return getConnection(
            dbConfig->createConnectionString(),
            dbConfig->getUsername(),
            dbConfig->getPassword());
    }
}