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
        DB_DRIVER_LOCK_GUARD(*m_connMutex);
        return m_databaseName;
    }

    expected<std::vector<std::string>, DBException> MongoDBConnection::listDatabases(std::nothrow_t) noexcept
    {
        MONGODB_CONNECTION_LOCK_OR_RETURN("C54C7EECE4D6", "Cannot list databases");

        bson_error_t error;
        BsonStrArray names(mongoc_client_get_database_names_with_opts(m_conn.get(), nullptr, &error));

        if (!names)
        {
            return unexpected<DBException>(DBException(
                "EED4BB95EA04",
                std::string("Failed to list databases: ") + error.message));
        }

        std::vector<std::string> databases;
        for (size_t i = 0; names.get()[i] != nullptr; ++i)
        {
            databases.emplace_back(names.get()[i]);
        }

        return databases;
    }

    expected<bool, DBException> MongoDBConnection::databaseExists(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (!databaseName.empty() && !system_utils::isValidDatabaseIdentifier(databaseName))
        {
            return cpp_dbc::unexpected(DBException(
                "R5CCK8YX8132",
                "Invalid database name: " + databaseName,
                system_utils::captureCallStack()));
        }

        auto dbsResult = listDatabases(std::nothrow);
        if (!dbsResult.has_value())
        {
            return cpp_dbc::unexpected(dbsResult.error());
        }
        const auto &databases = *dbsResult;
        return std::ranges::find(databases, databaseName) != databases.end();
    }

    expected<void, DBException> MongoDBConnection::useDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        MONGODB_CONNECTION_LOCK_OR_RETURN("TOH5ELBYU3J8", "Cannot use database");

        if (!databaseName.empty() && !system_utils::isValidDatabaseIdentifier(databaseName))
        {
            return cpp_dbc::unexpected(DBException(
                "PN5S7JPRGTL8",
                "Invalid database name: " + databaseName,
                system_utils::captureCallStack()));
        }

        m_databaseName = databaseName;
        return {};
    }

    expected<void, DBException> MongoDBConnection::dropDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        MONGODB_CONNECTION_LOCK_OR_RETURN("U06DKBMOC39W", "Cannot drop database");

        if (!databaseName.empty() && !system_utils::isValidDatabaseIdentifier(databaseName))
        {
            return cpp_dbc::unexpected(DBException(
                "VHBESPTAJGIP",
                "Invalid database name: " + databaseName,
                system_utils::captureCallStack()));
        }

        auto *rawDb = mongoc_client_get_database(m_conn.get(), databaseName.c_str());
        if (!rawDb)
        {
            return cpp_dbc::unexpected(DBException(
                "PL1TA1VJP6H7",
                "Failed to acquire database handle for drop",
                system_utils::captureCallStack()));
        }
        MongoDatabaseHandle db(rawDb);

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

    // ====================================================================
    // NOTHROW API - getCollection, listCollections, collectionExists, createCollection, dropCollection (real implementations)
    // ====================================================================

    expected<std::shared_ptr<DocumentDBCollection>, DBException> MongoDBConnection::getCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        MONGODB_CONNECTION_LOCK_OR_RETURN("34BF1ABBB585", "Cannot get collection");

        if (m_databaseName.empty())
        {
            return unexpected<DBException>(DBException(
                "4QCN6OXX3B8N",
                "No database selected. Call useDatabase() first"));
        }

        mongoc_collection_t *collection = mongoc_client_get_collection(
            m_conn.get(),
            m_databaseName.c_str(),
            collectionName.c_str());

        if (!collection)
        {
            return unexpected<DBException>(DBException(
                "49D69DDC2A47",
                "Failed to get collection: " + collectionName));
        }

        auto collResult = MongoDBCollection::create(std::nothrow,
                                                    collection,
                                                    collectionName,
                                                    m_databaseName,
                                                    weak_from_this());
        if (!collResult.has_value())
        {
            return unexpected<DBException>(collResult.error());
        }
        auto collectionPtr = collResult.value();

        registerCollection(std::nothrow, collectionPtr);

        return std::static_pointer_cast<DocumentDBCollection>(collectionPtr);
    }

    expected<std::vector<std::string>, DBException> MongoDBConnection::listCollections(std::nothrow_t) noexcept
    {
        MONGODB_CONNECTION_LOCK_OR_RETURN("N3SD6D8C6ACJ", "Cannot list collections");

        if (m_databaseName.empty())
        {
            return unexpected<DBException>(DBException(
                "PS6EQH1WUO63",
                "No database selected. Call useDatabase() first"));
        }

        auto *rawDb = mongoc_client_get_database(m_conn.get(), m_databaseName.c_str());
        if (!rawDb)
        {
            return cpp_dbc::unexpected(DBException(
                "K1QZOW0R5Q3Q",
                "Failed to acquire database handle for listing collections",
                system_utils::captureCallStack()));
        }
        MongoDatabaseHandle db(rawDb);

        bson_error_t error;
        BsonStrArray names(mongoc_database_get_collection_names_with_opts(db.get(), nullptr, &error));

        if (!names)
        {
            return unexpected<DBException>(DBException(
                "3OW3LV87GMU5",
                std::string("Failed to list collections: ") + error.message));
        }

        std::vector<std::string> collections;
        for (size_t i = 0; names.get()[i] != nullptr; ++i)
        {
            collections.emplace_back(names.get()[i]);
        }
        return collections;
    }

    expected<bool, DBException> MongoDBConnection::collectionExists(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        auto colsResult = listCollections(std::nothrow);
        if (!colsResult.has_value())
        {
            return cpp_dbc::unexpected(colsResult.error());
        }
        const auto &collections = *colsResult;
        return std::ranges::find(collections, collectionName) != collections.end();
    }

    expected<std::shared_ptr<DocumentDBCollection>, DBException> MongoDBConnection::createCollection(
        std::nothrow_t,
        const std::string &collectionName,
        const std::string &options) noexcept
    {
        MONGODB_CONNECTION_LOCK_OR_RETURN("QH38NDY2VMSQ", "Cannot create collection");

        if (m_databaseName.empty())
        {
            return unexpected<DBException>(DBException(
                "WVUFLLD4A97S",
                "No database selected. Call useDatabase() first"));
        }

        auto *rawDb = mongoc_client_get_database(m_conn.get(), m_databaseName.c_str());
        if (!rawDb)
        {
            return cpp_dbc::unexpected(DBException(
                "NQQE8ZZHEDK3",
                "Failed to acquire database handle for creating collection",
                system_utils::captureCallStack()));
        }
        MongoDatabaseHandle db(rawDb);

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
                    "WC66P18APOQR",
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
                "WCWBQDAJPLZE",
                std::string("Failed to create collection: ") + error.message));
        }

        auto collResult = MongoDBCollection::create(std::nothrow,
                                                    coll,
                                                    collectionName,
                                                    m_databaseName,
                                                    weak_from_this());
        if (!collResult.has_value())
        {
            return unexpected<DBException>(collResult.error());
        }
        auto collectionPtr = collResult.value();

        registerCollection(std::nothrow, collectionPtr);

        return std::static_pointer_cast<DocumentDBCollection>(collectionPtr);
    }

    expected<void, DBException> MongoDBConnection::dropCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        MONGODB_CONNECTION_LOCK_OR_RETURN("LCTMVU4COTWE", "Cannot drop collection");

        if (m_databaseName.empty())
        {
            return unexpected<DBException>(DBException(
                "IK7PNBHX4UYQ",
                "No database selected. Call useDatabase() first"));
        }

        mongoc_collection_t *coll = mongoc_client_get_collection(
            m_conn.get(), m_databaseName.c_str(), collectionName.c_str());

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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
