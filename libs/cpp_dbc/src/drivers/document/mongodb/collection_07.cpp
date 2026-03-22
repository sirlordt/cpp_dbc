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
 * @file collection_07.cpp
 * @brief MongoDB MongoDBCollection - Part 7 (nothrow API: drop, rename, aggregate, distinct, isConnectionValid)
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
    // NOTHROW API - drop, rename, aggregate, distinct, isConnectionValid (implementations)
    // ====================================================================

    expected<void, DBException> MongoDBCollection::drop(
        std::nothrow_t) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::drop(nothrow)");
        MONGODB_STMT_LOCK_OR_RETURN("60KC02UP28TD", "Connection closed");

        bson_error_t error;
        bool success = mongoc_collection_drop(m_collection.get(), &error);

        if (!success)
        {
            return unexpected<DBException>(DBException(
                "WKWQ8Y8GK4MB",
                std::string("drop failed: ") + error.message,
                system_utils::captureCallStack()));
        }

        return {};
    }

    expected<void, DBException> MongoDBCollection::rename(
        std::nothrow_t,
        const std::string &newName,
        bool dropTarget) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::rename(nothrow) to: %s", newName.c_str());
        MONGODB_STMT_LOCK_OR_RETURN("YAZPQV1NVX5Q", "Connection closed");

        bson_error_t error;
        bool success = mongoc_collection_rename(
            m_collection.get(), m_databaseName.c_str(), newName.c_str(), dropTarget, &error);

        if (!success)
        {
            return unexpected<DBException>(DBException(
                "X0DFPPGZHISW",
                std::string("rename failed: ") + error.message,
                system_utils::captureCallStack()));
        }

        m_name = newName;
        return {};
    }

    expected<std::shared_ptr<DocumentDBCursor>, DBException> MongoDBCollection::aggregate(
        std::nothrow_t,
        const std::string &pipeline) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::aggregate(nothrow)");
        MONGODB_STMT_LOCK_OR_RETURN("CMF7381Q36OF", "Connection closed");

        auto pipelineBsonResult = makeBsonHandleFromJson(std::nothrow, pipeline);
        if (!pipelineBsonResult.has_value())
        {
            return unexpected<DBException>(pipelineBsonResult.error());
        }
        BsonHandle pipelineBson = std::move(pipelineBsonResult.value());

        MongoCursorHandle cursorGuard(mongoc_collection_aggregate(
            m_collection.get(), MONGOC_QUERY_NONE, pipelineBson.get(), nullptr, nullptr));

        if (!cursorGuard)
        {
            return unexpected<DBException>(DBException(
                "0086MYZA084B",
                "Failed to create cursor for aggregate",
                system_utils::captureCallStack()));
        }

        auto cursorResult = MongoDBCursor::create(std::nothrow, cursorGuard.release(), m_connection);
        if (!cursorResult.has_value())
        {
            return unexpected<DBException>(cursorResult.error());
        }
        auto mongoCursor = cursorResult.value();

        // Register cursor with connection for cleanup tracking
        if (auto conn = m_connection.lock())
        {
            conn->registerCursor(std::nothrow, mongoCursor);
        }

        return std::shared_ptr<DocumentDBCursor>(mongoCursor);
    }

    expected<std::vector<std::string>, DBException> MongoDBCollection::distinct(
        std::nothrow_t,
        const std::string &fieldPath,
        const std::string &filter) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::distinct(nothrow)");
        MONGODB_STMT_LOCK_OR_RETURN("VNI5DI2C3HHP", "Connection closed");

        std::vector<std::string> result;

        bson_t *rawCmd = bson_new();
        if (!rawCmd)
        {
            return cpp_dbc::unexpected(DBException(
                "SACX7PWFEZBY",
                "Failed to allocate BSON for distinct command",
                system_utils::captureCallStack()));
        }
        BsonHandle cmd(rawCmd);
        BSON_APPEND_UTF8(cmd.get(), "distinct", m_name.c_str());
        BSON_APPEND_UTF8(cmd.get(), "key", fieldPath.c_str());

        if (!filter.empty())
        {
            auto filterResult = parseFilter(std::nothrow, filter);
            if (!filterResult.has_value())
            {
                return unexpected<DBException>(filterResult.error());
            }
            BsonHandle filterBson = std::move(filterResult.value());
            BSON_APPEND_DOCUMENT(cmd.get(), "query", filterBson.get());
        }

        bson_error_t error;
        bson_t *rawReply = bson_new();
        if (!rawReply)
        {
            return cpp_dbc::unexpected(DBException(
                "IWJNJOB42L3D",
                "Failed to allocate BSON for distinct reply",
                system_utils::captureCallStack()));
        }
        BsonHandle reply(rawReply);

        auto *rawDb = mongoc_client_get_database(mongodb_conn_lock_.nativeClient(), m_databaseName.c_str());
        if (!rawDb)
        {
            return cpp_dbc::unexpected(DBException(
                "2PWDO40FNS1F",
                "Failed to acquire database handle for distinct",
                system_utils::captureCallStack()));
        }
        MongoDatabaseHandle db(rawDb);

        bool success = mongoc_database_command_simple(db.get(), cmd.get(), nullptr, reply.get(), &error);

        if (!success)
        {
            return unexpected<DBException>(DBException(
                "2U5HMTIIHN30",
                std::string("distinct failed: ") + error.message,
                system_utils::captureCallStack()));
        }

        // Extract values from reply
        bson_iter_t iter;
        if (bson_iter_init_find(&iter, reply.get(), "values") && BSON_ITER_HOLDS_ARRAY(&iter))
        {
            bson_iter_t arrayIter;
            const uint8_t *data = nullptr;
            uint32_t length = 0;
            bson_iter_array(&iter, &length, &data);

            bson_t arrayBson;
            if (bson_init_static(&arrayBson, data, length) && bson_iter_init(&arrayIter, &arrayBson))
            {
                while (bson_iter_next(&arrayIter))
                {
                    if (BSON_ITER_HOLDS_UTF8(&arrayIter))
                    {
                        uint32_t strLen = 0;
                        const char *str = bson_iter_utf8(&arrayIter, &strLen);
                        result.emplace_back(str, strLen);
                    }
                    else
                    {
                        // Convert other types to JSON string
                        bson_t tempDoc = BSON_INITIALIZER;
                        bson_append_iter(&tempDoc, "v", 1, &arrayIter);
                        size_t jsonLen = 0;
                        char *json = bson_as_relaxed_extended_json(&tempDoc, &jsonLen);
                        if (json)
                        {
                            result.emplace_back(json, jsonLen);
                            bson_free(json);
                        }
                        bson_destroy(&tempDoc);
                    }
                }
            }
        }

        return result;
    }

    bool MongoDBCollection::isConnectionValid(std::nothrow_t) const noexcept
    {
        return !m_connection.expired();
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
