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

 @file result_set_02.cpp
 @brief SQLite database driver implementation - SQLiteDBResultSet nothrow methods (part 1)

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

    // Nothrow API
    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::next(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed)
            {
                return false;
            }

            int result = sqlite3_step(stmt);

            if (result == SQLITE_ROW)
            {
                m_rowPosition++;
                m_hasData = true;
                return true;
            }
            else if (result == SQLITE_DONE)
            {
                m_rowPosition++;
                m_hasData = false;
                return false;
            }
            else
            {
                // Error occurred
                return cpp_dbc::unexpected(DBException("SL1A2B3C4D5E", "Error stepping through SQLite result set: " + std::string(sqlite3_errmsg(sqlite3_db_handle(stmt))),
                                                       system_utils::captureCallStack()));
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SL2B3C4D5E6F",
                                                   std::string("next failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SL3C4D5E6F7G",
                                                   "next failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
    {
        try
        {
            return m_rowPosition == 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("IBFA1B2C3D4E",
                                                   std::string("isBeforeFirst failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("IBFA1B2C3D4F",
                                                   "isBeforeFirst failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        try
        {
            return m_rowPosition > 0 && !m_hasData;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("IALA1B2C3D4E",
                                                   std::string("isAfterLast failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("IALA1B2C3D4F",
                                                   "isAfterLast failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> SQLiteDBResultSet::getRow(std::nothrow_t) noexcept
    {
        try
        {
            return m_rowPosition;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GR1A1B2C3D4E",
                                                   std::string("getRow failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GR1A1B2C3D4F",
                                                   "getRow failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> SQLiteDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("0A7LU8DS9WF3", "Invalid column index or row position",
                                                       system_utils::captureCallStack()));
            }

            // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return 0; // Return 0 for NULL values (similar to JDBC)
            }

            return sqlite3_column_int(stmt, idx);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GI1A1B2C3D4E",
                                                   std::string("getInt failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GI1A1B2C3D4F",
                                                   "getInt failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int64_t, DBException> SQLiteDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("DDAABD02C9D3", "Invalid column index or row position",
                                                       system_utils::captureCallStack()));
            }

            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return 0;
            }

            return sqlite3_column_int64(stmt, idx);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GL1A1B2C3D4E",
                                                   std::string("getLong failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GL1A1B2C3D4F",
                                                   "getLong failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<double, DBException> SQLiteDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("SL4D5E6F7G8H", "Invalid column index or row position",
                                                       system_utils::captureCallStack()));
            }

            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return 0.0;
            }

            return sqlite3_column_double(stmt, idx);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GD1A1B2C3D4E",
                                                   std::string("getDouble failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GD1A1B2C3D4F",
                                                   "getDouble failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("1Y2Z3A4B5C6D", "Invalid column index or row position",
                                                       system_utils::captureCallStack()));
            }

            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return std::string("");
            }

            const char *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx));
            return text ? std::string(text) : std::string("");
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GS1A1B2C3D4E",
                                                   std::string("getString failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GS1A1B2C3D4F",
                                                   "getString failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("O1P2Q3R4S5T6", "Invalid column index or row position",
                                                       system_utils::captureCallStack()));
            }

            int idx = static_cast<int>(columnIndex - 1);

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return false;
            }

            return sqlite3_column_int(stmt, idx) != 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GB1A1B2C3D4E",
                                                   std::string("getBoolean failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GB1A1B2C3D4F",
                                                   "getBoolean failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("407EBCBBE843", "Invalid column index or row position",
                                                       system_utils::captureCallStack()));
            }

            int idx = static_cast<int>(columnIndex - 1);
            return sqlite3_column_type(stmt, idx) == SQLITE_NULL;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("IN1A1B2C3D4E",
                                                   std::string("isNull failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("IN1A1B2C3D4F",
                                                   "isNull failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getDate(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("C1OYCDLIMFUN", "Invalid column index or row position",
                                                       system_utils::captureCallStack()));
            }

            int idx = static_cast<int>(columnIndex - 1);
            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return std::string("");
            }

            const unsigned char *text = sqlite3_column_text(stmt, idx);
            return text ? std::string(reinterpret_cast<const char *>(text)) : std::string("");
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("MRUI7KG4HYBJ",
                                                   std::string("getDate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("QMSB6MQ38GAS",
                                                   "getDate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTimestamp(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("Y9AH4HKBJ7VN", "Invalid column index or row position",
                                                       system_utils::captureCallStack()));
            }

            int idx = static_cast<int>(columnIndex - 1);
            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return std::string("");
            }

            const unsigned char *text = sqlite3_column_text(stmt, idx);
            return text ? std::string(reinterpret_cast<const char *>(text)) : std::string("");
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RSLC8RAR7C9Y",
                                                   std::string("getTimestamp failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("Q8YNV0ZK7GXA",
                                                   "getTimestamp failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTime(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            sqlite3_stmt *stmt = getStmt();
            if (!stmt || m_closed || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("NJZT2YW7JKY5", "Invalid column index or row position",
                                                       system_utils::captureCallStack()));
            }

            int idx = static_cast<int>(columnIndex - 1);
            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return std::string("");
            }

            const unsigned char *text = sqlite3_column_text(stmt, idx);
            return text ? std::string(reinterpret_cast<const char *>(text)) : std::string("");
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("OT096KLND5RT",
                                                   std::string("getTime failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("8B0O08SH5WP2",
                                                   "getTime failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> SQLiteDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("SLIR8S9T0U1V", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getInt(std::nothrow, it->second + 1); // +1 because getInt(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GIN1A2B3C4D5",
                                                   std::string("getInt failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GIN1A2B3C4D6",
                                                   "getInt failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int64_t, DBException> SQLiteDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("20C1324B8D71", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getLong(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("GLN1A2B3C4D5",
                                                   std::string("getLong failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("GLN1A2B3C4D6",
                                                   "getLong failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
