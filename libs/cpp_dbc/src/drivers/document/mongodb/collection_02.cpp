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
 * @file collection_02.cpp
 * @brief MongoDB MongoDBCollection - Part 2 (INSERT throwing versions, FIND nothrow versions)
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
    // INSERT OPERATIONS - throwing versions (WRAPPERS)
    // ====================================================================

    DocumentInsertResult MongoDBCollection::insertOne(
        std::shared_ptr<DocumentDBData> document,
        const DocumentWriteOptions &options)
    {
        auto result = insertOne(std::nothrow, document, options);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    DocumentInsertResult MongoDBCollection::insertOne(
        const std::string &jsonDocument,
        const DocumentWriteOptions &options)
    {
        auto result = insertOne(std::nothrow, jsonDocument, options);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    DocumentInsertResult MongoDBCollection::insertMany(
        const std::vector<std::shared_ptr<DocumentDBData>> &documents,
        const DocumentWriteOptions &options)
    {
        auto result = insertMany(std::nothrow, documents, options);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    // ====================================================================
    // FIND OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
    // ====================================================================

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBCollection::findOne(
        std::nothrow_t,
        const std::string &filter) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::findOne(nothrow) - Finding in: " << m_name);
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "C4D5E6F7A8B9",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            BsonHandle filterBson = parseFilter(filter);
            bson_t opts = BSON_INITIALIZER;
            BSON_APPEND_INT64(&opts, "limit", 1);

            mongoc_cursor_t *rawCursor = mongoc_collection_find_with_opts(
                m_collection.get(), filterBson.get(), &opts, nullptr);
            bson_destroy(&opts);

            if (!rawCursor)
            {
                return unexpected<DBException>(DBException(
                    "E9F5A4B3C8D7",
                    "Failed to create cursor for findOne",
                    system_utils::captureCallStack()));
            }

            // Wrap cursor in RAII handle immediately to prevent leaks if exceptions occur
            MongoCursorHandle cursor(rawCursor);

            const bson_t *doc = nullptr;
            std::shared_ptr<DocumentDBData> result;

            if (mongoc_cursor_next(cursor.get(), &doc))
            {
                bson_t *docCopy = bson_copy(doc);
                if (!docCopy)
                {
                    return unexpected<DBException>(DBException(
                        "D5E6F7A8B9C1",
                        "Failed to copy BSON document in findOne (memory allocation failure)",
                        system_utils::captureCallStack()));
                }
                result = std::make_shared<MongoDBDocument>(docCopy);
            }

            bson_error_t error;
            if (mongoc_cursor_error(cursor.get(), &error))
            {
                return unexpected<DBException>(DBException(
                    "F0A6B5C4D9E8",
                    std::string("findOne error: ") + error.message,
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
                "D5E6F7A8B9C0",
                "Memory allocation failed in findOne",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "E6F7A8B9C0D1",
                std::string("Unexpected error in findOne: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "F7A8B9C0D1E2",
                "Unknown error in findOne",
                system_utils::captureCallStack()));
        }
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBCollection::findById(
        std::nothrow_t,
        const std::string &id) noexcept
    {
        try
        {
            // Use BSON construction to prevent JSON injection
            bson_t *filterBson = bson_new();
            if (!filterBson)
            {
                return unexpected<DBException>(DBException(
                    "A8B9C0D1E2F2",
                    "Failed to allocate BSON for filter",
                    system_utils::captureCallStack()));
            }

            if (bson_oid_is_valid(id.c_str(), id.length()))
            {
                bson_oid_t oid;
                bson_oid_init_from_string(&oid, id.c_str());
                BSON_APPEND_OID(filterBson, "_id", &oid);
            }
            else
            {
                BSON_APPEND_UTF8(filterBson, "_id", id.c_str());
            }

            size_t length = 0;
            char *json = bson_as_json(filterBson, &length);
            bson_destroy(filterBson);

            if (!json)
            {
                return unexpected<DBException>(DBException(
                    "A8B9C0D1E2F4",
                    "Failed to convert BSON filter to JSON",
                    system_utils::captureCallStack()));
            }

            std::string filter(json, length);
            bson_free(json);

            return findOne(std::nothrow, filter);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "A8B9C0D1E2F3",
                std::string("Error in findById: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "B9C0D1E2F3A4",
                "Unknown error in findById",
                system_utils::captureCallStack()));
        }
    }

    expected<std::shared_ptr<DocumentDBCursor>, DBException> MongoDBCollection::find(
        std::nothrow_t,
        const std::string &filter) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::find(nothrow) - Finding in: " << m_name);
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "C0D1E2F3A4B5",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            BsonHandle filterBson = parseFilter(filter);
            mongoc_cursor_t *rawCursor = mongoc_collection_find_with_opts(
                m_collection.get(), filterBson.get(), nullptr, nullptr);

            if (!rawCursor)
            {
                return unexpected<DBException>(DBException(
                    "A1B7C6D5E0F9",
                    "Failed to create cursor for find",
                    system_utils::captureCallStack()));
            }

            // Wrap cursor in RAII handle immediately to prevent leaks if make_shared throws
            MongoCursorHandle cursorHandle(rawCursor);

            // Create the MongoDBCursor - it will take ownership via release()
            std::shared_ptr<DocumentDBCursor> result = std::make_shared<MongoDBCursor>(m_client, cursorHandle.release(), m_connection);
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
                "Memory allocation failed in find",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "E2F3A4B5C6D7",
                std::string("Unexpected error in find: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "F3A4B5C6D7E8",
                "Unknown error in find",
                system_utils::captureCallStack()));
        }
    }

    expected<std::shared_ptr<DocumentDBCursor>, DBException> MongoDBCollection::find(
        std::nothrow_t,
        const std::string &filter,
        const std::string &projection) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::find(nothrow) with projection - Finding in: " << m_name);
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "A4B5C6D7E8F9",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            BsonHandle filterBson = parseFilter(filter);
            BsonHandle projBson = parseFilter(projection);

            bson_t opts = BSON_INITIALIZER;
            BSON_APPEND_DOCUMENT(&opts, "projection", projBson.get());

            mongoc_cursor_t *rawCursor = mongoc_collection_find_with_opts(
                m_collection.get(), filterBson.get(), &opts, nullptr);
            bson_destroy(&opts);

            if (!rawCursor)
            {
                return unexpected<DBException>(DBException(
                    "B2C8D7E6F1A0",
                    "Failed to create cursor for find with projection",
                    system_utils::captureCallStack()));
            }

            // Wrap cursor in RAII handle immediately to prevent leaks if make_shared throws
            MongoCursorHandle cursorHandle(rawCursor);

            // Create the MongoDBCursor - it will take ownership via release()
            std::shared_ptr<DocumentDBCursor> result = std::make_shared<MongoDBCursor>(m_client, cursorHandle.release(), m_connection);
            return result;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "B5C6D7E8F9A0",
                "Memory allocation failed in find",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "C6D7E8F9A0B1",
                std::string("Unexpected error in find: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "D7E8F9A0B1C2",
                "Unknown error in find",
                system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
