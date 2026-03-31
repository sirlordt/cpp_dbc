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
 * @file collection_04.cpp
 * @brief MongoDB MongoDBCollection - Part 4 (nothrow API: updateOne, updateMany, replaceOne)
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
    // NOTHROW API - updateOne, updateMany, replaceOne (implementations)
    // ====================================================================

    expected<DocumentUpdateResult, DBException> MongoDBCollection::updateOne(
        std::nothrow_t,
        const std::string &filter,
        const std::string &update,
        const DocumentUpdateOptions &options) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::updateOne(nothrow) - Updating in: %s", m_name.c_str());
        MONGODB_STMT_LOCK_OR_RETURN("Y92KDTUMAJAN", "Connection closed");

        auto filterResult = parseFilter(std::nothrow, filter);
        if (!filterResult.has_value())
        {
            return unexpected<DBException>(filterResult.error());
        }
        BsonHandle filterBson = std::move(filterResult.value());

        auto updateBsonResult = makeBsonHandleFromJson(std::nothrow, update);
        if (!updateBsonResult.has_value())
        {
            return unexpected<DBException>(updateBsonResult.error());
        }
        BsonHandle updateBson = std::move(updateBsonResult.value());

        bson_t opts = BSON_INITIALIZER;
        if (options.upsert)
        {
            BSON_APPEND_BOOL(&opts, "upsert", true);
        }

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
            {
                result.matchedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            if (bson_iter_init_find(&iter, &reply, "modifiedCount"))
            {
                result.modifiedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
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
                "JP3OM58MFOER",
                std::string("updateOne failed: ") + error.message,
                system_utils::captureCallStack()));
        }

        bson_destroy(&reply);
        return result;
    }

    expected<DocumentUpdateResult, DBException> MongoDBCollection::updateMany(
        std::nothrow_t,
        const std::string &filter,
        const std::string &update,
        const DocumentUpdateOptions &options) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::updateMany(nothrow) - Updating in: %s", m_name.c_str());
        MONGODB_STMT_LOCK_OR_RETURN("UFL4AW663UJ2", "Connection closed");

        auto filterResult = parseFilter(std::nothrow, filter);
        if (!filterResult.has_value())
        {
            return unexpected<DBException>(filterResult.error());
        }
        BsonHandle filterBson = std::move(filterResult.value());

        auto updateBsonResult = makeBsonHandleFromJson(std::nothrow, update);
        if (!updateBsonResult.has_value())
        {
            return unexpected<DBException>(updateBsonResult.error());
        }
        BsonHandle updateBson = std::move(updateBsonResult.value());

        bson_t opts = BSON_INITIALIZER;
        if (options.upsert)
        {
            BSON_APPEND_BOOL(&opts, "upsert", true);
        }

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
            {
                result.matchedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            if (bson_iter_init_find(&iter, &reply, "modifiedCount"))
            {
                result.modifiedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
        }
        else
        {
            bson_destroy(&reply);
            return unexpected<DBException>(DBException(
                "K0LHUO5BV67Q",
                std::string("updateMany failed: ") + error.message,
                system_utils::captureCallStack()));
        }

        bson_destroy(&reply);
        return result;
    }

    expected<DocumentUpdateResult, DBException> MongoDBCollection::replaceOne(
        std::nothrow_t,
        const std::string &filter,
        std::shared_ptr<DocumentDBData> replacement,
        const DocumentUpdateOptions &options) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::replaceOne(nothrow) - Replacing in: %s", m_name.c_str());
        MONGODB_STMT_LOCK_OR_RETURN("GV07UFPBM9P6", "Connection closed");

        auto mongoDoc = std::dynamic_pointer_cast<MongoDBDocument>(replacement);
        if (!mongoDoc)
        {
            return unexpected<DBException>(DBException(
                "C3D9E8F7A2B1",
                "Replacement must be a MongoDBDocument",
                system_utils::captureCallStack()));
        }

        auto filterResult = parseFilter(std::nothrow, filter);
        if (!filterResult.has_value())
        {
            return unexpected<DBException>(filterResult.error());
        }
        BsonHandle filterBson = std::move(filterResult.value());

        bson_t opts = BSON_INITIALIZER;
        if (options.upsert)
        {
            BSON_APPEND_BOOL(&opts, "upsert", true);
        }

        bson_error_t error;
        bson_t reply;
        bson_init(&reply);

        bool success = mongoc_collection_replace_one(
            m_collection.get(), filterBson.get(), mongoDoc->getBson(std::nothrow), &opts, &reply, &error);
        bson_destroy(&opts);

        DocumentUpdateResult result;
        result.acknowledged = success;

        if (success)
        {
            bson_iter_t iter;
            if (bson_iter_init_find(&iter, &reply, "matchedCount"))
            {
                result.matchedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            if (bson_iter_init_find(&iter, &reply, "modifiedCount"))
            {
                result.modifiedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
        }
        else
        {
            bson_destroy(&reply);
            return unexpected<DBException>(DBException(
                "CQPAGOVXJN0Z",
                std::string("replaceOne failed: ") + error.message,
                system_utils::captureCallStack()));
        }

        bson_destroy(&reply);
        return result;
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
