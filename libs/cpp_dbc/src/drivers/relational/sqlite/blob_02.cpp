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

 @file blob_02.cpp
 @brief SQLite database driver implementation - SQLiteBlob nothrow methods (factories, overrides, save)

*/

#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"

#if USE_SQLITE

namespace cpp_dbc::SQLite
{

    // ── Nothrow static factories ─────────────────────────────────────────────────
    // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.

    cpp_dbc::expected<std::shared_ptr<SQLiteBlob>, DBException> SQLiteBlob::create(std::nothrow_t, std::shared_ptr<sqlite3> db) noexcept
    {
        auto obj = std::make_shared<SQLiteBlob>(PrivateCtorTag{}, std::nothrow, db);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    cpp_dbc::expected<std::shared_ptr<SQLiteBlob>, DBException> SQLiteBlob::create(std::nothrow_t, std::shared_ptr<sqlite3> db,
                                                                                  const std::string &tableName, const std::string &columnName, const std::string &rowId) noexcept
    {
        auto obj = std::make_shared<SQLiteBlob>(PrivateCtorTag{}, std::nothrow, db, tableName, columnName, rowId);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    cpp_dbc::expected<std::shared_ptr<SQLiteBlob>, DBException> SQLiteBlob::create(std::nothrow_t, std::shared_ptr<sqlite3> db,
                                                                                  const std::vector<uint8_t> &initialData) noexcept
    {
        auto obj = std::make_shared<SQLiteBlob>(PrivateCtorTag{}, std::nothrow, db, initialData);
        if (obj->m_initFailed)
        {
            return cpp_dbc::unexpected(std::move(*obj->m_initError));
        }
        return obj;
    }

    // ── Nothrow public methods ───────────────────────────────────────────────────

    cpp_dbc::expected<void, DBException> SQLiteBlob::copyFrom(std::nothrow_t, const SQLiteBlob &other) noexcept
    {
        if (this == &other)
        {
            return {};
        }
        auto r = MemoryBlob::copyFrom(std::nothrow, other);
        if (!r.has_value())
        {
            return r;
        }
        m_conn = other.m_conn;
        m_tableName = other.m_tableName;
        m_columnName = other.m_columnName;
        m_rowId = other.m_rowId;
        m_loaded = other.m_loaded;
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteBlob::ensureLoaded(std::nothrow_t) const noexcept
    {
        if (m_loaded || m_tableName.empty() || m_columnName.empty() || m_rowId.empty())
        {
            return {};
        }
        auto tableResult = validateIdentifier(std::nothrow, m_tableName);
        if (!tableResult.has_value())
        {
            return cpp_dbc::unexpected(tableResult.error());
        }
        auto colResult = validateIdentifier(std::nothrow, m_columnName);
        if (!colResult.has_value())
        {
            return cpp_dbc::unexpected(colResult.error());
        }

        auto dbResult = getSQLiteConnection(std::nothrow);
        if (!dbResult.has_value())
        {
            return cpp_dbc::unexpected(dbResult.error());
        }
        sqlite3 *db = dbResult.value();

        std::string query = "SELECT " + m_columnName + " FROM " + m_tableName + " WHERE rowid = ?";
        sqlite3_stmt *stmt = nullptr;
        int result = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (result != SQLITE_OK)
        {
            return cpp_dbc::unexpected(DBException("BCOMBRIZ3NOO", "Failed to prepare statement for BLOB loading: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
        }
        result = sqlite3_bind_text(stmt, 1, m_rowId.c_str(), -1, SQLITE_STATIC);
        if (result != SQLITE_OK)
        {
            sqlite3_finalize(stmt);
            return cpp_dbc::unexpected(DBException("2F9K4N8V7QXW", "Failed to bind rowid parameter: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
        }
        result = sqlite3_step(stmt);
        if (result == SQLITE_ROW)
        {
            const void *blobData = sqlite3_column_blob(stmt, 0);
            int blobSize = sqlite3_column_bytes(stmt, 0);
            if (blobData && blobSize > 0)
            {
                m_data.resize(blobSize);
                std::memcpy(m_data.data(), blobData, blobSize);
            }
            else
            {
                m_data.clear();
            }
        }
        else
        {
            sqlite3_finalize(stmt);
            return cpp_dbc::unexpected(DBException("D281D99D6FAC", "Failed to fetch BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
        }
        sqlite3_finalize(stmt);
        m_loaded = true;
        return {};
    }

    // ── Nothrow Blob overrides ───────────────────────────────────────────────────

    cpp_dbc::expected<size_t, DBException> SQLiteBlob::length(std::nothrow_t) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::length(std::nothrow);
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> SQLiteBlob::getBytes(std::nothrow_t, size_t pos, size_t length) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::getBytes(std::nothrow, pos, length);
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> SQLiteBlob::getBinaryStream(std::nothrow_t) const noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::getBinaryStream(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<OutputStream>, DBException> SQLiteBlob::setBinaryStream(std::nothrow_t, size_t pos) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::setBinaryStream(std::nothrow, pos);
    }

    cpp_dbc::expected<void, DBException> SQLiteBlob::setBytes(std::nothrow_t, size_t pos, const std::vector<uint8_t> &bytes) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::setBytes(std::nothrow, pos, bytes);
    }

    cpp_dbc::expected<void, DBException> SQLiteBlob::setBytes(std::nothrow_t, size_t pos, const uint8_t *bytes, size_t length) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::setBytes(std::nothrow, pos, bytes, length);
    }

    cpp_dbc::expected<void, DBException> SQLiteBlob::truncate(std::nothrow_t, size_t len) noexcept
    {
        auto r = ensureLoaded(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        return MemoryBlob::truncate(std::nothrow, len);
    }

    cpp_dbc::expected<void, DBException> SQLiteBlob::save(std::nothrow_t) noexcept
    {
        if (m_tableName.empty() || m_columnName.empty() || m_rowId.empty())
        {
            return {}; // Nothing to save
        }

        // Ensure BLOB data is loaded before saving to prevent overwriting with empty data
        auto loadResult = ensureLoaded(std::nothrow);
        if (!loadResult.has_value())
        {
            return cpp_dbc::unexpected(loadResult.error());
        }

        // Validate identifiers to prevent SQL injection
        auto tableResult = validateIdentifier(std::nothrow, m_tableName);
        if (!tableResult.has_value())
        {
            return cpp_dbc::unexpected(tableResult.error());
        }
        auto colResult = validateIdentifier(std::nothrow, m_columnName);
        if (!colResult.has_value())
        {
            return cpp_dbc::unexpected(colResult.error());
        }

        // Get the SQLite connection safely
        auto dbResult = getSQLiteConnection(std::nothrow);
        if (!dbResult.has_value())
        {
            return cpp_dbc::unexpected(dbResult.error());
        }
        sqlite3 *db = dbResult.value();

        // Construct a query with parameterized rowid to prevent SQL injection
        std::string query = "UPDATE " + m_tableName + " SET " + m_columnName + " = ? WHERE rowid = ?";

        // Prepare the statement
        sqlite3_stmt *stmt = nullptr;
        int result = sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr);
        if (result != SQLITE_OK)
        {
            return cpp_dbc::unexpected(DBException("78BBDB81BED9", "Failed to prepare statement for BLOB saving: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
        }

        // Bind the BLOB data (parameter 1)
        result = sqlite3_bind_blob(stmt, 1, m_data.data(), static_cast<int>(m_data.size()), SQLITE_STATIC);
        if (result != SQLITE_OK)
        {
            sqlite3_finalize(stmt);
            return cpp_dbc::unexpected(DBException("ZIDTSWSKRVIY", "Failed to bind BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
        }

        // Bind the rowid parameter (parameter 2)
        result = sqlite3_bind_text(stmt, 2, m_rowId.c_str(), -1, SQLITE_STATIC);
        if (result != SQLITE_OK)
        {
            sqlite3_finalize(stmt);
            return cpp_dbc::unexpected(DBException("5L7M3P9K8TJV", "Failed to bind rowid parameter: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
        }

        // Execute the statement
        result = sqlite3_step(stmt);
        if (result != SQLITE_DONE)
        {
            sqlite3_finalize(stmt);
            return cpp_dbc::unexpected(DBException("LPJVG75T44E5", "Failed to save BLOB data: " + std::string(sqlite3_errmsg(db)), system_utils::captureCallStack()));
        }

        // Finalize the statement
        sqlite3_finalize(stmt);
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteBlob::free(std::nothrow_t) noexcept
    {
        auto r = MemoryBlob::free(std::nothrow);
        if (!r.has_value())
        {
            return cpp_dbc::unexpected(r.error());
        }
        m_loaded = false;
        return {};
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
