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
 @brief SQLite database driver implementation - SQLiteDBResultSet nothrow methods (part 2)

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

    cpp_dbc::expected<int64_t, DBException> SQLiteDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

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

    cpp_dbc::expected<int64_t, DBException> SQLiteDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("1WW27H1A2ORC", "Column not found: " + columnName,
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

    cpp_dbc::expected<double, DBException> SQLiteDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

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

    cpp_dbc::expected<double, DBException> SQLiteDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("SL5E6F7G8H9I", "Column not found: " + columnName,
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

    cpp_dbc::expected<std::string, DBException> SQLiteDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

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

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

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

    cpp_dbc::expected<bool, DBException> SQLiteDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            std::lock_guard<std::recursive_mutex> globalLock(*m_globalFileMutex);

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

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
