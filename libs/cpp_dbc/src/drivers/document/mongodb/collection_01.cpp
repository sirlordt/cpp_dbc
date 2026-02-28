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
 * @brief MongoDB MongoDBCollection - Part 1 (private nothrow helpers, constructor, getName/getNamespace, count)
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

    expected<void, DBException> MongoDBCollection::validateConnection(std::nothrow_t) const noexcept
    {
        if (m_client.expired())
        {
            return unexpected<DBException>(DBException(
                "F4A0B9C8D3E2",
                "MongoDB connection has been closed",
                system_utils::captureCallStack()));
        }
        return {};
    }

    expected<mongoc_client_t *, DBException> MongoDBCollection::getClient(std::nothrow_t) const noexcept
    {
        auto client = m_client.lock();
        if (!client)
        {
            return unexpected<DBException>(DBException(
                "A5B1C0D9E4F3",
                "MongoDB connection has been closed",
                system_utils::captureCallStack()));
        }
        return client.get();
    }

    expected<BsonHandle, DBException> MongoDBCollection::parseFilter(std::nothrow_t, const std::string &filter) const noexcept
    {
        try
        {
            if (filter.empty())
            {
                return makeBsonHandle();
            }
            return makeBsonHandleFromJson(filter);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "B2C3D4E5F6A1",
                std::string("Failed to parse filter: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "C3D4E5F6A1B2",
                "Unknown error parsing filter",
                system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBCollection::throwMongoError(std::nothrow_t, const bson_error_t &error, const std::string &operation) const noexcept
    {
        return unexpected<DBException>(DBException(
            "B6C2D1E0F5A4",
            operation + " failed: " + std::string(error.message),
            system_utils::captureCallStack()));
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

    #ifdef __cpp_exceptions
    std::string MongoDBCollection::getName() const { return m_name; }
    std::string MongoDBCollection::getNamespace() const { return m_databaseName + "." + m_name; }

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

    bool MongoDBCollection::isConnectionValid() const
    {
        return !m_client.expired();
    }
    #endif // __cpp_exceptions

    expected<std::string, DBException> MongoDBCollection::getName(std::nothrow_t) const noexcept
    {
        return m_name;
    }

    expected<std::string, DBException> MongoDBCollection::getNamespace(std::nothrow_t) const noexcept
    {
        return m_databaseName + "." + m_name;
    }

    expected<uint64_t, DBException> MongoDBCollection::estimatedDocumentCount(std::nothrow_t) noexcept
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        auto connResult = validateConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return unexpected<DBException>(connResult.error());
        }
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
        MONGODB_LOCK_GUARD(*m_connMutex);
        auto connResult = validateConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return unexpected<DBException>(connResult.error());
        }
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
