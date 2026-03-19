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
#include <charconv>
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

        if (m_materialized)
        {
            m_materializedRows.clear();
            m_currentRowPtrs.clear();
            m_currentLengths.clear();
            m_currentRow = nullptr;
            m_rowPosition = 0;
            m_rowCount = 0;
            m_fieldCount = 0;
        }
        else if (m_result)
        {
            // Smart pointer will automatically call mysql_free_result via MySQLResDeleter
            m_result.reset();
            m_currentRow = nullptr;
            m_rowPosition = 0;
            m_rowCount = 0;
            m_fieldCount = 0;
        }
        m_closed.store(true, std::memory_order_seq_cst);
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

        if (m_materialized)
        {
            if (m_rowPosition >= m_rowCount)
            {
                m_currentRow = nullptr;
                m_rowPosition = m_rowCount + 1; // ensure isAfterLast() returns true
                return false;
            }

            const auto &row = m_materializedRows[m_rowPosition];
            for (size_t i = 0; i < m_fieldCount; ++i)
            {
                if (row[i].has_value())
                {
                    // const_cast is required here: MYSQL_ROW is typedef char** (MySQL C API).
                    // m_currentRow (MYSQL_ROW) must point to m_currentRowPtrs.data() so that
                    // ALL existing getters (getInt, getString, etc.) work unchanged for both
                    // native and materialized modes. The materialized data lives in
                    // m_materializedRows (vector<optional<string>>) and c_str() returns
                    // const char*. The cast bridges this const mismatch. All access through
                    // m_currentRow is provably read-only — no getter writes through the pointer.
                    // This is a MySQL C API limitation: the typedef cannot be changed to char const**.
                    m_currentRowPtrs[i] = const_cast<char *>(row[i].value().c_str()); // NOSONAR(cpp:S859) — MySQL C API: MYSQL_ROW is char**; all access is read-only
                    m_currentLengths[i] = row[i].value().size();
                }
                else
                {
                    m_currentRowPtrs[i] = nullptr;
                    m_currentLengths[i] = 0;
                }
            }
            m_currentRow = m_currentRowPtrs.data();
            m_rowPosition++;
            return true;
        }

        if (!m_result || m_rowPosition >= m_rowCount)
        {
            m_currentRow = nullptr;
            m_rowPosition = m_rowCount + 1; // ensure isAfterLast() returns true
            return false;
        }

        m_currentRow = mysql_fetch_row(m_result.get());
        if (m_currentRow)
        {
            m_rowPosition++;
            return true;
        }

        m_rowPosition = m_rowCount + 1; // ensure isAfterLast() returns true
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

        if (m_materialized)
        {
            return m_rowPosition > m_rowCount;
        }
        return m_result && m_rowPosition > m_rowCount;
    }

    cpp_dbc::expected<uint64_t, DBException> MySQLDBResultSet::getRow(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        return m_rowPosition;
    }

    cpp_dbc::expected<int, DBException> MySQLDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        auto validateResult = validateCurrentRow(std::nothrow);
        if (!validateResult.has_value())
        {
            return cpp_dbc::unexpected(validateResult.error());
        }

        if (columnIndex < 1 || columnIndex > m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("DYVPR8BITRGA", "Invalid column index", system_utils::captureCallStack()));
        }

        // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
        size_t idx = columnIndex - 1;
        if (m_currentRow[idx] == nullptr)
        {
            return 0; // Return 0 for NULL values (similar to JDBC)
        }

        int result = 0;
        const char *str = m_currentRow[idx];
        auto [ptr, ec] = std::from_chars(str, str + std::strlen(str), result);
        if (ec != std::errc{})
        {
            return cpp_dbc::unexpected(DBException("B5BCKFSM2AZ2",
                "getInt failed: invalid numeric value",
                system_utils::captureCallStack()));
        }
        return result;
    }

    cpp_dbc::expected<int, DBException> MySQLDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("MUF6UPVRIYQT", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getInt(std::nothrow, it->second + 1); // +1 because getInt(int) is 1-based
    }

    cpp_dbc::expected<int64_t, DBException> MySQLDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        auto validateResult = validateCurrentRow(std::nothrow);
        if (!validateResult.has_value())
        {
            return cpp_dbc::unexpected(validateResult.error());
        }

        if (columnIndex < 1 || columnIndex > m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("SV0U7Y3LYTDK", "Invalid column index", system_utils::captureCallStack()));
        }

        size_t idx = columnIndex - 1;
        if (m_currentRow[idx] == nullptr)
        {
            return 0;
        }

        int64_t result = 0;
        const char *str = m_currentRow[idx];
        auto [ptr, ec] = std::from_chars(str, str + std::strlen(str), result);
        if (ec != std::errc{})
        {
            return cpp_dbc::unexpected(DBException("WXI2DXO5WKVH",
                "getLong failed: invalid numeric value",
                system_utils::captureCallStack()));
        }
        return result;
    }

    cpp_dbc::expected<int64_t, DBException> MySQLDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("A5XKD4KSD5US", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getLong(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<double, DBException> MySQLDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_mutex);

        auto validateResult = validateCurrentRow(std::nothrow);
        if (!validateResult.has_value())
        {
            return cpp_dbc::unexpected(validateResult.error());
        }

        if (columnIndex < 1 || columnIndex > m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("KTP04IOWI9DS", "Invalid column index", system_utils::captureCallStack()));
        }

        size_t idx = columnIndex - 1;
        if (m_currentRow[idx] == nullptr)
        {
            return 0.0;
        }

        double result = 0.0;
        const char *str = m_currentRow[idx];
        auto [ptr, ec] = std::from_chars(str, str + std::strlen(str), result);
        if (ec != std::errc{})
        {
            return cpp_dbc::unexpected(DBException("YJ4N8TJ9LG63",
                "getDouble failed: invalid numeric value",
                system_utils::captureCallStack()));
        }
        return result;
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
