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
 * @file driver_01.cpp
 * @brief MongoDB database driver implementation - MongoDBDriver class
 */

#include "cpp_dbc/drivers/document/driver_mongodb.hpp"

#if USE_MONGODB

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include "cpp_dbc/common/system_utils.hpp"
#include "mongodb_internal.hpp"

namespace cpp_dbc::MongoDB
{
    // ============================================================================
    // Static member initialization
    // ============================================================================

    std::once_flag MongoDBDriver::s_initFlag;
    std::atomic<bool> MongoDBDriver::s_initialized{false}; // NOSONAR - Explicit template arg for clarity in static member definition

    // ============================================================================
    // MongoDBDriver Implementation
    // ============================================================================

    void MongoDBDriver::initializeMongoc()
    {
        MONGODB_DEBUG("MongoDBDriver::initializeMongoc - Initializing MongoDB C driver");
        mongoc_init();
        s_initialized = true;
        MONGODB_DEBUG("MongoDBDriver::initializeMongoc - Done");
    }

    MongoDBDriver::MongoDBDriver()
    {
        MONGODB_DEBUG("MongoDBDriver::constructor - Creating driver");
        std::call_once(s_initFlag, initializeMongoc);
        MONGODB_DEBUG("MongoDBDriver::constructor - Done");
    }

    MongoDBDriver::~MongoDBDriver()
    {
        MONGODB_DEBUG("MongoDBDriver::destructor - Destroying driver");
    }

    std::shared_ptr<DBConnection> MongoDBDriver::connect(
        const std::string &url,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options)
    {
        return connectDocument(url, user, password, options);
    }

    bool MongoDBDriver::acceptsURL(const std::string &url)
    {
        return url.starts_with("cpp_dbc:mongodb://");
    }

    std::shared_ptr<DocumentDBConnection> MongoDBDriver::connectDocument(
        const std::string &url,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options)
    {
        MONGODB_DEBUG("MongoDBDriver::connectDocument - Connecting to: " << url);
        MONGODB_LOCK_GUARD(m_mutex);

        if (!acceptsURL(url))
        {
            throw DBException("MG1L2M3N4O5P", "Invalid MongoDB URL: " + url, system_utils::captureCallStack());
        }

        // Strip the 'cpp_dbc:' prefix if present
        std::string mongoUrl = url;
        if (url.starts_with("cpp_dbc:"))
        {
            mongoUrl = url.substr(8); // Remove "cpp_dbc:" prefix
        }

        auto conn = std::make_shared<MongoDBConnection>(mongoUrl, user, password, options);
        MONGODB_DEBUG("MongoDBDriver::connectDocument - Connection established");
        return conn;
    }

    int MongoDBDriver::getDefaultPort() const
    {
        return 27017;
    }

    std::string MongoDBDriver::getURIScheme() const
    {
        return "mongodb";
    }

    std::map<std::string, std::string> MongoDBDriver::parseURI(const std::string &uri)
    {
        std::map<std::string, std::string> result;

        bson_error_t error;
        MongoUriHandle mongoUri(mongoc_uri_new_with_error(uri.c_str(), &error));

        if (!mongoUri)
        {
            throw DBException("J0K1L2M3N4O5", std::string("Invalid URI: ") + error.message, system_utils::captureCallStack());
        }

        // Extract components
        const mongoc_host_list_t *hosts = mongoc_uri_get_hosts(mongoUri.get());
        if (hosts && hosts->host)
        {
            result["host"] = hosts->host;
            result["port"] = std::to_string(hosts->port);
        }

        const char *database = mongoc_uri_get_database(mongoUri.get());
        if (database)
            result["database"] = database;

        const char *username = mongoc_uri_get_username(mongoUri.get());
        if (username)
            result["username"] = username;

        const char *authSource = mongoc_uri_get_auth_source(mongoUri.get());
        if (authSource)
            result["authSource"] = authSource;

        const char *replicaSet = mongoc_uri_get_replica_set(mongoUri.get());
        if (replicaSet)
            result["replicaSet"] = replicaSet;

        return result;
    }

