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
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

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

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getDate(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("PBQCIWA2TJZ3", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getDate(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D3YH1LHQ1CBY",
                                                   std::string("getDate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("HBPSL6UPB6AH",
                                                   "getDate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTimestamp(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

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

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTimestamp(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("R09JSU56AXFJ", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getTimestamp(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("22EKF4Q293YB",
                                                   std::string("getTimestamp failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("WC33FJ13ZU13",
                                                   "getTimestamp failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTime(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

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

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getTime(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("F2901E52MFWC", "Column not found: " + columnName,
                                                       system_utils::captureCallStack()));
            }

            return getTime(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("UG5NGYTUB1LR",
                                                   std::string("getTime failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("AILUOP4YRQAW",
                                                   "getTime failed: unknown error",
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

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
