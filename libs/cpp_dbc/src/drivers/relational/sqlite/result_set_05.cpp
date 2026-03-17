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

 @file result_set_05.cpp
 @brief SQLite database driver implementation - SQLiteDBResultSet nothrow methods (part 4 - blob/binary)

*/

#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <climits> // Para INT_MAX
#include <cstdlib> // Para getenv
#include <fstream> // Para std::ifstream
#include <charconv>
#include "sqlite_internal.hpp"

#if USE_SQLITE

namespace cpp_dbc::SQLite
{

    // BLOB support methods for SQLiteDBResultSet
    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> SQLiteDBResultSet::getBlob(std::nothrow_t, size_t columnIndex) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        sqlite3_stmt *stmt = getStmt();
        if (!stmt || m_closed.load(std::memory_order_seq_cst) || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("B1C2D3E4F5G6", "Invalid column index or row position for getBlob",
                                                   system_utils::captureCallStack()));
        }

        // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
        int idx = static_cast<int>(columnIndex - 1);

        if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
        {
            return std::shared_ptr<Blob>(std::make_shared<SQLite::SQLiteBlob>(std::shared_ptr<sqlite3>()));
        }

        // Check if the column is a BLOB type
        if (sqlite3_column_type(stmt, idx) != SQLITE_BLOB)
        {
            return cpp_dbc::unexpected(DBException("H7I8J9K0L1M2", "Column is not a BLOB type",
                                                   system_utils::captureCallStack()));
        }

        // Get the binary data
        const void *blobData = sqlite3_column_blob(stmt, idx);
        int blobSize = sqlite3_column_bytes(stmt, idx);

        // Create a vector with the data
        std::vector<uint8_t> data;
        if (blobData && blobSize > 0)
        {
            data.resize(blobSize);
            std::memcpy(data.data(), blobData, blobSize);
        }

        // Get a shared_ptr to the connection
        auto conn = m_connection.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("N3O4P5Q6R7S8", "Connection is no longer valid",
                                                   system_utils::captureCallStack()));
        }

        // Create a new BLOB object with the data (pass shared_ptr for safe reference)
        return std::shared_ptr<Blob>(std::make_shared<SQLite::SQLiteBlob>(conn->m_db, data));
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> SQLiteDBResultSet::getBlob(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("T9U0V1W2X3Y4", "Column not found: " + columnName,
                                                   system_utils::captureCallStack()));
        }

        return getBlob(std::nothrow, it->second + 1); // +1 because getBlob(int) is 1-based
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> SQLiteDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        sqlite3_stmt *stmt = getStmt();
        if (!stmt || m_closed.load(std::memory_order_seq_cst) || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("CEE30385E0BB", "Invalid column index or row position for getBinaryStream",
                                                   system_utils::captureCallStack()));
        }

        // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
        int idx = static_cast<int>(columnIndex - 1);

        if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
        {
            // Return an empty stream
            return std::shared_ptr<InputStream>(std::make_shared<SQLite::SQLiteInputStream>(nullptr, 0));
        }

        // Get the binary data
        const void *blobData = sqlite3_column_blob(stmt, idx);
        int blobSize = sqlite3_column_bytes(stmt, idx);

        // Create a new input stream with the data
        return std::shared_ptr<InputStream>(std::make_shared<SQLite::SQLiteInputStream>(blobData, blobSize));
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> SQLiteDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("F1G2H3I4J5K6", "Column not found: " + columnName,
                                                   system_utils::captureCallStack()));
        }

        return getBinaryStream(std::nothrow, it->second + 1); // +1 because getBinaryStream(int) is 1-based
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> SQLiteDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        sqlite3_stmt *stmt = getStmt();
        if (!stmt || m_closed.load(std::memory_order_seq_cst) || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("L7M8N9O0P1Q2", "Invalid column index or row position for getBytes",
                                                   system_utils::captureCallStack()));
        }

        // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
        int idx = static_cast<int>(columnIndex - 1);

        if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
        {
            return std::vector<uint8_t>{};
        }

        // Get the binary data
        const void *blobData = sqlite3_column_blob(stmt, idx);
        int blobSize = sqlite3_column_bytes(stmt, idx);

        // Create a vector with the data
        std::vector<uint8_t> data;
        if (blobData && blobSize > 0)
        {
            data.resize(blobSize);
            std::memcpy(data.data(), blobData, blobSize);
        }

        return data;
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> SQLiteDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("WDX9BB6U91FM", "Column not found: " + columnName,
                                                   system_utils::captureCallStack()));
        }

        return getBytes(std::nothrow, it->second + 1); // +1 because getBytes(int) is 1-based
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
