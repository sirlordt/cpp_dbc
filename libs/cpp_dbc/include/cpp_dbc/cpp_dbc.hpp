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

 @file cpp_dbc.hpp
 @brief Main header for the cpp_dbc library - Multi-paradigm database connectivity

*/

#ifndef CPP_DBC_HPP
#define CPP_DBC_HPP

// Configuration macros for conditional compilation
#ifndef USE_MYSQL
#define USE_MYSQL 1 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_POSTGRESQL
#define USE_POSTGRESQL 1 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_SQLITE
#define USE_SQLITE 0 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_MONGODB
#define USE_MONGODB 0 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_SCYLLADB
#define USE_SCYLLADB 0 // NOSONAR - Macro required for conditional compilation
#endif

#ifndef USE_REDIS
#define USE_REDIS 0 // NOSONAR - Macro required for conditional compilation
#endif

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <stdexcept>
#include <cstdint>
#include <any>
#include <optional>

// Core headers
#include "cpp_dbc/core/db_types.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/core/streams.hpp"
#include "cpp_dbc/core/db_result_set.hpp"
#include "cpp_dbc/core/db_connection.hpp"
#include "cpp_dbc/core/db_driver.hpp"

// Relational database headers
#include "cpp_dbc/core/relational/relational_db_result_set.hpp"
#include "cpp_dbc/core/relational/relational_db_prepared_statement.hpp"
#include "cpp_dbc/core/relational/relational_db_connection.hpp"
#include "cpp_dbc/core/relational/relational_db_driver.hpp"

// Common utilities
#include "cpp_dbc/backward.hpp"
#include "cpp_dbc/common/system_utils.hpp"

// Driver implementations (conditionally included)
#if USE_MYSQL
#include "cpp_dbc/drivers/relational/driver_mysql.hpp"
#endif

#if USE_POSTGRESQL
#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"
#endif

#if USE_SQLITE
#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#endif

#if USE_FIREBIRD
#include "cpp_dbc/drivers/relational/driver_firebird.hpp"
#endif

#if USE_MONGODB
#include "cpp_dbc/drivers/document/driver_mongodb.hpp"
#endif

#if USE_SCYLLADB
#include "cpp_dbc/drivers/columnar/driver_scylladb.hpp"
#endif

#if USE_REDIS
#include "cpp_dbc/drivers/kv/driver_redis.hpp"
#endif

// Forward declaration of configuration classes
namespace cpp_dbc::config
{
    class DatabaseConfig;
    class DatabaseConfigManager;
}

namespace cpp_dbc
{

    /**
     * @brief Central manager for registering database drivers and obtaining connections
     *
     * DriverManager is the main entry point for the cpp_dbc library. It maintains
     * a registry of database drivers and provides factory methods to create
     * database connections from URLs.
     *
     * ```cpp
     * // Register a MySQL driver and obtain a connection
     * cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
     * auto conn = cpp_dbc::DriverManager::getDBConnection(
     *     "jdbc:mysql://localhost:3306/mydb", "user", "pass");
     * // Use conn, then close when done
     * conn->close();
     * ```
     *
     * ```cpp
     * // Cast to a specific paradigm connection
     * auto relConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
     *     cpp_dbc::DriverManager::getDBConnection("jdbc:mysql://localhost/mydb", "u", "p"));
     * auto rs = relConn->executeQuery("SELECT id, name FROM users");
     * while (rs->next()) {
     *     std::cout << rs->getString("name") << std::endl;
     * }
     * relConn->close();
     * ```
     */
    class DriverManager
    {
    private:
        static std::map<std::string, std::shared_ptr<DBDriver>> drivers;

    public:
        /**
         * @brief Register a driver using its own name (from getName())
         *
         * @param driver The driver instance to register
         * @throws DBException if a driver with the same name is already registered
         *
         * ```cpp
         * cpp_dbc::DriverManager::registerDriver(
         *     std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
         * ```
         */
        static void registerDriver(std::shared_ptr<DBDriver> driver);

