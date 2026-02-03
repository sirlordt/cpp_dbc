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
 * @file connection_02.cpp
 * @brief MongoDB MongoDBConnection - Part 2 (collection operations, commands, sessions, transactions)
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

    std::shared_ptr<DocumentDBCollection> MongoDBConnection::getCollection(const std::string &collectionName)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();

        if (m_databaseName.empty())
        {
            throw DBException("P0Q1R2S3T4U5", "No database selected - call useDatabase() first", system_utils::captureCallStack());
        }

        mongoc_collection_t *coll = mongoc_client_get_collection(
            m_client.get(), m_databaseName.c_str(), collectionName.c_str());

        if (!coll)
        {
            throw DBException("Q1R2S3T4U5V6", "Failed to get collection: " + collectionName, system_utils::captureCallStack());
        }

        return std::make_shared<MongoDBCollection>(
            std::weak_ptr<mongoc_client_t>(m_client), coll, collectionName, m_databaseName, weak_from_this()
#if DB_DRIVER_THREAD_SAFE
            , m_connMutex
#endif
        );
    }

    std::vector<std::string> MongoDBConnection::listCollections()
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();

        if (m_databaseName.empty())
        {
            throw DBException("R2S3T4U5V6W7", "No database selected - call useDatabase() first", system_utils::captureCallStack());
        }

        std::vector<std::string> result;

        MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), m_databaseName.c_str()));

        bson_error_t error;
        char **names = mongoc_database_get_collection_names_with_opts(db.get(), nullptr, &error);

        if (!names)
        {
            throw DBException("S3T4U5V6W7X8", std::string("Failed to list collections: ") + error.message, system_utils::captureCallStack());
        }

        for (char **ptr = names; *ptr; ptr++)
        {
            result.emplace_back(*ptr);
        }

        bson_strfreev(names);
        return result;
    }

    bool MongoDBConnection::collectionExists(const std::string &collectionName)
    {
        auto collections = listCollections();
        return std::ranges::find(collections, collectionName) != collections.end();
    }

    std::shared_ptr<DocumentDBCollection> MongoDBConnection::createCollection(
        const std::string &collectionName,
        const std::string &options)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();

        if (m_databaseName.empty())
        {
            throw DBException("T4U5V6W7X8Y9", "No database selected - call useDatabase() first", system_utils::captureCallStack());
        }

        MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), m_databaseName.c_str()));

        // Use RAII for options BSON to prevent memory leaks
        BsonHandle optsHandle;
        if (!options.empty())
        {
            bson_error_t parseError;
            bson_t *opts = bson_new_from_json(
                reinterpret_cast<const uint8_t *>(options.c_str()),
                static_cast<ssize_t>(options.length()),
                &parseError);
            if (!opts)
            {
                throw DBException("U5V6W7X8Y9Z0", std::string("Invalid options JSON: ") + parseError.message, system_utils::captureCallStack());
            }
            optsHandle.reset(opts);
        }

        bson_error_t error;
        mongoc_collection_t *coll = mongoc_database_create_collection(
            db.get(), collectionName.c_str(), optsHandle.get(), &error);

        if (!coll)
        {
            throw DBException("V6W7X8Y9Z0A1", std::string("Failed to create collection: ") + error.message, system_utils::captureCallStack());
        }

        return std::make_shared<MongoDBCollection>(
            std::weak_ptr<mongoc_client_t>(m_client), coll, collectionName, m_databaseName, weak_from_this()
#if DB_DRIVER_THREAD_SAFE
            , m_connMutex
#endif
        );
    }

    void MongoDBConnection::dropCollection(const std::string &collectionName)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();

        if (m_databaseName.empty())
        {
            throw DBException("W7X8Y9Z0A1B2", "No database selected - call useDatabase() first", system_utils::captureCallStack());
        }

        mongoc_collection_t *coll = mongoc_client_get_collection(
            m_client.get(), m_databaseName.c_str(), collectionName.c_str());

        if (!coll)
        {
            throw DBException("X8Y9Z0A1B2C3", "Failed to get collection: " + collectionName, system_utils::captureCallStack());
        }

        MongoCollectionHandle collHandle(coll);

        bson_error_t error;
        bool success = mongoc_collection_drop(collHandle.get(), &error);

        if (!success)
        {
            throw DBException("Y9Z0A1B2C3D4", std::string("Failed to drop collection: ") + error.message, system_utils::captureCallStack());
        }
    }

    std::shared_ptr<DocumentDBData> MongoDBConnection::createDocument()
    {
        return std::make_shared<MongoDBDocument>();
    }

    std::shared_ptr<DocumentDBData> MongoDBConnection::createDocument(const std::string &json)
    {
        return std::make_shared<MongoDBDocument>(json);
    }

    std::shared_ptr<DocumentDBData> MongoDBConnection::runCommand(const std::string &command)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();

        if (m_databaseName.empty())
        {
            throw DBException("Z0A1B2C3D4E5", "No database selected - call useDatabase() first", system_utils::captureCallStack());
        }

        BsonHandle cmdBson = makeBsonHandleFromJson(command);

        MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), m_databaseName.c_str()));

        bson_error_t error;
        bson_t reply;
        bson_init(&reply);

        bool success = mongoc_database_command_simple(db.get(), cmdBson.get(), nullptr, &reply, &error);

        if (!success)
        {
            bson_destroy(&reply);
            throw DBException("GWZKCW7PLOKY", std::string("Command failed: ") + error.message, system_utils::captureCallStack());
        }

        bson_t *replyCopy = bson_copy(&reply);
        bson_destroy(&reply);

        if (!replyCopy)
        {
            throw DBException("JADHTSVC5KEH", "Failed to copy command reply (memory allocation failure)", system_utils::captureCallStack());
        }

        return std::make_shared<MongoDBDocument>(replyCopy);
    }

    std::shared_ptr<DocumentDBData> MongoDBConnection::getServerInfo()
    {
        return runCommand("{\"buildInfo\": 1}");
    }

    std::shared_ptr<DocumentDBData> MongoDBConnection::getServerStatus()
    {
        return runCommand("{\"serverStatus\": 1}");
    }

    bool MongoDBConnection::ping()
    {
        MONGODB_LOCK_GUARD(*m_connMutex);

        if (m_closed)
            return false;

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

    std::string MongoDBConnection::startSession()
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
            throw DBException("B2C3D4E5F6G7", std::string("Failed to start session: ") + error.message, system_utils::captureCallStack());
        }

        std::string sessionId = generateSessionId();

        {
            std::scoped_lock sessionsLock(m_sessionsMutex);
            m_sessions[sessionId] = MongoSessionHandle(session);
        }

        return sessionId;
    }

    void MongoDBConnection::endSession(const std::string &sessionId)
    {
        std::scoped_lock lock(m_sessionsMutex);

        auto it = m_sessions.find(sessionId);
        if (it != m_sessions.end())
        {
            m_sessions.erase(it);
        }
    }

    void MongoDBConnection::startTransaction(const std::string &sessionId)
    {
        std::scoped_lock lock(m_sessionsMutex);

        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end())
        {
            throw DBException("C3D4E5F6G7H8", "Session not found: " + sessionId, system_utils::captureCallStack());
        }

        bson_error_t error;
        bool success = mongoc_client_session_start_transaction(it->second.get(), nullptr, &error);

        if (!success)
        {
            throw DBException("D4E5F6G7H8I9", std::string("Failed to start transaction: ") + error.message, system_utils::captureCallStack());
        }
    }

    void MongoDBConnection::commitTransaction(const std::string &sessionId)
    {
        std::scoped_lock lock(m_sessionsMutex);

        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end())
        {
            throw DBException("E5F6G7H8I9J0", "Session not found: " + sessionId, system_utils::captureCallStack());
        }

        bson_t reply;
        bson_init(&reply);

        bson_error_t error;
        bool success = mongoc_client_session_commit_transaction(it->second.get(), &reply, &error);

        bson_destroy(&reply);

        if (!success)
        {
            throw DBException("F6G7H8I9J0K1", std::string("Failed to commit transaction: ") + error.message, system_utils::captureCallStack());
        }
    }

    void MongoDBConnection::abortTransaction(const std::string &sessionId)
    {
        std::scoped_lock lock(m_sessionsMutex);

        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end())
        {
            throw DBException("G7H8I9J0K1L2", "Session not found: " + sessionId, system_utils::captureCallStack());
        }

        bson_error_t error;
        bool success = mongoc_client_session_abort_transaction(it->second.get(), &error);

        if (!success)
        {
            throw DBException("H8I9J0K1L2M3", std::string("Failed to abort transaction: ") + error.message, system_utils::captureCallStack());
        }
    }

    bool MongoDBConnection::supportsTransactions()
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

    void MongoDBConnection::prepareForPoolReturn()
    {
        MONGODB_DEBUG("MongoDBConnection::prepareForPoolReturn - Cleaning up connection");

        // Close all active cursors
        {
            std::scoped_lock cursorsLock(m_cursorsMutex);
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
                        MONGODB_DEBUG("prepareForPoolReturn - Exception closing cursor: " << ex.what());
                    }
                }
            }
            m_activeCursors.clear();
        }

        // End all active sessions (this also aborts any active transactions)
        {
            std::scoped_lock sessionsLock(m_sessionsMutex);
            m_sessions.clear();
        }

        // Clear active collections (they use weak_ptr so just clear the registry)
        {
            std::scoped_lock collectionsLock(m_collectionsMutex);
            m_activeCollections.clear();
        }

        MONGODB_DEBUG("MongoDBConnection::prepareForPoolReturn - Cleanup complete");
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
