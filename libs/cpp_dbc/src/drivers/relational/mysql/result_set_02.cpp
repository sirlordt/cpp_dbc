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
 @brief MySQL database driver implementation - MySQLDBResultSet nothrow methods (part 1)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include "cpp_dbc/drivers/relational/mysql_blob.hpp"
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

    // Nothrow API implementations

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::next(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_result || m_rowPosition >= m_rowCount)
            {
                m_currentRow = nullptr; // Ensure currentRow is invalidated
                return false;
            }

            m_currentRow = mysql_fetch_row(m_result.get());
            if (m_currentRow)
            {
                m_rowPosition++;
                return true;
            }

            return false;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("B8C4D0E6F2A4",
                                                   std::string("next failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B8C4D0E6F2A5",
                                                   "next failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            return m_rowPosition == 0;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C9D5E1F7A4B1",
                                                   std::string("isBeforeFirst failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C9D5E1F7A4B2",
                                                   "isBeforeFirst failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            return m_result && m_rowPosition > m_rowCount;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D0E6F2A8B5C2",
                                                   std::string("isAfterLast failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D0E6F2A8B5C3",
                                                   "isAfterLast failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> MySQLDBResultSet::getRow(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            return m_rowPosition;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("E1F7A3B9C6D3",
                                                   std::string("getRow failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E1F7A3B9C6D4",
                                                   "getRow failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> MySQLDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            validateCurrentRow();

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("7O8P9Q0R1S2T", "Invalid column index", system_utils::captureCallStack()));
            }

            // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return 0; // Return 0 for NULL values (similar to JDBC)
            }

            return std::stoi(m_currentRow[idx]);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F2A8B4C0D7E4",
                                                   std::string("getInt failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F2A8B4C0D7E5",
                                                   "getInt failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<long, DBException> MySQLDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            validateCurrentRow();

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("P7Z8A9B0C1D2", "Invalid column index", system_utils::captureCallStack()));
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return 0;
            }

            return std::stol(m_currentRow[idx]);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("A3B9C5D1E8F6",
                                                   std::string("getLong failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("A3B9C5D1E8F7",
                                                   "getLong failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<double, DBException> MySQLDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            validateCurrentRow();

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("P8Z9A0B1C2D3", "Invalid column index", system_utils::captureCallStack()));
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return 0.0;
            }

            return std::stod(m_currentRow[idx]);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("B4C0D6E2F9A7",
                                                   std::string("getDouble failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B4C0D6E2F9A8",
                                                   "getDouble failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            validateCurrentRow();

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("089F37F0D90E", "Invalid column index", system_utils::captureCallStack()));
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

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            validateCurrentRow();

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

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            validateCurrentRow();

            if (columnIndex < 1 || columnIndex > m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("9BB5941B830C", "Invalid column index", system_utils::captureCallStack()));
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

    cpp_dbc::expected<int, DBException> MySQLDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("P9Z0A1B2C3D4", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getInt(std::nothrow, it->second + 1); // +1 because getInt(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C9D5E1F7A4B7",
                                                   std::string("getInt failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C9D5E1F7A4B8",
                                                   "getInt failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<long, DBException> MySQLDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("5G6H7I8J9K0L", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getLong(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D0E6F2A8B5C8",
                                                   std::string("getLong failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D0E6F2A8B5C9",
                                                   "getLong failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
