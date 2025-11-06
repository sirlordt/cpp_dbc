// driver_manager.cpp
// Implementation of the DriverManager class

#include "cpp_dbc/cpp_dbc.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include <iostream>

namespace cpp_dbc
{
    // Initialize static variables
    std::map<std::string, std::shared_ptr<Driver>> DriverManager::drivers;

    void DriverManager::registerDriver(const std::string &name, std::shared_ptr<Driver> driver)
    {
        // std::cout << "Registering driver: " << name << std::endl;
        // Only register if not already registered to avoid conflicts
        if (drivers.find(name) == drivers.end())
        {
            drivers[name] = driver;
        }
    }

    std::shared_ptr<Connection> DriverManager::getConnection(const std::string &url,
                                                             const std::string &user,
                                                             const std::string &password)
    {
        // std::cout << "Getting connection for URL: " << url << std::endl;

        // Parse the URL to determine which driver to use
        // URL format: cpp_dbc:driverName://host:port/database
        size_t prefixPos = url.find("cpp_dbc:");
        if (prefixPos != 0)
        {
            // std::cout << "Invalid URL format: missing cpp_dbc: prefix" << std::endl;
            throw DBException("Invalid URL format. Expected cpp_dbc:driverName://host:port/database");
        }

        size_t driverEndPos = url.find("://", 7);
        if (driverEndPos == std::string::npos)
        {
            // std::cout << "Invalid URL format: missing :// separator" << std::endl;
            throw DBException("Invalid URL format. Expected cpp_dbc:driverName://host:port/database");
        }

        std::string driverName = url.substr(8, driverEndPos - 8);
        // std::cout << "Looking for driver: '" << driverName << "'" << std::endl;

        // Print all registered drivers (commented out to reduce noise in tests)
        // std::cout << "Registered drivers:" << std::endl;
        // for (const auto &driver : drivers)
        // {
        //     std::cout << "  - '" << driver.first << "'" << std::endl;
        // }

        // Find the driver
        auto it = drivers.find(driverName);
        if (it == drivers.end())
        {
            // std::cout << "Driver '" << driverName << "' not found!" << std::endl;
            throw DBException("No suitable driver found for " + url);
        }

        // std::cout << "Driver '" << driverName << "' found, creating connection..." << std::endl;

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
            throw DBException("Database configuration not found: " + configName);
        }

        return getConnection(
            dbConfig->createConnectionString(),
            dbConfig->getUsername(),
            dbConfig->getPassword());
    }

    std::vector<std::string> DriverManager::getRegisteredDrivers()
    {
        std::vector<std::string> driverNames;
        driverNames.reserve(drivers.size());

        for (const auto &driver : drivers)
        {
            driverNames.push_back(driver.first);
        }

        return driverNames;
    }

    void DriverManager::clearDrivers()
    {
        drivers.clear();
    }

    void DriverManager::unregisterDriver(const std::string &name)
    {
        drivers.erase(name);
    }
}