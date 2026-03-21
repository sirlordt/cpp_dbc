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
 * @brief MongoDB MongoDBConnection - Part 5 (MongoDB-specific methods and nothrow API:
 *        createDocument, runCommand, getServerInfo, getServerStatus, ping,
 *        startSession, endSession, startTransaction, commitTransaction,
 *        abortTransaction, supportsTransactions, prepareForPoolReturn)
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

    // ====================================================================
    // NOTHROW API - createDocument, runCommand, getServerInfo, getServerStatus (real implementations)
    // ====================================================================

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBConnection::createDocument(std::nothrow_t) noexcept
    {
        auto r = MongoDBDocument::create(std::nothrow);
        if (!r.has_value())
        {
            return unexpected<DBException>(r.error());
        }
        return std::static_pointer_cast<DocumentDBData>(r.value());
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBConnection::createDocument(
        std::nothrow_t, const std::string &json) noexcept
    {
        auto r = MongoDBDocument::create(std::nothrow, json);
        if (!r.has_value())
        {
            return unexpected<DBException>(r.error());
        }
        return std::static_pointer_cast<DocumentDBData>(r.value());
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBConnection::runCommand(
        std::nothrow_t, const std::string &command) noexcept
    {
        MONGODB_LOCK_GUARD(*m_connMutex);

        if (m_closed.load(std::memory_order_seq_cst))
        {
            return unexpected<DBException>(DBException(
                "44TYH8VEG840",
                "Connection has been closed"));
        }

        if (m_databaseName.empty())
        {
            return unexpected<DBException>(DBException(
                "2Q3BVH2J9131",
                "No database selected. Call useDatabase() first"));
        }

        auto cmdBsonResult = makeBsonHandleFromJson(std::nothrow, command);
        if (!cmdBsonResult.has_value())
        {
            return unexpected<DBException>(cmdBsonResult.error());
        }
        BsonHandle cmdBson = std::move(cmdBsonResult.value());

        MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), m_databaseName.c_str()));

        bson_error_t error;
        bson_t reply;
        bson_init(&reply);

        bool success = mongoc_database_command_simple(db.get(), cmdBson.get(), nullptr, &reply, &error);

        if (!success)
        {
            bson_destroy(&reply);
            return unexpected<DBException>(DBException(
                "1T8I8LQS1TCB",
                std::string("Command failed: ") + error.message));
        }

        bson_t *replyCopy = bson_copy(&reply);
        bson_destroy(&reply);

        if (!replyCopy)
        {
            return unexpected<DBException>(DBException(
                "9J4REIE6R4YN",
                "Failed to copy command reply"));
        }

        auto docResult = MongoDBDocument::create(std::nothrow, replyCopy);
        if (!docResult.has_value())
        {
            return unexpected<DBException>(docResult.error());
        }
        return std::static_pointer_cast<DocumentDBData>(docResult.value());
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBConnection::getServerInfoAsDocument(std::nothrow_t) noexcept
    {
        return runCommand(std::nothrow, "{\"buildInfo\": 1}");
    }

    expected<std::string, DBException> MongoDBConnection::getServerVersion(std::nothrow_t) noexcept
    {
        auto infoResult = getServerInfoAsDocument(std::nothrow);
        if (!infoResult.has_value())
        {
            return cpp_dbc::unexpected(infoResult.error());
        }

        auto jsonResult = infoResult.value()->toJson(std::nothrow);
        if (!jsonResult.has_value())
        {
            return cpp_dbc::unexpected(DBException(
                "J1RQ2VIMGJKT",
                "Failed to convert buildInfo to JSON",
                system_utils::captureCallStack()));
        }

        // Parse "version" field from buildInfo JSON
        // buildInfo response contains: { "version": "x.y.z", ... }
        const auto &json = jsonResult.value();
        auto versionPos = json.find("\"version\"");
        if (versionPos == std::string::npos)
        {
            return cpp_dbc::unexpected(DBException(
                "TWYLCN2CBFA1",
                "buildInfo response does not contain 'version' field",
                system_utils::captureCallStack()));
        }

        auto colonPos = json.find(':', versionPos);
        if (colonPos == std::string::npos)
        {
            return cpp_dbc::unexpected(DBException(
                "58TNO5XZPFEX",
                "Malformed buildInfo: no colon after 'version' key",
                system_utils::captureCallStack()));
        }

        auto quoteStart = json.find('"', colonPos + 1);
        auto quoteEnd = json.find('"', quoteStart + 1);
        if (quoteStart == std::string::npos || quoteEnd == std::string::npos)
        {
            return cpp_dbc::unexpected(DBException(
                "SP5ODWC0F7DS",
                "Malformed buildInfo: cannot extract version string",
                system_utils::captureCallStack()));
        }

        return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    }

    expected<std::map<std::string, std::string>, DBException> MongoDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        auto infoResult = getServerInfoAsDocument(std::nothrow);
        if (!infoResult.has_value())
        {
            return cpp_dbc::unexpected(infoResult.error());
        }

        auto jsonResult = infoResult.value()->toJson(std::nothrow);
        if (!jsonResult.has_value())
        {
            return cpp_dbc::unexpected(DBException(
                "FVBU9EJNANCK",
                "Failed to convert buildInfo to JSON",
                system_utils::captureCallStack()));
        }

        // Parse top-level string fields from the buildInfo JSON into a flat map.
        // MongoDB buildInfo contains: version, gitVersion, sysInfo, allocator, modules, etc.
        std::map<std::string, std::string> info;
        const auto &json = jsonResult.value();

        // Helper lambda to extract a string value for a given key
        auto extractField = [&json](const std::string &key) -> std::string // NOSONAR(cpp:S1188) — simple linear JSON field extractor, splitting would reduce readability
        {
            auto keyStr = "\"" + key + "\"";
            auto keyPos = json.find(keyStr);
            if (keyPos == std::string::npos)
            {
                return "";
            }
            auto colonPos = json.find(':', keyPos + keyStr.size());
            if (colonPos == std::string::npos)
            {
                return "";
            }
            // Skip whitespace after colon
            auto valueStart = json.find_first_not_of(" \t\n\r", colonPos + 1);
            if (valueStart == std::string::npos)
            {
                return "";
            }
            if (json[valueStart] == '"')
            {
                // String value
                auto valueEnd = json.find('"', valueStart + 1);
                if (valueEnd == std::string::npos)
                {
                    return "";
                }
                return json.substr(valueStart + 1, valueEnd - valueStart - 1);
            }
            // Numeric or boolean value — read until comma, brace, or bracket
            auto valueEnd = json.find_first_of(",}]", valueStart);
            if (valueEnd == std::string::npos)
            {
                return "";
            }
            auto val = json.substr(valueStart, valueEnd - valueStart);
            // Trim trailing whitespace
            while (!val.empty() && (val.back() == ' ' || val.back() == '\t' || val.back() == '\n' || val.back() == '\r'))
            {
                val.pop_back();
            }
            return val;
        };

        // Extract known buildInfo fields
        info["ServerVersion"] = extractField("version");
        auto gitVersion = extractField("gitVersion");
        if (!gitVersion.empty())
        {
            info["GitVersion"] = gitVersion;
        }
        auto sysInfo = extractField("sysInfo");
        if (!sysInfo.empty())
        {
            info["SysInfo"] = sysInfo;
        }
        auto allocator = extractField("allocator");
        if (!allocator.empty())
        {
            info["Allocator"] = allocator;
        }
        auto javascriptEngine = extractField("javascriptEngine");
        if (!javascriptEngine.empty())
        {
            info["JavascriptEngine"] = javascriptEngine;
        }
        auto bits = extractField("bits");
        if (!bits.empty())
        {
            info["Bits"] = bits;
        }
        auto maxBsonObjectSize = extractField("maxBsonObjectSize");
        if (!maxBsonObjectSize.empty())
        {
            info["MaxBsonObjectSize"] = maxBsonObjectSize;
        }

        return info;
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBConnection::getServerStatus(std::nothrow_t) noexcept
    {
        return runCommand(std::nothrow, "{\"serverStatus\": 1}");
    }

    // ====================================================================
    // NOTHROW API - ping, startSession, endSession, startTransaction, commitTransaction,
    //               abortTransaction, supportsTransactions, prepareForPoolReturn (real implementations)
    // ====================================================================

    expected<bool, DBException> MongoDBConnection::ping(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load(std::memory_order_seq_cst))
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("DBYCQKE0VYE4",
                std::string("Exception in ping: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
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
            {
                auto r = validateConnection(std::nothrow);
                if (!r.has_value())
                {
                    return cpp_dbc::unexpected(r.error());
                }
            }

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
            m_sessions[sessionId] = MongoSessionHandle(session);
            return sessionId;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("BCHTJZZLWQIE",
                std::string("Exception in startSession: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
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
            MONGODB_LOCK_GUARD(*m_connMutex);
            auto it = m_sessions.find(sessionId);
            if (it != m_sessions.end())
            {
                m_sessions.erase(it);
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RXK2INDHV9CO",
                std::string("Exception in endSession: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
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
            MONGODB_LOCK_GUARD(*m_connMutex);

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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("VCLTNNKFT0IK",
                std::string("Exception in startTransaction: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
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
            MONGODB_LOCK_GUARD(*m_connMutex);

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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("NP61YLGUZ60N",
                std::string("Exception in commitTransaction: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
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
            MONGODB_LOCK_GUARD(*m_connMutex);

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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("HLS4PYB1NRN4",
                std::string("Exception in abortTransaction: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
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
                MONGODB_DEBUG("supportsTransactions: Failed to select server - %s", error.message);
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
                MONGODB_DEBUG("supportsTransactions: Server type '%s' does not support transactions", serverType);
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("FE471BR4PG7X",
                std::string("Exception in supportsTransactions: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected(DBException("LBIGF6VZJ2R7",
                "Unknown exception in supportsTransactions",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException>
    MongoDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        m_transactionIsolation = level;
        return {};
    }

    expected<TransactionIsolationLevel, DBException>
    MongoDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        return m_transactionIsolation;
    }

    expected<void, DBException>
    MongoDBConnection::prepareForPoolReturn(std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBConnection::prepareForPoolReturn(nothrow) - Cleaning up connection");

            MONGODB_LOCK_GUARD(*m_connMutex);

            // Close all active cursors
            for (const auto &weakCursor : m_activeCursors)
            {
                if (auto cursor = weakCursor.lock())
                {
                    auto r = cursor->close(std::nothrow);
                    if (!r.has_value())
                    {
                        MONGODB_DEBUG("prepareForPoolReturn(nothrow) - Error ignored during cursor cleanup: %s", r.error().what());
                    }
                }
            }
            m_activeCursors.clear();

            // End all active sessions (also aborts any active transactions)
            m_sessions.clear();

            // Clear active collections
            m_activeCollections.clear();

            // Restore isolation level if requested (store-only, no DB command)
            if (isolationLevel != TransactionIsolationLevel::TRANSACTION_NONE)
            {
                m_transactionIsolation = isolationLevel;
            }

            MONGODB_DEBUG("MongoDBConnection::prepareForPoolReturn(nothrow) - Cleanup complete");
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("OINP1LDHWCQ7",
                std::string("Exception in prepareForPoolReturn: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
