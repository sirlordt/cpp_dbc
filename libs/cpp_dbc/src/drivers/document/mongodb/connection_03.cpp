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
 * @file connection_03.cpp
 * @brief MongoDB MongoDBConnection - Part 3 (nothrow versions: database and collection operations)
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
    // MongoDBConnection NOTHROW VERSIONS - Database and Collection Operations
    // ====================================================================

    expected<std::vector<std::string>, DBException> MongoDBConnection::listDatabases(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load())
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
                "1DCF59899097",
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
                "79046EE25E4F",
                "Unknown error in listDatabases"));
        }
    }

    expected<std::shared_ptr<DocumentDBCollection>, DBException> MongoDBConnection::getCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load())
            {
                return unexpected<DBException>(DBException(
                    "34BF1ABBB585",
                    "Connection has been closed"));
            }

            if (m_databaseName.empty())
            {
                return unexpected<DBException>(DBException(
                    "79292056CE1F",
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

            auto collectionPtr = std::make_shared<MongoDBCollection>(
                m_client,
                collection,
                collectionName,
                m_databaseName,
                weak_from_this()
#if DB_DRIVER_THREAD_SAFE
                , m_connMutex
#endif
            );

            return std::static_pointer_cast<DocumentDBCollection>(collectionPtr);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "D8267AB49B49",
                "Memory allocation failed in getCollection"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "277C75E974F0",
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

            if (m_closed.load())
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

    expected<std::shared_ptr<DocumentDBCollection>, DBException> MongoDBConnection::createCollection(
        std::nothrow_t,
        const std::string &collectionName,
        const std::string &options) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load())
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
                bson_destroy(opts);

            if (!coll)
            {
                return unexpected<DBException>(DBException(
                    "4D5E6F7A8B9C",
                    std::string("Failed to create collection: ") + error.message));
            }

            auto collectionPtr = std::make_shared<MongoDBCollection>(
                m_client,
                coll,
                collectionName,
                m_databaseName,
                weak_from_this()
#if DB_DRIVER_THREAD_SAFE
                , m_connMutex
#endif
            );

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

            if (m_closed.load())
            {
                return unexpected<DBException>(DBException(
                    "8B9C0D1E2F3A",
                    "Connection has been closed"));
            }

            if (m_databaseName.empty())
            {
                return unexpected<DBException>(DBException(
                    "9C0D1E2F3A4B",
                    "No database selected. Call useDatabase() first"));
            }

            mongoc_collection_t *coll = mongoc_client_get_collection(
                m_client.get(), m_databaseName.c_str(), collectionName.c_str());

            if (!coll)
            {
                return unexpected<DBException>(DBException(
                    "0D1E2F3A4B5C",
                    "Failed to get collection: " + collectionName));
            }

            MongoCollectionHandle collHandle(coll);

            bson_error_t error;
            bool success = mongoc_collection_drop(collHandle.get(), &error);

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "1E2F3A4B5C6D",
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
                "2F3A4B5C6D7E",
                "Memory allocation failed in dropCollection"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "3A4B5C6D7E8F",
                std::string("Unexpected error in dropCollection: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "4B5C6D7E8F9A",
                "Unknown error in dropCollection"));
        }
    }

    expected<void, DBException> MongoDBConnection::dropDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_closed.load())
            {
                return unexpected<DBException>(DBException(
                    "5C6D7E8F9A0B",
                    "Connection has been closed"));
            }

            MongoDatabaseHandle db(mongoc_client_get_database(m_client.get(), databaseName.c_str()));

            bson_error_t error;
            bool success = mongoc_database_drop(db.get(), &error);

            if (!success)
            {
                return unexpected<DBException>(DBException(
                    "6D7E8F9A0B1C",
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
                "7E8F9A0B1C2D",
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

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
