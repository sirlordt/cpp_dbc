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
 * @file collection_01.cpp
 * @brief MongoDB MongoDBCollection - Part 1 (private nothrow helpers, constructor, throwing API wrappers, getName/getNamespace/count nothrow)
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
    // MongoDBCollection Implementation - Private Nothrow Helpers
    // ============================================================================

    expected<BsonHandle, DBException> MongoDBCollection::parseFilter(std::nothrow_t, const std::string &filter) const noexcept
    {
        if (filter.empty())
        {
            return makeBsonHandle();
        }
        return makeBsonHandleFromJson(std::nothrow, filter);
    }

    expected<void, DBException> MongoDBCollection::throwMongoError(std::nothrow_t, const bson_error_t &error, const std::string &operation) const noexcept
    {
        return unexpected<DBException>(DBException(
            "B6C2D1E0F5A4",
            operation + " failed: " + std::string(error.message),
            system_utils::captureCallStack()));
    }

    expected<std::string, DBException> MongoDBCollection::buildIdFilter(std::nothrow_t, const std::string &id) const noexcept
    {
        // Use BSON construction to prevent JSON injection
        bson_t *filterBson = bson_new();
        if (!filterBson)
        {
            return unexpected<DBException>(DBException(
                "G37K0HMC7KPG",
                "Failed to allocate BSON for _id filter",
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
                "YLRSZGOWNANI",
                "Failed to convert BSON _id filter to JSON",
                system_utils::captureCallStack()));
        }

        std::string filter(json, length);
        bson_free(json);

        return filter;
    }

    // ============================================================================
    // MongoDBCollection Implementation - Nothrow Constructor
    // Public for std::make_shared access, but effectively private via PrivateCtorTag.
    // ============================================================================

    MongoDBCollection::MongoDBCollection(MongoDBCollection::PrivateCtorTag,
                                         std::nothrow_t,
                                         mongoc_collection_t *collection,
                                         const std::string &name,
                                         const std::string &databaseName,
                                         std::weak_ptr<MongoDBConnection> connection) noexcept
        : m_connection(std::move(connection)),
          m_collection(collection),
          m_name(name),
          m_databaseName(databaseName)
    {
        MONGODB_DEBUG("MongoDBCollection::constructor(nothrow) - Creating collection: %s in database: %s", name.c_str(), databaseName.c_str());
        if (!m_collection)
        {
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("Q1U86NUBRYWR", "Cannot create collection from null pointer", system_utils::captureCallStack());
            return;
        }
        MONGODB_DEBUG("MongoDBCollection::constructor(nothrow) - Done");
    }

    // ============================================================================
    // MongoDBCollection Implementation - THROWING API (all wrappers)
    // ============================================================================

