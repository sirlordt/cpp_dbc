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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file db_driver.hpp
 * @brief Abstract base class for all database drivers
 */

#ifndef CPP_DBC_CORE_DB_DRIVER_HPP
#define CPP_DBC_CORE_DB_DRIVER_HPP

#include <any>
#include <map>
#include <memory>
#include <string>
#include "db_connection.hpp"
#include "db_types.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract base class for all database drivers
     *
     * Drivers are responsible for creating connections to specific database engines.
     * Register drivers with DriverManager, then use DriverManager::getDBConnection()
     * to obtain connections automatically.
     *
     * ```cpp
     * // Register and use a driver directly
     * auto driver = std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>();
     * if (driver->acceptsURL("cpp_dbc:mysql://localhost/mydb")) {
     *     auto conn = driver->connect("cpp_dbc:mysql://localhost/mydb", "user", "pass");
     *     // ... use connection ...
     *     conn->close();
     * }
     * ```
     *
     * @see DriverManager, RelationalDBDriver, DocumentDBDriver, KVDBDriver, ColumnarDBDriver
     */
    class DBDriver
    {
    public:
        virtual ~DBDriver() = default;

        /**
         * @brief Connect to a database
         *
         * @param url The database URL (e.g., "cpp_dbc:mysql://host:port/database")
         * @param user The username for authentication
         * @param password The password for authentication
         * @param options Additional connection options
         * @return std::shared_ptr<DBConnection> A connection to the database
         * @throws DBException if the connection fails
         *
         * ```cpp
         * auto conn = driver->connect("cpp_dbc:mysql://localhost:3306/mydb", "root", "pass");
         * // ... use connection ...
         * conn->close();
         * ```
         */
        virtual std::shared_ptr<DBConnection> connect(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) = 0;

        /**
         * @brief Check if this driver accepts the given URL
         *
         * Each driver recognizes a specific URL scheme (e.g., "cpp_dbc:mysql://", "cpp_dbc:postgresql://").
         *
         * @param url The database URL to check
         * @return true if this driver can handle the URL
         * @return false if this driver cannot handle the URL
         *
         * ```cpp
         * auto driver = std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>();
         * bool ok = driver->acceptsURL("cpp_dbc:mysql://localhost/mydb");  // true
         * bool no = driver->acceptsURL("cpp_dbc:postgresql://localhost/mydb");  // false
         * ```
         */
        virtual bool acceptsURL(const std::string &url) = 0;

        /**
         * @brief Get the database type supported by this driver
         *
         * @return DBType The type of database (RELATIONAL, DOCUMENT, KEY_VALUE, COLUMNAR)
         */
        virtual DBType getDBType() const = 0;

        /**
         * @brief Execute a driver-specific command without requiring a connection
         *
         * Supported commands vary by driver:
         * - Firebird: "create_database" - Creates a new database file
         *
         * @param params A map of parameters for the command:
         *               - "command": The command name (e.g., "create_database")
         *               - "url": The database URL
         *               - "user": The username
         *               - "password": The password
         * @return Result code (0 for success, non-zero for errors)
         * @throws DBException if the command fails
         *
         * ```cpp
         * auto driver = std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>();
         * std::map<std::string, std::any> params;
         * params["command"] = std::string("create_database");
         * params["url"] = std::string("cpp_dbc:firebird://localhost/test.fdb");
         * params["user"] = std::string("SYSDBA");
         * params["password"] = std::string("masterkey");
         * driver->command(params);
         * ```
         */
        virtual int command([[maybe_unused]] const std::map<std::string, std::any> &)
        {
            // Default implementation does nothing - override in specific drivers
            return 0;
        }

        /**
         * @brief Get the driver's registered name
         *
         * @return The driver name (e.g., "mysql", "postgresql", "mongodb")
         */
        virtual std::string getName() const noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_DRIVER_HPP