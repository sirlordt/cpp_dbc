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
 * @brief MongoDB MongoDBCursor - Part 2 (nothrow versions)
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
    // MongoDBCursor NOTHROW VERSIONS
    // ====================================================================

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBCursor::current(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            if (!m_cursor)
            {
                return unexpected<DBException>(DBException(
                    "8F9A0B1C2D3E",
                    "Cursor is not initialized"));
            }

            if (!m_currentDoc)
            {
                return unexpected<DBException>(DBException(
                    "9A0B1C2D3E4F",
                    "No current document - call next() first"));
            }

            return std::static_pointer_cast<DocumentDBData>(m_currentDoc);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "0B1C2D3E4F5A",
                std::string("Error in current: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "1C2D3E4F5A6B",
                "Unknown error in current"));
        }
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> MongoDBCursor::nextDocument(std::nothrow_t) noexcept
    {
        try
        {
            if (!next())
            {
                return unexpected<DBException>(DBException(
                    "2D3E4F5A6B7C",
                    "No more documents in cursor"));
            }
            return std::static_pointer_cast<DocumentDBData>(m_currentDoc);
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "3E4F5A6B7C8D",
                std::string("Error in nextDocument: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "4F5A6B7C8D9E",
                "Unknown error in nextDocument"));
        }
    }

    expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> MongoDBCursor::toVector(std::nothrow_t) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "5A6B7C8D9E0F",
                    "Connection has been closed"));
            }

            if (!m_cursor)
            {
                return unexpected<DBException>(DBException(
                    "6B7C8D9E0F1A",
                    "Cursor is not initialized"));
            }

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
                return unexpected<DBException>(DBException(
                    "7C8D9E0F1A2B",
                    std::string("Cursor error: ") + error.message));
            }

            m_exhausted = true;
            m_currentDoc.reset();
            return result;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "8D9E0F1A2B3C",
                "Memory allocation failed in toVector"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "9E0F1A2B3C4D",
                std::string("Error in toVector: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "0F1A2B3C4D5E",
                "Unknown error in toVector"));
        }
    }

    expected<std::vector<std::shared_ptr<DocumentDBData>>, DBException> MongoDBCursor::getBatch(
        std::nothrow_t, size_t batchSize) noexcept
    {
        try
        {
            MONGODB_LOCK_GUARD(m_mutex);

            if (m_client.expired())
            {
                return unexpected<DBException>(DBException(
                    "1A2B3C4D5E6F",
                    "Connection has been closed"));
            }

            if (!m_cursor)
            {
                return unexpected<DBException>(DBException(
                    "2B3C4D5E6F7A",
                    "Cursor is not initialized"));
            }

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
                return unexpected<DBException>(DBException(
                    "3C4D5E6F7A8B",
                    std::string("Cursor error: ") + error.message));
            }

            if (count < batchSize)
                m_exhausted = true;

            return result;
        }
        catch (const DBException &ex)
        {
            return unexpected<DBException>(ex);
        }
        catch ([[maybe_unused]] const std::bad_alloc &ex)
        {
            return unexpected<DBException>(DBException(
                "4D5E6F7A8B9C",
                "Memory allocation failed in getBatch"));
        }
        catch (const std::exception &ex)
        {
            return unexpected<DBException>(DBException(
                "5E6F7A8B9C0D",
                std::string("Error in getBatch: ") + ex.what()));
        }
        catch (...)
        {
            return unexpected<DBException>(DBException(
                "6F7A8B9C0D1E",
                "Unknown error in getBatch"));
        }
    }

} // namespace cpp_dbc::MongoDB

#endif // USE_MONGODB
