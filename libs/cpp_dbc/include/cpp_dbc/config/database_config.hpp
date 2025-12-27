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
    class DBConnection;
    class RelationalDBConnectionPool;
}

namespace cpp_dbc
{
    namespace config
    {

        /**
         * @brief Class representing database connection options
         */
        class DBConnectionOptions
        {
        private:
            std::map<std::string, std::string> options;

        public:
            DBConnectionOptions() = default;

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
            std::string m_name;
            std::string m_type;
            std::string m_host;
            unsigned int m_port{0};
            std::string m_database;
            std::string m_username;
            std::string m_password;
            DBConnectionOptions m_options;

        public:
            DatabaseConfig() : m_port(0) {}

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
                const std::string &password) : m_name(name), m_type(type), m_host(host), m_port(port),
                                               m_database(database), m_username(username), m_password(password) {}

            // Getters
            const std::string &getName() const { return m_name; }
            const std::string &getType() const { return m_type; }
            const std::string &getHost() const { return m_host; }
            unsigned int getPort() const { return m_port; }
            const std::string &getDatabase() const { return m_database; }
            const std::string &getUsername() const { return m_username; }
            const std::string &getPassword() const { return m_password; }
            const DBConnectionOptions &getOptionsObj() const { return m_options; }

            /**
             * @brief Get all connection options as a map
             * @return Map of option names to option values
             */
            const std::map<std::string, std::string> &getOptions() const { return m_options.getAllOptions(); }

            // Setters
            void setName(const std::string &value) { m_name = value; }
            void setType(const std::string &value) { m_type = value; }
            void setHost(const std::string &value) { m_host = value; }
            void setPort(unsigned int value) { m_port = value; }
            void setDatabase(const std::string &value) { m_database = value; }
            void setUsername(const std::string &value) { m_username = value; }
            void setPassword(const std::string &value) { m_password = value; }

            /**
             * @brief Set a connection option
             * @param key Option name
             * @param value Option value
             */
            void setOption(const std::string &key, const std::string &value)
            {
                m_options.setOption(key, value);
            }

            /**
             * @brief Get a connection option
             * @param key Option name
             * @param defaultValue Default value if option is not found
             * @return Option value or default value if not found
             */
            std::string getOption(const std::string &key, const std::string &defaultValue = "") const
            {
                return m_options.getOption(key, defaultValue);
            }

            /**
             * @brief Create a connection string for this database
             * @return Connection string in the format "cpp_dbc:type://host:port/database"
             */
            std::string createConnectionString() const
            {
                // SQLite connection strings are different - they don't have host/port
                if (m_host.empty() || m_port == 0)
                {
                    return "cpp_dbc:" + m_type + "://" + m_database;
                }
                else
                {
                    return "cpp_dbc:" + m_type + "://" + m_host + ":" + std::to_string(m_port) + "/" + m_database;
                }
            }

            /**
             * @brief Create a connection using this configuration
             * @return A shared pointer to a DBConnection object
             */
            std::shared_ptr<DBConnection> createDBConnection() const;
        };

        /**
         * @brief Class representing connection pool configuration
         */
        class DBConnectionPoolConfig
        {
        private:
            std::string m_name;
            std::string m_url;
            std::string m_username;
            std::string m_password;
            std::map<std::string, std::string> m_options;
            unsigned int m_initialSize{5};
            unsigned int m_maxSize{20};
            unsigned int m_minIdle{3};
            unsigned long m_connectionTimeout{30000};
            unsigned long m_idleTimeout{300000};
            unsigned long m_validationInterval{5000};
            unsigned long m_maxLifetimeMillis{1800000};
            bool m_testOnBorrow{true};
            bool m_testOnReturn{false};
            std::string m_validationQuery;
            TransactionIsolationLevel m_transactionIsolation;

