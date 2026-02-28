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
 * @brief MongoDB MongoDBCollection - Part 3 (nothrow API: find)
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
    // THROWING API - find (wrappers)
    // ====================================================================

    #ifdef __cpp_exceptions
    std::shared_ptr<DocumentDBCursor> MongoDBCollection::find(const std::string &filter)
    {
        auto r = find(std::nothrow, filter);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }

    std::shared_ptr<DocumentDBCursor> MongoDBCollection::find(
        const std::string &filter,
        const std::string &projection)
    {
        auto r = find(std::nothrow, filter, projection);
        if (!r.has_value()) { throw r.error(); }
        return r.value();
    }
    #endif // __cpp_exceptions

    // ====================================================================
    // NOTHROW API - find (real implementations)
    // ====================================================================

    expected<std::shared_ptr<DocumentDBCursor>, DBException> MongoDBCollection::find(
        std::nothrow_t,
        const std::string &filter) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::find(nothrow) - Finding in: " << m_name);
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "C0D1E2F3A4B5",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            auto filterResult = parseFilter(std::nothrow, filter);
            if (!filterResult.has_value())
            {
                return unexpected<DBException>(filterResult.error());
            }
            BsonHandle filterBson = std::move(filterResult.value());
            mongoc_cursor_t *rawCursor = mongoc_collection_find_with_opts(
                m_collection.get(), filterBson.get(), nullptr, nullptr);

            if (!rawCursor)
            {
                return unexpected<DBException>(DBException(
                    "A1B7C6D5E0F9",
                    "Failed to create cursor for find",
                    system_utils::captureCallStack()));
            }

            // Wrap cursor in RAII handle immediately to prevent leaks if create() fails
            MongoCursorHandle cursorHandle(rawCursor);

            // Create the MongoDBCursor - it will take ownership via release()
            auto cursorResult = MongoDBCursor::create(std::nothrow, m_client, cursorHandle.release(), m_connection
#if DB_DRIVER_THREAD_SAFE
                , m_connMutex
#endif
            );
            if (!cursorResult.has_value())
            {
                return unexpected<DBException>(cursorResult.error());
            }
            auto cursor = cursorResult.value();

            // Register cursor with connection for cleanup tracking
            if (auto conn = m_connection.lock())
            {
                conn->registerCursor(cursor);
            }

            return std::shared_ptr<DocumentDBCursor>(cursor);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "D1E2F3A4B5C6",
                "Memory allocation failed in find",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "E2F3A4B5C6D7",
                std::string("Unexpected error in find: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "F3A4B5C6D7E8",
                "Unknown error in find",
                system_utils::captureCallStack()));
        }
    }

    expected<std::shared_ptr<DocumentDBCursor>, DBException> MongoDBCollection::find(
        std::nothrow_t,
        const std::string &filter,
        const std::string &projection) noexcept
    {
        try
        {
            MONGODB_DEBUG("MongoDBCollection::find(nothrow) with projection - Finding in: " << m_name);
            MONGODB_LOCK_GUARD(*m_connMutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "A4B5C6D7E8F9",
                    "Connection has been closed",
                    system_utils::captureCallStack()));
            }

            auto filterResult = parseFilter(std::nothrow, filter);
            if (!filterResult.has_value())
            {
                return unexpected<DBException>(filterResult.error());
            }
            BsonHandle filterBson = std::move(filterResult.value());

            auto projResult = parseFilter(std::nothrow, projection);
            if (!projResult.has_value())
            {
                return unexpected<DBException>(projResult.error());
            }
            BsonHandle projBson = std::move(projResult.value());

            bson_t opts = BSON_INITIALIZER;
            BSON_APPEND_DOCUMENT(&opts, "projection", projBson.get());

            mongoc_cursor_t *rawCursor = mongoc_collection_find_with_opts(
                m_collection.get(), filterBson.get(), &opts, nullptr);
            bson_destroy(&opts);

            if (!rawCursor)
            {
                return unexpected<DBException>(DBException(
                    "B2C8D7E6F1A0",
                    "Failed to create cursor for find with projection",
                    system_utils::captureCallStack()));
            }

            // Wrap cursor in RAII handle immediately to prevent leaks if create() fails
            MongoCursorHandle cursorHandle(rawCursor);

            // Create the MongoDBCursor - it will take ownership via release()
            auto cursorResult = MongoDBCursor::create(std::nothrow, m_client, cursorHandle.release(), m_connection
#if DB_DRIVER_THREAD_SAFE
                , m_connMutex
#endif
            );
            if (!cursorResult.has_value())
            {
                return unexpected<DBException>(cursorResult.error());
            }
            auto cursor = cursorResult.value();

            // Register cursor with connection for cleanup tracking
            if (auto conn = m_connection.lock())
            {
                conn->registerCursor(cursor);
            }

            return std::shared_ptr<DocumentDBCursor>(cursor);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "B5C6D7E8F9A0",
                "Memory allocation failed in find",
                system_utils::captureCallStack()));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "C6D7E8F9A0B1",
                std::string("Unexpected error in find: ") + ex.what(),
                system_utils::captureCallStack()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "D7E8F9A0B1C2",
                "Unknown error in find",
                system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