    std::string MongoDBDriver::buildURI(
        const std::string &host,
        int port,
        const std::string &database,
        const std::map<std::string, std::string> &options)
    {
        std::ostringstream uri;

        // Start with scheme
        uri << "mongodb://";

        // Add host
        uri << (host.empty() ? "localhost" : host);

        // Add port
        if (port > 0)
        {
            uri << ":" << port;
        }

        // Add database
        if (!database.empty())
        {
            uri << "/" << database;
        }

        // Add options
        bool firstOption = true;
        for (const auto &[key, value] : options)
        {
            uri << (firstOption ? "?" : "&");
            uri << key << "=" << value;
            firstOption = false;
        }

        return uri.str();
    }

    bool MongoDBDriver::supportsReplicaSets() const
    {
        return true;
    }

    bool MongoDBDriver::supportsSharding() const
    {
        return true;
    }

    std::string MongoDBDriver::getDriverVersion() const
    {
        return MONGOC_VERSION_S;
    }

    void MongoDBDriver::cleanup()
    {
        MONGODB_DEBUG("MongoDBDriver::cleanup - Cleaning up MongoDB C driver");
        if (s_initialized)
        {
            mongoc_cleanup();
            s_initialized = false;
            MONGODB_DEBUG("MongoDBDriver::cleanup - Done");
        }
    }

    bool MongoDBDriver::isInitialized()
    {
        return s_initialized;
    }

    bool MongoDBDriver::validateURI(const std::string &uri)
    {
        bson_error_t error;
        mongoc_uri_t *mongoUri = mongoc_uri_new_with_error(uri.c_str(), &error);

        if (mongoUri)
        {
            mongoc_uri_destroy(mongoUri);
            return true;
        }

        return false;
    }

    // ====================================================================
    // MongoDBDriver NOTHROW VERSIONS
    // ====================================================================

    expected<std::shared_ptr<DocumentDBConnection>, DBException> MongoDBDriver::connectDocument(
        std::nothrow_t,
        const std::string &url,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBDriver::connectDocument(nothrow) - Connecting to: " << url);
            MONGODB_LOCK_GUARD(m_mutex);

            if (!acceptsURL(url))
            {
                return unexpected<DBException>(DBException(
                    "1C2D3E4F5A6B",
                    "Invalid MongoDB URL: " + url));
            }

            // Strip the 'cpp_dbc:' prefix if present
            std::string mongoUrl = url;
            if (url.starts_with("cpp_dbc:"))
            {
                mongoUrl = url.substr(8);
            }

            auto conn = std::make_shared<MongoDBConnection>(mongoUrl, user, password, options);
            MONGODB_DEBUG("MongoDBDriver::connectDocument(nothrow) - Connection established");
            return std::static_pointer_cast<DocumentDBConnection>(conn);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "2D3E4F5A6B7C",
                "Memory allocation failed in connectDocument"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "3E4F5A6B7C8D",
                std::string("Error in connectDocument: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "4F5A6B7C8D9E",
                "Unknown error in connectDocument"));
        }
    }

    expected<std::map<std::string, std::string>, DBException> MongoDBDriver::parseURI(
        std::nothrow_t, const std::string &uri) noexcept
    {
        try
        {
            std::map<std::string, std::string> result;

            bson_error_t error;
            MongoUriHandle mongoUri(mongoc_uri_new_with_error(uri.c_str(), &error));

            if (!mongoUri)
            {
                return unexpected<DBException>(DBException(
                    "5A6B7C8D9E0F",
                    std::string("Invalid URI: ") + error.message));
            }

            // Extract components
            const mongoc_host_list_t *hosts = mongoc_uri_get_hosts(mongoUri.get());
            if (hosts && hosts->host)
            {
                result["host"] = hosts->host;
                result["port"] = std::to_string(hosts->port);
            }

            const char *database = mongoc_uri_get_database(mongoUri.get());
            if (database)
                result["database"] = database;

            const char *username = mongoc_uri_get_username(mongoUri.get());
            if (username)
                result["username"] = username;

            const char *authSource = mongoc_uri_get_auth_source(mongoUri.get());
            if (authSource)
                result["authSource"] = authSource;

            const char *replicaSet = mongoc_uri_get_replica_set(mongoUri.get());
            if (replicaSet)
                result["replicaSet"] = replicaSet;

            return result;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "6B7C8D9E0F1A",
                "Memory allocation failed in parseURI"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "7C8D9E0F1A2B",
                std::string("Error in parseURI: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "8D9E0F1A2B3C",
                "Unknown error in parseURI"));
        }
    }

    std::string MongoDBDriver::getName() const noexcept
    {
        return "mongodb";
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
