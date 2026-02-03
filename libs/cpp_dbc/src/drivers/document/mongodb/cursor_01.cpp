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

    void MongoDBCursor::validateConnection() const
    {
        if (m_client.expired())
        {
            throw DBException("D0E6F5A4B9C8", "MongoDB connection has been closed", system_utils::captureCallStack());
        }
    }

    void MongoDBCursor::validateCursor() const
    {
        if (!m_cursor)
        {
            throw DBException("E1F7A6B5C0D9", "Cursor is not initialized or has been closed", system_utils::captureCallStack());
        }
    }

    mongoc_client_t *MongoDBCursor::getClient() const
    {
        auto client = m_client.lock();
        if (!client)
        {
            throw DBException("F2A8B7C6D1E0", "MongoDB connection has been closed", system_utils::captureCallStack());
        }
        return client.get();
    }

    // ============================================================================
    // MongoDBCursor Implementation - Constructor, Destructor, Move
    // ============================================================================

#if DB_DRIVER_THREAD_SAFE
    MongoDBCursor::MongoDBCursor(std::weak_ptr<mongoc_client_t> client, mongoc_cursor_t *cursor,
                                  std::weak_ptr<MongoDBConnection> connection, SharedConnMutex connMutex)
        : m_client(std::move(client)),
          m_connection(std::move(connection)),
          m_cursor(cursor),
          m_connMutex(std::move(connMutex))
    {
        MONGODB_DEBUG("MongoDBCursor::constructor - Creating cursor");
        if (!m_cursor)
        {
            throw DBException("C9D5E4F3A7B8", "Cannot create cursor from null pointer", system_utils::captureCallStack());
        }

        // Register with connection for cleanup tracking
        if (auto conn = m_connection.lock())
        {
            conn->registerCursor(this);
        }

        MONGODB_DEBUG("MongoDBCursor::constructor - Done");
    }
#else
    MongoDBCursor::MongoDBCursor(std::weak_ptr<mongoc_client_t> client, mongoc_cursor_t *cursor,
                                  std::weak_ptr<MongoDBConnection> connection)
        : m_client(std::move(client)),
          m_connection(std::move(connection)),
          m_cursor(cursor)
    {
        MONGODB_DEBUG("MongoDBCursor::constructor - Creating cursor");
        if (!m_cursor)
        {
            throw DBException("C9D5E4F3A7B8", "Cannot create cursor from null pointer", system_utils::captureCallStack());
        }

        // Register with connection for cleanup tracking
        if (auto conn = m_connection.lock())
        {
            conn->registerCursor(this);
        }

        MONGODB_DEBUG("MongoDBCursor::constructor - Done");
    }
#endif

    MongoDBCursor::~MongoDBCursor()
    {
        MONGODB_DEBUG("MongoDBCursor::destructor - Destroying cursor");

        // Unregister from connection
        if (auto conn = m_connection.lock())
        {
            conn->unregisterCursor(this);
        }

        MONGODB_DEBUG("MongoDBCursor::destructor - Done");
    }

    MongoDBCursor::MongoDBCursor(MongoDBCursor &&other) noexcept
        : m_client(std::move(other.m_client)),
          m_connection(std::move(other.m_connection)),
          m_cursor(std::move(other.m_cursor)),
          m_currentDoc(std::move(other.m_currentDoc)),
          m_position(other.m_position),
          m_iterationStarted(other.m_iterationStarted),
          m_exhausted(other.m_exhausted)
#if DB_DRIVER_THREAD_SAFE
          , m_connMutex(std::move(other.m_connMutex))
