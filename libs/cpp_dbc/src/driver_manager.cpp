/*

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

 @file driver_manager.cpp
 @brief Implementation for the cpp_dbc library

*/

#include "cpp_dbc/cpp_dbc.hpp"
#include "cpp_dbc/common/system_constants.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include <iostream>

namespace cpp_dbc
{
    // Initialize static variables
    std::map<std::string, std::shared_ptr<DBDriver>> DriverManager::drivers;

    void DriverManager::registerDriver(const std::string &name, std::shared_ptr<DBDriver> driver)
    {
        // std::cout << "Registering driver: " << name << std::endl;
        // Only register if not already registered to avoid conflicts
        if (!drivers.contains(name))
        {
            drivers[name] = driver;
        }
    }

    void DriverManager::registerDriver(std::shared_ptr<DBDriver> driver)
    {
        // std::cout << "Registering driver: " << name << std::endl;
        // Only register if not already registered to avoid conflicts
        if (!drivers.contains(driver->getName()))
        {
            drivers[driver->getName()] = driver;
        }
    }

#ifdef __cpp_exceptions

    std::shared_ptr<DBConnection> DriverManager::getDBConnection(const std::string &url,
                                                                 const std::string &user,
                                                                 const std::string &password,
                                                                 const std::map<std::string, std::string> &options)
    {
        auto result = getDBConnection(std::nothrow, url, user, password, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DBConnection> DriverManager::getDBConnection(const config::DatabaseConfig &dbConfig)
    {
        auto result = getDBConnection(std::nothrow, dbConfig);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DBConnection> DriverManager::getDBConnection(const config::DatabaseConfigManager &configManager,
                                                                 const std::string &configName)
    {
        auto result = getDBConnection(std::nothrow, configManager, configName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException>
    DriverManager::getDBConnection(std::nothrow_t,
                                   const std::string &url,
                                   const std::string &user,
                                   const std::string &password,
                                   const std::map<std::string, std::string> &options) noexcept
    {
        // Parse the URL to determine which driver to use
        // URL format: cpp_dbc:driverName://host:port/database
        if (!url.starts_with(system_constants::URI_PREFIX))
        {
            return cpp_dbc::unexpected(DBException("1S2T3U4V5W6X", "Invalid URL format. Expected cpp_dbc:driverName://host:port/database", system_utils::captureCallStack()));
        }

        size_t driverEndPos = url.find("://", system_constants::URI_PREFIX.size());
        if (driverEndPos == std::string::npos)
        {
            return cpp_dbc::unexpected(DBException("7Y8Z9A0B1C2D", "Invalid URL format. Expected cpp_dbc:driverName://host:port/database", system_utils::captureCallStack()));
        }

        std::string driverName = url.substr(8, driverEndPos - 8);

        // Find the driver
        auto it = drivers.find(driverName);
        if (it == drivers.end())
        {
            return cpp_dbc::unexpected(DBException("3E4F5G6H7I8J", "No suitable driver found for " + url, system_utils::captureCallStack()));
        }

        // Use the driver to create a connection (nothrow overload — logic lives there)
        return it->second->connect(std::nothrow, url, user, password, options);
    }

    cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException>
    DriverManager::getDBConnection(std::nothrow_t,
                                   const config::DatabaseConfig &dbConfig) noexcept
    {
        return getDBConnection(std::nothrow,
                               dbConfig.createConnectionString(),
                               dbConfig.getUsername(),
                               dbConfig.getPassword(),
                               dbConfig.getOptions());
    }

    cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException>
    DriverManager::getDBConnection(std::nothrow_t,
                                   const config::DatabaseConfigManager &configManager,
                                   const std::string &configName) noexcept
    {
        auto dbConfigOpt = configManager.getDatabaseByName(configName);
        if (!dbConfigOpt.has_value())
        {
            return cpp_dbc::unexpected(DBException("9K0L1M2N3O4P", "Database configuration not found: " + configName, system_utils::captureCallStack()));
        }

        const auto &dbConfig = dbConfigOpt->get();
        return getDBConnection(std::nothrow,
                               dbConfig.createConnectionString(),
                               dbConfig.getUsername(),
                               dbConfig.getPassword(),
                               dbConfig.getOptions());
    }

    std::vector<std::string> DriverManager::getRegisteredDrivers()
    {
        std::vector<std::string> driverNames;
        driverNames.reserve(drivers.size());

        for (const auto &[name, driverPtr] : drivers)
        {
            driverNames.push_back(name);
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

    std::optional<std::shared_ptr<DBDriver>> DriverManager::getDriver(const std::string &name)
    {
        auto it = drivers.find(name);
        if (it == drivers.end())
        {
            return std::nullopt;
        }
        return it->second;
    }
}