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
 * @file cursor_01.cpp
 * @brief MongoDB MongoDBCursor - Part 1 (private helpers, constructor, destructor, basic methods)
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
    // MongoDBCursor Implementation - Private Helpers
    // ============================================================================

    expected<void, DBException> MongoDBCursor::validateConnection(std::nothrow_t) const noexcept
    {
        if (m_client.expired())
        {
            return unexpected(DBException("D0E6F5A4B9C8", "MongoDB connection has been closed", system_utils::captureCallStack()));
        }
        return {};
    }

    expected<void, DBException> MongoDBCursor::validateCursor(std::nothrow_t) const noexcept
    {
        if (!m_cursor)
        {
            return unexpected(DBException("E1F7A6B5C0D9", "Cursor is not initialized or has been closed", system_utils::captureCallStack()));
        }
        return {};
    }

    expected<mongoc_client_t *, DBException> MongoDBCursor::getClient(std::nothrow_t) const noexcept
    {
        auto client = m_client.lock();
        if (!client)
        {
            return unexpected(DBException("F2A8B7C6D1E0", "MongoDB connection has been closed", system_utils::captureCallStack()));
        }
        return client.get();
    }

    // ============================================================================
    // MongoDBCursor Implementation - Constructor, Destructor, Move
    // ============================================================================

    MongoDBCursor::~MongoDBCursor()
    {
        MONGODB_DEBUG("MongoDBCursor::destructor - Destroying cursor");
        // Note: No need to unregister - weak_ptr expires automatically
        // and is cleaned up during registerCursor or closeAllCursors
        MONGODB_DEBUG("MongoDBCursor::destructor - Done");
    }

    // Nothrow constructors
    // Public for std::make_shared access, but effectively private via PrivateCtorTag.

#if DB_DRIVER_THREAD_SAFE
    MongoDBCursor::MongoDBCursor(MongoDBCursor::PrivateCtorTag,
                                 std::nothrow_t,
                                 std::weak_ptr<mongoc_client_t> client,
                                 mongoc_cursor_t *cursor,
                                 std::weak_ptr<MongoDBConnection> connection,
                                 SharedConnMutex connMutex) noexcept
        : m_client(std::move(client)),
          m_connection(std::move(connection)),
          m_cursor(cursor),
          m_connMutex(std::move(connMutex))
    {
        MONGODB_DEBUG("MongoDBCursor::constructor(nothrow) - Creating cursor");
        if (!m_cursor)
        {
            m_initFailed = true;
            m_initError = DBException("C9D5E4F3A7B8", "Cannot create cursor from null pointer", system_utils::captureCallStack());
            return;
        }
        // Note: Cursor registration is done by MongoDBCollection after make_shared
        // to enable weak_from_this() usage
        MONGODB_DEBUG("MongoDBCursor::constructor(nothrow) - Done");
    }
#else
    MongoDBCursor::MongoDBCursor(MongoDBCursor::PrivateCtorTag,
                                 std::nothrow_t,
                                 std::weak_ptr<mongoc_client_t> client,
                                 mongoc_cursor_t *cursor,
                                 std::weak_ptr<MongoDBConnection> connection) noexcept
        : m_client(std::move(client)),
          m_connection(std::move(connection)),
          m_cursor(cursor)
    {
        MONGODB_DEBUG("MongoDBCursor::constructor(nothrow) - Creating cursor");
        if (!m_cursor)
        {
            m_initFailed = true;
            m_initError = DBException("C9D5E4F3A7B8", "Cannot create cursor from null pointer", system_utils::captureCallStack());
            return;
        }
        // Note: Cursor registration is done by MongoDBCollection after make_shared
        // to enable weak_from_this() usage
        MONGODB_DEBUG("MongoDBCursor::constructor(nothrow) - Done");
    }
#endif

#ifdef __cpp_exceptions
    // Throwing wrappers (same order as in cursor.hpp)

    // ============================================================================
    // MongoDBCursor Implementation - DocumentDBCursor Interface (throwing wrappers)
    // ============================================================================

    void MongoDBCursor::close()
    {
        auto r = close(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    bool MongoDBCursor::isEmpty()
    {
        auto r = isEmpty(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    bool MongoDBCursor::next()
    {
        auto r = next(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    bool MongoDBCursor::hasNext()
    {
        auto r = hasNext(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::shared_ptr<DocumentDBData> MongoDBCursor::current()
    {
        auto r = current(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::shared_ptr<DocumentDBData> MongoDBCursor::nextDocument()
    {
        auto r = nextDocument(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::vector<std::shared_ptr<DocumentDBData>> MongoDBCursor::toVector()
    {
        auto r = toVector(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    std::vector<std::shared_ptr<DocumentDBData>> MongoDBCursor::getBatch(size_t batchSize)
    {
        auto r = getBatch(std::nothrow, batchSize);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    int64_t MongoDBCursor::count()
    {
        auto r = count(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    uint64_t MongoDBCursor::getPosition()
    {
        auto r = getPosition(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    DocumentDBCursor &MongoDBCursor::skip(uint64_t n)
    {
        auto r = skip(std::nothrow, n);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r->get();
    }

    DocumentDBCursor &MongoDBCursor::limit(uint64_t n)
    {
        auto r = limit(std::nothrow, n);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r->get();
    }

    DocumentDBCursor &MongoDBCursor::sort(const std::string &fieldPath, bool ascending)
    {
        auto r = sort(std::nothrow, fieldPath, ascending);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r->get();
    }

    bool MongoDBCursor::isExhausted()
    {
        auto r = isExhausted(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return *r;
    }

    [[noreturn]] void MongoDBCursor::rewind()
    {
        // rewind(std::nothrow) always returns unexpected for MongoDB cursors
        auto r = rewind(std::nothrow);
        throw r.error();
    }

    // ============================================================================
    // MongoDBCursor Implementation - MongoDB-specific methods
    // ============================================================================

    bool MongoDBCursor::isConnectionValid() const noexcept
    {
        return !m_client.expired();
    }

    std::string MongoDBCursor::getError() const
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        if (!m_cursor)
        {
            return "";
        }
        bson_error_t error;
        if (mongoc_cursor_error(m_cursor.get(), &error))
        {
            return error.message;
        }
        return "";
    }
#endif // __cpp_exceptions

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
