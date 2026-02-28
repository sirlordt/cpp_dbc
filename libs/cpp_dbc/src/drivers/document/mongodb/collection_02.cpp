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
 * @brief MongoDB MongoDBCollection - Part 2 (nothrow API: insertOne, insertMany, findOne, findById)
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
    // THROWING API - insertOne, insertMany, findOne, findById (wrappers)
    // ====================================================================

    #ifdef __cpp_exceptions
    DocumentInsertResult MongoDBCollection::insertOne(
        std::shared_ptr<DocumentDBData> document,
        const DocumentWriteOptions &options)
    {
        auto r = insertOne(std::nothrow, std::move(document), options);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    DocumentInsertResult MongoDBCollection::insertOne(
        const std::string &jsonDocument,
        const DocumentWriteOptions &options)
    {
        auto r = insertOne(std::nothrow, jsonDocument, options);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    DocumentInsertResult MongoDBCollection::insertMany(
        const std::vector<std::shared_ptr<DocumentDBData>> &documents,
        const DocumentWriteOptions &options)
    {
        auto r = insertMany(std::nothrow, documents, options);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    std::shared_ptr<DocumentDBData> MongoDBCollection::findOne(const std::string &filter)
    {
        auto r = findOne(std::nothrow, filter);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    std::shared_ptr<DocumentDBData> MongoDBCollection::findById(const std::string &id)
    {
        auto r = findById(std::nothrow, id);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }
    #endif // __cpp_exceptions

    // ====================================================================
    // NOTHROW API - insertOne, insertMany (real implementations)
    // ====================================================================

    expected<DocumentInsertResult, DBException> MongoDBCollection::insertOne(
        std::nothrow_t,
        std::shared_ptr<DocumentDBData> document,
        const DocumentWriteOptions &options) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::insertOne(nothrow) - Inserting document into: " << m_name);
            MONGODB_LOCK_GUARD(*m_connMutex);

            // Validate connection (inline check - no throwing helper)
            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "A1B2C3D4E5F6",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(document);
            if (!mongoDoc)
            {
                return unexpected<DBException>(DBException(
                    "C7D3E2F1A6B5",
                    "Document must be a MongoDBDocument",
                    system_utils::captureCallStack()));
            }

            bson_t *bson = mongoDoc->getBsonMutable();
            bson_iter_t iter;
            if (!bson_iter_init_find(&iter, bson, "_id"))
            {
                bson_oid_t oid;
                bson_oid_init(&oid, nullptr);
                BSON_APPEND_OID(bson, "_id", &oid);
            }

            // Build options document for MongoDB
            bson_t opts;
            bson_init(&opts);
            if (options.bypassValidation)
            {
                BSON_APPEND_BOOL(&opts, "bypassDocumentValidation", true);
            }

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_insert_one(
                m_collection.get(), bson, &opts, &reply, &error);

            bson_destroy(&opts);

            if (!success)
            {
                bson_destroy(&reply);
                return unexpected<DBException>(DBException(
                    "F7G8H9I0J1K2",
                    std::string("Insert failed: ") + error.message,
                    system_utils::captureCallStack()));
            }

            DocumentInsertResult result;
            result.acknowledged = true;
            result.insertedId = mongoDoc->getId();
            result.insertedCount = 1;

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
                "46F76AEC33F1",
                "Memory allocation failed in insertOne",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "74B74B69CF86",
                std::string("Unexpected error in insertOne: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "A8C0D480F03C",
                "Unknown error in insertOne",
                system_utils::captureCallStack()));
        }
    }

    expected<DocumentInsertResult, DBException> MongoDBCollection::insertOne(
        std::nothrow_t,
        const std::string &jsonDocument,
        const DocumentWriteOptions &options) noexcept
    {
        try
        {
            auto doc = std::make_shared<MongoDBDocument>(jsonDocument);
            return insertOne(std::nothrow, doc, options);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "B7C8D9E0F1A2",
                std::string("Failed to parse JSON document: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "C8D9E0F1A2B3",
                "Unknown error parsing JSON document",
                system_utils::captureCallStack()));
        }
    }

    expected<DocumentInsertResult, DBException> MongoDBCollection::insertMany(
        std::nothrow_t,
        const std::vector<std::shared_ptr<DocumentDBData>> &documents,
        const DocumentWriteOptions &options) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::insertMany(nothrow) - Inserting " << documents.size() << " documents");
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "D9E0F1A2B3C4",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            if (documents.empty())
            {
                DocumentInsertResult result;
                result.acknowledged = true;
                result.insertedCount = 0;
                return result;
            }

            // Suppress -Wignored-attributes warning for bson_t pointer vector
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif
            std::vector<const bson_t *> bsonDocs;
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
            bsonDocs.reserve(documents.size());

            for (const auto &doc : documents)
            {
                auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(doc);
                if (!mongoDoc)
                {
                    return unexpected<DBException>(DBException(
                        "D8E4F3A2B7C6",
                        "All documents must be MongoDBDocument instances",
                        system_utils::captureCallStack()));
                }

                bson_t *bson = mongoDoc->getBsonMutable();
                bson_iter_t iter;
                if (!bson_iter_init_find(&iter, bson, "_id"))
                {
                    bson_oid_t oid;
                    bson_oid_init(&oid, nullptr);
                    BSON_APPEND_OID(bson, "_id", &oid);
                }
                bsonDocs.push_back(mongoDoc->getBson());
            }

            bson_t opts = BSON_INITIALIZER;
            BSON_APPEND_BOOL(&opts, "ordered", options.ordered);
            if (options.bypassValidation)
            {
                BSON_APPEND_BOOL(&opts, "bypassDocumentValidation", true);
            }

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_insert_many(
                m_collection.get(), bsonDocs.data(), bsonDocs.size(), &opts, &reply, &error);

            bson_destroy(&opts);

            DocumentInsertResult result;
            result.acknowledged = success;

            if (success)
            {
                result.insertedCount = documents.size();
                for (const auto &doc : documents)
                {
                    auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(doc);
                    if (mongoDoc)
                        result.insertedIds.push_back(mongoDoc->getId());
                }
            }
            else
            {
                bson_destroy(&reply);
                return unexpected<DBException>(DBException(
                    "E0F1A2B3C4D5",
                    std::string("insertMany failed: ") + error.message,
                    system_utils::captureCallStack()));
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
                "F1A2B3C4D5E6",
                "Memory allocation failed in insertMany",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "A2B3C4D5E6F7",
                std::string("Unexpected error in insertMany: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "B3C4D5E6F7A8",
                "Unknown error in insertMany",
                system_utils::captureCallStack()));
        }
    }

    // ====================================================================
    // NOTHROW API - findOne, findById (real implementations)
    // ====================================================================

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBCollection::findOne(
        std::nothrow_t,
        const std::string &filter) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::findOne(nothrow) - Finding in: " << m_name);
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "C4D5E6F7A8B9",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            auto filterResult = parseFilter(std::nothrow, filter);
            if (!filterResult.has_value())
            {
                return unexpected<DBException>(filterResult.error());
            }
            BsonHandle filterBson = std::move(filterResult.value());
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
                // Use RAII guard to prevent leak if make_shared throws
                BsonHandle guard(docCopy);
                result = std::make_shared<MongoDBDocument>(guard.release());
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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
