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

#include <iostream>
#include <sstream>
#include "cpp_dbc/common/system_constants.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "mongodb_internal.hpp"

#if USE_MONGODB

namespace cpp_dbc::MongoDB
{
    // ============================================================================
    // Static member initialization
    // ============================================================================

    std::atomic<bool> MongoDBDriver::s_initialized{false}; // NOSONAR - Explicit template arg for clarity in static member definition
    std::mutex MongoDBDriver::s_initMutex;

    // ============================================================================
    // MongoDBDriver Implementation
    // ============================================================================

    cpp_dbc::expected<bool, DBException> MongoDBDriver::initialize(std::nothrow_t) noexcept
    {
        MONGODB_DEBUG("MongoDBDriver::initialize - Initializing MongoDB C driver");
        mongoc_init();
        s_initialized.store(true, std::memory_order_release);
        MONGODB_DEBUG("MongoDBDriver::initialize - Done");
        return true;
    }

    MongoDBDriver::MongoDBDriver()
    {
        MONGODB_DEBUG("MongoDBDriver::constructor - Creating driver");
        // Double-checked locking: compatible with -fno-exceptions (std::call_once can throw
        // std::system_error internally). Also allows re-initialization after cleanup().
        if (!s_initialized.load(std::memory_order_acquire))
        {
            std::scoped_lock lock(s_initMutex);
            if (!s_initialized.load(std::memory_order_relaxed))
            {
                auto initResult = initialize(std::nothrow);
                if (!initResult.has_value())
                {
                    MONGODB_DEBUG("MongoDBDriver::constructor - Initialization failed: " << initResult.error().what());
                }
            }
        }
        MONGODB_DEBUG("MongoDBDriver::constructor - Done");
    }

