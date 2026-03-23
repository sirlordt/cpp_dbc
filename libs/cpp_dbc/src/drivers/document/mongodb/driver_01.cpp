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
#include "cpp_dbc/common/serial_queue.hpp"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>
#include "cpp_dbc/common/system_constants.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "mongodb_internal.hpp"

#if USE_MONGODB

namespace cpp_dbc::MongoDB
{
    // ── Static member initialization ──────────────────────────────────────────
    // IMPORTANT: s_instance MUST be declared LAST — see mysql/driver_01.cpp for
    // the full explanation of the static destruction order requirement.
    std::mutex                     MongoDBDriver::s_instanceMutex;
    std::mutex                     MongoDBDriver::s_registryMutex;
    std::set<std::weak_ptr<MongoDBConnection>,
             std::owner_less<std::weak_ptr<MongoDBConnection>>> MongoDBDriver::s_connectionRegistry;
    std::shared_ptr<MongoDBDriver> MongoDBDriver::s_instance;

    // ============================================================================
    // MongoDBDriver Implementation
    // ============================================================================

    // ── Private helper methods ──────────────────────────────────────────────

    cpp_dbc::expected<bool, DBException> MongoDBDriver::initialize(std::nothrow_t) noexcept
    {
        MONGODB_DEBUG("MongoDBDriver::initialize - Initializing MongoDB C driver");
        mongoc_init();
        MONGODB_DEBUG("MongoDBDriver::initialize - Done");
        return true;
    }

    void MongoDBDriver::registerConnection(std::nothrow_t, std::weak_ptr<MongoDBConnection> conn) noexcept
    {
        size_t registrySize = 0;
        {
            std::scoped_lock lock(s_registryMutex);
            s_connectionRegistry.insert(std::move(conn));
            registrySize = s_connectionRegistry.size();
        }

        // Coalesced cleanup: only post when the registry has grown past the
        // cleanup threshold and no cleanup task is already queued.
        if (registrySize > 25 && !s_cleanupPending.exchange(true, std::memory_order_seq_cst))
        {
            SerialQueue::global().post(
                []()
                {
                    {
                        std::scoped_lock lock(s_registryMutex);
                        std::erase_if(s_connectionRegistry,
                            [](const auto &w)
                            {
                                return w.expired();
                            });
                    }
                    s_cleanupPending.store(false, std::memory_order_seq_cst);
                });
        }
    }

    void MongoDBDriver::unregisterConnection(std::nothrow_t, const std::weak_ptr<MongoDBConnection> &conn) noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        s_connectionRegistry.erase(conn);
    }

    void MongoDBDriver::closeAllOpenConnections(std::nothrow_t) noexcept
    {
        // Mark driver as closed — reject any new connection attempts
        m_closed.store(true, std::memory_order_seq_cst);

        // Close all open connections before the driver goes away.
        // Collect under lock first, then close outside the lock to avoid
        // deadlock with unregisterConnection() (which also acquires s_registryMutex).
        std::vector<std::shared_ptr<MongoDBConnection>> connectionsToClose;
        {
            std::scoped_lock lock(s_registryMutex);
            for (const auto &weak : s_connectionRegistry)
            {
                auto conn = weak.lock();
                if (conn)
                {
                    connectionsToClose.push_back(std::move(conn));
                }
            }
            s_connectionRegistry.clear();
        }
        for (const auto &conn : connectionsToClose)
        {
            [[maybe_unused]] auto closeResult = conn->close(std::nothrow);
        }
    }

    // ── Constructor, Destructor ───────────────────────────────────────────────

    MongoDBDriver::MongoDBDriver(MongoDBDriver::PrivateCtorTag, std::nothrow_t) noexcept
    {
        MONGODB_DEBUG("MongoDBDriver::constructor - Creating driver");
        auto result = initialize(std::nothrow);
        if (!result.has_value())
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>(std::move(result.error()));
        }
        MONGODB_DEBUG("MongoDBDriver::constructor - Done");
    }

    MongoDBDriver::~MongoDBDriver()
    {
        MONGODB_DEBUG("MongoDBDriver::destructor - Destroying driver");

        closeAllOpenConnections(std::nothrow);

        cleanup();
    }

