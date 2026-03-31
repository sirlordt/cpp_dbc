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
    // NOTHROW API - insertOne, insertMany, findOne, findById (implementations)
    // ====================================================================

    expected<DocumentInsertResult, DBException> MongoDBCollection::insertOne(
        std::nothrow_t,
        std::shared_ptr<DocumentDBData> document,
        const DocumentWriteOptions &options) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::insertOne(nothrow) - Inserting document into: %s", m_name.c_str());
        MONGODB_STMT_LOCK_OR_RETURN("DK1M2CSS4DUK", "Connection closed");

        auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(document);
        if (!mongoDoc)
        {
            return unexpected<DBException>(DBException(
                "C7D3E2F1A6B5",
                "Document must be a MongoDBDocument",
                system_utils::captureCallStack()));
        }

        bson_t *bson = mongoDoc->getBsonMutable(std::nothrow);
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
                "HFCOF0QROEDU",
                std::string("Insert failed: ") + error.message,
                system_utils::captureCallStack()));
        }

        DocumentInsertResult result;
        result.acknowledged = true;
        result.insertedCount = 1;
        auto idResult = mongoDoc->getId(std::nothrow);
        if (idResult.has_value())
        {
            result.insertedId = idResult.value();
        }
        else
        {
            // Insert already committed — do not convert a post-write id extraction
            // failure into an overall error (caller might retry, causing duplicates).
            MONGODB_DEBUG("MongoDBCollection::insertOne - getId failed after successful insert: %s",
                          idResult.error().what_s().data());
        }

        bson_destroy(&reply);
        return result;
    }

    expected<DocumentInsertResult, DBException> MongoDBCollection::insertOne(
        std::nothrow_t,
        const std::string &jsonDocument,
        const DocumentWriteOptions &options) noexcept
    {
        auto r = MongoDBDocument::create(std::nothrow, jsonDocument);
        if (!r.has_value())
        {
            return unexpected<DBException>(r.error());
        }
        return insertOne(std::nothrow, r.value(), options);
    }

    expected<DocumentInsertResult, DBException> MongoDBCollection::insertMany(
        std::nothrow_t,
        const std::vector<std::shared_ptr<DocumentDBData>> &documents,
        const DocumentWriteOptions &options) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::insertMany(nothrow) - Inserting %zu documents", documents.size());
        MONGODB_STMT_LOCK_OR_RETURN("XX7F1O4E27K2", "Connection closed");

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

            bson_t *bson = mongoDoc->getBsonMutable(std::nothrow);
            bson_iter_t iter;
            if (!bson_iter_init_find(&iter, bson, "_id"))
            {
                bson_oid_t oid;
                bson_oid_init(&oid, nullptr);
                BSON_APPEND_OID(bson, "_id", &oid);
            }
            bsonDocs.push_back(mongoDoc->getBson(std::nothrow));
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
                {
                    auto idResult = mongoDoc->getId(std::nothrow);
                    if (idResult.has_value())
                    {
                        result.insertedIds.push_back(idResult.value());
                    }
                    else
                    {
                        // Batch already committed — do not convert a post-write id extraction
                        // failure into an overall error (caller might retry, causing duplicates).
                        MONGODB_DEBUG("MongoDBCollection::insertMany - getId failed after successful insert: %s",
                                      idResult.error().what_s().data());
                        result.insertedIds.emplace_back();
                    }
                }
            }
        }
        else
        {
            bson_destroy(&reply);
            return unexpected<DBException>(DBException(
                "Y1V7RX08Y2RS",
                std::string("insertMany failed: ") + error.message,
                system_utils::captureCallStack()));
        }

        bson_destroy(&reply);
        return result;
    }


    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBCollection::findOne(
        std::nothrow_t,
        const std::string &filter) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::findOne(nothrow) - Finding in: %s", m_name.c_str());
        MONGODB_STMT_LOCK_OR_RETURN("ETSIWX2HUHVB", "Connection closed");

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
                    "FP7XUYAL6092",
                    "Failed to copy BSON document in findOne (memory allocation failure)",
                    system_utils::captureCallStack()));
            }
            // create() takes ownership of docCopy; on success m_bson holds it.
            // docCopy is guaranteed non-null here (bson_copy returned it above).
            auto docResult = MongoDBDocument::create(std::nothrow, docCopy);
            if (!docResult.has_value())
            {
                return unexpected<DBException>(docResult.error());
            }
            result = docResult.value();
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

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBCollection::findById(
        std::nothrow_t,
        const std::string &id) noexcept
    {
        auto filterResult = buildIdFilter(std::nothrow, id);
        if (!filterResult.has_value())
        {
            return unexpected<DBException>(filterResult.error());
        }
        return findOne(std::nothrow, filterResult.value());
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
