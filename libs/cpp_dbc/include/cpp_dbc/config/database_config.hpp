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

 @file database_config.hpp
 @brief Class representing database connection options

*/

#ifndef CPP_DBC_DATABASE_CONFIG_HPP
#define CPP_DBC_DATABASE_CONFIG_HPP

#include <cpp_dbc/cpp_dbc.hpp>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include <optional>

// Forward declarations
namespace cpp_dbc
{
    class Connection;
    class ConnectionPool;
}

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
            unsigned int port{0};
            std::string database;
            std::string username;
            std::string password;
            ConnectionOptions options;

        public:
            DatabaseConfig() : port(0) {}

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
            unsigned int getPort() const { return port; }
            const std::string &getDatabase() const { return database; }
            const std::string &getUsername() const { return username; }
            const std::string &getPassword() const { return password; }
            const ConnectionOptions &getOptionsObj() const { return options; }

            /**
             * @brief Get all connection options as a map
             * @return Map of option names to option values
             */
            const std::map<std::string, std::string> &getOptions() const { return options.getAllOptions(); }

            // Setters
            void setName(const std::string &value) { name = value; }
            void setType(const std::string &value) { type = value; }
            void setHost(const std::string &value) { host = value; }
            void setPort(unsigned int value) { port = value; }
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
             * @return Connection string in the format "cpp_dbc:type://host:port/database"
             */
            std::string createConnectionString() const
            {
                // SQLite connection strings are different - they don't have host/port
                if (host.empty() || port == 0)
                {
                    return "cpp_dbc:" + type + "://" + database;
                }
                else
                {
                    return "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;
                }
            }

            /**
             * @brief Create a connection using this configuration
             * @return A shared pointer to a Connection object
             */
            std::shared_ptr<Connection> createConnection() const;
        };

        /**
         * @brief Class representing connection pool configuration
         */
        class ConnectionPoolConfig
        {
        private:
            std::string name;
            std::string url;
            std::string username;
            std::string password;
            std::map<std::string, std::string> options;
            unsigned int initialSize{5};
            unsigned int maxSize{20};
            unsigned int minIdle{3};
            unsigned long connectionTimeout{30000};
            unsigned long idleTimeout{300000};
            unsigned long validationInterval{5000};
            unsigned long maxLifetimeMillis{1800000};
            bool testOnBorrow{true};
            bool testOnReturn{false};
            std::string validationQuery;
            TransactionIsolationLevel transactionIsolation;

        public:
            ConnectionPoolConfig() : /*initialSize(5),
                                     maxSize(20),
                                     minIdle(3),
                                     connectionTimeout(30000),
                                     idleTimeout(300000),
                                     validationInterval(5000),
                                     maxLifetimeMillis(1800000),
                                     testOnBorrow(true),
                                     testOnReturn(false),*/
                                     validationQuery("SELECT 1"),
                                     transactionIsolation(TransactionIsolationLevel::TRANSACTION_READ_COMMITTED)
            {
            }

            /**
             * @brief Constructor with basic parameters
             * @param name Pool configuration name
             * @param initialSize Initial pool size
             * @param maxSize Maximum pool size
             * @param connectionTimeout Connection timeout in milliseconds
             * @param idleTimeout Idle timeout in milliseconds
             * @param validationInterval Validation interval in milliseconds
             */
            ConnectionPoolConfig(
                const std::string &name,
                unsigned int initialSize,
                unsigned int maxSize,
                unsigned long connectionTimeout,
                unsigned long idleTimeout,
                unsigned long validationInterval) : name(name),
                                                    initialSize(initialSize),
                                                    maxSize(maxSize),
                                                    // minIdle(3),
                                                    connectionTimeout(connectionTimeout),
                                                    idleTimeout(idleTimeout),
                                                    validationInterval(validationInterval),
                                                    // maxLifetimeMillis(1800000),
                                                    // testOnBorrow(true),
                                                    // testOnReturn(false),
                                                    validationQuery("SELECT 1"),
                                                    transactionIsolation(TransactionIsolationLevel::TRANSACTION_READ_COMMITTED)
            {
            }

            /**
             * @brief Full constructor with all parameters
             */
            ConnectionPoolConfig(
                const std::string &name,
                const std::string &url,
                const std::string &username,
                const std::string &password,
                unsigned int initialSize,
                unsigned int maxSize,
                unsigned int minIdle,
                unsigned long connectionTimeout,
                unsigned long idleTimeout,
                unsigned long validationInterval,
                unsigned long maxLifetimeMillis,
                bool testOnBorrow,
                bool testOnReturn,
                const std::string &validationQuery,
                TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED) : name(name),
                                                                                                                          url(url),
                                                                                                                          username(username),
                                                                                                                          password(password),
                                                                                                                          initialSize(initialSize),
                                                                                                                          maxSize(maxSize),
                                                                                                                          minIdle(minIdle),
                                                                                                                          connectionTimeout(connectionTimeout),
                                                                                                                          idleTimeout(idleTimeout),
                                                                                                                          validationInterval(validationInterval),
                                                                                                                          maxLifetimeMillis(maxLifetimeMillis),
                                                                                                                          testOnBorrow(testOnBorrow),
                                                                                                                          testOnReturn(testOnReturn),
                                                                                                                          validationQuery(validationQuery),
                                                                                                                          transactionIsolation(transactionIsolation) {}

            // Getters
            const std::string &getName() const { return name; }
            const std::string &getUrl() const { return url; }
            const std::string &getUsername() const { return username; }
            const std::string &getPassword() const { return password; }
            unsigned int getInitialSize() const { return initialSize; }
            unsigned int getMaxSize() const { return maxSize; }
            unsigned int getMinIdle() const { return minIdle; }
            unsigned long getConnectionTimeout() const { return connectionTimeout; }
            unsigned long getIdleTimeout() const { return idleTimeout; }
            unsigned long getValidationInterval() const { return validationInterval; }
            unsigned long getMaxLifetimeMillis() const { return maxLifetimeMillis; }
            bool getTestOnBorrow() const { return testOnBorrow; }
            bool getTestOnReturn() const { return testOnReturn; }
            const std::string &getValidationQuery() const { return validationQuery; }
            TransactionIsolationLevel getTransactionIsolation() const { return transactionIsolation; }
            const std::map<std::string, std::string> &getOptions() const { return options; }

            // Setters
            void setName(const std::string &value) { name = value; }
            void setUrl(const std::string &value) { url = value; }
            void setUsername(const std::string &value) { username = value; }
            void setPassword(const std::string &value) { password = value; }
            void setInitialSize(unsigned int value) { initialSize = value; }
            void setMaxSize(unsigned int value) { maxSize = value; }
            void setMinIdle(unsigned int value) { minIdle = value; }
            void setConnectionTimeout(unsigned long value) { connectionTimeout = value; }
            void setIdleTimeout(unsigned long value) { idleTimeout = value; }
            void setValidationInterval(unsigned long value) { validationInterval = value; }
            void setMaxLifetimeMillis(unsigned long value) { maxLifetimeMillis = value; }
            void setTestOnBorrow(bool value) { testOnBorrow = value; }
            void setTestOnReturn(bool value) { testOnReturn = value; }
            void setValidationQuery(const std::string &value) { validationQuery = value; }
            void setTransactionIsolation(TransactionIsolationLevel value) { transactionIsolation = value; }
            void setOptions(const std::map<std::string, std::string> &value) { options = value; }

            /**
             * @brief Configure this pool with database connection details
             * @param dbConfig The database configuration to use
             * @return Reference to this object for method chaining
             */
            ConnectionPoolConfig &withDatabaseConfig(const DatabaseConfig &dbConfig)
            {
                url = dbConfig.createConnectionString();
                username = dbConfig.getUsername();
                password = dbConfig.getPassword();
                options = dbConfig.getOptions();
                return *this;
            }
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
             * @return Optional reference to database configuration, empty if not found
             */
            std::optional<std::reference_wrapper<const DatabaseConfig>> getDatabaseByName(const std::string &name) const
            {
                for (const auto &db : databases)
                {
                    if (db.getName() == name)
                    {
                        return std::cref(db);
                    }
                }
                return std::nullopt;
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
             * @return Optional reference to connection pool configuration, empty if not found
             */
            std::optional<std::reference_wrapper<const ConnectionPoolConfig>> getConnectionPoolConfig(const std::string &name = "default") const
            {
                auto it = connectionPools.find(name);
                if (it != connectionPools.end())
                {
                    return std::cref(it->second);
                }
                return std::nullopt;
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

            /**
             * @brief Create a connection using a named database configuration
             * @param configName Name of the database configuration
             * @return A shared pointer to a Connection object, or nullptr if the configuration doesn't exist
             */
            std::shared_ptr<Connection> createConnection(const std::string &configName) const;

            /**
             * @brief Create a connection pool using a named database configuration and pool configuration
             * @param dbConfigName Name of the database configuration
             * @param poolConfigName Name of the pool configuration (default: "default")
             * @return A shared pointer to a ConnectionPool object, or nullptr if any configuration doesn't exist
             */
            std::shared_ptr<ConnectionPool> createConnectionPool(const std::string &dbConfigName,
                                                                 const std::string &poolConfigName = "default") const;
        };

    } // namespace config
} // namespace cpp_dbc

#endif // CPP_DBC_DATABASE_CONFIG_HPP