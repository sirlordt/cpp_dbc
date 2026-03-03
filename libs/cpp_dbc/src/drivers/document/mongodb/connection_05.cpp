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
 * @file connection_05.cpp
 * @brief MongoDB MongoDBConnection - Part 5 (nothrow versions: DBConnection interface and session/transaction operations)
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
    // MongoDBConnection - DBConnection nothrow interface implementations
    // ============================================================================

    expected<void, DBException> MongoDBConnection::close(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed)
                return {};

            MONGODB_DEBUG("MongoDBConnection::close(nothrow) - Closing connection");

            // Close all active cursors BEFORE destroying the client
            {
                std::scoped_lock cursorsLock(m_cursorsMutex);
                MONGODB_DEBUG("MongoDBConnection::close(nothrow) - Closing " << m_activeCursors.size() << " active cursors");
                for (const auto &weakCursor : m_activeCursors)
                {
                    if (auto cursor = weakCursor.lock())
                    {
                        try { cursor->close(); }
                        catch ([[maybe_unused]] const std::exception &ex)
                        {
                            MONGODB_DEBUG("MongoDBConnection::close(nothrow) - Exception ignored during cursor cleanup: " << ex.what());
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

            m_client.reset();
            m_closed = true;

            MONGODB_DEBUG("MongoDBConnection::close(nothrow) - Connection closed");
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("7DGLQ7C4QX4R",
                std::string("Exception in close: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("VS3RNHCXXBMC",
                "Unknown exception in close",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBConnection::reset(std::nothrow_t) noexcept
    {
        return prepareForPoolReturn(std::nothrow);
    }

    expected<bool, DBException> MongoDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load();
    }

    expected<void, DBException> MongoDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        if (m_pooled)
        {
            return reset(std::nothrow);
        }
        return close(std::nothrow);
    }

    expected<bool, DBException> MongoDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return m_pooled;
    }

    expected<std::string, DBException> MongoDBConnection::getURL(std::nothrow_t) const noexcept
    {
        try
        {
            return m_url;
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("GVJMECK6CHOE",
                std::string("Exception in getURL: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("TUBIOISE94Y7",
                "Unknown exception in getURL",
                system_utils::captureCallStack()));
        }
    }

    // ============================================================================
    // MongoDBConnection - DocumentDBConnection nothrow interface implementations
    // ============================================================================

    expected<void, DBException> MongoDBConnection::prepareForPoolReturn(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBConnection::prepareForPoolReturn(nothrow) - Cleaning up connection");

            // Close all active cursors
            {
                std::scoped_lock cursorsLock(m_cursorsMutex);
                for (const auto &weakCursor : m_activeCursors)
                {
                    if (auto cursor = weakCursor.lock())
                    {
                        try { cursor->close(); }
                        catch ([[maybe_unused]] const std::exception &ex)
                        {
                            MONGODB_DEBUG("prepareForPoolReturn(nothrow) - Exception closing cursor: " << ex.what());
                        }
                    }
                }
                m_activeCursors.clear();
            }

            // End all active sessions (also aborts any active transactions)
            {
                std::scoped_lock sessionsLock(m_sessionsMutex);
                m_sessions.clear();
            }

            // Clear active collections
            {
                std::scoped_lock collectionsLock(m_collectionsMutex);
                m_activeCollections.clear();
            }

            MONGODB_DEBUG("MongoDBConnection::prepareForPoolReturn(nothrow) - Cleanup complete");
            return {};
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("OINP1LDHWCQ7",
                std::string("Exception in prepareForPoolReturn: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("MZTNXIRUAJZZ",
                "Unknown exception in prepareForPoolReturn",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        // No-op for MongoDB: unlike relational databases (e.g. Firebird MVCC),
        // MongoDB does not require any state refresh when borrowing from the pool.
        return {};
    }

    expected<std::string, DBException> MongoDBConnection::getDatabaseName(std::nothrow_t) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);
            return m_databaseName;
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("SXWDJ4BE7HDN",
                std::string("Exception in getDatabaseName: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("ZD984T94Z3QJ",
                "Unknown exception in getDatabaseName",
                system_utils::captureCallStack()));
        }
    }

    expected<bool, DBException> MongoDBConnection::databaseExists(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        try
        {
            auto dbsResult = listDatabases(std::nothrow);
            if (!dbsResult)
            {
                return cpp_dbc::unexpected(dbsResult.error());
            }
            const auto &databases = *dbsResult;
            return std::ranges::find(databases, databaseName) != databases.end();
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("CLA4YAJP87LT",
                std::string("Exception in databaseExists: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("M38AKVVOH6MQ",
                "Unknown exception in databaseExists",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBConnection::useDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);
            validateConnection();
            m_databaseName = databaseName;
            return {};
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("W7OJJY9U0NHH",
                std::string("Exception in useDatabase: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("AK6049UQC43W",
                "Unknown exception in useDatabase",
                system_utils::captureCallStack()));
        }
    }

    expected<bool, DBException> MongoDBConnection::collectionExists(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        try
        {
            auto colsResult = listCollections(std::nothrow);
            if (!colsResult)
            {
                return cpp_dbc::unexpected(colsResult.error());
            }
            const auto &collections = *colsResult;
            return std::ranges::find(collections, collectionName) != collections.end();
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("HCA7AJHZUCGE",
                std::string("Exception in collectionExists: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("R3LWE02BEA9G",
                "Unknown exception in collectionExists",
                system_utils::captureCallStack()));
        }
    }

    expected<bool, DBException> MongoDBConnection::ping(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed)
            {
                return false;
            }

            bson_t pingCmd = BSON_INITIALIZER;
            BSON_APPEND_INT32(&pingCmd, "ping", 1);

            bson_t reply;
            bson_init(&reply);

            MongoDatabaseHandle adminDb(mongoc_client_get_database(m_client.get(), "admin"));

            bson_error_t error;
            bool success = mongoc_database_command_simple(adminDb.get(), &pingCmd, nullptr, &reply, &error);

            bson_destroy(&pingCmd);
            bson_destroy(&reply);

            return success;
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("DBYCQKE0VYE4",
                std::string("Exception in ping: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("8J8QQ3PWPMI7",
                "Unknown exception in ping",
                system_utils::captureCallStack()));
        }
    }

    expected<std::string, DBException> MongoDBConnection::startSession(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);
            validateConnection();

            mongoc_session_opt_t *opts = mongoc_session_opts_new();
            mongoc_session_opts_set_causal_consistency(opts, true);

            bson_error_t error;
            mongoc_client_session_t *session = mongoc_client_start_session(m_client.get(), opts, &error);

            mongoc_session_opts_destroy(opts);

            if (!session)
            {
                return cpp_dbc::unexpected(DBException("B2C3D4E5F6G7",
                    std::string("Failed to start session: ") + error.message,
                    system_utils::captureCallStack()));
            }

            std::string sessionId = generateSessionId();

            {
                std::scoped_lock sessionsLock(m_sessionsMutex);
                m_sessions[sessionId] = MongoSessionHandle(session);
            }

            return sessionId;
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("BCHTJZZLWQIE",
                std::string("Exception in startSession: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("VYHRUJIES2UT",
                "Unknown exception in startSession",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBConnection::endSession(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        try
        {
            std::scoped_lock lock(m_sessionsMutex);
            auto it = m_sessions.find(sessionId);
            if (it != m_sessions.end())
            {
                m_sessions.erase(it);
            }
            return {};
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("RXK2INDHV9CO",
                std::string("Exception in endSession: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("P5T6IBVVY4LY",
                "Unknown exception in endSession",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBConnection::startTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        try
        {
            std::scoped_lock lock(m_sessionsMutex);

            auto it = m_sessions.find(sessionId);
            if (it == m_sessions.end())
            {
                return cpp_dbc::unexpected(DBException("C3D4E5F6G7H8",
                    "Session not found: " + sessionId,
                    system_utils::captureCallStack()));
            }

            bson_error_t error;
            bool success = mongoc_client_session_start_transaction(it->second.get(), nullptr, &error);

            if (!success)
            {
                return cpp_dbc::unexpected(DBException("D4E5F6G7H8I9",
                    std::string("Failed to start transaction: ") + error.message,
                    system_utils::captureCallStack()));
            }

            return {};
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("VCLTNNKFT0IK",
                std::string("Exception in startTransaction: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("27BAFS8P63VP",
                "Unknown exception in startTransaction",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBConnection::commitTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        try
        {
            std::scoped_lock lock(m_sessionsMutex);

            auto it = m_sessions.find(sessionId);
            if (it == m_sessions.end())
            {
                return cpp_dbc::unexpected(DBException("E5F6G7H8I9J0",
                    "Session not found: " + sessionId,
                    system_utils::captureCallStack()));
            }

            bson_t reply;
            bson_init(&reply);

            bson_error_t error;
            bool success = mongoc_client_session_commit_transaction(it->second.get(), &reply, &error);

            bson_destroy(&reply);

            if (!success)
            {
                return cpp_dbc::unexpected(DBException("F6G7H8I9J0K1",
                    std::string("Failed to commit transaction: ") + error.message,
                    system_utils::captureCallStack()));
            }

            return {};
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("NP61YLGUZ60N",
                std::string("Exception in commitTransaction: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("YUHT2EY5YZ9L",
                "Unknown exception in commitTransaction",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBConnection::abortTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        try
        {
            std::scoped_lock lock(m_sessionsMutex);

            auto it = m_sessions.find(sessionId);
            if (it == m_sessions.end())
            {
                return cpp_dbc::unexpected(DBException("G7H8I9J0K1L2",
                    "Session not found: " + sessionId,
                    system_utils::captureCallStack()));
            }

            bson_error_t error;
            bool success = mongoc_client_session_abort_transaction(it->second.get(), &error);

            if (!success)
            {
                return cpp_dbc::unexpected(DBException("H8I9J0K1L2M3",
                    std::string("Failed to abort transaction: ") + error.message,
                    system_utils::captureCallStack()));
            }

            return {};
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("HLS4PYB1NRN4",
                std::string("Exception in abortTransaction: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("VXMIHE0LNXWG",
                "Unknown exception in abortTransaction",
                system_utils::captureCallStack()));
        }
    }

    expected<bool, DBException> MongoDBConnection::supportsTransactions(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (!m_client)
            {
                return false;
            }

            // Select a server to get its description
            bson_error_t error;
            mongoc_server_description_t *serverDesc = mongoc_client_select_server(
                m_client.get(), false, nullptr, &error);

            if (!serverDesc)
            {
                MONGODB_DEBUG("supportsTransactions: Failed to select server - " << error.message);
                return false;
            }

            // Check server type using mongoc_server_description_type()
            // Transactions are only supported on replica sets and mongos (sharded clusters)
            const char *serverType = mongoc_server_description_type(serverDesc);
            if (!serverType)
            {
                mongoc_server_description_destroy(serverDesc);
                return false;
            }

            // Standalone servers do NOT support transactions
            // Only RSPrimary, RSSecondary, and Mongos support transactions
            bool isReplicaSet = (strcmp(serverType, "RSPrimary") == 0 ||
                                 strcmp(serverType, "RSSecondary") == 0 ||
                                 strcmp(serverType, "RSArbiter") == 0 ||
                                 strcmp(serverType, "RSOther") == 0);
            bool isMongos = (strcmp(serverType, "Mongos") == 0);

            if (!isReplicaSet && !isMongos)
            {
                MONGODB_DEBUG("supportsTransactions: Server type '" << serverType << "' does not support transactions");
                mongoc_server_description_destroy(serverDesc);
                return false;
            }

            // Get the hello/ismaster response
            const bson_t *helloResponse = mongoc_server_description_hello_response(serverDesc);
            if (!helloResponse)
            {
                mongoc_server_description_destroy(serverDesc);
                return false;
            }

            // Check for logicalSessionTimeoutMinutes (required for transactions)
            // Also verify it's not BSON_TYPE_NULL
            bson_iter_t iter;
            if (!bson_iter_init_find(&iter, helloResponse, "logicalSessionTimeoutMinutes") ||
                bson_iter_type(&iter) == BSON_TYPE_NULL)
            {
                mongoc_server_description_destroy(serverDesc);
                return false;
            }

            // Get maxWireVersion safely
            int32_t maxWireVersion = 0;
            if (bson_iter_init_find(&iter, helloResponse, "maxWireVersion") &&
                BSON_ITER_HOLDS_INT32(&iter))
            {
                maxWireVersion = bson_iter_int32(&iter);
            }

            mongoc_server_description_destroy(serverDesc);

            // Transactions require:
            // - Replica set: maxWireVersion >= 7 (MongoDB 4.0+)
            // - Mongos (sharded): maxWireVersion >= 8 (MongoDB 4.2+)
            if (isMongos)
            {
                return maxWireVersion >= 8;
            }

            return maxWireVersion >= 7;
        }
        catch (const DBException &e) { return cpp_dbc::unexpected(e); }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("FE471BR4PG7X",
                std::string("Exception in supportsTransactions: ") + e.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("LBIGF6VZJ2R7",
                "Unknown exception in supportsTransactions",
                system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
