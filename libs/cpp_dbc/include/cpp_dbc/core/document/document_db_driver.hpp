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
 * @file document_db_driver.hpp
 * @brief Abstract class for document database drivers
 */

#ifndef CPP_DBC_DOCUMENT_DB_DRIVER_HPP
#define CPP_DBC_DOCUMENT_DB_DRIVER_HPP

#include <map>
#include <memory>
#include <string>
#include "cpp_dbc/core/db_driver.hpp"
#include "document_db_connection.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract class for document database drivers
     *
     * This class extends DBDriver with methods specific to document databases.
     * It provides a typed connect method that returns DocumentDBConnection.
     *
     * Document database drivers handle connection to databases like:
     * - MongoDB
     * - CouchDB
     * - Amazon DocumentDB
     * - Azure Cosmos DB (document mode)
     *
     * Implementations: MongoDBDriver, CouchDBDriver, etc.
     */
    class DocumentDBDriver : public DBDriver
    {
    public:
        virtual ~DocumentDBDriver() = default;

        /**
         * @brief Get the database type
         * @return Always returns DBType::DOCUMENT
         */
        DBType getDBType() const override
        {
            return DBType::DOCUMENT;
        }

        /**
         * @brief Connect to a document database
         *
         * This is the typed version that returns a DocumentDBConnection.
         *
         * @param url The database URL (e.g., "mongodb://host:port/database")
         * @param user The username for authentication (may be empty if auth is in URL)
         * @param password The password for authentication (may be empty if auth is in URL)
         * @param options Additional connection options
         * @return std::shared_ptr<DocumentDBConnection> A connection to the database
         * @throws DBException if the connection fails
         */
        virtual std::shared_ptr<DocumentDBConnection> connectDocument(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) = 0;

        /**
         * @brief Connect to a database (base class implementation)
         *
         * This method delegates to connectDocument() and returns the result
         * as a DBConnection pointer.
         */
        std::shared_ptr<DBConnection> connect(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            return connectDocument(url, user, password, options);
        }

        // Document database specific driver methods

        /**
         * @brief Get the default port for this database type
         * @return The default port number (e.g., 27017 for MongoDB)
         */
        virtual int getDefaultPort() const = 0;

        /**
         * @brief Get the URI scheme for this database type
         * @return The URI scheme (e.g., "mongodb", "mongodb+srv", "couchdb")
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
         * @brief Check if the driver supports replica sets
         * @return true if replica sets are supported
         */
        virtual bool supportsReplicaSets() const = 0;

        /**
         * @brief Check if the driver supports sharding
         * @return true if sharding is supported
         */
        virtual bool supportsSharding() const = 0;

        /**
         * @brief Get the driver version
         * @return The driver version string
         */
        virtual std::string getDriverVersion() const = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_DOCUMENT_DB_DRIVER_HPP