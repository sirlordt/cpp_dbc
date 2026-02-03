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
 * @file collection_03.cpp
 * @brief MongoDB MongoDBCollection - Part 3 (FIND throwing versions, UPDATE nothrow versions)
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
    // FIND OPERATIONS - throwing versions (WRAPPERS)
    // ====================================================================

    std::shared_ptr<DocumentDBData> MongoDBCollection::findOne(const std::string &filter)
    {
        auto result = findOne(std::nothrow, filter);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<DocumentDBData> MongoDBCollection::findById(const std::string &id)
    {
        auto result = findById(std::nothrow, id);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<DocumentDBCursor> MongoDBCollection::find(const std::string &filter)
    {
        auto result = find(std::nothrow, filter);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<DocumentDBCursor> MongoDBCollection::find(
        const std::string &filter,
        const std::string &projection)
    {
        auto result = find(std::nothrow, filter, projection);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    // ====================================================================
    // UPDATE OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
    // ====================================================================

    expected<DocumentUpdateResult, DBException> MongoDBCollection::updateOne(
        std::nothrow_t,
        const std::string &filter,
        const std::string &update,
        const DocumentUpdateOptions &options) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::updateOne(nothrow) - Updating in: " << m_name);
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "E8F9A0B1C2D3",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            BsonHandle filterBson = parseFilter(filter);
            BsonHandle updateBson = makeBsonHandleFromJson(update);

            bson_t opts = BSON_INITIALIZER;
            if (options.upsert)
                BSON_APPEND_BOOL(&opts, "upsert", true);

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_update_one(
                m_collection.get(), filterBson.get(), updateBson.get(), &opts, &reply, &error);
            bson_destroy(&opts);

            DocumentUpdateResult result;
            result.acknowledged = success;

            if (success)
            {
                bson_iter_t iter;
                if (bson_iter_init_find(&iter, &reply, "matchedCount"))
                    result.matchedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
                if (bson_iter_init_find(&iter, &reply, "modifiedCount"))
                    result.modifiedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
                if (bson_iter_init_find(&iter, &reply, "upsertedId") && BSON_ITER_HOLDS_OID(&iter))
                {
                    const bson_oid_t *oid = bson_iter_oid(&iter);
                    std::array<char, 25> oidStr{};
                    bson_oid_to_string(oid, oidStr.data());
                    result.upsertedId = oidStr.data();
                }
            }
            else
            {
                bson_destroy(&reply);
                return unexpected<DBException>(DBException(
                    "F9A0B1C2D3E4",
                    std::string("updateOne failed: ") + error.message,
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
                "A0B1C2D3E4F5",
                "Memory allocation failed in updateOne",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "B1C2D3E4F5A6",
                std::string("Unexpected error in updateOne: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "C2D3E4F5A6B7",
                "Unknown error in updateOne",
                system_utils::captureCallStack()));
        }
    }

    expected<DocumentUpdateResult, DBException> MongoDBCollection::updateMany(
        std::nothrow_t,
        const std::string &filter,
        const std::string &update,
        const DocumentUpdateOptions &options) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::updateMany(nothrow) - Updating in: " << m_name);
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "D3E4F5A6B7C8",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            BsonHandle filterBson = parseFilter(filter);
            BsonHandle updateBson = makeBsonHandleFromJson(update);

            bson_t opts = BSON_INITIALIZER;
            if (options.upsert)
                BSON_APPEND_BOOL(&opts, "upsert", true);

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_update_many(
                m_collection.get(), filterBson.get(), updateBson.get(), &opts, &reply, &error);
            bson_destroy(&opts);

            DocumentUpdateResult result;
            result.acknowledged = success;

            if (success)
            {
                bson_iter_t iter;
                if (bson_iter_init_find(&iter, &reply, "matchedCount"))
                    result.matchedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
                if (bson_iter_init_find(&iter, &reply, "modifiedCount"))
                    result.modifiedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            else
            {
                bson_destroy(&reply);
                return unexpected<DBException>(DBException(
                    "E4F5A6B7C8D9",
                    std::string("updateMany failed: ") + error.message,
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
                "F5A6B7C8D9E0",
                "Memory allocation failed in updateMany",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "A6B7C8D9E0F1",
                std::string("Unexpected error in updateMany: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "B7C8D9E0F1A2",
                "Unknown error in updateMany",
                system_utils::captureCallStack()));
        }
    }

    expected<DocumentUpdateResult, DBException> MongoDBCollection::replaceOne(
        std::nothrow_t,
        const std::string &filter,
        std::shared_ptr<DocumentDBData> replacement,
        const DocumentUpdateOptions &options) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::replaceOne(nothrow) - Replacing in: " << m_name);
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "C8D9E0F1A2B3",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(replacement);
            if (!mongoDoc)
            {
                return unexpected<DBException>(DBException(
                    "C3D9E8F7A2B1",
                    "Replacement must be a MongoDBDocument",
                    system_utils::captureCallStack()));
            }

            BsonHandle filterBson = parseFilter(filter);

            bson_t opts = BSON_INITIALIZER;
            if (options.upsert)
                BSON_APPEND_BOOL(&opts, "upsert", true);

            bson_error_t error;
            bson_t reply;
            bson_init(&reply);

            bool success = mongoc_collection_replace_one(
                m_collection.get(), filterBson.get(), mongoDoc->getBson(), &opts, &reply, &error);
            bson_destroy(&opts);

            DocumentUpdateResult result;
            result.acknowledged = success;

            if (success)
            {
                bson_iter_t iter;
                if (bson_iter_init_find(&iter, &reply, "matchedCount"))
                    result.matchedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
                if (bson_iter_init_find(&iter, &reply, "modifiedCount"))
                    result.modifiedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            else
            {
                bson_destroy(&reply);
                return unexpected<DBException>(DBException(
                    "D9E0F1A2B3C4",
                    std::string("replaceOne failed: ") + error.message,
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
                "E0F1A2B3C4D5",
                "Memory allocation failed in replaceOne",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "F1A2B3C4D5E6",
                std::string("Unexpected error in replaceOne: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "A2B3C4D5E6F7",
                "Unknown error in replaceOne",
                system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