        public:
            DBConnectionPoolConfig() : /*initialSize(5),
                                     maxSize(20),
                                     minIdle(3),
                                     connectionTimeout(30000),
                                     idleTimeout(300000),
                                     validationInterval(5000),
                                     maxLifetimeMillis(1800000),
                                     testOnBorrow(true),
                                     testOnReturn(false),*/
                                       m_validationQuery("SELECT 1"),
                                       m_transactionIsolation(TransactionIsolationLevel::TRANSACTION_READ_COMMITTED)
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
            DBConnectionPoolConfig(
                const std::string &name,
                unsigned int initialSize,
                unsigned int maxSize,
                unsigned long connectionTimeout,
                unsigned long idleTimeout,
                unsigned long validationInterval) : m_name(name),
                                                    m_initialSize(initialSize),
                                                    m_maxSize(maxSize),
                                                    // m_minIdle(3),
                                                    m_connectionTimeout(connectionTimeout),
                                                    m_idleTimeout(idleTimeout),
                                                    m_validationInterval(validationInterval),
                                                    // m_maxLifetimeMillis(1800000),
                                                    // m_testOnBorrow(true),
                                                    // m_testOnReturn(false),
                                                    m_validationQuery("SELECT 1"),
                                                    m_transactionIsolation(TransactionIsolationLevel::TRANSACTION_READ_COMMITTED)
            {
            }

            /**
             * @brief Full constructor with all parameters
             */
            DBConnectionPoolConfig(
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
                TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED) : m_name(name),
                                                                                                                          m_url(url),
                                                                                                                          m_username(username),
                                                                                                                          m_password(password),
                                                                                                                          m_initialSize(initialSize),
                                                                                                                          m_maxSize(maxSize),
                                                                                                                          m_minIdle(minIdle),
                                                                                                                          m_connectionTimeout(connectionTimeout),
                                                                                                                          m_idleTimeout(idleTimeout),
                                                                                                                          m_validationInterval(validationInterval),
                                                                                                                          m_maxLifetimeMillis(maxLifetimeMillis),
                                                                                                                          m_testOnBorrow(testOnBorrow),
                                                                                                                          m_testOnReturn(testOnReturn),
                                                                                                                          m_validationQuery(validationQuery),
                                                                                                                          m_transactionIsolation(transactionIsolation) {}

            // Getters
            const std::string &getName() const { return m_name; }
            const std::string &getUrl() const { return m_url; }
            const std::string &getUsername() const { return m_username; }
            const std::string &getPassword() const { return m_password; }
            unsigned int getInitialSize() const { return m_initialSize; }
            unsigned int getMaxSize() const { return m_maxSize; }
            unsigned int getMinIdle() const { return m_minIdle; }
            unsigned long getConnectionTimeout() const { return m_connectionTimeout; }
            unsigned long getIdleTimeout() const { return m_idleTimeout; }
            unsigned long getValidationInterval() const { return m_validationInterval; }
            unsigned long getMaxLifetimeMillis() const { return m_maxLifetimeMillis; }
            bool getTestOnBorrow() const { return m_testOnBorrow; }
            bool getTestOnReturn() const { return m_testOnReturn; }
            const std::string &getValidationQuery() const { return m_validationQuery; }
            TransactionIsolationLevel getTransactionIsolation() const { return m_transactionIsolation; }
            const std::map<std::string, std::string> &getOptions() const { return m_options; }

            // Setters
            void setName(const std::string &value) { m_name = value; }
            void setUrl(const std::string &value) { m_url = value; }
            void setUsername(const std::string &value) { m_username = value; }
            void setPassword(const std::string &value) { m_password = value; }
            void setInitialSize(unsigned int value) { m_initialSize = value; }
            void setMaxSize(unsigned int value) { m_maxSize = value; }
            void setMinIdle(unsigned int value) { m_minIdle = value; }
            void setConnectionTimeout(unsigned long value) { m_connectionTimeout = value; }
            void setIdleTimeout(unsigned long value) { m_idleTimeout = value; }
            void setValidationInterval(unsigned long value) { m_validationInterval = value; }
            void setMaxLifetimeMillis(unsigned long value) { m_maxLifetimeMillis = value; }
            void setTestOnBorrow(bool value) { m_testOnBorrow = value; }
            void setTestOnReturn(bool value) { m_testOnReturn = value; }
            void setValidationQuery(const std::string &value) { m_validationQuery = value; }
            void setTransactionIsolation(TransactionIsolationLevel value) { m_transactionIsolation = value; }
            void setOptions(const std::map<std::string, std::string> &value) { m_options = value; }

