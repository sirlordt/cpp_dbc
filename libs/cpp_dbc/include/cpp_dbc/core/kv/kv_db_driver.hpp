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
 * @file kv_db_driver.hpp
 * @brief Abstract class for key-value database drivers
 */

#ifndef CPP_DBC_KV_DB_DRIVER_HPP
#define CPP_DBC_KV_DB_DRIVER_HPP

#include <map>
#include <memory>
#include <string>
#include <new> // For std::nothrow_t
#include "cpp_dbc/core/db_driver.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "kv_db_connection.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract class for key-value database drivers
     *
     * Extends DBDriver with a typed connect method that returns
     * KVDBConnection. Also provides URI parsing/building helpers.
     *
     * ```cpp
     * auto driver = std::make_shared<cpp_dbc::Redis::RedisDBDriver>();
     * cpp_dbc::DriverManager::registerDriver(driver);
     * auto conn = driver->connectKV("redis://localhost:6379", "", "");
     * conn->setString("key", "value");
     * conn->close();
     * ```
     *
     * Implementations: RedisDBDriver
     *
     * @see KVDBConnection, DBDriver
     */
    class KVDBDriver : public DBDriver
    {
    public:
        ~KVDBDriver() override = default;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        /**
         * @brief Connect to a key-value database
         *
         * This is the typed version that returns a KVDBConnection.
         *
         * @param url The database URL (e.g., "redis://host:port")
         * @param user The username for authentication (may be empty if auth is in URL or not required)
         * @param password The password for authentication (may be empty if auth is in URL or not required)
         * @param options Additional connection options
         * @return std::shared_ptr<KVDBConnection> A connection to the database
         * @throws DBException if the connection fails
         */
        virtual std::shared_ptr<KVDBConnection> connectKV(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) = 0;

        /**
         * @brief Connect to a database (base class implementation)
         *
         * This method delegates to connectKV(nothrow) and rethrows on error.
         */
        std::shared_ptr<DBConnection> connect(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            auto result = connectKV(std::nothrow, url, user, password, options);
            if (!result.has_value())
            {
                throw result.error();
            }
            return std::static_pointer_cast<DBConnection>(result.value());
        }

        /**
         * @brief Parse a connection URI and extract components
         * @param uri The connection URI
         * @return A map containing parsed components (host, port, etc.)
         * @throws DBException if the URI is invalid
         */
        virtual std::map<std::string, std::string> parseURI(const std::string &uri) = 0;

        /**
         * @brief Build a connection URI from components
         * @param host The hostname
         * @param port The port number
         * @param db The database number/name (if applicable)
         * @param options Additional options
         * @return The constructed URI
         */
        virtual std::string buildURI(
            const std::string &host,
            int port,
            const std::string &db = "",
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) = 0;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Get the database type
         * @return Always returns DBType::KEY_VALUE
         */
        DBType getDBType() const noexcept override
        {
            return DBType::KEY_VALUE;
        }

        /**
         * @brief Connect to a key-value database (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param url The database URL
         * @param user The username for authentication
         * @param password The password for authentication
         * @param options Additional connection options
         * @return expected containing connection to the database, or DBException on failure
         */
        virtual expected<std::shared_ptr<KVDBConnection>, DBException> connectKV(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept = 0;

        /**
         * @brief Connect to a database (base class nothrow implementation)
         *
         * This method delegates to connectKV(nothrow) and returns the result.
         */
        expected<std::shared_ptr<DBConnection>, DBException> connect(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override
        {
            auto result = connectKV(std::nothrow, url, user, password, options);
            if (!result.has_value())
            {
                return cpp_dbc::unexpected(result.error());
            }
            return std::static_pointer_cast<DBConnection>(result.value());
        }

        /**
         * @brief Parse a connection URI and extract components (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param uri The connection URI
         * @return expected containing map of parsed components, or DBException on failure
         */
        virtual expected<std::map<std::string, std::string>, DBException> parseURI(
            std::nothrow_t, const std::string &uri) noexcept = 0;

        // Key-value database specific driver methods (trivial — cannot fail)

        /**
         * @brief Get the URL prefix accepted by this driver
         *
         * Returns the full connection URL prefix that this driver handles,
         * e.g. `"cpp_dbc:redis://"`. This value is also the prefix that
         * `acceptsURL()` checks against.
         *
         * Example format: `cpp_dbc:<engine>://host:port/db`
         *
         * @return The URL prefix string (e.g., "cpp_dbc:redis://")
         */
        virtual std::string getURIScheme() const noexcept = 0;

        /**
         * @brief Check if the driver supports clustering
         * @return true if clustering is supported
         */
        virtual bool supportsClustering() const noexcept = 0;

        /**
         * @brief Check if the driver supports replication
         * @return true if replication is supported
         */
        virtual bool supportsReplication() const noexcept = 0;

        /**
         * @brief Get the driver version
         * @return The driver version string
         */
        virtual std::string getDriverVersion() const noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_KV_DB_DRIVER_HPP
