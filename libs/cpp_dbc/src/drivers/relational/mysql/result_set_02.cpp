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

    cpp_dbc::expected<void, DBException> MySQLDBResultSet::close(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        if (m_result)
        {
            // Smart pointer will automatically call mysql_free_result via MySQLResDeleter
            m_result.reset();
            m_currentRow = nullptr;
            m_rowPosition = 0;
            m_rowCount = 0;
            m_fieldCount = 0;
        }
        return {};
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::isEmpty(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);
        return m_rowCount == 0;
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::next(std::nothrow_t) noexcept
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

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        return m_rowPosition == 0;
    }

    cpp_dbc::expected<bool, DBException> MySQLDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        return m_result && m_rowPosition > m_rowCount;
    }

    cpp_dbc::expected<uint64_t, DBException> MySQLDBResultSet::getRow(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        return m_rowPosition;
    }

    cpp_dbc::expected<int, DBException> MySQLDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        // try/catch is required: std::stoi can throw std::invalid_argument or std::out_of_range
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
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected(DBException("F2A8B4C0D7E5",
                                                   "getInt failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> MySQLDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("P9Z0A1B2C3D4", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getInt(std::nothrow, it->second + 1); // +1 because getInt(int) is 1-based
    }

    cpp_dbc::expected<int64_t, DBException> MySQLDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        // try/catch is required: std::stoll can throw std::invalid_argument or std::out_of_range
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
                return cpp_dbc::unexpected(DBException("P7Z8A9B0C1D2", "Invalid column index", system_utils::captureCallStack()));
            }

            size_t idx = columnIndex - 1;
            if (m_currentRow[idx] == nullptr)
            {
                return 0;
            }

            return std::stoll(m_currentRow[idx]);
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
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected(DBException("A3B9C5D1E8F7",
                                                   "getLong failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int64_t, DBException> MySQLDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("5G6H7I8J9K0L", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getLong(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<double, DBException> MySQLDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        // try/catch is required: std::stod can throw std::invalid_argument or std::out_of_range
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
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected(DBException("B4C0D6E2F9A8",
                                                   "getDouble failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<double, DBException> MySQLDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("FA0HK6HTPRIP", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getDouble(std::nothrow, it->second + 1);
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