#endif
    {
        // Update connection registration: unregister moved-from, register moved-to
        if (auto conn = m_connection.lock())
        {
            conn->unregisterCursor(&other);
            conn->registerCursor(this);
        }
        other.m_connection.reset();
        other.m_exhausted = true;
    }

    MongoDBCursor &MongoDBCursor::operator=(MongoDBCursor &&other) noexcept
    {
        if (this != &other)
        {
            // Unregister from old connection
            if (auto conn = m_connection.lock())
            {
                conn->unregisterCursor(this);
            }

            m_client = std::move(other.m_client);
            m_connection = std::move(other.m_connection);
            m_cursor = std::move(other.m_cursor);
            m_currentDoc = std::move(other.m_currentDoc);
            m_position = other.m_position;
            m_iterationStarted = other.m_iterationStarted;
            m_exhausted = other.m_exhausted;
#if DB_DRIVER_THREAD_SAFE
            m_connMutex = std::move(other.m_connMutex);
#endif

            // Update connection registration: unregister moved-from, register moved-to
            if (auto conn = m_connection.lock())
            {
                conn->unregisterCursor(&other);
                conn->registerCursor(this);
            }

            other.m_connection.reset();
            other.m_exhausted = true;
        }
        return *this;
    }

    // ============================================================================
    // MongoDBCursor Implementation - DocumentDBCursor Interface
    // ============================================================================

    void MongoDBCursor::close()
    {
        MONGODB_DEBUG("MongoDBCursor::close - Closing cursor");
        MONGODB_LOCK_GUARD(*m_connMutex);
        m_cursor.reset();
        m_currentDoc.reset();
        m_exhausted = true;
        MONGODB_DEBUG("MongoDBCursor::close - Done");
    }

    bool MongoDBCursor::isEmpty()
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateCursor();
        if (!m_iterationStarted)
        {
            // Inline hasNext() logic to avoid self-deadlock (mutex is non-recursive)
            validateConnection();
            if (m_exhausted)
                return true;
            return !mongoc_cursor_more(m_cursor.get());
        }
        return m_exhausted && !m_currentDoc;
    }

    bool MongoDBCursor::next()
    {
        MONGODB_DEBUG("MongoDBCursor::next - Moving to next document, position: " << m_position);
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();
        validateCursor();

        m_iterationStarted = true;

        const bson_t *doc = nullptr;
        if (mongoc_cursor_next(m_cursor.get(), &doc))
        {
            bson_t *docCopy = bson_copy(doc);
            if (!docCopy)
            {
                throw DBException("A3B9C8D7E2F1", "Failed to copy document from cursor", system_utils::captureCallStack());
            }
            m_currentDoc = std::make_shared<MongoDBDocument>(docCopy);
            m_position++;
            MONGODB_DEBUG("MongoDBCursor::next - Found document at position: " << m_position);
            return true;
        }

        bson_error_t error;
        if (mongoc_cursor_error(m_cursor.get(), &error))
        {
            throw DBException("B4C0D9E8F3A2", std::string("Cursor error: ") + error.message, system_utils::captureCallStack());
        }

        MONGODB_DEBUG("MongoDBCursor::next - Cursor exhausted");
        m_exhausted = true;
        m_currentDoc.reset();
        return false;
    }

    bool MongoDBCursor::hasNext()
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();
        validateCursor();
        if (m_exhausted)
            return false;
        return mongoc_cursor_more(m_cursor.get());
    }

    std::shared_ptr<DocumentDBData> MongoDBCursor::current()
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateCursor();
        if (!m_currentDoc)
        {
            throw DBException("C5D1E0F9A4B3", "No current document - call next() first", system_utils::captureCallStack());
        }
        return m_currentDoc;
    }

    std::shared_ptr<DocumentDBData> MongoDBCursor::nextDocument()
    {
        if (!next())
        {
            throw DBException("D6E2F1A0B5C4", "No more documents in cursor", system_utils::captureCallStack());
        }
        return m_currentDoc;
    }

    std::vector<std::shared_ptr<DocumentDBData>> MongoDBCursor::toVector()
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();
        validateCursor();

        std::vector<std::shared_ptr<DocumentDBData>> result;
        const bson_t *doc = nullptr;
        while (mongoc_cursor_next(m_cursor.get(), &doc))
        {
            bson_t *docCopy = bson_copy(doc);
            if (docCopy)
            {
                result.push_back(std::make_shared<MongoDBDocument>(docCopy));
            }
        }

        bson_error_t error;
        if (mongoc_cursor_error(m_cursor.get(), &error))
        {
            throw DBException("E7F3A2B1C6D5", std::string("Cursor error: ") + error.message, system_utils::captureCallStack());
        }

        m_exhausted = true;
        m_currentDoc.reset();
        return result;
    }

    std::vector<std::shared_ptr<DocumentDBData>> MongoDBCursor::getBatch(size_t batchSize)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        validateConnection();
        validateCursor();

        std::vector<std::shared_ptr<DocumentDBData>> result;
        result.reserve(batchSize);

        const bson_t *doc = nullptr;
        size_t count = 0;

        while (count < batchSize && mongoc_cursor_next(m_cursor.get(), &doc))
        {
            bson_t *docCopy = bson_copy(doc);
            if (docCopy)
            {
                result.push_back(std::make_shared<MongoDBDocument>(docCopy));
                count++;
                m_position++;
            }
        }

        bson_error_t error;
        if (mongoc_cursor_error(m_cursor.get(), &error))
        {
            throw DBException("F8A4B3C2D7E6", std::string("Cursor error: ") + error.message, system_utils::captureCallStack());
        }

        if (count < batchSize)
            m_exhausted = true;
        return result;
    }

    int64_t MongoDBCursor::count() { return -1; }
    uint64_t MongoDBCursor::getPosition() { return m_position; }

    DocumentDBCursor &MongoDBCursor::skip(uint64_t n)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        if (m_iterationStarted)
        {
            throw DBException("A9B5C4D3E8F7", "Cannot modify cursor after iteration has begun", system_utils::captureCallStack());
        }
        m_skipCount = n;
        return *this;
    }

    DocumentDBCursor &MongoDBCursor::limit(uint64_t n)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        if (m_iterationStarted)
        {
            throw DBException("B0C6D5E4F9A8", "Cannot modify cursor after iteration has begun", system_utils::captureCallStack());
        }
        m_limitCount = n;
        return *this;
    }

    DocumentDBCursor &MongoDBCursor::sort(const std::string &fieldPath, bool ascending)
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        if (m_iterationStarted)
        {
            throw DBException("C1D7E6F5A0B9", "Cannot modify cursor after iteration has begun", system_utils::captureCallStack());
        }
        std::ostringstream oss;
        oss << "{\"" << fieldPath << "\": " << (ascending ? "1" : "-1") << "}";
        m_sortSpec = oss.str();
        return *this;
    }

    bool MongoDBCursor::isExhausted() { return m_exhausted; }

    [[noreturn]] void MongoDBCursor::rewind()
    {
        throw DBException("D2E8F7A6B1C0", "MongoDB cursors do not support rewinding", system_utils::captureCallStack());
    }

    bool MongoDBCursor::isConnectionValid() const { return !m_client.expired(); }

    std::string MongoDBCursor::getError() const
    {
        MONGODB_LOCK_GUARD(*m_connMutex);
        if (!m_cursor)
            return "";
        bson_error_t error;
        if (mongoc_cursor_error(m_cursor.get(), &error))
        {
            return error.message;
        }
        return "";
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
