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
 @brief MySQL database driver implementation - MySQLDBResultSet nothrow methods (part 2)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

namespace cpp_dbc::MySQL
{

    cpp_dbc::expected<std::string, DBException> MySQLDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            auto validateResult = validateCurrentRow(std::nothrow);
            if (!validateResult.has_value())
            {
                return cpp_dbc::unexpected(validateResult.error());
            }

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("Q38QHCPB7C0M", "Invalid column index", system_utils::captureCallStack()));
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return std::string("");
            }

            return std::string(m_currentRow[idx]);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C5D1E7F3A0B8",
                                                   std::string("getString failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C5D1E7F3A0B9",
                                                   "getString failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBResultSet::getString(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("TQ2STI8SS92T", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getString(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F2A8B4C0D7EA",
                                                   std::string("getString failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F2A8B4C0D7EB",
                                                   "getString failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            auto validateResult = validateCurrentRow(std::nothrow);
            if (!validateResult.has_value())
            {
                return cpp_dbc::unexpected(validateResult.error());
            }

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("V3W4X5Y6Z7A8", "Invalid column index", system_utils::captureCallStack()));
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return false;
            }

            auto value = std::string(m_currentRow[idx]);
            return (value == "1" || value == "true" || value == "TRUE" || value == "True");
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D6E2F8A4B1C9",
                                                   std::string("getBoolean failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D6E2F8A4B1CA",
                                                   "getBoolean failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::getBoolean(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("4B61K1QJ51HG", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getBoolean(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("A3B9C5D1E8FB",
                                                   std::string("getBoolean failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("A3B9C5D1E8FC",
                                                   "getBoolean failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            auto validateResult = validateCurrentRow(std::nothrow);
            if (!validateResult.has_value())
            {
                return cpp_dbc::unexpected(validateResult.error());
            }

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("66D4GQ4I5SIS", "Invalid column index", system_utils::captureCallStack()));
            }

            size_t idx = columnIndex - 1;
            return m_currentRow[idx] == nullptr;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("MY4D5E6F7G8H",
                                                   std::string("isNull failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("MY5E6F7G8H9I",
                                                   "isNull failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::isNull(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("9FS2I4NTH5DF", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return isNull(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C5D1E7F3A0BD",
                                                   std::string("isNull failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("MY6F7G8H9I0J",
                                                   "isNull failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBResultSet::getDate(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            auto validateResult = validateCurrentRow(std::nothrow);
            if (!validateResult.has_value())
            {
                return cpp_dbc::unexpected(validateResult.error());
            }

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("ULW1JW7OYOMY", "Invalid column index", system_utils::captureCallStack()));
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return std::string("");
            }

            return std::string(m_currentRow[idx]);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("29XLMMG3LB2Q",
                                                   std::string("getDate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("KZM116CBUKWI",
                                                   "getDate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBResultSet::getDate(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("136RPPGVZRFA", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getDate(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("AJZUSFAYYKRW",
                                                   std::string("getDate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("7UQPAX1BY99S",
                                                   "getDate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBResultSet::getTimestamp(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            auto validateResult = validateCurrentRow(std::nothrow);
            if (!validateResult.has_value())
            {
                return cpp_dbc::unexpected(validateResult.error());
            }

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("5VOZKELPIGHL", "Invalid column index", system_utils::captureCallStack()));
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return std::string("");
            }

            return std::string(m_currentRow[idx]);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("KQIG7YA2Q0VV",
                                                   std::string("getTimestamp failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("ATAUAVPH59XX",
                                                   "getTimestamp failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBResultSet::getTimestamp(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("2C7YP6KVKFZ0", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getTimestamp(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("1LE8LCO1SULZ",
                                                   std::string("getTimestamp failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("FJDMUTNVG1Z2",
                                                   "getTimestamp failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBResultSet::getTime(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            auto validateResult = validateCurrentRow(std::nothrow);
            if (!validateResult.has_value())
            {
                return cpp_dbc::unexpected(validateResult.error());
            }

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("VO406EFS4OB8", "Invalid column index", system_utils::captureCallStack()));
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return std::string("");
            }

            return std::string(m_currentRow[idx]);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("DWH1OQJ9DGPF",
                                                   std::string("getTime failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("LL5N8MEO7R5G",
                                                   "getTime failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBResultSet::getTime(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("4ST36MNYWXXT", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getTime(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RL7J9IQH6KO8",
                                                   std::string("getTime failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("H1DXG2WL4ITI",
                                                   "getTime failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> MySQLDBResultSet::getColumnNames(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            return m_columnNames;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("E7F3A9B5C2D9",
                                                   std::string("getColumnNames failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E7F3A9B5C2DA",
                                                   "getColumnNames failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