#ifdef __cpp_exceptions
    std::shared_ptr<MongoDBDriver> MongoDBDriver::getInstance()
    {
        auto result = getInstance(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

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

    // ====================================================================
    // MongoDBDriver NOTHROW VERSIONS
    // ====================================================================

    cpp_dbc::expected<std::shared_ptr<MongoDBDriver>, DBException>
    MongoDBDriver::getInstance(std::nothrow_t) noexcept
    {
        std::scoped_lock lock(s_instanceMutex);
        if (s_instance)
        {
            return s_instance;
        }
        // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
        // Constructor errors are captured in m_initFailed / m_initError.
        auto inst = std::make_shared<MongoDBDriver>(MongoDBDriver::PrivateCtorTag{}, std::nothrow);
        if (inst->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*inst->m_initError));
        }
        s_instance = inst;
        return s_instance;
    }

    std::string MongoDBDriver::getURIScheme() const noexcept
    {
        return std::string(cpp_dbc::system_constants::URI_PREFIX) + "mongodb://<host>:<port>/<database>";
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

    void MongoDBDriver::cleanup() noexcept
    {
        MONGODB_DEBUG("MongoDBDriver::cleanup - Cleaning up MongoDB C driver");
        mongoc_cleanup();
        MONGODB_DEBUG("MongoDBDriver::cleanup - Done");
    }

    bool MongoDBDriver::validateURI(const std::string &uri) noexcept
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

    expected<std::shared_ptr<DocumentDBConnection>, DBException> MongoDBDriver::connectDocument(
        std::nothrow_t,
        const std::string &uri,
        const std::string &user,
        const std::string &password,
        const std::map<std::string, std::string> &options) noexcept
    {
        if (m_closed.load(std::memory_order_seq_cst))
        {
            return cpp_dbc::unexpected(DBException("T5KXPF2BWN9H",
                                                   "Driver is closed, no more connections allowed",
                                                   system_utils::captureCallStack()));
        }

        MONGODB_DEBUG("MongoDBDriver::connectDocument(nothrow) - Connecting to: %s", uri.c_str());

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
            MONGODB_DEBUG("MongoDBDriver::connectDocument(nothrow) - Connection failed: %s", connResult.error().what());
            return unexpected<DBException>(connResult.error());
        }

        MONGODB_DEBUG("MongoDBDriver::connectDocument(nothrow) - Connection established");
        auto conn = connResult.value();
        registerConnection(std::nothrow, conn);
        return std::static_pointer_cast<DocumentDBConnection>(conn);
    }

    expected<std::map<std::string, std::string>, DBException> MongoDBDriver::parseURI(
        std::nothrow_t, const std::string &uri) noexcept
    {
        // Require cpp_dbc: prefix and strip it before passing to mongoc
        if (!uri.starts_with(cpp_dbc::system_constants::URI_PREFIX))
        {
            return unexpected<DBException>(DBException(
                "N9YJ2DHLFUF7",
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
        uri << cpp_dbc::system_constants::URI_PREFIX << "mongodb://";

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

        // Validate and add database
        if (!database.empty())
        {
            if (!system_utils::isValidDatabaseIdentifier(database))
            {
                return cpp_dbc::unexpected(DBException(
                    "XLGG2NN4B7J6",
                    "Invalid database name: " + database,
                    system_utils::captureCallStack()));
            }
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

    size_t MongoDBDriver::getConnectionAlive() noexcept
    {
        std::scoped_lock lock(s_registryMutex);
        return static_cast<size_t>(std::ranges::count_if(
            s_connectionRegistry,
            [](const auto &w)
            {
                return !w.expired();
            }));
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
