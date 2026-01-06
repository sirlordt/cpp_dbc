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
 * @file relational_db_driver.hpp
 * @brief Abstract class for relational database drivers
 */

#ifndef CPP_DBC_RELATIONAL_DB_DRIVER_HPP
#define CPP_DBC_RELATIONAL_DB_DRIVER_HPP

#include <map>
#include <memory>
#include <string>
#include <new> // For std::nothrow_t
#include "cpp_dbc/core/db_driver.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "relational_db_connection.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract class for relational database drivers
     *
     * This class extends DBDriver with methods specific to relational databases.
     * It provides a typed connect method that returns RelationalDBConnection.
     *
     * Implementations: MySQLDriver, PostgreSQLDriver, SQLiteDriver, FirebirdDriver
     */
    class RelationalDBDriver : public DBDriver
    {
    public:
        virtual ~RelationalDBDriver() = default;

        /**
         * @brief Get the database type
         * @return Always returns DBType::RELATIONAL
         */
        DBType getDBType() const override
        {
            return DBType::RELATIONAL;
        }

        /**
         * @brief Connect to a relational database
         *
         * This is the typed version that returns a RelationalDBConnection.
         *
         * @param url The database URL (e.g., "jdbc:mysql://host:port/database")
         * @param user The username for authentication
         * @param password The password for authentication
         * @param options Additional connection options
         * @return std::shared_ptr<RelationalDBConnection> A connection to the database
         * @throws DBException if the connection fails
         */
        virtual std::shared_ptr<RelationalDBConnection> connectRelational(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) = 0;

        /**
         * @brief Connect to a database (base class implementation)
         *
         * This method delegates to connectRelational() and returns the result
         * as a DBConnection pointer.
         */
        std::shared_ptr<DBConnection> connect(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            return connectRelational(url, user, password, options);
        }

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        /**
         * @brief Connect to a relational database (nothrow version)
         *
         * This is the exception-free version that returns expected<T, E>.
         *
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param url The database URL
         * @param user The username for authentication
         * @param password The password for authentication
         * @param options Additional connection options
         * @return expected containing a connection to the database, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>
        connectRelational(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept = 0;

        /**
         * @brief Connect to a database (base class nothrow implementation)
         *
         * This method delegates to connectRelational(nothrow) and returns the result.
         */
        cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> connect(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept
        {
            auto result = connectRelational(std::nothrow, url, user, password, options);
            if (!result)
            {
                return cpp_dbc::unexpected(result.error());
            }
            return std::static_pointer_cast<DBConnection>(*result);
        }
    };

} // namespace cpp_dbc

#endif // CPP_DBC_RELATIONAL_DB_DRIVER_HPP
