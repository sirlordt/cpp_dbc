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
#include <new>
#include <string>
#include "db_connection.hpp"
#include "db_expected.hpp"
#include "db_exception.hpp"
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
     * if (driver->acceptURI("cpp_dbc:mysql://localhost/mydb")) {
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

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
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
        virtual int command([[maybe_unused]] const std::map<std::string, std::any> &params)
        {
            auto result = command(std::nothrow, params);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

        /**
         * @brief Check if this driver accepts the given URL
         *
         * @param url The database URL to check
         * @return true if this driver can handle the URL
         * @return false if this driver cannot handle the URL
         * @throws DBException on error
         */
        virtual bool acceptURI(const std::string &url)
        {
            auto result = acceptURI(std::nothrow, url);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

        /**
         * @brief Parse a connection URI and extract components
         *
         * @param uri The connection URI (e.g., "cpp_dbc:mysql://host:port/database")
         * @return A map containing parsed components (host, port, database, etc.)
         * @throws DBException if the URI is invalid
         */
        virtual std::map<std::string, std::string> parseURI(const std::string &uri)
        {
            auto result = parseURI(std::nothrow, uri);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

        /**
         * @brief Build a connection URI from components
         *
         * @param host The hostname
         * @param port The port number
         * @param database The database name or path
         * @param options Additional options
         * @return The constructed URI string
         * @throws DBException if the URI cannot be built
         */
        virtual std::string buildURI(
            const std::string &host,
            int port,
            const std::string &database,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>())
        {
            auto result = buildURI(std::nothrow, host, port, database, options);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Connect to a database (nothrow version)
         *
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param url The database URL
         * @param user The username for authentication
         * @param password The password for authentication
         * @param options Additional connection options
         * @return expected containing a connection to the database, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> connect(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept = 0;

        /**
         * @brief Check if this driver accepts the given URL (nothrow version)
         *
         * Each driver recognizes a specific URL scheme (e.g., "cpp_dbc:mysql://", "cpp_dbc:postgresql://").
         *
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param url The database URL to check
         * @return expected containing true if this driver can handle the URL, false otherwise
         *
         * ```cpp
         * auto driver = std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>();
         * auto ok = driver->acceptURI(std::nothrow, "cpp_dbc:mysql://localhost/mydb");   // true
         * auto no = driver->acceptURI(std::nothrow, "cpp_dbc:postgresql://localhost/mydb"); // false
         * ```
         */
        virtual cpp_dbc::expected<bool, DBException> acceptURI(
            std::nothrow_t, const std::string &url) noexcept
        {
            return parseURI(std::nothrow, url).has_value();
        }

        /**
         * @brief Parse a connection URI and extract components (nothrow version)
         *
         * Each driver implements this to extract connection components
         * from a driver-specific URI format.
         *
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param uri The connection URI
         * @return expected containing map of parsed components (host, port, database, etc.),
         *         or DBException on failure
         */
        virtual cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(
            std::nothrow_t, const std::string &uri) noexcept = 0;

        /**
         * @brief Build a connection URI from components (nothrow version)
         *
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param host The hostname
         * @param port The port number
         * @param database The database name or path
         * @param options Additional options
         * @return expected containing the constructed URI string, or DBException on failure
         */
        virtual cpp_dbc::expected<std::string, DBException> buildURI(
            std::nothrow_t,
            const std::string &host,
            int port,
            const std::string &database,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept = 0;

        /**
         * @brief Get the database type supported by this driver
         *
         * @return DBType The type of database (RELATIONAL, DOCUMENT, KEY_VALUE, COLUMNAR)
         */
        virtual DBType getDBType() const noexcept = 0;

        /**
         * @brief Get the full URI scheme template accepted by this driver
         *
         * Returns a human-readable URI template showing all components that
         * this driver's `parseURI()` expects. Placeholders use angle brackets.
         *
         * Examples:
         * - `"cpp_dbc:mysql://<host>:<port>/<database>"`
         * - `"cpp_dbc:sqlite://<path>"`
         * - `"cpp_dbc:redis://<host>:<port>/<db>"`
         *
         * @return The URI scheme template string
         */
        virtual std::string getURIScheme() const noexcept = 0;

        /**
         * @brief Execute a driver-specific command without requiring a connection (nothrow version)
         *
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param params Command parameters as a map of string to std::any
         * @return expected containing result code (0 for success), or DBException on failure
         */
        virtual cpp_dbc::expected<int, DBException> command(
            std::nothrow_t,
            [[maybe_unused]] const std::map<std::string, std::any> &params) noexcept
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