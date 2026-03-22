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
    // NOTHROW API - find (implementations)
    // ====================================================================

    expected<std::shared_ptr<DocumentDBCursor>, DBException> MongoDBCollection::find(
        std::nothrow_t,
        const std::string &filter) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::find(nothrow) - Finding in: %s", m_name.c_str());
        MONGODB_STMT_LOCK_OR_RETURN("30DM98JEZP3V", "Connection closed");

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
        auto cursorResult = MongoDBCursor::create(std::nothrow, cursorHandle.release(), m_connection);
        if (!cursorResult.has_value())
        {
            return unexpected<DBException>(cursorResult.error());
        }
        auto cursor = cursorResult.value();

        // Register cursor with connection for cleanup tracking
        if (auto conn = m_connection.lock())
        {
            conn->registerCursor(std::nothrow, cursor);
        }

        return std::shared_ptr<DocumentDBCursor>(cursor);
    }

    expected<std::shared_ptr<DocumentDBCursor>, DBException> MongoDBCollection::find(
        std::nothrow_t,
        const std::string &filter,
        const std::string &projection) noexcept
    {
        MONGODB_DEBUG("MongoDBCollection::find(nothrow) with projection - Finding in: %s", m_name.c_str());
        MONGODB_STMT_LOCK_OR_RETURN("G5WTH2GFNGLV", "Connection closed");

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
        auto cursorResult = MongoDBCursor::create(std::nothrow, cursorHandle.release(), m_connection);
        if (!cursorResult.has_value())
        {
            return unexpected<DBException>(cursorResult.error());
        }
        auto cursor = cursorResult.value();

        // Register cursor with connection for cleanup tracking
        if (auto conn = m_connection.lock())
        {
            conn->registerCursor(std::nothrow, cursor);
        }

        return std::shared_ptr<DocumentDBCursor>(cursor);
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