#ifdef __cpp_exceptions

    std::string MongoDBCollection::getName() const
    {
        auto r = getName(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::string MongoDBCollection::getNamespace() const
    {
        auto r = getNamespace(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    uint64_t MongoDBCollection::estimatedDocumentCount()
    {
        auto r = estimatedDocumentCount(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    uint64_t MongoDBCollection::countDocuments(const std::string &filter)
    {
        auto r = countDocuments(std::nothrow, filter);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    DocumentInsertResult MongoDBCollection::insertOne(
        std::shared_ptr<DocumentDBData> document,
        const DocumentWriteOptions &options)
    {
        auto r = insertOne(std::nothrow, std::move(document), options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    DocumentInsertResult MongoDBCollection::insertOne(
        const std::string &jsonDocument,
        const DocumentWriteOptions &options)
    {
        auto r = insertOne(std::nothrow, jsonDocument, options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    DocumentInsertResult MongoDBCollection::insertMany(
        const std::vector<std::shared_ptr<DocumentDBData>> &documents,
        const DocumentWriteOptions &options)
    {
        auto r = insertMany(std::nothrow, documents, options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<DocumentDBData> MongoDBCollection::findOne(const std::string &filter)
    {
        auto r = findOne(std::nothrow, filter);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<DocumentDBData> MongoDBCollection::findById(const std::string &id)
    {
        auto r = findById(std::nothrow, id);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<DocumentDBCursor> MongoDBCollection::find(const std::string &filter)
    {
        auto r = find(std::nothrow, filter);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<DocumentDBCursor> MongoDBCollection::find(
        const std::string &filter,
        const std::string &projection)
    {
        auto r = find(std::nothrow, filter, projection);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    DocumentUpdateResult MongoDBCollection::updateOne(
        const std::string &filter,
        const std::string &update,
        const DocumentUpdateOptions &options)
    {
        auto r = updateOne(std::nothrow, filter, update, options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    DocumentUpdateResult MongoDBCollection::updateMany(
        const std::string &filter,
        const std::string &update,
        const DocumentUpdateOptions &options)
    {
        auto r = updateMany(std::nothrow, filter, update, options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    DocumentUpdateResult MongoDBCollection::replaceOne(
        const std::string &filter,
        std::shared_ptr<DocumentDBData> replacement,
        const DocumentUpdateOptions &options)
    {
        auto r = replaceOne(std::nothrow, filter, std::move(replacement), options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    DocumentDeleteResult MongoDBCollection::deleteOne(const std::string &filter)
    {
        auto r = deleteOne(std::nothrow, filter);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    DocumentDeleteResult MongoDBCollection::deleteMany(const std::string &filter)
    {
        auto r = deleteMany(std::nothrow, filter);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    DocumentDeleteResult MongoDBCollection::deleteById(const std::string &id)
    {
        auto r = deleteById(std::nothrow, id);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::string MongoDBCollection::createIndex(
        const std::string &keys,
        const std::string &options)
    {
        auto r = createIndex(std::nothrow, keys, options);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    void MongoDBCollection::dropIndex(const std::string &indexName)
    {
        auto r = dropIndex(std::nothrow, indexName);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MongoDBCollection::dropAllIndexes()
    {
        auto r = dropAllIndexes(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    std::vector<std::string> MongoDBCollection::listIndexes()
    {
        auto r = listIndexes(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    void MongoDBCollection::drop()
    {
        auto r = drop(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void MongoDBCollection::rename(const std::string &newName, bool dropTarget)
    {
        auto r = rename(std::nothrow, newName, dropTarget);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    std::shared_ptr<DocumentDBCursor> MongoDBCollection::aggregate(const std::string &pipeline)
    {
        auto r = aggregate(std::nothrow, pipeline);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::vector<std::string> MongoDBCollection::distinct(
        const std::string &fieldPath,
        const std::string &filter)
    {
        auto r = distinct(std::nothrow, fieldPath, filter);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

#endif // __cpp_exceptions

    // ============================================================================
    // MongoDBCollection Implementation - NOTHROW API: getName, getNamespace, count
    // ============================================================================

    expected<std::string, DBException> MongoDBCollection::getName(std::nothrow_t) const noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("BAMRP5GEYR9Z", "Collection closed");
        return m_name;
    }

    expected<std::string, DBException> MongoDBCollection::getNamespace(std::nothrow_t) const noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("IU165L6GVQ7B", "Collection closed");
        return m_databaseName + "." + m_name;
    }

    expected<uint64_t, DBException> MongoDBCollection::estimatedDocumentCount(std::nothrow_t) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("BHHU315FL6KV", "Collection closed");
        bson_error_t error;
        int64_t count = mongoc_collection_estimated_document_count(
            m_collection.get(), nullptr, nullptr, nullptr, &error);
        if (count < 0)
        {
            auto errResult = throwMongoError(std::nothrow, error, "estimatedDocumentCount");
            return unexpected<DBException>(errResult.error());
        }
        return static_cast<uint64_t>(count);
    }

    expected<uint64_t, DBException> MongoDBCollection::countDocuments(std::nothrow_t, const std::string &filter) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("L9K2CI3G8NTP", "Collection closed");
        auto filterResult = parseFilter(std::nothrow, filter);
        if (!filterResult.has_value())
        {
            return unexpected<DBException>(filterResult.error());
        }
        bson_error_t error;
        int64_t count = mongoc_collection_count_documents(
            m_collection.get(), filterResult.value().get(), nullptr, nullptr, nullptr, &error);
        if (count < 0)
        {
            auto errResult = throwMongoError(std::nothrow, error, "countDocuments");
            return unexpected<DBException>(errResult.error());
        }
        return static_cast<uint64_t>(count);
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
