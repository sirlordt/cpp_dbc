// database_config.cpp
// Implementation of the DatabaseConfig and DatabaseConfigManager classes
//
// This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
// See the LICENSE.md file in the project root for more information.
//

#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/cpp_dbc.hpp"
#include "cpp_dbc/connection_pool.hpp"

namespace cpp_dbc
{
    namespace config
    {
        // DatabaseConfig implementation
        std::shared_ptr<Connection> DatabaseConfig::createConnection() const
        {
            return DriverManager::getConnection(
                createConnectionString(),
                getUsername(),
                getPassword(),
                getOptions());
        }

        // DatabaseConfigManager implementation
        std::shared_ptr<Connection> DatabaseConfigManager::createConnection(const std::string &configName) const
        {
            const auto *dbConfig = getDatabaseByName(configName);
            if (!dbConfig)
            {
                return nullptr;
            }

            return dbConfig->createConnection();
        }

        std::shared_ptr<ConnectionPool> DatabaseConfigManager::createConnectionPool(
            const std::string &dbConfigName,
            const std::string &poolConfigName) const
        {
            const auto *dbConfig = getDatabaseByName(dbConfigName);
            if (!dbConfig)
            {
                return nullptr;
            }

            const auto *poolConfig = getConnectionPoolConfig(poolConfigName);
            if (!poolConfig)
            {
                return nullptr;
            }

            // Create a ConnectionPoolConfig with database connection details
            cpp_dbc::config::ConnectionPoolConfig config = *poolConfig;
            config.setUrl(dbConfig->createConnectionString());
            config.setUsername(dbConfig->getUsername());
            config.setPassword(dbConfig->getPassword());

            // Create the connection pool
            return cpp_dbc::ConnectionPool::create(config);
        }
    }
}