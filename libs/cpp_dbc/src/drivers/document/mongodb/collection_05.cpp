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
 * @file collection_05.cpp
 * @brief MongoDB MongoDBCollection - Part 5 (DELETE throwing versions, INDEX nothrow versions)
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
    // DELETE OPERATIONS - throwing versions (WRAPPERS)
    // ====================================================================

    DocumentDeleteResult MongoDBCollection::deleteOne(const std::string &filter)
    {
        auto result = deleteOne(std::nothrow, filter);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    DocumentDeleteResult MongoDBCollection::deleteMany(const std::string &filter)
    {
        auto result = deleteMany(std::nothrow, filter);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    DocumentDeleteResult MongoDBCollection::deleteById(const std::string &id)
    {
        auto result = deleteById(std::nothrow, id);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    // ====================================================================
    // INDEX OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
    // ====================================================================

    expected<std::string, DBException> MongoDBCollection::createIndex(
        std::nothrow_t,
        const std::string &keys,
        const std::string &options) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::createIndex(nothrow) - Creating index in: " << m_name);
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "B5C6D7E8F9A0",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            BsonHandle keysBson = makeBsonHandleFromJson(keys);

            mongoc_index_opt_t indexOpts;
            mongoc_index_opt_init(&indexOpts);

            bool isUnique = false;
            bool isSparse = false;
            std::string indexName;

            if (!options.empty())
            {
                bson_error_t parseError;
                bson_t *optsBson = bson_new_from_json(
                    reinterpret_cast<const uint8_t *>(options.c_str()),
                    static_cast<ssize_t>(options.length()),
                    &parseError);

                if (optsBson)
                {
                    bson_iter_t iter;
                    if (bson_iter_init_find(&iter, optsBson, "unique") && BSON_ITER_HOLDS_BOOL(&iter))
                        isUnique = bson_iter_bool(&iter);
                    if (bson_iter_init_find(&iter, optsBson, "sparse") && BSON_ITER_HOLDS_BOOL(&iter))
                        isSparse = bson_iter_bool(&iter);
                    if (bson_iter_init_find(&iter, optsBson, "name") && BSON_ITER_HOLDS_UTF8(&iter))
                    {
                        uint32_t len = 0;
                        const char *name = bson_iter_utf8(&iter, &len);
                        indexName = std::string(name, len);
                    }
                    bson_destroy(optsBson);
                }
            }

            indexOpts.unique = isUnique;
            indexOpts.sparse = isSparse;
            if (!indexName.empty())
                indexOpts.name = indexName.c_str();

            char *generatedName = mongoc_collection_keys_to_index_string(keysBson.get());
            std::string result;
            if (indexName.empty())
            {
                result = generatedName ? generatedName : "";
            }
            else
            {
                result = indexName;
            }
            bson_free(generatedName);

            bson_error_t error;

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
            bool success = mongoc_collection_create_index(
                m_collection.get(), keysBson.get(), &indexOpts, &error);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "C6D7E8F9A0B1",
                    std::string("createIndex failed: ") + error.message,
                    system_utils::captureCallStack()));
            }

            return result;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "D7E8F9A0B1C2",
                "Memory allocation failed in createIndex",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "E8F9A0B1C2D3",
                std::string("Unexpected error in createIndex: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "F9A0B1C2D3E4",
                "Unknown error in createIndex",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBCollection::dropIndex(
        std::nothrow_t,
        const std::string &indexName) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::dropIndex(nothrow) - Dropping index: " << indexName);
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "A0B1C2D3E4F5",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            bson_error_t error;
            bool success = mongoc_collection_drop_index(m_collection.get(), indexName.c_str(), &error);

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "B1C2D3E4F5A6",
                    std::string("dropIndex failed: ") + error.message,
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
                "C2D3E4F5A6B7",
                std::string("Unexpected error in dropIndex: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "D3E4F5A6B7C8",
                "Unknown error in dropIndex",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBCollection::dropAllIndexes(
        std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::dropAllIndexes(nothrow)");
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "E4F5A6B7C8D9",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            bson_t cmd = BSON_INITIALIZER;
            BSON_APPEND_UTF8(&cmd, "dropIndexes", m_name.c_str());
            BSON_APPEND_UTF8(&cmd, "index", "*");

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            auto client = getClient();
            MongoDatabaseHandle db(mongoc_client_get_database(client, m_databaseName.c_str()));

            bool success = mongoc_database_command_simple(db.get(), &cmd, nullptr, &reply, &error);

            bson_destroy(&cmd);
            bson_destroy(&reply);

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "F5A6B7C8D9E0",
                    std::string("dropAllIndexes failed: ") + error.message,
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
                "A6B7C8D9E0F1",
                std::string("Unexpected error in dropAllIndexes: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "B7C8D9E0F1A2",
                "Unknown error in dropAllIndexes",
                system_utils::captureCallStack()));
        }
    }

    expected<std::vector<std::string>, DBException> MongoDBCollection::listIndexes(
        std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::listIndexes(nothrow)");
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "C8D9E0F1A2B3",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            std::vector<std::string> result;

            mongoc_cursor_t *cursor = mongoc_collection_find_indexes_with_opts(
                m_collection.get(), nullptr);

            if (!cursor)
            {
                return unexpected<DBException>(DBException(
                    "G1H2I3J4K5L6",
                    "Failed to list indexes",
                    system_utils::captureCallStack()));
            }

            const bson_t *doc = nullptr;
            while (mongoc_cursor_next(cursor, &doc))
            {
                size_t length = 0;
                char *json = bson_as_relaxed_extended_json(doc, &length);
                if (json)
                {
                    result.emplace_back(json, length);
                    bson_free(json);
                }
            }

            bson_error_t error;
            if (mongoc_cursor_error(cursor, &error))
            {
                mongoc_cursor_destroy(cursor);
                return unexpected<DBException>(DBException(
                    "H2I3J4K5L6M7",
                    std::string("listIndexes error: ") + error.message,
                    system_utils::captureCallStack()));
            }

            mongoc_cursor_destroy(cursor);
            return result;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "D9E0F1A2B3C4",
                "Memory allocation failed in listIndexes",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "E0F1A2B3C4D5",
                std::string("Unexpected error in listIndexes: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "F1A2B3C4D5E6",
                "Unknown error in listIndexes",
                system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
