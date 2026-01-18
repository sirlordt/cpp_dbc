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
        /**
 * @brief Virtual destructor to allow proper cleanup of resources in derived columnar driver implementations.
 */
virtual ~ColumnarDBDriver() = default;

        /**
         * @brief Identify this driver as a columnar database driver.
         *
         * @return DBType::COLUMNAR indicating the driver targets columnar databases.
         */
        DBType getDBType() const override
        {
            return DBType::COLUMNAR;
        }

        /**
         * @brief Connect to a columnar database
         *
         * This is the typed version that returns a ColumnarDBConnection.
         *
         * @param url The database URL (e.g., "clickhouse://host:port/database")
         * @param user The username for authentication
         * @param password The password for authentication
         * @param options Additional connection options
         * @return std::shared_ptr<ColumnarDBConnection> A connection to the database
         * @throws DBException if the connection fails
         */
        virtual std::shared_ptr<ColumnarDBConnection> connectColumnar(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) = 0;

        /**
         * @brief Establishes a connection to a columnar database and returns it as a DBConnection pointer.
         *
         * @param url Connection URI or address for the database.
         * @param user Username for authentication.
         * @param password Password for authentication.
         * @param options Optional key/value connection options.
         * @return std::shared_ptr<DBConnection> Shared pointer to the established DBConnection (owns the connection).
         */
        std::shared_ptr<DBConnection> connect(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            return connectColumnar(url, user, password, options);
        }

        // Columnar database specific driver methods

        /**
         * @brief Get the default port for this database type
         * @return The default port number (e.g., 8123 for ClickHouse HTTP, 9000 for TCP)
         */
        virtual int getDefaultPort() const = 0;

        /**
         * @brief Get the URI scheme for this database type
         * @return The URI scheme (e.g., "clickhouse", "scylladb")
         */
        virtual std::string getURIScheme() const = 0;

        /**
         * @brief Parse a connection URI and extract components
         * @param uri The connection URI
         * @return A map containing parsed components (host, port, database, etc.)
         * @throws DBException if the URI is invalid
         */
        virtual std::map<std::string, std::string> parseURI(const std::string &uri) = 0;

        /**
         * @brief Build a connection URI from components
         * @param host The hostname
         * @param port The port number
         * @param database The database name
         * @param options Additional options
         * @return The constructed URI
         */
        virtual std::string buildURI(
            const std::string &host,
            int port,
            const std::string &database,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) = 0;

        /**
         * @brief Check if the driver supports clustering/sharding
         * @return true if clustering is supported and configured
         */
        virtual bool supportsClustering() const = 0;

        /**
         * @brief Check if the driver supports asynchronous operations
         * @return true if async operations are supported
         */
        virtual bool supportsAsync() const = 0;

        /**
         * @brief Get the driver version
         * @return The driver version string
         */
        virtual std::string getDriverVersion() const = 0;

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        /**
         * @brief Connect to a columnar database (nothrow version)
         * @return expected containing connection to the database, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException>
        connectColumnar(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept = 0;

        /**
         * @brief Attempt a nothrow connection to the columnar database.
         *
         * @returns cpp_dbc::expected holding a `std::shared_ptr<DBConnection>` on success, or an unexpected `DBException` on failure.
         */
        cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> connect(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept
        {
            auto result = connectColumnar(std::nothrow, url, user, password, options);
            if (!result)
            {
                return cpp_dbc::unexpected(result.error());
            }
            return std::static_pointer_cast<DBConnection>(*result);
        }

        /**
         * @brief Parse a connection URI and extract components (nothrow version)
         * @return expected containing map of parsed components, or DBException on failure
         */
        virtual cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(
            std::nothrow_t, const std::string &uri) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_COLUMNAR_DB_DRIVER_HPP