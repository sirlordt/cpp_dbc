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
 * @file collection_06.cpp
 * @brief MongoDB MongoDBCollection - Part 6 (COLLECTION nothrow versions: drop, rename, aggregate, distinct)
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
    // COLLECTION OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
    // ====================================================================

    expected<void, DBException> MongoDBCollection::drop(
        std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::drop(nothrow)");
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "A2B3C4D5E6F7",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            bson_error_t error;
            bool success = mongoc_collection_drop(m_collection.get(), &error);

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "B3C4D5E6F7A8",
                    std::string("drop failed: ") + error.message,
                    system_utils::captureCallStack()));
            }

            return {};
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "C4D5E6F7A8B9",
                std::string("Unexpected error in drop: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "D5E6F7A8B9C0",
                "Unknown error in drop",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBCollection::rename(
        std::nothrow_t,
        const std::string &newName,
        bool dropTarget) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::rename(nothrow) to: " << newName);
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "E6F7A8B9C0D1",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            bson_error_t error;
            bool success = mongoc_collection_rename(
                m_collection.get(), m_databaseName.c_str(), newName.c_str(), dropTarget, &error);

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "F7A8B9C0D1E2",
                    std::string("rename failed: ") + error.message,
                    system_utils::captureCallStack()));
            }

            m_name = newName;
            return {};
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "A8B9C0D1E2F3",
                std::string("Unexpected error in rename: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "B9C0D1E2F3A4",
                "Unknown error in rename",
                system_utils::captureCallStack()));
        }
    }

    expected<std::shared_ptr<DocumentDBCursor>, DBException> MongoDBCollection::aggregate(
        std::nothrow_t,
        const std::string &pipeline) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::aggregate(nothrow)");
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "C0D1E2F3A4B5",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            BsonHandle pipelineBson = makeBsonHandleFromJson(pipeline);

            mongoc_cursor_t *cursor = mongoc_collection_aggregate(
                m_collection.get(), MONGOC_QUERY_NONE, pipelineBson.get(), nullptr, nullptr);

            if (!cursor)
            {
                return unexpected<DBException>(DBException(
                    "I3J4K5L6M7N8",
                    "Failed to create cursor for aggregate",
                    system_utils::captureCallStack()));
            }

            std::shared_ptr<DocumentDBCursor> result = std::make_shared<MongoDBCursor>(m_client, cursor, m_connection
#if DB_DRIVER_THREAD_SAFE
                , m_connMutex
#endif
            );
            return result;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "D1E2F3A4B5C6",
                "Memory allocation failed in aggregate",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "E2F3A4B5C6D7",
                std::string("Unexpected error in aggregate: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "F3A4B5C6D7E8",
                "Unknown error in aggregate",
                system_utils::captureCallStack()));
        }
    }

    expected<std::vector<std::string>, DBException> MongoDBCollection::distinct(
        std::nothrow_t,
        const std::string &fieldPath,
        const std::string &filter) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::distinct(nothrow)");
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "A4B5C6D7E8F9",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            std::vector<std::string> result;

            bson_t cmd = BSON_INITIALIZER;
            BSON_APPEND_UTF8(&cmd, "distinct", m_name.c_str());
            BSON_APPEND_UTF8(&cmd, "key", fieldPath.c_str());

            if (!filter.empty())
            {
                BsonHandle filterBson = parseFilter(filter);
                BSON_APPEND_DOCUMENT(&cmd, "query", filterBson.get());
            }

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            auto client = getClient();
            MongoDatabaseHandle db(mongoc_client_get_database(client, m_databaseName.c_str()));

            bool success = mongoc_database_command_simple(db.get(), &cmd, nullptr, &reply, &error);

            bson_destroy(&cmd);

            if (!success)
            {
                bson_destroy(&reply);
                return unexpected<DBException>(DBException(
                    "B5C6D7E8F9A0",
                    std::string("distinct failed: ") + error.message,
                    system_utils::captureCallStack()));
            }

            // Extract values from reply
            bson_iter_t iter;
            if (bson_iter_init_find(&iter, &reply, "values") && BSON_ITER_HOLDS_ARRAY(&iter))
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

            bson_destroy(&reply);
            return result;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "C6D7E8F9A0B1",
                "Memory allocation failed in distinct",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "D7E8F9A0B1C2",
                std::string("Unexpected error in distinct: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "E8F9A0B1C2D3",
                "Unknown error in distinct",
                system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
