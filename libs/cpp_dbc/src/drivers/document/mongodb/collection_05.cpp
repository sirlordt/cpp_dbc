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
 * @file collection_05.cpp
 * @brief MongoDB MongoDBCollection - Part 5 (nothrow API: deleteOne, deleteMany, deleteById)
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
    // NOTHROW API - deleteOne, deleteMany, deleteById (implementations)
    // ====================================================================

    expected<DocumentDeleteResult, DBException> MongoDBCollection::deleteOne(
        std::nothrow_t,
        const std::string &filter) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::deleteOne(nothrow) - Deleting from: %s", m_name.c_str());
        MONGODB_STMT_LOCK_OR_RETURN("B6A0V7R1M3X3", "Connection closed");

        auto filterResult = parseFilter(std::nothrow, filter);
        if (!filterResult.has_value())
        {
            return unexpected<DBException>(filterResult.error());
        }
        BsonHandle filterBson = std::move(filterResult.value());

        bson_error_t error;
        bson_t reply;
        bson_init(&reply);

        bool success = mongoc_collection_delete_one(
            m_collection.get(), filterBson.get(), nullptr, &reply, &error);

        DocumentDeleteResult result;
        result.acknowledged = success;

        if (success)
        {
            bson_iter_t iter;
            if (bson_iter_init_find(&iter, &reply, "deletedCount"))
            {
                result.deletedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
        }
        else
        {
            bson_destroy(&reply);
            return unexpected<DBException>(DBException(
                "4DDW3BUKHACF",
                std::string("deleteOne failed: ") + error.message,
                system_utils::captureCallStack()));
        }

        bson_destroy(&reply);
        return result;
    }

    expected<DocumentDeleteResult, DBException> MongoDBCollection::deleteMany(
        std::nothrow_t,
        const std::string &filter) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::deleteMany(nothrow) - Deleting from: %s", m_name.c_str());
        MONGODB_STMT_LOCK_OR_RETURN("3J4J8KKRTRN9", "Connection closed");

        auto filterResult = parseFilter(std::nothrow, filter);
        if (!filterResult.has_value())
        {
            return unexpected<DBException>(filterResult.error());
        }
        BsonHandle filterBson = std::move(filterResult.value());

        bson_error_t error;
        bson_t reply;
        bson_init(&reply);

        bool success = mongoc_collection_delete_many(
            m_collection.get(), filterBson.get(), nullptr, &reply, &error);

        DocumentDeleteResult result;
        result.acknowledged = success;

        if (success)
        {
            bson_iter_t iter;
            if (bson_iter_init_find(&iter, &reply, "deletedCount"))
            {
                result.deletedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
        }
        else
        {
            bson_destroy(&reply);
            return unexpected<DBException>(DBException(
                "GZER4DMZR6NA",
                std::string("deleteMany failed: ") + error.message,
                system_utils::captureCallStack()));
        }

        bson_destroy(&reply);
        return result;
    }

    expected<DocumentDeleteResult, DBException> MongoDBCollection::deleteById(
        std::nothrow_t,
        const std::string &id) noexcept
    {
        auto filterResult = buildIdFilter(std::nothrow, id);
        if (!filterResult.has_value())
        {
            return unexpected<DBException>(filterResult.error());
        }
        return deleteOne(std::nothrow, filterResult.value());
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