    // Note: The destructor does NOT call cleanup() because mongoc_cleanup() is destructive
    // and irreversible within the same process. It frees global handshake data that was
    // allocated via pthread_once during library loading (_dl_init). After mongoc_cleanup()
    // runs, that data cannot be re-initialized — pthread_once has already fired and will
    // not run again, even if mongoc_init() is called. Any subsequent MongoDB operation
    // (e.g. a new connection in the next test case) will read freed memory, causing Valgrind
    // to report hundreds of "Invalid read" errors (use-after-free in
    // _mongoc_handshake_build_doc_with_application). Use cleanup() explicitly only at
    // process exit if needed.
    MongoDBDriver::~MongoDBDriver()
    {
        MONGODB_DEBUG("MongoDBDriver::destructor - Destroying driver");
        // cleanup();
    }

#ifdef __cpp_exceptions
    std::shared_ptr<DocumentDBConnection> MongoDBDriver::connectDocument(
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options)
    {
        auto r = connectDocument(std::nothrow, uri, user, password, options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

#endif // __cpp_exceptions

    std::string MongoDBDriver::getURIScheme() const noexcept
    {
        return "cpp_dbc:mongodb://<host>:<port>/<database>";
    }

    bool MongoDBDriver::supportsReplicaSets() const noexcept
    {
        return true;
    }

    bool MongoDBDriver::supportsSharding() const noexcept
    {
        return true;
    }

    std::string MongoDBDriver::getDriverVersion() const noexcept
    {
        return MONGOC_VERSION_S;
    }

    void MongoDBDriver::cleanup()
    {
        MONGODB_DEBUG("MongoDBDriver::cleanup - Cleaning up MongoDB C driver");
        if (s_initialized.load(std::memory_order_acquire))
        {
            mongoc_cleanup();
            s_initialized.store(false, std::memory_order_release);
            MONGODB_DEBUG("MongoDBDriver::cleanup - Done");
        }
    }

    bool MongoDBDriver::isInitialized()
    {
        return s_initialized.load(std::memory_order_acquire);
    }

    bool MongoDBDriver::validateURI(const std::string &uri)
    {
        // Strip cpp_dbc: prefix — mongoc expects native mongodb:// URIs
        std::string nativeUri = uri;
        if (nativeUri.starts_with(cpp_dbc::system_constants::URI_PREFIX))
        {
            nativeUri = nativeUri.substr(cpp_dbc::system_constants::URI_PREFIX.size());
        }
        bson_error_t error;
        mongoc_uri_t *mongoUri = mongoc_uri_new_with_error(nativeUri.c_str(), &error);

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
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        MONGODB_DEBUG("MongoDBDriver::connectDocument(nothrow) - Connecting to: " << uri);

        auto uriCheck = acceptURI(std::nothrow, uri);
        if (!uriCheck.has_value())
        {
            return unexpected<DBException>(uriCheck.error());
        }

        // Pass the full canonical URI — MongoDBConnection strips the cpp_dbc:
        // prefix internally before calling mongoc, while preserving the
        // canonical URI in m_uri.
        auto connResult = MongoDBConnection::create(std::nothrow, uri, user, password, options);
        if (!connResult.has_value())
        {
            MONGODB_DEBUG("MongoDBDriver::connectDocument(nothrow) - Connection failed: " << connResult.error().what());
            return unexpected<DBException>(connResult.error());
        }

        MONGODB_DEBUG("MongoDBDriver::connectDocument(nothrow) - Connection established");
        return std::static_pointer_cast<DocumentDBConnection>(connResult.value());
    }

    expected<std::map<std::string, std::string>, DBException> MongoDBDriver::parseURI(
        std::nothrow_t, const std::string &uri) noexcept
    {
        // Require cpp_dbc: prefix and strip it before passing to mongoc
        if (!uri.starts_with(cpp_dbc::system_constants::URI_PREFIX))
        {
            return unexpected<DBException>(DBException(
                "1C2D3E4F5A6B",
                "Invalid MongoDB URI: " + uri));
        }
        std::string nativeUri = uri.substr(cpp_dbc::system_constants::URI_PREFIX.size());

        std::map<std::string, std::string> result;

        bson_error_t error;
        MongoUriHandle mongoUri(mongoc_uri_new_with_error(nativeUri.c_str(), &error));

        if (!mongoUri)
        {
            return unexpected<DBException>(DBException(
                "A9G0ICAHJT85",
                std::string("Invalid URI: ") + error.message));
        }

        // Extract components — all mongoc_uri_get_* are C functions, they do not throw
        const mongoc_host_list_t *hosts = mongoc_uri_get_hosts(mongoUri.get());
        if (hosts && hosts->host)
        {
            result["host"] = hosts->host;
            result["port"] = std::to_string(hosts->port);
        }

        const char *database = mongoc_uri_get_database(mongoUri.get());
        if (database)
        {
            result["database"] = database;
        }

        const char *username = mongoc_uri_get_username(mongoUri.get());
        if (username)
        {
            result["username"] = username;
        }

        const char *authSource = mongoc_uri_get_auth_source(mongoUri.get());
        if (authSource)
        {
            result["authSource"] = authSource;
        }

        const char *replicaSet = mongoc_uri_get_replica_set(mongoUri.get());
        if (replicaSet)
        {
            result["replicaSet"] = replicaSet;
        }

        return result;
    }

    cpp_dbc::expected<std::string, DBException> MongoDBDriver::buildURI(
        std::nothrow_t,
        const std::string &host,
        int port,
        const std::string &database,
        const std::map<std::string, std::string> &options) noexcept
    {
        std::ostringstream uri;

        // Start with scheme (cpp_dbc: prefix + native mongodb://)
        uri << "cpp_dbc:mongodb://";

        // Add host — bracket raw IPv6 (e.g. "::1" → "[::1]")
        if (host.empty())
        {
            uri << "localhost";
        }
        else if (host.contains(':') &&
                 !(host.front() == '[' && host.back() == ']'))
        {
            uri << "[" << host << "]";
        }
        else
        {
            uri << host;
        }

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

    std::string MongoDBDriver::getName() const noexcept
    {
        return "mongodb";
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
