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
#include <new>
#include <string>
#include "cpp_dbc/core/db_driver.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "document_db_connection.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract class for document database drivers
     *
     * Extends DBDriver with a typed connect method that returns
     * DocumentDBConnection. Also provides URI parsing/building helpers.
     *
     * ```cpp
     * auto driver = std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>();
     * cpp_dbc::DriverManager::registerDriver(driver);
     * auto conn = driver->connectDocument("mongodb://localhost:27017/mydb", "", "");
     * std::cout << "Database: " << conn->getDatabaseName() << std::endl;
     * conn->close();
     * ```
     *
     * Implementations: MongoDBDriver
     *
     * @see DocumentDBConnection, DBDriver
     */
    class DocumentDBDriver : public DBDriver
    {
    public:
        ~DocumentDBDriver() override = default;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
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
         * This method delegates to connectDocument(nothrow) and rethrows on error.
         */
        std::shared_ptr<DBConnection> connect(
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            auto result = connectDocument(std::nothrow, url, user, password, options);
            if (!result.has_value())
            {
                throw result.error();
            }
            return std::static_pointer_cast<DBConnection>(result.value());
        }

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

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        /**
         * @brief Get the database type
         * @return Always returns DBType::DOCUMENT
         */
        DBType getDBType() const noexcept override
        {
            return DBType::DOCUMENT;
        }

        /**
         * @brief Connect to a document database (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing connection to the database, or DBException on failure
         */
        virtual expected<std::shared_ptr<DocumentDBConnection>, DBException> connectDocument(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept = 0;

        /**
         * @brief Connect to a database (base class nothrow implementation)
         *
         * This method delegates to connectDocument(nothrow) and returns the result.
         */
        expected<std::shared_ptr<DBConnection>, DBException> connect(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override
        {
            auto result = connectDocument(std::nothrow, url, user, password, options);
            if (!result.has_value())
            {
                return cpp_dbc::unexpected(result.error());
            }
            return std::static_pointer_cast<DBConnection>(result.value());
        }

        /**
         * @brief Parse a connection URI and extract components (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing map of parsed components, or DBException on failure
         */
        virtual expected<std::map<std::string, std::string>, DBException> parseURI(
            std::nothrow_t, const std::string &uri) noexcept = 0;

        // Document database specific driver methods (trivial — cannot fail)

        /**
         * @brief Get the URL prefix accepted by this driver
         *
         * Returns the full connection URL prefix that this driver handles,
         * e.g. `"cpp_dbc:mongodb://"`. This value is also the prefix that
         * `acceptsURL()` checks against.
         *
         * Example format: `cpp_dbc:<engine>://host:port/database`
         *
         * @return The URL prefix string (e.g., "cpp_dbc:mongodb://")
         */
        virtual std::string getURIScheme() const noexcept = 0;

        /**
         * @brief Check if the driver supports replica sets
         * @return true if replica sets are supported
         */
        virtual bool supportsReplicaSets() const noexcept = 0;

        /**
         * @brief Check if the driver supports sharding
         * @return true if sharding is supported
         */
        virtual bool supportsSharding() const noexcept = 0;

        /**
         * @brief Get the driver version
         * @return The driver version string
         */
        virtual std::string getDriverVersion() const noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_DOCUMENT_DB_DRIVER_HPP