            /**
             * @brief Configure this pool with database connection details
             * @param dbConfig The database configuration to use
             * @return Reference to this object for method chaining
             */
            DBConnectionPoolConfig &withDatabaseConfig(const DatabaseConfig &dbConfig)
            {
                m_url = dbConfig.createConnectionString();
                m_username = dbConfig.getUsername();
                m_password = dbConfig.getPassword();
                m_options = dbConfig.getOptions();
                return *this;
            }
        };

        /**
         * @brief Class representing test queries for database testing
         */
        class TestQueries
        {
        private:
            std::string m_connectionTest;
            std::map<std::string, std::map<std::string, std::string>> m_databaseQueries;

        public:
            TestQueries() = default;

            /**
             * @brief Set connection test query
             * @param query Query string
             */
            void setConnectionTest(const std::string &query)
            {
                m_connectionTest = query;
            }

            /**
             * @brief Get connection test query
             * @return Query string
             */
            const std::string &getConnectionTest() const
            {
                return m_connectionTest;
            }

            /**
             * @brief Set a test query for a specific database type
             * @param dbType Database type (mysql, postgresql, etc.)
             * @param queryName Query name
             * @param query Query string
             */
            void setQuery(const std::string &dbType, const std::string &queryName, const std::string &query)
            {
                m_databaseQueries[dbType][queryName] = query;
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
                auto dbIt = m_databaseQueries.find(dbType);
                if (dbIt != m_databaseQueries.end())
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
                auto it = m_databaseQueries.find(dbType);
                if (it != m_databaseQueries.end())
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
            std::vector<DatabaseConfig> m_databases;
            std::map<std::string, DBConnectionPoolConfig> m_connectionPools;
            TestQueries m_testQueries;

        public:
            DatabaseConfigManager() = default;

            /**
             * @brief Add a database configuration
             * @param config Database configuration
             */
            void addDatabaseConfig(const DatabaseConfig &config)
            {
                m_databases.push_back(config);
            }

            /**
             * @brief Get all database configurations
             * @return Vector of database configurations
             */
            const std::vector<DatabaseConfig> &getAllDatabases() const
            {
                return m_databases;
            }

            /**
             * @brief Get database configurations of a specific type
             * @param type Database type (mysql, postgresql, etc.)
             * @return Vector of database configurations
             */
            std::vector<DatabaseConfig> getDatabasesByType(const std::string &type) const
            {
                std::vector<DatabaseConfig> result;
                for (const auto &db : m_databases)
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
                for (const auto &db : m_databases)
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
            void addDBConnectionPoolConfig(const DBConnectionPoolConfig &config)
            {
                m_connectionPools[config.getName()] = config;
            }

            /**
             * @brief Get a connection pool configuration by name
             * @param name Connection pool configuration name
             * @return Optional reference to connection pool configuration, empty if not found
             */
            std::optional<std::reference_wrapper<const DBConnectionPoolConfig>> getDBConnectionPoolConfig(const std::string &name = "default") const
            {
                auto it = m_connectionPools.find(name);
                if (it != m_connectionPools.end())
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
                m_testQueries = queries;
            }

            /**
             * @brief Get test queries
             * @return Test queries
             */
            const TestQueries &getTestQueries() const
            {
                return m_testQueries;
            }

            /**
             * @brief Create a connection using a named database configuration
             * @param configName Name of the database configuration
             * @return A shared pointer to a DBConnection object, or nullptr if the configuration doesn't exist
             */
            std::shared_ptr<DBConnection> createDBConnection(const std::string &configName) const;

            /**
             * @brief Create a connection pool using a named database configuration and pool configuration
             * @param dbConfigName Name of the database configuration
             * @param poolConfigName Name of the pool configuration (default: "default")
             * @return A shared pointer to a RelationalDBConnectionPool object, or nullptr if any configuration doesn't exist
             */
            std::shared_ptr<RelationalDBConnectionPool> createDBConnectionPool(const std::string &dbConfigName,
                                                                               const std::string &poolConfigName = "default") const;
        };

    } // namespace config
} // namespace cpp_dbc

#endif // CPP_DBC_DATABASE_CONFIG_HPP