        /**
         * @brief Register a driver under a custom name
         *
         * @param name The name to register the driver under
         * @param driver The driver instance to register
         * @throws DBException if a driver with the same name is already registered
         *
         * ```cpp
         * cpp_dbc::DriverManager::registerDriver("my-mysql",
         *     std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
         * ```
         */
        static void registerDriver(const std::string &name, std::shared_ptr<DBDriver> driver);

        /**
         * @brief Create a database connection from a URL
         *
         * Iterates through registered drivers to find one that accepts the URL,
         * then uses it to create a connection.
         *
         * @param url The database URL (e.g., "jdbc:mysql://host:port/database")
         * @param user The username for authentication
         * @param password The password for authentication
         * @param options Additional driver-specific connection options
         * @return A shared pointer to the new connection
         * @throws DBException if no driver accepts the URL or connection fails
         *
         * ```cpp
         * auto conn = cpp_dbc::DriverManager::getDBConnection(
         *     "jdbc:mysql://localhost:3306/mydb", "root", "secret");
         * // ... use connection ...
         * conn->close();
         * ```
         */
        static std::shared_ptr<DBConnection> getDBConnection(const std::string &url,
                                                             const std::string &user,
                                                             const std::string &password,
                                                             const std::map<std::string, std::string> &options = std::map<std::string, std::string>());

        /**
         * @brief Create a database connection from a DatabaseConfig object
         *
         * @param dbConfig The database configuration
         * @return A shared pointer to the new connection
         * @throws DBException if no driver accepts the URL or connection fails
         *
         * ```cpp
         * auto configMgr = cpp_dbc::config::YamlConfigLoader::loadFromFile("db.yaml");
         * auto conn = cpp_dbc::DriverManager::getDBConnection(configMgr.getConfig("mysql_main"));
         * ```
         */
        static std::shared_ptr<DBConnection> getDBConnection(const config::DatabaseConfig &dbConfig);

        /**
         * @brief Create a database connection by config name from a ConfigManager
         *
         * @param configManager The configuration manager containing named configs
         * @param configName The name of the configuration to use
         * @return A shared pointer to the new connection
         * @throws DBException if config not found, no driver accepts the URL, or connection fails
         *
         * ```cpp
         * auto configMgr = cpp_dbc::config::YamlConfigLoader::loadFromFile("db.yaml");
         * auto conn = cpp_dbc::DriverManager::getDBConnection(configMgr, "mysql_main");
         * ```
         */
        static std::shared_ptr<DBConnection> getDBConnection(const config::DatabaseConfigManager &configManager,
                                                             const std::string &configName);

        /**
         * @brief Get a list of all registered driver names
         *
         * @return Vector of driver names
         *
         * ```cpp
         * for (const auto& name : cpp_dbc::DriverManager::getRegisteredDrivers()) {
         *     std::cout << "Driver: " << name << std::endl;
         * }
         * ```
         */
        static std::vector<std::string> getRegisteredDrivers();

        /**
         * @brief Remove all registered drivers
         *
         * Useful for testing or application shutdown.
         */
        static void clearDrivers();

        /**
         * @brief Remove a specific driver by name
         *
         * @param name The name of the driver to remove
         */
        static void unregisterDriver(const std::string &name);

        /**
         * @brief Get a driver instance by name
         *
         * @param name The name of the driver
         * @return The driver if found, std::nullopt otherwise
         *
         * ```cpp
         * if (auto driver = cpp_dbc::DriverManager::getDriver("mysql")) {
         *     auto conn = (*driver)->connect("jdbc:mysql://localhost/mydb", "u", "p");
         * }
         * ```
         */
        static std::optional<std::shared_ptr<DBDriver>> getDriver(const std::string &name);
    };

} // namespace cpp_dbc

#endif // CPP_DBC_HPP
