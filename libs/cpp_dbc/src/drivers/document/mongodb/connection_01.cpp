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

    MongoDBConnection::MongoDBConnection(const std::string &uri,
                                         const std::string &user,
                                         const std::string &password,
                                         const std::map<std::string, std::string> &options)
        : m_url(uri)
#if DB_DRIVER_THREAD_SAFE
        , m_connMutex(std::make_shared<std::recursive_mutex>())
#endif
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
        if (!options.empty())
        {
            bool hasQuery = connectionUri.contains('?');
            for (const auto &[key, value] : options)
            {
                connectionUri += (hasQuery ? "&" : "?");
                connectionUri += key + "=" + value;
                hasQuery = true;
            }
        }

        // Parse and validate URI
        bson_error_t error;
        MongoUriHandle mongoUri(mongoc_uri_new_with_error(connectionUri.c_str(), &error));

        if (!mongoUri)
        {
            throw DBException("J4K5L6M7N8O9", std::string("Invalid MongoDB URI: ") + error.message, system_utils::captureCallStack());
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
            throw DBException("K5L6M7N8O9P0", "Failed to create MongoDB client", system_utils::captureCallStack());
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
            throw DBException("L6M7N8O9P0Q1", std::string("Failed to connect to MongoDB: ") + error.message, system_utils::captureCallStack());
        }

        m_closed = false;
        MONGODB_DEBUG("MongoDBConnection::constructor - Connected successfully");
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

    MongoDBConnection::MongoDBConnection(MongoDBConnection &&other) noexcept
        : m_client(std::move(other.m_client)),
          m_databaseName(std::move(other.m_databaseName)),
          m_url(std::move(other.m_url)),
          m_closed(other.m_closed.load()),
          m_pooled(other.m_pooled),
          m_sessions(std::move(other.m_sessions)),
          m_sessionCounter(other.m_sessionCounter.load())
    {
        other.m_closed = true;
    }

    MongoDBConnection &MongoDBConnection::operator=(MongoDBConnection &&other) noexcept
    {
        if (this != &other)
        {
            // Guard close() call since it can throw and this function is noexcept.
            // We must ensure the object is in a valid state even if close() fails.
            if (!m_closed)
            {
                try
                {
                    close();
                }
                catch (...)
                {
                    // close() threw - mark as closed to maintain invariants
                    m_closed = true;
                    MONGODB_DEBUG("MongoDBConnection::operator= - close() threw during move assignment, marking as closed");
                }
            }

            m_client = std::move(other.m_client);
            m_databaseName = std::move(other.m_databaseName);
            m_url = std::move(other.m_url);
            m_closed = other.m_closed.load();
            m_pooled = other.m_pooled;
            m_sessions = std::move(other.m_sessions);
            m_sessionCounter = other.m_sessionCounter.load();

            other.m_closed = true;
        }
        return *this;
    }

    // ============================================================================
    // MongoDBConnection Implementation - DBConnection Interface
    // ============================================================================

    void MongoDBConnection::close()
    {
        MONGODB_LOCK_GUARD(*m_connMutex);

        if (m_closed)
            return;

        MONGODB_DEBUG("MongoDBConnection::close - Closing connection");

        // Close all active cursors BEFORE destroying the client
        // This is critical: cursors must be destroyed before the client
        {
            std::scoped_lock cursorsLock(m_cursorsMutex);
            MONGODB_DEBUG("MongoDBConnection::close - Closing " << m_activeCursors.size() << " active cursors");

            for (const auto &weakCursor : m_activeCursors)
            {
                if (auto cursor = weakCursor.lock())
                {
                    try
                    {
                        cursor->close();
                    }
                    catch ([[maybe_unused]] const std::exception &ex)
                    {
                        // Ignore errors during cleanup
                        MONGODB_DEBUG("MongoDBConnection::close - Exception ignored during cursor cleanup: " << ex.what());
                    }
                }
            }

            m_activeCursors.clear();
        }

        // End all active sessions
        {
            std::scoped_lock sessionsLock(m_sessionsMutex);
            m_sessions.clear();
        }

        // Clear active collections
        {
            std::scoped_lock collectionsLock(m_collectionsMutex);
            m_activeCollections.clear();
        }

        // Now it's safe to destroy the client
        m_client.reset();
        m_closed = true;

        MONGODB_DEBUG("MongoDBConnection::close - Connection closed");
    }

    bool MongoDBConnection::isClosed() const
    {
        return m_closed;
    }

    void MongoDBConnection::returnToPool()
    {
        // If pooled, just mark as available (pool handles this)
        // If not pooled, close the connection
        if (!m_pooled)
        {
            close();
        }
    }

    bool MongoDBConnection::isPooled()
    {
        return m_pooled;
    }

    std::string MongoDBConnection::getURL() const
    {
        return m_url;
    }

    // ============================================================================
    // MongoDBConnection Implementation - DocumentDBConnection Interface (Database Ops)
    // ============================================================================

    std::string MongoDBConnection::getDatabaseName() const
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        return m_databaseName;
    }

    std::vector<std::string> MongoDBConnection::listDatabases()
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();

        std::vector<std::string> result;

        bson_error_t error;
        char **names = mongoc_client_get_database_names_with_opts(m_client.get(), nullptr, &error);

        if (!names)
        {
            throw DBException("N8O9P0Q1R2S3", std::string("Failed to list databases: ") + error.message, system_utils::captureCallStack());
        }

        for (char **ptr = names; *ptr; ptr++)
        {
            result.emplace_back(*ptr);
        }

        bson_strfreev(names);
        return result;
    }

    bool MongoDBConnection::databaseExists(const std::string &databaseName)
    {
        auto databases = listDatabases();
        return std::ranges::find(databases, databaseName) != databases.end();
    }

    void MongoDBConnection::useDatabase(const std::string &databaseName)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();
        m_databaseName = databaseName;
    }

    void MongoDBConnection::dropDatabase(const std::string &databaseName)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();

        MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), databaseName.c_str()));

        bson_error_t error;
        bool success = mongoc_database_drop(db.get(), &error);

        if (!success)
        {
            throw DBException("O9P0Q1R2S3T4", std::string("Failed to drop database: ") + error.message, system_utils::captureCallStack());
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
