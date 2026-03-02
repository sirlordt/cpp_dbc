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
 * @file connection_01.cpp
 * @brief MongoDB MongoDBConnection - Part 1 (private helpers, constructor, destructor, move, close, database operations)
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
    // MongoDBConnection Implementation - Private Helpers
    // ============================================================================

    void MongoDBConnection::validateConnection() const
    {
        if (m_closed)
        {
            throw DBException("M7N8O9P0Q1R2", "MongoDB connection is closed", system_utils::captureCallStack());
        }
    }

    std::string MongoDBConnection::generateSessionId()
    {
        uint64_t id = m_sessionCounter.fetch_add(1);
        std::ostringstream oss;
        oss << "session_" << id << "_" << std::chrono::steady_clock::now().time_since_epoch().count();
        return oss.str();
    }

    // ============================================================================
    // MongoDBConnection Implementation - Collection Registration (Public)
    // ============================================================================

    void MongoDBConnection::registerCollection(std::weak_ptr<MongoDBCollection> collection)
    {
        std::scoped_lock lock(m_collectionsMutex);
        if (m_activeCollections.size() > 50)
        {
            std::erase_if(m_activeCollections, [](const auto &w) { return w.expired(); });
        }
        m_activeCollections.insert(std::move(collection));
        MONGODB_DEBUG("MongoDBConnection::registerCollection - Registered collection, total: " << m_activeCollections.size());
    }

    void MongoDBConnection::unregisterCollection(std::weak_ptr<MongoDBCollection> collection)
    {
        std::scoped_lock lock(m_collectionsMutex);
        m_activeCollections.erase(collection);
        MONGODB_DEBUG("MongoDBConnection::unregisterCollection - Unregistered collection, remaining: " << m_activeCollections.size());
    }

    // ============================================================================
    // MongoDBConnection Implementation - Cursor Registration (Public)
    // ============================================================================

    void MongoDBConnection::registerCursor(std::weak_ptr<MongoDBCursor> cursor)
    {
        std::scoped_lock lock(m_cursorsMutex);
        if (m_activeCursors.size() > 50)
        {
            std::erase_if(m_activeCursors, [](const auto &w) { return w.expired(); });
        }
        m_activeCursors.insert(std::move(cursor));
        MONGODB_DEBUG("MongoDBConnection::registerCursor - Registered cursor, total: " << m_activeCursors.size());
    }

    void MongoDBConnection::unregisterCursor(std::weak_ptr<MongoDBCursor> cursor)
    {
        std::scoped_lock lock(m_cursorsMutex);
        m_activeCursors.erase(cursor);
        MONGODB_DEBUG("MongoDBConnection::unregisterCursor - Unregistered cursor, remaining: " << m_activeCursors.size());
    }

    // ============================================================================
    // MongoDBConnection Implementation - Constructor, Destructor, Move
    // ============================================================================

    MongoDBConnection::MongoDBConnection(std::nothrow_t,
                                         const std::string &uri,
                                         const std::string &user,
                                         const std::string &password,
                                         const std::map<std::string, std::string> &options) noexcept
        : m_url(uri)
#if DB_DRIVER_THREAD_SAFE
        , m_connMutex(std::make_shared<std::recursive_mutex>())
