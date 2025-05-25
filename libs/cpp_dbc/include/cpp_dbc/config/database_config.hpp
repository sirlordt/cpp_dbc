#ifndef CPP_DBC_DATABASE_CONFIG_HPP
#define CPP_DBC_DATABASE_CONFIG_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>

namespace cpp_dbc
{
    namespace config
    {

        /**
         * @brief Class representing database connection options
         */
        class ConnectionOptions
        {
        private:
            std::map<std::string, std::string> options;

        public:
            ConnectionOptions() = default;

            /**
             * @brief Set an option value
             * @param key Option name
             * @param value Option value
             */
            void setOption(const std::string &key, const std::string &value)
            {
                options[key] = value;
            }

            /**
             * @brief Get an option value
             * @param key Option name
             * @param defaultValue Default value if option is not found
             * @return Option value or default value if not found
             */
            std::string getOption(const std::string &key, const std::string &defaultValue = "") const
            {
                auto it = options.find(key);
                if (it != options.end())
                {
                    return it->second;
                }
                return defaultValue;
            }

            /**
             * @brief Check if an option exists
             * @param key Option name
             * @return true if option exists, false otherwise
             */
            bool hasOption(const std::string &key) const
            {
                return options.find(key) != options.end();
            }

            /**
             * @brief Get all options
             * @return Map of all options
             */
            const std::map<std::string, std::string> &getAllOptions() const
            {
                return options;
            }
        };

        /**
         * @brief Class representing a database configuration
         */
        class DatabaseConfig
        {
        private:
            std::string name;
            std::string type;
            std::string host;
            int port;
            std::string database;
            std::string username;
            std::string password;
            ConnectionOptions options;

        public:
            DatabaseConfig() = default;

            /**
             * @brief Constructor with parameters
             * @param name Database configuration name
             * @param type Database type (mysql, postgresql, etc.)
             * @param host Database host
             * @param port Database port
             * @param database Database name
             * @param username Database username
             * @param password Database password
             */
            DatabaseConfig(
                const std::string &name,
                const std::string &type,
                const std::string &host,
                int port,
                const std::string &database,
                const std::string &username,
                const std::string &password) : name(name), type(type), host(host), port(port),
                                               database(database), username(username), password(password) {}

            // Getters
            const std::string &getName() const { return name; }
            const std::string &getType() const { return type; }
            const std::string &getHost() const { return host; }
            int getPort() const { return port; }
            const std::string &getDatabase() const { return database; }
            const std::string &getUsername() const { return username; }
            const std::string &getPassword() const { return password; }
            const ConnectionOptions &getOptions() const { return options; }

            // Setters
            void setName(const std::string &value) { name = value; }
            void setType(const std::string &value) { type = value; }
            void setHost(const std::string &value) { host = value; }
            void setPort(int value) { port = value; }
            void setDatabase(const std::string &value) { database = value; }
            void setUsername(const std::string &value) { username = value; }
            void setPassword(const std::string &value) { password = value; }

            /**
             * @brief Set a connection option
             * @param key Option name
             * @param value Option value
             */
            void setOption(const std::string &key, const std::string &value)
            {
                options.setOption(key, value);
            }

            /**
             * @brief Get a connection option
             * @param key Option name
             * @param defaultValue Default value if option is not found
             * @return Option value or default value if not found
             */
            std::string getOption(const std::string &key, const std::string &defaultValue = "") const
            {
                return options.getOption(key, defaultValue);
            }

            /**
             * @brief Create a connection string for this database
             * @return Connection string in the format "cppdbc:type://host:port/database"
             */
            std::string createConnectionString() const
            {
                return "cppdbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;
            }
        };

        /**
         * @brief Class representing connection pool configuration
         */
        class ConnectionPoolConfig
        {
        private:
            std::string name;
            int initialSize;
            int maxSize;
            int connectionTimeout;
            int idleTimeout;
            int validationInterval;

        public:
            ConnectionPoolConfig() = default;

            /**
             * @brief Constructor with parameters
             * @param name Pool configuration name
             * @param initialSize Initial pool size
             * @param maxSize Maximum pool size
             * @param connectionTimeout Connection timeout in milliseconds
             * @param idleTimeout Idle timeout in milliseconds
             * @param validationInterval Validation interval in milliseconds
             */
            ConnectionPoolConfig(
                const std::string &name,
                int initialSize,
                int maxSize,
                int connectionTimeout,
                int idleTimeout,
                int validationInterval) : name(name), initialSize(initialSize), maxSize(maxSize),
                                          connectionTimeout(connectionTimeout), idleTimeout(idleTimeout),
                                          validationInterval(validationInterval) {}

            // Getters
            const std::string &getName() const { return name; }
            int getInitialSize() const { return initialSize; }
            int getMaxSize() const { return maxSize; }
            int getConnectionTimeout() const { return connectionTimeout; }
            int getIdleTimeout() const { return idleTimeout; }
            int getValidationInterval() const { return validationInterval; }

