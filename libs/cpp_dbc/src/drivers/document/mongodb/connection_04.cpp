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
 * @file connection_04.cpp
 * @brief MongoDB MongoDBConnection - Part 4 (nothrow API: getDatabaseName, listDatabases, databaseExists,
 *        useDatabase, dropDatabase, getCollection, listCollections, collectionExists, createCollection, dropCollection)
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
    // NOTHROW API - getDatabaseName, listDatabases, databaseExists, useDatabase, dropDatabase (real implementations)
    // ====================================================================

    expected<std::string, DBException> MongoDBConnection::getDatabaseName(std::nothrow_t) const noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);
            return m_databaseName;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SXWDJ4BE7HDN",
                                                   std::string("Exception in getDatabaseName: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("ZD984T94Z3QJ",
                                                   "Unknown exception in getDatabaseName",
                                                   system_utils::captureCallStack()));
        }
    }

    expected<std::vector<std::string>, DBException> MongoDBConnection::listDatabases(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load(std::memory_order_acquire))
            {
                return unexpected<DBException>(DBException(
                    "C54C7EECE4D6",
                    "Connection has been closed"));
            }

            bson_error_t error;
            char **names = mongoc_client_get_database_names_with_opts(m_client.get(), nullptr, &error);

            if (!names)
            {
                return unexpected<DBException>(DBException(
                    "EED4BB95EA04",
                    std::string("Failed to list databases: ") + error.message));
            }

            std::vector<std::string> databases;
            for (size_t i = 0; names[i] != nullptr; ++i)
            {
                databases.emplace_back(names[i]);
            }

            bson_strfreev(names);
            return databases;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "OWXNS6VQP61O",
                "Memory allocation failed in listDatabases"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "3D10AE1E27C2",
                std::string("Unexpected error in listDatabases: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "EGOK917DH41Q",
                "Unknown error in listDatabases"));
        }
    }

    expected<bool, DBException> MongoDBConnection::databaseExists(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        try
        {
            auto dbsResult = listDatabases(std::nothrow);
            if (!dbsResult.has_value())
            {
                return cpp_dbc::unexpected(dbsResult.error());
            }
            const auto &databases = *dbsResult;
            return std::ranges::find(databases, databaseName) != databases.end();
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("CLA4YAJP87LT",
                                                   std::string("Exception in databaseExists: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("M38AKVVOH6MQ",
                                                   "Unknown exception in databaseExists",
                                                   system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBConnection::useDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);
            {
                auto r = validateConnection(std::nothrow);
                if (!r.has_value())
                {
                    return cpp_dbc::unexpected(r.error());
                }
            }
            m_databaseName = databaseName;
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("W7OJJY9U0NHH",
                                                   std::string("Exception in useDatabase: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("AK6049UQC43W",
                                                   "Unknown exception in useDatabase",
                                                   system_utils::captureCallStack()));
        }
    }

    expected<void, DBException> MongoDBConnection::dropDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load(std::memory_order_acquire))
            {
                return unexpected<DBException>(DBException(
                    "U06DKBMOC39W",
                    "Connection has been closed"));
            }

            MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), databaseName.c_str()));

            bson_error_t error;
            bool success = mongoc_database_drop(db.get(), &error);

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "3OT3OF7PRQFA",
                    std::string("Failed to drop database: ") + error.message));
            }

            return {};
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "6UUWT77M2KLB",
                "Memory allocation failed in dropDatabase"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "8F9A0B1C2D3E",
                std::string("Unexpected error in dropDatabase: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "9A0B1C2D3E4F",
                "Unknown error in dropDatabase"));
        }
    }

    // ====================================================================
    // NOTHROW API - getCollection, listCollections, collectionExists, createCollection, dropCollection (real implementations)
    // ====================================================================

    expected<std::shared_ptr<DocumentDBCollection>, DBException> MongoDBConnection::getCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load(std::memory_order_acquire))
            {
                return unexpected<DBException>(DBException(
                    "34BF1ABBB585",
                    "Connection has been closed"));
            }

            if (m_databaseName.empty())
            {
                return unexpected<DBException>(DBException(
                    "4QCN6OXX3B8N",
                    "No database selected. Call useDatabase() first"));
            }

            mongoc_collection_t *collection = mongoc_client_get_collection(
                m_client.get(),
                m_databaseName.c_str(),
                collectionName.c_str());

            if (!collection)
            {
                return unexpected<DBException>(DBException(
                    "49D69DDC2A47",
                    "Failed to get collection: " + collectionName));
            }

            auto collResult = MongoDBCollection::create(std::nothrow,
                                                        m_client,
                                                        collection,
                                                        collectionName,
                                                        m_databaseName,
                                                        weak_from_this()
#if DB_DRIVER_THREAD_SAFE
                                                            ,
                                                        m_connMutex
#endif
            );
            if (!collResult.has_value())
            {
                return unexpected<DBException>(collResult.error());
            }
            auto collectionPtr = collResult.value();

            registerCollection(std::nothrow, collectionPtr);

            return std::static_pointer_cast<DocumentDBCollection>(collectionPtr);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "4SA8Z30QAS7Q",
                "Memory allocation failed in getCollection"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "T05R4LCMCDEW",
                std::string("Unexpected error in getCollection: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "FDE1D469FA3C",
                "Unknown error in getCollection"));
        }
    }

    expected<std::vector<std::string>, DBException> MongoDBConnection::listCollections(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load(std::memory_order_acquire))
            {
                return unexpected<DBException>(DBException(
                    "5A6B7C8D9E0F",
                    "Connection has been closed"));
            }

            if (m_databaseName.empty())
            {
                return unexpected<DBException>(DBException(
                    "6B7C8D9E0F1A",
                    "No database selected. Call useDatabase() first"));
            }

            MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), m_databaseName.c_str()));

            bson_error_t error;
            char **names = mongoc_database_get_collection_names_with_opts(db.get(), nullptr, &error);

            if (!names)
            {
                return unexpected<DBException>(DBException(
                    "7C8D9E0F1A2B",
                    std::string("Failed to list collections: ") + error.message));
            }

            std::vector<std::string> collections;
            for (size_t i = 0; names[i] != nullptr; ++i)
            {
                collections.emplace_back(names[i]);
            }

            bson_strfreev(names);
            return collections;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "8D9E0F1A2B3C",
                "Memory allocation failed in listCollections"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "9E0F1A2B3C4D",
                std::string("Unexpected error in listCollections: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "0F1A2B3C4D5E",
                "Unknown error in listCollections"));
        }
    }

    expected<bool, DBException> MongoDBConnection::collectionExists(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        try
        {
            auto colsResult = listCollections(std::nothrow);
            if (!colsResult.has_value())
            {
                return cpp_dbc::unexpected(colsResult.error());
            }
            const auto &collections = *colsResult;
            return std::ranges::find(collections, collectionName) != collections.end();
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("HCA7AJHZUCGE",
                                                   std::string("Exception in collectionExists: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("R3LWE02BEA9G",
                                                   "Unknown exception in collectionExists",
                                                   system_utils::captureCallStack()));
        }
    }

    expected<std::shared_ptr<DocumentDBCollection>, DBException> MongoDBConnection::createCollection(
        std::nothrow_t,
        const std::string &collectionName,
        const std::string &options) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load(std::memory_order_acquire))
            {
                return unexpected<DBException>(DBException(
                    "1A2B3C4D5E6F",
                    "Connection has been closed"));
            }

            if (m_databaseName.empty())
            {
                return unexpected<DBException>(DBException(
                    "2B3C4D5E6F7A",
                    "No database selected. Call useDatabase() first"));
            }

            MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), m_databaseName.c_str()));

            bson_t *opts = nullptr;
            if (!options.empty())
            {
                bson_error_t parseError;
                opts = bson_new_from_json(
                    reinterpret_cast<const uint8_t *>(options.c_str()),
                    static_cast<ssize_t>(options.length()),
                    &parseError);
                if (!opts)
                {
                    return unexpected<DBException>(DBException(
                        "3C4D5E6F7A8B",
                        std::string("Invalid options JSON: ") + parseError.message));
                }
            }

            bson_error_t error;
            mongoc_collection_t *coll = mongoc_database_create_collection(
                db.get(), collectionName.c_str(), opts, &error);

            if (opts)
            {
                bson_destroy(opts);
            }

            if (!coll)
            {
                return unexpected<DBException>(DBException(
                    "4D5E6F7A8B9C",
                    std::string("Failed to create collection: ") + error.message));
            }

            auto collResult = MongoDBCollection::create(std::nothrow,
                                                        m_client,
                                                        coll,
                                                        collectionName,
                                                        m_databaseName,
                                                        weak_from_this()
#if DB_DRIVER_THREAD_SAFE
                                                            ,
                                                        m_connMutex
#endif
            );
            if (!collResult.has_value())
            {
                return unexpected<DBException>(collResult.error());
            }
            auto collectionPtr = collResult.value();

            registerCollection(std::nothrow, collectionPtr);

            return std::static_pointer_cast<DocumentDBCollection>(collectionPtr);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "5E6F7A8B9C0D",
                "Memory allocation failed in createCollection"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "6F7A8B9C0D1E",
                std::string("Unexpected error in createCollection: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "Q3R4S5T6U7V8",
                "Unknown error in createCollection"));
        }
    }

    expected<void, DBException> MongoDBConnection::dropCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load(std::memory_order_acquire))
            {
                return unexpected<DBException>(DBException(
                    "LCTMVU4COTWE",
                    "Connection has been closed"));
            }

            if (m_databaseName.empty())
            {
                return unexpected<DBException>(DBException(
                    "IK7PNBHX4UYQ",
                    "No database selected. Call useDatabase() first"));
            }

            mongoc_collection_t *coll = mongoc_client_get_collection(
                m_client.get(), m_databaseName.c_str(), collectionName.c_str());

            if (!coll)
            {
                return unexpected<DBException>(DBException(
                    "SNKN547TL3LV",
                    "Failed to get collection: " + collectionName));
            }

            MongoCollectionHandle collHandle(coll);

            bson_error_t error;
            bool success = mongoc_collection_drop(collHandle.get(), &error);

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "25KZMHWC7LOM",
                    std::string("Failed to drop collection: ") + error.message));
            }

            return {};
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "WKYWTDNRHHQ7",
                "Memory allocation failed in dropCollection"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "ORKDZSTMMSM0",
                std::string("Unexpected error in dropCollection: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "CKJ52W4NXG22",
                "Unknown error in dropCollection"));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
