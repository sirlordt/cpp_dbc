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

 @file result_set_03.cpp
 @brief SQLite database driver implementation - SQLiteDBResultSet nothrow methods (part 2 - blob/binary)

*/

#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#include "cpp_dbc/drivers/relational/sqlite_blob.hpp"
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

    cpp_dbc::expected<double, DBException> SQLiteDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("5S6T7U8V9W0X", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getDouble(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GDN1A2B3C4D5",
                                                   std::string("getDouble failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GDN1A2B3C4D6",
                                                   "getDouble failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getString(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("93A82C42FA7B", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getString(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GSN1A2B3C4D5",
                                                   std::string("getString failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GSN1A2B3C4D6",
                                                   "getString failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::getBoolean(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto intResult = getInt(std::nothrow, columnName);
            if (!intResult)
            {
                return cpp_dbc::unexpected(intResult.error());
            }
            return *intResult != 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBN1A2B3C4D5",
                                                   std::string("getBoolean failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBN1A2B3C4D6",
                                                   "getBoolean failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isNull(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("8BAE4B58A947", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return isNull(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("INN1A2B3C4D5",
                                                   std::string("isNull failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("INN1A2B3C4D6",
                                                   "isNull failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> SQLiteDBResultSet::getColumnNames(std::nothrow_t) noexcept
    {
        try
        {
            return m_columnNames;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GCN1A2B3C4D5",
                                                   std::string("getColumnNames failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GCN1A2B3C4D6",
                                                   "getColumnNames failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<size_t, DBException> SQLiteDBResultSet::getColumnCount(std::nothrow_t) noexcept
    {
        try
        {
            return m_fieldCount;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GCC1A2B3C4D5",
                                                   std::string("getColumnCount failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GCC1A2B3C4D6",
                                                   "getColumnCount failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    // BLOB support methods for SQLiteDBResultSet
    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> SQLiteDBResultSet::getBlob(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBLO1A2B3C4D",
                                                   std::string("getBlob failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBLO1A2B3C4E",
                                                   "getBlob failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<Blob>, DBException> SQLiteDBResultSet::getBlob(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("T9U0V1W2X3Y4", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getBlob(std::nothrow, it->second + 1); // +1 because getBlob(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBLN1A2B3C4D",
                                                   std::string("getBlob failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBLN1A2B3C4E",
                                                   "getBlob failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> SQLiteDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBIS1A2B3C4D",
                                                   std::string("getBinaryStream failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBIS1A2B3C4E",
                                                   "getBinaryStream failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> SQLiteDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("F1G2H3I4J5K6", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getBinaryStream(std::nothrow, it->second + 1); // +1 because getBinaryStream(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBIN1A2B3C4D",
                                                   std::string("getBinaryStream failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBIN1A2B3C4E",
                                                   "getBinaryStream failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> SQLiteDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBYT1A2B3C4D",
                                                   std::string("getBytes failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBYT1A2B3C4E",
                                                   "getBytes failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<uint8_t>, DBException> SQLiteDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("R3S4T5U6V7W8", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getBytes(std::nothrow, it->second + 1); // +1 because getBytes(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GBYN1A2B3C4D",
                                                   std::string("getBytes failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GBYN1A2B3C4E",
                                                   "getBytes failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
