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
     * This is the root of the driver hierarchy. All database paradigms
     * (relational, document, key-value, columnar) derive their driver
     * classes from this base class.
     *
     * The base class provides the fundamental operations for:
     * - Connecting to a database
     * - Checking if a URL is supported
     * - Executing driver-specific commands
     * - Identifying the database type
     */
    class DBDriver
    {
    public:
        virtual ~DBDriver() = default;

        /**
         * @brief Connect to a database
         *
         * @param url The database URL (e.g., "jdbc:mysql://host:port/database")
         * @param user The username for authentication
         * @param password The password for authentication
         * @param options Additional connection options
         * @return std::shared_ptr<DBConnection> A connection to the database
         * @throws DBException if the connection fails
         */
        virtual std::shared_ptr<DBConnection> connect(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) = 0;

        /**
         * @brief Check if this driver accepts the given URL
         *
         * @param url The database URL to check
         * @return true if this driver can handle the URL
         * @return false if this driver cannot handle the URL
         */
        virtual bool acceptsURL(const std::string &url) = 0;

        /**
         * @brief Get the database type supported by this driver
         *
         * @return DBType The type of database (RELATIONAL, DOCUMENT, KEY_VALUE, COLUMNAR)
         */
        virtual DBType getDBType() const = 0;

        /**
         * @brief Execute a driver-specific command
         *
         * This method allows executing driver-specific operations that don't require
         * an existing connection, such as creating a database.
         *
         * Supported commands vary by driver:
         * - Firebird: "create_database" - Creates a new database file
         *
         * @param params A map of parameters for the command:
         *               - "command": The command name (e.g., "create_database")
         *               - "url": The database URL
         *               - "user": The username
         *               - "password": The password
         *               - Additional driver-specific options
         * @return Result code (0 for success, non-zero for errors)
         * @throws DBException if the command fails
         */
        virtual int command([[maybe_unused]] const std::map<std::string, std::any> &)
        {
            // Default implementation does nothing - override in specific drivers
            return 0;
        }
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_DRIVER_HPP