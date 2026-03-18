/*

 * Copyright 2025 Tomas R Moreno P <tomasr.morenop@gmail.com>. All Rights Reserved.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file blob_01.cpp
 @brief SQLite database driver implementation - SQLiteBlob (private helpers, throwing methods)

*/

#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"

#if USE_SQLITE

namespace cpp_dbc::SQLite
{

    // ── Private helpers ───────────────────────────────────────────────────────────

    cpp_dbc::expected<sqlite3 *, DBException> SQLiteBlob::getSQLiteConnection(std::nothrow_t) const noexcept
    {
        auto conn = m_db.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("8VHM5QBP014Y", "SQLite connection has been closed", system_utils::captureCallStack()));
        }
        return conn.get();
    }

    cpp_dbc::expected<void, DBException> SQLiteBlob::validateIdentifier(std::nothrow_t, const std::string &identifier) noexcept
    {
        if (identifier.empty())
        {
            return cpp_dbc::unexpected(DBException("SL0KQV7GOWPA", "Empty SQL identifier not allowed", system_utils::captureCallStack()));
        }
        for (char c : identifier)
        {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            {
                return cpp_dbc::unexpected(DBException("UJBY12IVMBRI",
                                                       "Invalid character in SQL identifier '" + identifier + "': only alphanumeric and underscore allowed",
                                                       system_utils::captureCallStack()));
            }
        }
        return {};
    }

    // ── Throwing API ──────────────────────────────────────────────────────────────
#ifdef __cpp_exceptions

    std::shared_ptr<SQLiteBlob> SQLiteBlob::create(std::shared_ptr<sqlite3> db)
    {
        auto r = create(std::nothrow, db);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<SQLiteBlob> SQLiteBlob::create(std::shared_ptr<sqlite3> db,
                                                   const std::string &tableName, const std::string &columnName, const std::string &rowId)
    {
        auto r = create(std::nothrow, db, tableName, columnName, rowId);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<SQLiteBlob> SQLiteBlob::create(std::shared_ptr<sqlite3> db,
                                                   const std::vector<uint8_t> &initialData)
    {
        auto r = create(std::nothrow, db, initialData);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    void SQLiteBlob::copyFrom(const SQLiteBlob &other)
    {
        auto r = copyFrom(std::nothrow, other);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void SQLiteBlob::ensureLoaded() const
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    size_t SQLiteBlob::length() const
    {
        auto r = length(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::vector<uint8_t> SQLiteBlob::getBytes(size_t pos, size_t length) const
    {
        auto r = getBytes(std::nothrow, pos, length);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<InputStream> SQLiteBlob::getBinaryStream() const
    {
        auto r = getBinaryStream(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    std::shared_ptr<OutputStream> SQLiteBlob::setBinaryStream(size_t pos)
    {
        auto r = setBinaryStream(std::nothrow, pos);
        if (!r.has_value())
        {
            throw r.error();
        }
        return r.value();
    }

    void SQLiteBlob::setBytes(size_t pos, const std::vector<uint8_t> &bytes)
    {
        auto r = setBytes(std::nothrow, pos, bytes);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void SQLiteBlob::setBytes(size_t pos, const uint8_t *bytes, size_t length)
    {
        auto r = setBytes(std::nothrow, pos, bytes, length);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void SQLiteBlob::truncate(size_t len)
    {
        auto r = truncate(std::nothrow, len);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void SQLiteBlob::save()
    {
        auto r = save(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

    void SQLiteBlob::free()
    {
        auto r = free(std::nothrow);
        if (!r.has_value())
        {
            throw r.error();
        }
    }

#endif // __cpp_exceptions

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