            // Setters
            void setName(const std::string &value) { name = value; }
            void setInitialSize(int value) { initialSize = value; }
            void setMaxSize(int value) { maxSize = value; }
            void setConnectionTimeout(int value) { connectionTimeout = value; }
            void setIdleTimeout(int value) { idleTimeout = value; }
            void setValidationInterval(int value) { validationInterval = value; }
        };

        /**
         * @brief Class representing test queries for database testing
         */
        class TestQueries
        {
        private:
            std::string connectionTest;
            std::map<std::string, std::map<std::string, std::string>> databaseQueries;

        public:
            TestQueries() = default;

            /**
             * @brief Set connection test query
             * @param query Query string
             */
            void setConnectionTest(const std::string &query)
            {
                connectionTest = query;
            }

            /**
             * @brief Get connection test query
             * @return Query string
             */
            const std::string &getConnectionTest() const
            {
                return connectionTest;
            }

            /**
             * @brief Set a test query for a specific database type
             * @param dbType Database type (mysql, postgresql, etc.)
             * @param queryName Query name
             * @param query Query string
             */
            void setQuery(const std::string &dbType, const std::string &queryName, const std::string &query)
            {
                databaseQueries[dbType][queryName] = query;
            }

            /**
             * @brief Get a test query for a specific database type
             * @param dbType Database type (mysql, postgresql, etc.)
             * @param queryName Query name
             * @param defaultValue Default value if query is not found
             * @return Query string or default value if not found
             */
            std::string getQuery(const std::string &dbType, const std::string &queryName, const std::string &defaultValue = "") const
            {
                auto dbIt = databaseQueries.find(dbType);
                if (dbIt != databaseQueries.end())
                {
                    auto queryIt = dbIt->second.find(queryName);
                    if (queryIt != dbIt->second.end())
                    {
                        return queryIt->second;
                    }
                }
                return defaultValue;
            }

            /**
             * @brief Get all queries for a specific database type
             * @param dbType Database type (mysql, postgresql, etc.)
             * @return Map of query names to query strings
             */
            std::map<std::string, std::string> getQueriesForType(const std::string &dbType) const
            {
                auto it = databaseQueries.find(dbType);
                if (it != databaseQueries.end())
                {
                    return it->second;
                }
                return {};
            }
        };

        /**
         * @brief Class managing database configurations
         */
        class DatabaseConfigManager
        {
        private:
            std::vector<DatabaseConfig> databases;
            std::map<std::string, ConnectionPoolConfig> connectionPools;
            TestQueries testQueries;

        public:
            DatabaseConfigManager() = default;

            /**
             * @brief Add a database configuration
             * @param config Database configuration
             */
            void addDatabaseConfig(const DatabaseConfig &config)
            {
                databases.push_back(config);
            }

            /**
             * @brief Get all database configurations
             * @return Vector of database configurations
             */
            const std::vector<DatabaseConfig> &getAllDatabases() const
            {
                return databases;
            }

            /**
             * @brief Get database configurations of a specific type
             * @param type Database type (mysql, postgresql, etc.)
             * @return Vector of database configurations
             */
            std::vector<DatabaseConfig> getDatabasesByType(const std::string &type) const
            {
                std::vector<DatabaseConfig> result;
                for (const auto &db : databases)
                {
                    if (db.getType() == type)
                    {
                        result.push_back(db);
                    }
                }
                return result;
            }

            /**
             * @brief Get a database configuration by name
             * @param name Database configuration name
             * @return Pointer to database configuration or nullptr if not found
             */
            const DatabaseConfig *getDatabaseByName(const std::string &name) const
            {
                for (const auto &db : databases)
                {
                    if (db.getName() == name)
                    {
                        return &db;
                    }
                }
                return nullptr;
            }

            /**
             * @brief Add a connection pool configuration
             * @param config Connection pool configuration
             */
            void addConnectionPoolConfig(const ConnectionPoolConfig &config)
            {
                connectionPools[config.getName()] = config;
            }

            /**
             * @brief Get a connection pool configuration by name
             * @param name Connection pool configuration name
             * @return Pointer to connection pool configuration or nullptr if not found
             */
            const ConnectionPoolConfig *getConnectionPoolConfig(const std::string &name = "default") const
            {
                auto it = connectionPools.find(name);
                if (it != connectionPools.end())
                {
                    return &it->second;
                }
                return nullptr;
            }

            /**
             * @brief Set test queries
             * @param queries Test queries
             */
            void setTestQueries(const TestQueries &queries)
            {
                testQueries = queries;
            }

            /**
             * @brief Get test queries
             * @return Test queries
             */
            const TestQueries &getTestQueries() const
            {
                return testQueries;
            }
        };

    } // namespace config
} // namespace cpp_dbc

#endif // CPP_DBC_DATABASE_CONFIG_HPP