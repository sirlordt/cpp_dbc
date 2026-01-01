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
#include "cpp_dbc/core/db_driver.hpp"
#include "kv_db_connection.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract class for key-value database drivers
     *
     * This class extends DBDriver with methods specific to key-value databases.
     * It provides a typed connect method that returns KVDBConnection.
     *
     * Key-value database drivers handle connection to databases like:
     * - Redis
     * - Memcached
     * - etcd
     * - RocksDB
     *
     * Implementations: RedisDriver, MemcachedDriver, etc.
     */
    class KVDBDriver : public DBDriver
    {
    public:
        virtual ~KVDBDriver() = default;

        /**
         * @brief Get the database type
         * @return Always returns DBType::KEY_VALUE
         */
        DBType getDBType() const override
        {
            return DBType::KEY_VALUE;
        }

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
         * This method delegates to connectKV() and returns the result
         * as a DBConnection pointer.
         */
        std::shared_ptr<DBConnection> connect(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            return connectKV(url, user, password, options);
        }

        // Key-value database specific driver methods

        /**
         * @brief Get the default port for this database type
         * @return The default port number (e.g., 6379 for Redis)
         */
        virtual int getDefaultPort() const = 0;

        /**
         * @brief Get the URI scheme for this database type
         * @return The URI scheme (e.g., "redis", "memcached")
         */
        virtual std::string getURIScheme() const = 0;

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

        /**
         * @brief Check if the driver supports clustering
         * @return true if clustering is supported
         */
        virtual bool supportsClustering() const = 0;

        /**
         * @brief Check if the driver supports replication
         * @return true if replication is supported
         */
        virtual bool supportsReplication() const = 0;

        /**
         * @brief Get the driver version
         * @return The driver version string
         */
        virtual std::string getDriverVersion() const = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_KV_DB_DRIVER_HPP