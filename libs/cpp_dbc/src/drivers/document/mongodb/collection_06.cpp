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
 * @brief MongoDB MongoDBCollection - Part 6 (nothrow API: createIndex, dropIndex, dropAllIndexes, listIndexes)
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
    // NOTHROW API - createIndex, dropIndex, dropAllIndexes, listIndexes (implementations)
    // ====================================================================

    expected<std::string, DBException> MongoDBCollection::createIndex(
        std::nothrow_t,
        const std::string &keys,
        const std::string &options) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::createIndex(nothrow) - Creating index in: %s", m_name.c_str());
            MONGODB_STMT_LOCK_OR_RETURN("0YQZ6L3A2KIT", "Connection closed");

            auto keysBsonResult = makeBsonHandleFromJson(std::nothrow, keys);
            if (!keysBsonResult.has_value())
            {
                return unexpected<DBException>(keysBsonResult.error());
            }
            BsonHandle keysBson = std::move(keysBsonResult.value());

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

                if (!optsBson)
                {
                    return unexpected<DBException>(DBException(
                        "B5C6D7E8F9A1",
                        std::string("Failed to parse index options JSON: ") + parseError.message,
                        system_utils::captureCallStack()));
                }

                bson_iter_t iter;
                if (bson_iter_init_find(&iter, optsBson, "unique") && BSON_ITER_HOLDS_BOOL(&iter))
                {
                    isUnique = bson_iter_bool(&iter);
                }
                if (bson_iter_init_find(&iter, optsBson, "sparse") && BSON_ITER_HOLDS_BOOL(&iter))
                {
                    isSparse = bson_iter_bool(&iter);
                }
                if (bson_iter_init_find(&iter, optsBson, "name") && BSON_ITER_HOLDS_UTF8(&iter))
                {
                    uint32_t len = 0;
                    const char *name = bson_iter_utf8(&iter, &len);
                    indexName = std::string(name, len);
                }
                bson_destroy(optsBson);
            }

            indexOpts.unique = isUnique;
            indexOpts.sparse = isSparse;
            if (!indexName.empty())
            {
                indexOpts.name = indexName.c_str();
            }

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
                    "7XT99XJD1MTY",
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
                "9LPAK5FMMYYG",
                "Memory allocation failed in createIndex",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "6S4ZDEJIWD46",
                std::string("Unexpected error in createIndex: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return unexpected<DBException>(DBException(
                "2X6F38U6BCGB",
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
            MONGODB_DEBUG("MongoDBCollection::dropIndex(nothrow) - Dropping index: %s", indexName.c_str());
            MONGODB_STMT_LOCK_OR_RETURN("VEL150XDHYR0", "Connection closed");

            bson_error_t error;
            bool success = mongoc_collection_drop_index(m_collection.get(), indexName.c_str(), &error);

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "FJBVW4E7VW6I",
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
                "2RZ9WXVWKBYD",
                std::string("Unexpected error in dropIndex: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return unexpected<DBException>(DBException(
                "530EXDK9GE03",
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
            MONGODB_STMT_LOCK_OR_RETURN("N0T9W636KMQL", "Connection closed");

            bson_t cmd = BSON_INITIALIZER;
            BSON_APPEND_UTF8(&cmd, "dropIndexes", m_name.c_str());
            BSON_APPEND_UTF8(&cmd, "index", "*");

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            MongoDatabaseHandle db(mongoc_client_get_database(mongodb_conn_lock_.nativeClient(), m_databaseName.c_str()));

            bool success = mongoc_database_command_simple(db.get(), &cmd, nullptr, &reply, &error);

            bson_destroy(&cmd);
            bson_destroy(&reply);

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "SOKCW51FVEH5",
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
                "FA4WVLT3XAI7",
                std::string("Unexpected error in dropAllIndexes: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return unexpected<DBException>(DBException(
                "HJSHBTM33109",
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
            MONGODB_STMT_LOCK_OR_RETURN("UUX8YNHN90X5", "Connection closed");

            std::vector<std::string> result;

            MongoCursorHandle cursor(mongoc_collection_find_indexes_with_opts(
                m_collection.get(), nullptr));

            if (!cursor)
            {
                return unexpected<DBException>(DBException(
                    "G1H2I3J4K5L6",
                    "Failed to list indexes",
                    system_utils::captureCallStack()));
            }

            const bson_t *doc = nullptr;
            while (mongoc_cursor_next(cursor.get(), &doc))
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
            if (mongoc_cursor_error(cursor.get(), &error))
            {
                return unexpected<DBException>(DBException(
                    "H2I3J4K5L6M7",
                    std::string("listIndexes error: ") + error.message,
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
                "SEBH3RD25UXD",
                "Memory allocation failed in listIndexes",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "UXFKWGMA9P3P",
                std::string("Unexpected error in listIndexes: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return unexpected<DBException>(DBException(
                "J6QVBK077HHA",
                "Unknown error in listIndexes",
                system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