#endif
    {
        try
        {
            MONGODB_DEBUG("MongoDBConnection::constructor - Connecting to: " << uri);
            // Build the connection URI with credentials if provided
            std::string connectionUri = uri;

            // If user/password provided and not in URI, add them
            if (!user.empty() && !uri.contains("@"))
            {
                // Parse the URI to insert credentials
                size_t schemeEnd = uri.find("://");
                if (schemeEnd != std::string::npos)
                {
                    std::string scheme = uri.substr(0, schemeEnd + 3);
                    std::string rest = uri.substr(schemeEnd + 3);
                    connectionUri = scheme + user + ":" + password + "@" + rest;
                }
            }

            // Add options to URI if provided
            // Convert snake_case option names to lowerCamelCase as expected by mongoc
            if (!options.empty())
            {
                bool hasQuery = connectionUri.contains('?');
                for (const auto &[key, value] : options)
                {
                    connectionUri += (hasQuery ? "&" : "?");
                    // Convert option name from snake_case (YAML convention) to lowerCamelCase (mongoc convention)
                    std::string normalizedKey = system_utils::snakeCaseToLowerCamelCase(key);
                    connectionUri += normalizedKey + "=" + value;
                    hasQuery = true;
                }
            }

            // Parse and validate URI
            bson_error_t error;
            MongoUriHandle mongoUri(mongoc_uri_new_with_error(connectionUri.c_str(), &error));

            if (!mongoUri)
            {
                m_initFailed = true;
                m_initError = DBException("J4K5L6M7N8O9", std::string("Invalid MongoDB URI: ") + error.message, system_utils::captureCallStack());
                return;
            }

            // Extract database name from URI
            const char *dbName = mongoc_uri_get_database(mongoUri.get());
            if (dbName)
            {
                m_databaseName = dbName;
            }

            // Create client
            mongoc_client_t *rawClient = mongoc_client_new_from_uri(mongoUri.get());
            if (!rawClient)
            {
                m_initFailed = true;
                m_initError = DBException("K5L6M7N8O9P0", "Failed to create MongoDB client", system_utils::captureCallStack());
                return;
            }

            // Set application name for monitoring
            mongoc_client_set_appname(rawClient, "cpp_dbc");

            // Wrap in shared_ptr with custom deleter
            m_client = MongoClientHandle(rawClient, MongoClientDeleter());

            // Test connection with ping
            bson_t pingCmd = BSON_INITIALIZER;
            BSON_APPEND_INT32(&pingCmd, "ping", 1);

            bson_t reply;
            bson_init(&reply);

            MongoDatabaseHandle adminDb(mongoc_client_get_database(m_client.get(), "admin"));

            bool pingSuccess = mongoc_database_command_simple(
                adminDb.get(), &pingCmd, nullptr, &reply, &error);

            bson_destroy(&pingCmd);
            bson_destroy(&reply);

            if (!pingSuccess)
            {
                m_client.reset();
                m_initFailed = true;
                m_initError = DBException("L6M7N8O9P0Q1", std::string("Failed to connect to MongoDB: ") + error.message, system_utils::captureCallStack());
                return;
            }

            m_closed = false;
            MONGODB_DEBUG("MongoDBConnection::constructor - Connected successfully");
        }
        catch (const std::exception &ex)
        {
            m_initFailed = true;
            m_initError = DBException("MGDBCNCT0ERR", std::string("Unexpected error during connection: ") + ex.what(), system_utils::captureCallStack());
        }
        catch (...)
        {
            m_initFailed = true;
            m_initError = DBException("MGDBCNCTUNER", "Unknown error during connection", system_utils::captureCallStack());
        }
    }

    MongoDBConnection::~MongoDBConnection()
    {
        MONGODB_DEBUG("MongoDBConnection::destructor - Destroying connection");
        if (!m_closed)
        {
            try
            {
                // Explicitly call our own close() since virtual dispatch doesn't work in destructors
                MongoDBConnection::close();
            }
            catch (...)
            {
                // Suppress exceptions in destructor
                MONGODB_DEBUG("MongoDBConnection::destructor - Exception suppressed during close");
            }
        }
        MONGODB_DEBUG("MongoDBConnection::destructor - Done");
    }

    // ============================================================================
    // MongoDBConnection Implementation - DBConnection Interface
    // ============================================================================

    void MongoDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool MongoDBConnection::isClosed() const
    {
        return m_closed;
    }

    void MongoDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool MongoDBConnection::isPooled() const
    {
        return m_pooled;
    }

    std::string MongoDBConnection::getURL() const
    {
        return m_url;
    }

    void MongoDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    // ============================================================================
    // MongoDBConnection Implementation - DocumentDBConnection Interface (Database Ops)
    // ============================================================================

    std::string MongoDBConnection::getDatabaseName() const
    {
        auto result = getDatabaseName(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::vector<std::string> MongoDBConnection::listDatabases()
    {
        auto result = listDatabases(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MongoDBConnection::databaseExists(const std::string &databaseName)
    {
        auto result = databaseExists(std::nothrow, databaseName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void MongoDBConnection::useDatabase(const std::string &databaseName)
    {
        auto result = useDatabase(std::nothrow, databaseName);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MongoDBConnection::dropDatabase(const std::string &databaseName)
    {
        auto result = dropDatabase(std::nothrow, databaseName);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
