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
 * @brief MongoDB MongoDBCollection - Part 4 (UPDATE throwing versions, DELETE nothrow versions)
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
    // UPDATE OPERATIONS - throwing versions (WRAPPERS)
    // ====================================================================

    DocumentUpdateResult MongoDBCollection::updateOne(
        const std::string &filter,
        const std::string &update,
        const DocumentUpdateOptions &options)
    {
        auto result = updateOne(std::nothrow, filter, update, options);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    DocumentUpdateResult MongoDBCollection::updateMany(
        const std::string &filter,
        const std::string &update,
        const DocumentUpdateOptions &options)
    {
        auto result = updateMany(std::nothrow, filter, update, options);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    DocumentUpdateResult MongoDBCollection::replaceOne(
        const std::string &filter,
        std::shared_ptr<DocumentDBData> replacement,
        const DocumentUpdateOptions &options)
    {
        auto result = replaceOne(std::nothrow, filter, replacement, options);
        if (!result)
        {
            throw result.error();
        }
        return *result;
    }

    // ====================================================================
    // DELETE OPERATIONS - nothrow versions (REAL IMPLEMENTATIONS)
    // ====================================================================

    expected<DocumentDeleteResult, DBException> MongoDBCollection::deleteOne(
        std::nothrow_t,
        const std::string &filter) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::deleteOne(nothrow) - Deleting from: " << m_name);
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "B3C4D5E6F7A8",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            BsonHandle filterBson = parseFilter(filter);

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
                    result.deletedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            else
            {
                bson_destroy(&reply);
                return unexpected<DBException>(DBException(
                    "C4D5E6F7A8B9",
                    std::string("deleteOne failed: ") + error.message,
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
                "D5E6F7A8B9C0",
                "Memory allocation failed in deleteOne",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "E6F7A8B9C0D1",
                std::string("Unexpected error in deleteOne: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "F7A8B9C0D1E2",
                "Unknown error in deleteOne",
                system_utils::captureCallStack()));
        }
    }

    expected<DocumentDeleteResult, DBException> MongoDBCollection::deleteMany(
        std::nothrow_t,
        const std::string &filter) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::deleteMany(nothrow) - Deleting from: " << m_name);
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "A8B9C0D1E2F3",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            BsonHandle filterBson = parseFilter(filter);

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
                    result.deletedCount = static_cast<uint64_t>(bson_iter_as_int64(&iter));
            }
            else
            {
                bson_destroy(&reply);
                return unexpected<DBException>(DBException(
                    "B9C0D1E2F3A4",
                    std::string("deleteMany failed: ") + error.message,
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
                "C0D1E2F3A4B5",
                "Memory allocation failed in deleteMany",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "D1E2F3A4B5C6",
                std::string("Unexpected error in deleteMany: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "E2F3A4B5C6D7",
                "Unknown error in deleteMany",
                system_utils::captureCallStack()));
        }
    }

    expected<DocumentDeleteResult, DBException> MongoDBCollection::deleteById(
        std::nothrow_t,
        const std::string &id) noexcept
    {
        try
        {
            std::string filter;
            if (bson_oid_is_valid(id.c_str(), id.length()))
            {
                filter = R"({"_id": {"$oid": ")" + id + R"("}})";
            }
            else
            {
                filter = R"({"_id": ")" + id + R"("})";
            }
            return deleteOne(std::nothrow, filter);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "F3A4B5C6D7E8",
                std::string("Error in deleteById: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "A4B5C6D7E8F9",
                "Unknown error in deleteById",
                system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
