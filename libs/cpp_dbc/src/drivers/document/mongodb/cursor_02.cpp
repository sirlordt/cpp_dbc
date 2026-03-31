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
 * @file cursor_02.cpp
 * @brief MongoDB MongoDBCursor - Part 2 (nothrow implementations - REAL LOGIC)
 */

#include "cpp_dbc/drivers/document/driver_mongodb.hpp"

#if USE_MONGODB

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include "cpp_dbc/common/system_utils.hpp"
#include "mongodb_internal.hpp"

namespace cpp_dbc::MongoDB
{

    // ====================================================================
    // MongoDBCursor NOTHROW IMPLEMENTATIONS (REAL LOGIC)
    // ====================================================================

    expected<void, DBException> MongoDBCursor::close(std::nothrow_t) noexcept
    {
        MONGODB_DEBUG("MongoDBCursor::close - Closing cursor");
        MONGODB_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();
        m_cursor.reset();
        m_currentDoc.reset();
        m_exhausted = true;
        m_closed.store(true, std::memory_order_seq_cst);
        MONGODB_DEBUG("MongoDBCursor::close - Done");
        return {};
    }

    expected<bool, DBException> MongoDBCursor::isEmpty(std::nothrow_t) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("B07GGHUWADCZ", "Cursor connection closed");
        {
            auto r = validateCursor(std::nothrow);
            if (!r.has_value())
            {
                return unexpected<DBException>(r.error());
            }
        }
        if (!m_iterationStarted)
        {
            if (m_exhausted)
            {
                return true;
            }
            return !mongoc_cursor_more(m_cursor.get());
        }
        return m_exhausted && !m_currentDoc;
    }

    expected<bool, DBException> MongoDBCursor::next(std::nothrow_t) noexcept
    {
        MONGODB_DEBUG("MongoDBCursor::next(nothrow) - Moving to next document, position: %lu", static_cast<unsigned long>(m_position));
        MONGODB_STMT_LOCK_OR_RETURN("I6UDO6T9PSS5", "Cursor connection closed");

        {
            auto r = validateCursor(std::nothrow);
            if (!r.has_value())
            {
                return unexpected<DBException>(r.error());
            }
        }

        m_iterationStarted = true;

        // If we have a peeked document from hasNext(), use it
        if (m_peekedDoc)
        {
            m_currentDoc = std::move(m_peekedDoc);
            m_peekedDoc.reset();
            m_position++;
            MONGODB_DEBUG("MongoDBCursor::next - Using peeked document at position: %lu", static_cast<unsigned long>(m_position));
            return true;
        }

        const bson_t *doc = nullptr;
        if (mongoc_cursor_next(m_cursor.get(), &doc))
        {
            bson_t *docCopy = bson_copy(doc);
            if (!docCopy)
            {
                return unexpected<DBException>(DBException(
                    "A3B9C8D7E2F1",
                    "Failed to copy document from cursor",
                    system_utils::captureCallStack()));
            }
            auto docResult = MongoDBDocument::create(std::nothrow, docCopy);
            if (!docResult.has_value())
            {
                return unexpected<DBException>(docResult.error());
            }
            m_currentDoc = docResult.value();
            m_position++;
            MONGODB_DEBUG("MongoDBCursor::next - Found document at position: %lu", static_cast<unsigned long>(m_position));
            return true;
        }

        bson_error_t error;
        if (mongoc_cursor_error(m_cursor.get(), &error))
        {
            return unexpected<DBException>(DBException(
                "B4C0D9E8F3A2",
                std::string("Cursor error: ") + error.message,
                system_utils::captureCallStack()));
        }

        MONGODB_DEBUG("MongoDBCursor::next - Cursor exhausted");
        m_exhausted = true;
        m_currentDoc.reset();
        return false;
    }

    expected<bool, DBException> MongoDBCursor::hasNext(std::nothrow_t) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("DGEVA6GF4KP8", "Cursor connection closed");

        {
            auto r = validateCursor(std::nothrow);
            if (!r.has_value())
            {
                return unexpected<DBException>(r.error());
            }
        }

        if (m_exhausted)
        {
            return false;
        }

        // If we already have a peeked document, we know there's a next one
        if (m_peekedDoc)
        {
            return true;
        }

        // 2026-03-22T00:00:00Z
        // Bug: mongoc_cursor_more() may report remaining data unreliably for certain
        // cursor types, causing hasNext() to return true when no documents remain.
        // Solution: Peek one BSON document ahead with mongoc_cursor_next() and store
        // it in m_peekedDoc so hasNext() reflects the real iteration state.
        const bson_t *doc = nullptr;
        if (mongoc_cursor_next(m_cursor.get(), &doc))
        {
            bson_t *docCopy = bson_copy(doc);
            if (!docCopy)
            {
                return unexpected<DBException>(DBException(
                    "38F1FRMH5NXW",
                    "Failed to copy peeked document",
                    system_utils::captureCallStack()));
            }
            auto docResult = MongoDBDocument::create(std::nothrow, docCopy);
            if (!docResult.has_value())
            {
                return unexpected<DBException>(docResult.error());
            }
            m_peekedDoc = docResult.value();
            MONGODB_DEBUG("MongoDBCursor::hasNext - Peeked next document, has more: true");
            return true;
        }

        // Check for cursor errors
        bson_error_t error;
        if (mongoc_cursor_error(m_cursor.get(), &error))
        {
            return unexpected<DBException>(DBException(
                "UE875SSEXTXD",
                std::string("Cursor error during hasNext: ") + error.message,
                system_utils::captureCallStack()));
        }

        // No more documents
        MONGODB_DEBUG("MongoDBCursor::hasNext - No more documents");
        m_exhausted = true;
        return false;
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBCursor::current(std::nothrow_t) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("KWMGJGO7ANSM", "Cursor connection closed");

        {
            auto r = validateCursor(std::nothrow);
            if (!r.has_value())
            {
                return unexpected<DBException>(r.error());
            }
        }

        if (!m_currentDoc)
        {
            return unexpected<DBException>(DBException(
                "C5D1E0F9A4B3",
                "No current document - call next() first",
                system_utils::captureCallStack()));
        }

        return std::static_pointer_cast<DocumentDBData>(m_currentDoc);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBCursor::nextDocument(std::nothrow_t) noexcept
    {
        auto r = next(std::nothrow);
        if (!r.has_value())
        {
            return unexpected<DBException>(r.error());
        }
        if (!*r)
        {
            return unexpected<DBException>(DBException(
                "GW2IOCQLTF69",
                "No more documents in cursor",
                system_utils::captureCallStack()));
        }
        return std::static_pointer_cast<DocumentDBData>(m_currentDoc);
    }

    expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> MongoDBCursor::toVector(std::nothrow_t) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("9VB6PR9ZO1B8", "Cursor connection closed");

        {
            auto r = validateCursor(std::nothrow);
            if (!r.has_value())
            {
                return unexpected<DBException>(r.error());
            }
        }

        std::vector<std::shared_ptr<DocumentDBData>> result;

        // Consume any document buffered by a preceding hasNext() call
        if (m_peekedDoc)
        {
            result.push_back(std::static_pointer_cast<DocumentDBData>(m_peekedDoc));
            m_peekedDoc.reset();
        }

        const bson_t *doc = nullptr;
        while (mongoc_cursor_next(m_cursor.get(), &doc))
        {
            bson_t *docCopy = bson_copy(doc);
            if (docCopy)
            {
                auto docResult = MongoDBDocument::create(std::nothrow, docCopy);
                if (docResult.has_value())
                {
                    result.push_back(docResult.value());
                }
            }
        }

        bson_error_t error;
        if (mongoc_cursor_error(m_cursor.get(), &error))
        {
            return unexpected<DBException>(DBException(
                "E7F3A2B1C6D5",
                std::string("Cursor error: ") + error.message,
                system_utils::captureCallStack()));
        }

        m_exhausted = true;
        m_currentDoc.reset();
        return result;
    }

    expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> MongoDBCursor::getBatch(
        std::nothrow_t, size_t batchSize) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("UQ1QDYJF8MWW", "Cursor connection closed");

        {
            auto r = validateCursor(std::nothrow);
            if (!r.has_value())
            {
                return unexpected<DBException>(r.error());
            }
        }

        std::vector<std::shared_ptr<DocumentDBData>> result;
        result.reserve(batchSize);

        size_t count = 0;

        // Consume any document buffered by a preceding hasNext() call
        if (m_peekedDoc && count < batchSize)
        {
            result.push_back(std::static_pointer_cast<DocumentDBData>(m_peekedDoc));
            m_peekedDoc.reset();
            count++;
            m_position++;
        }

        const bson_t *doc = nullptr;
        while (count < batchSize && mongoc_cursor_next(m_cursor.get(), &doc))
        {
            bson_t *docCopy = bson_copy(doc);
            if (docCopy)
            {
                auto docResult = MongoDBDocument::create(std::nothrow, docCopy);
                if (docResult.has_value())
                {
                    result.push_back(docResult.value());
                    count++;
                    m_position++;
                }
            }
        }

        bson_error_t error;
        if (mongoc_cursor_error(m_cursor.get(), &error))
        {
            return unexpected<DBException>(DBException(
                "F8A4B3C2D7E6",
                std::string("Cursor error: ") + error.message,
                system_utils::captureCallStack()));
        }

        if (count < batchSize)
        {
            m_exhausted = true;
        }

        return result;
    }

    expected<int64_t, DBException> MongoDBCursor::count(std::nothrow_t) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("648YVC6RBL4G", "Cursor connection closed");
        return int64_t{-1};
    }

    expected<uint64_t, DBException> MongoDBCursor::getPosition(std::nothrow_t) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("XI67DVJTD8LB", "Cursor connection closed");
        return m_position;
    }

    expected<std::reference_wrapper<DocumentDBCursor>, DBException> MongoDBCursor::skip(
        std::nothrow_t, uint64_t n) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("EMKWQOS1T3YP", "Cursor connection closed");
        if (m_iterationStarted)
        {
            return unexpected<DBException>(DBException(
                "A9B5C4D3E8F7",
                "Cannot modify cursor after iteration has begun",
                system_utils::captureCallStack()));
        }
        m_skipCount = n;
        return std::ref(static_cast<DocumentDBCursor &>(*this));
    }

    expected<std::reference_wrapper<DocumentDBCursor>, DBException> MongoDBCursor::limit(
        std::nothrow_t, uint64_t n) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("R9Q75XOWE3AL", "Cursor connection closed");
        if (m_iterationStarted)
        {
            return unexpected<DBException>(DBException(
                "B0C6D5E4F9A8",
                "Cannot modify cursor after iteration has begun",
                system_utils::captureCallStack()));
        }
        m_limitCount = n;
        return std::ref(static_cast<DocumentDBCursor &>(*this));
    }

    expected<std::reference_wrapper<DocumentDBCursor>, DBException> MongoDBCursor::sort(
        std::nothrow_t, const std::string &fieldPath, bool ascending) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("T2NQJPQ2JFE3", "Cursor connection closed");
        if (m_iterationStarted)
        {
            return unexpected<DBException>(DBException(
                "C1D7E6F5A0B9",
                "Cannot modify cursor after iteration has begun",
                system_utils::captureCallStack()));
        }
        std::string sortJson = "{\"" + fieldPath + "\": " + (ascending ? "1" : "-1") + "}";
        m_sortSpec = std::move(sortJson);
        return std::ref(static_cast<DocumentDBCursor &>(*this));
    }

    expected<bool, DBException> MongoDBCursor::isExhausted(std::nothrow_t) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("D2IRWX17YG56", "Cursor connection closed");
        return m_exhausted;
    }

    expected<void, DBException> MongoDBCursor::rewind(std::nothrow_t) noexcept
    {
        MONGODB_STMT_LOCK_OR_RETURN("710KRZLKFILT", "Cursor connection closed");
        // MongoDB cursors do not support rewinding - always return an error
        return unexpected<DBException>(DBException(
            "D2E8F7A6B1C0",
            "MongoDB cursors do not support rewinding",
            system_utils::captureCallStack()));
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
