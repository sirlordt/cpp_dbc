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
 * @file columnar_db_driver.hpp
 * @brief Abstract class for columnar database drivers
 */

#ifndef CPP_DBC_COLUMNAR_DB_DRIVER_HPP
#define CPP_DBC_COLUMNAR_DB_DRIVER_HPP

#include <map>
#include <memory>
#include <string>
#include <new> // For std::nothrow_t
#include "cpp_dbc/core/db_driver.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "columnar_db_connection.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract class for columnar database drivers
     *
     * This class extends DBDriver with methods specific to columnar databases.
     * It provides a typed connect method that returns ColumnarDBConnection.
     *
     * Columnar database drivers handle connection to databases like:
     * - ClickHouse
     * - ScyllaDB
     * - Apache Cassandra
     * - Amazon Redshift
     * - Google BigQuery
     *
     * Implementations: ClickHouseDriver, ScyllaDBDriver, etc.
     */
    class ColumnarDBDriver : public DBDriver
    {
    public:
        ~ColumnarDBDriver() override = default;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        /**
         * @brief Connect to a columnar database
         *
         * This is the typed version that returns a ColumnarDBConnection.
         *
         * @param uri The database URI (e.g., "clickhouse://host:port/database")
         * @param user The username for authentication
         * @param password The password for authentication
         * @param options Additional connection options
         * @return std::shared_ptr<ColumnarDBConnection> A connection to the database
         * @throws DBException if the connection fails
         */
        virtual std::shared_ptr<ColumnarDBConnection> connectColumnar(
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) = 0;

        /**
         * @brief Connect to a database (base class implementation)
         *
         * This method delegates to connectColumnar(nothrow) and rethrows on error.
         */
        std::shared_ptr<DBConnection> connect(
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            auto result = connectColumnar(std::nothrow, uri, user, password, options);
            if (!result.has_value())
            {
                throw result.error();
            }
            return std::static_pointer_cast<DBConnection>(result.value());
        }

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Get the database type
         * @return Always returns DBType::COLUMNAR
         */
        DBType getDBType() const noexcept override
        {
            return DBType::COLUMNAR;
        }

        /**
         * @brief Connect to a columnar database (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing connection to the database, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException>
        connectColumnar(
            std::nothrow_t,
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept = 0;

        /**
         * @brief Connect to a database (base class nothrow implementation)
         *
         * This method delegates to connectColumnar(nothrow) and returns the result.
         */
        cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> connect(
            std::nothrow_t,
            const std::string &uri,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override
        {
            auto result = connectColumnar(std::nothrow, uri, user, password, options);
            if (!result.has_value())
            {
                return cpp_dbc::unexpected(result.error());
            }
            return std::static_pointer_cast<DBConnection>(result.value());
        }

        // Columnar database specific driver methods (trivial — cannot fail)

        /**
         * @brief Check if the driver supports clustering/sharding
         * @return true if clustering is supported and configured
         */
        virtual bool supportsClustering() const noexcept = 0;

        /**
         * @brief Check if the driver supports asynchronous operations
         * @return true if async operations are supported
         */
        virtual bool supportsAsync() const noexcept = 0;

    };

} // namespace cpp_dbc

#endif // CPP_DBC_COLUMNAR_DB_DRIVER_HPP
