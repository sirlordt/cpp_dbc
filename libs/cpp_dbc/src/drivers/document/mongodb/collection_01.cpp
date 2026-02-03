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
 * @brief MongoDB MongoDBCollection - Part 1 (private helpers, constructor, count, insertOne nothrow versions)
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
    // MongoDBCollection Implementation - Private Helpers
    // ============================================================================

    void MongoDBCollection::validateConnection() const
    {
        if (m_client.expired())
        {
            throw DBException("F4A0B9C8D3E2", "MongoDB connection has been closed", system_utils::captureCallStack());
        }
    }

    mongoc_client_t *MongoDBCollection::getClient() const
    {
        auto client = m_client.lock();
        if (!client)
        {
            throw DBException("A5B1C0D9E4F3", "MongoDB connection has been closed", system_utils::captureCallStack());
        }
        return client.get();
    }

    BsonHandle MongoDBCollection::parseFilter(const std::string &filter) const
    {
        if (filter.empty())
            return makeBsonHandle();
        return makeBsonHandleFromJson(filter);
    }

    void MongoDBCollection::throwMongoError(const bson_error_t &error, const std::string &operation) const
    {
        throw DBException("B6C2D1E0F5A4", operation + " failed: " + std::string(error.message), system_utils::captureCallStack());
    }

    // ============================================================================
    // MongoDBCollection Implementation - Constructor, Move
    // ============================================================================

#if DB_DRIVER_THREAD_SAFE
    MongoDBCollection::MongoDBCollection(std::weak_ptr<mongoc_client_t> client,
                                         mongoc_collection_t *collection,
                                         const std::string &name,
                                         const std::string &databaseName,
                                         std::weak_ptr<MongoDBConnection> connection,
                                         SharedConnMutex connMutex)
        : m_client(std::move(client)),
          m_connection(std::move(connection)),
          m_collection(collection),
          m_name(name),
          m_databaseName(databaseName),
          m_connMutex(std::move(connMutex))
    {
        MONGODB_DEBUG("MongoDBCollection::constructor - Creating collection: " << name << " in database: " << databaseName);
        if (!m_collection)
        {
            throw DBException("E3F9A8B7C2D1", "Cannot create collection from null pointer", system_utils::captureCallStack());
        }
        MONGODB_DEBUG("MongoDBCollection::constructor - Done");
    }
#else
    MongoDBCollection::MongoDBCollection(std::weak_ptr<mongoc_client_t> client,
                                         mongoc_collection_t *collection,
                                         const std::string &name,
                                         const std::string &databaseName,
                                         std::weak_ptr<MongoDBConnection> connection)
        : m_client(std::move(client)),
          m_connection(std::move(connection)),
          m_collection(collection),
          m_name(name),
          m_databaseName(databaseName)
    {
        MONGODB_DEBUG("MongoDBCollection::constructor - Creating collection: " << name << " in database: " << databaseName);
        if (!m_collection)
        {
            throw DBException("E3F9A8B7C2D1", "Cannot create collection from null pointer", system_utils::captureCallStack());
        }
        MONGODB_DEBUG("MongoDBCollection::constructor - Done");
    }
#endif

    MongoDBCollection::MongoDBCollection(MongoDBCollection &&other) noexcept
        : m_client(std::move(other.m_client)),
          m_connection(std::move(other.m_connection)),
          m_collection(std::move(other.m_collection)),
          m_name(std::move(other.m_name)),
          m_databaseName(std::move(other.m_databaseName))
#if DB_DRIVER_THREAD_SAFE
          , m_connMutex(std::move(other.m_connMutex))
#endif
    {}

    MongoDBCollection &MongoDBCollection::operator=(MongoDBCollection &&other) noexcept
    {
        if (this != &other)
        {
            m_client = std::move(other.m_client);
            m_connection = std::move(other.m_connection);
            m_collection = std::move(other.m_collection);
            m_name = std::move(other.m_name);
            m_databaseName = std::move(other.m_databaseName);
#if DB_DRIVER_THREAD_SAFE
            m_connMutex = std::move(other.m_connMutex);
#endif
        }
        return *this;
    }

    // ============================================================================
    // MongoDBCollection Implementation - DocumentDBCollection Interface (Name, Count)
    // ============================================================================

    std::string MongoDBCollection::getName() const { return m_name; }
    std::string MongoDBCollection::getNamespace() const { return m_databaseName + "." + m_name; }

    uint64_t MongoDBCollection::estimatedDocumentCount()
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();
        bson_error_t error;
        int64_t count = mongoc_collection_estimated_document_count(
            m_collection.get(), nullptr, nullptr, nullptr, &error);
        if (count < 0)
            throwMongoError(error, "estimatedDocumentCount");
        return static_cast<uint64_t>(count);
    }

    uint64_t MongoDBCollection::countDocuments(const std::string &filter)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();
        BsonHandle filterBson = parseFilter(filter);
        bson_error_t error;
        int64_t count = mongoc_collection_count_documents(
            m_collection.get(), filterBson.get(), nullptr, nullptr, nullptr, &error);
        if (count < 0)
            throwMongoError(error, "countDocuments");
        return static_cast<uint64_t>(count);
    }

    // ====================================================================
    // INSERT OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
