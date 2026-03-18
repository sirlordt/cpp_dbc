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

 @file result_set_04.cpp
 @brief SQLite database driver implementation - SQLiteDBResultSet nothrow methods (part 3)

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

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getDate(std::nothrow_t, size_t columnIndex) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("3ROHBN2PXN8R", "Result set is closed");

        sqlite3_stmt *stmt = getStmt(std::nothrow);
        if (!stmt || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
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

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getDate(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("PBQCIWA2TJZ3", "Column not found: " + columnName,
                                                   system_utils::captureCallStack()));
        }

        return getDate(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTimestamp(std::nothrow_t, size_t columnIndex) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("Q2HBW2D3MY8B", "Result set is closed");

        sqlite3_stmt *stmt = getStmt(std::nothrow);
        if (!stmt || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
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

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTimestamp(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("R09JSU56AXFJ", "Column not found: " + columnName,
                                                   system_utils::captureCallStack()));
        }

        return getTimestamp(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTime(std::nothrow_t, size_t columnIndex) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("TRI8ODVVF3GO", "Result set is closed");

        sqlite3_stmt *stmt = getStmt(std::nothrow);
        if (!stmt || !m_hasData || columnIndex < 1 || columnIndex > m_fieldCount)
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

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTime(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("F2901E52MFWC", "Column not found: " + columnName,
                                                   system_utils::captureCallStack()));
        }

        return getTime(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> SQLiteDBResultSet::getColumnNames(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("JMCBOO4MZQ8S", "Result set is closed");
        return m_columnNames;
    }

    cpp_dbc::expected<size_t, DBException> SQLiteDBResultSet::getColumnCount(std::nothrow_t) noexcept
    {
        SQLITE_STMT_LOCK_OR_RETURN("5MCT0MQX2LPX", "Result set is closed");
        return m_fieldCount;
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
