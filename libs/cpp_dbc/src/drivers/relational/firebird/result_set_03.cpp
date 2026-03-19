/**

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
 @brief Firebird database driver implementation - FirebirdDBResultSet nothrow methods: navigation and scalar getters (index+name pairs)

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

#include <sstream>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::next(std::nothrow_t) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("ZALQADMF0DBS", "Connection lost");

        FIREBIRD_DEBUG("FirebirdResultSet::next - Starting");
        FIREBIRD_DEBUG("  m_closed: %s", (m_closed.load(std::memory_order_seq_cst) ? "true" : "false"));
        FIREBIRD_DEBUG("  m_stmt valid: %s", (m_stmt ? "yes" : "no"));

        if (m_closed.load(std::memory_order_seq_cst))
        {
            FIREBIRD_DEBUG("FirebirdResultSet::next - ResultSet is closed, returning false");
            return false;
        }

        if (!m_stmt)
        {
            FIREBIRD_DEBUG("FirebirdResultSet::next - m_stmt is null, returning false");
            return false;
        }

        if (!*m_stmt)
        {
            FIREBIRD_DEBUG("FirebirdResultSet::next - *m_stmt is 0 (invalid handle), returning false");
            return false;
        }

        FIREBIRD_DEBUG("  m_stmt handle value: %p", (void *)(uintptr_t)*m_stmt);
        FIREBIRD_DEBUG("  m_sqlda valid: %s", (m_sqlda ? "yes" : "no"));
        if (m_sqlda)
        {
            FIREBIRD_DEBUG("  m_sqlda->sqld: %d", (int)m_sqlda->sqld);
        }

        ISC_STATUS_ARRAY status;
        isc_stmt_handle *stmtPtr = m_stmt.get();
        FIREBIRD_DEBUG("  Calling isc_dsql_fetch with stmtPtr=%p, *stmtPtr=%p", (void *)stmtPtr, (void *)(uintptr_t)*stmtPtr);

        ISC_STATUS fetchStatus = isc_dsql_fetch(status, stmtPtr, SQL_DIALECT_V6, m_sqlda.get());
        FIREBIRD_DEBUG("  isc_dsql_fetch returned: %ld", (long)fetchStatus);

        if (fetchStatus == 0)
        {
            m_rowPosition++;
            m_hasData = true;
            FIREBIRD_DEBUG("FirebirdResultSet::next - Got row %llu", (unsigned long long)m_rowPosition);
            // Debug: print null indicators after fetch
            for (size_t i = 0; i < m_fieldCount; ++i)
            {
                FIREBIRD_DEBUG("  After fetch - Column %zu: nullInd=%d, sqlind=%p, *sqlind=%d",
                               i, (int)m_nullIndicators[i], (void *)m_sqlda->sqlvar[i].sqlind,
                               (m_sqlda->sqlvar[i].sqlind ? (int)*m_sqlda->sqlvar[i].sqlind : -999));
            }
            return true;
        }
        else if (fetchStatus == 100)
        {
            m_hasData = false;
            FIREBIRD_DEBUG("FirebirdResultSet::next - No more rows (status 100)");
            return false;
        }
        else
        {
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("FirebirdResultSet::next - Error: %s", errorMsg.c_str());
            return cpp_dbc::unexpected(DBException("B8C4D0E6F2A3", "Error fetching row: " + errorMsg,
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("GCBPQQTEZH46", "Connection lost");
        return m_rowPosition == 0 && !m_hasData;
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("G1PFWP82F0TY", "Connection lost");
        return !m_hasData && m_rowPosition > 0;
    }

    cpp_dbc::expected<uint64_t, DBException> FirebirdDBResultSet::getRow(std::nothrow_t) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("49D6QLPRDFO5", "Connection lost");
        return m_rowPosition;
    }

    cpp_dbc::expected<int, DBException> FirebirdDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("KKJ6AU7N7GC0", "Connection lost");
        auto valResult = getColumnValue(std::nothrow, columnIndex);
        if (!valResult.has_value())
        {
            return cpp_dbc::unexpected(valResult.error());
        }
        const std::string &value = valResult.value();
        if (value.empty())
        {
            return 0;
        }
        int result = 0;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
        if (ec != std::errc{})
        {
            return cpp_dbc::unexpected(DBException("E5ACFAD9B0C4",
                "Invalid integer value: " + value, system_utils::captureCallStack()));
        }
        return result;
    }

    cpp_dbc::expected<int, DBException> FirebirdDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("FB1A2B3C4D5E", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getInt(std::nothrow, it->second);
    }

    cpp_dbc::expected<int64_t, DBException> FirebirdDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("BKG4AMM82M79", "Connection lost");
        auto valResult = getColumnValue(std::nothrow, columnIndex);
        if (!valResult.has_value())
        {
            return cpp_dbc::unexpected(valResult.error());
        }
        const std::string &value = valResult.value();
        if (value.empty())
        {
            return static_cast<int64_t>(0);
        }
        int64_t result = 0;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
        if (ec != std::errc{})
        {
            return cpp_dbc::unexpected(DBException("G7CEFBE1D2A6",
                "Invalid long value: " + value, system_utils::captureCallStack()));
        }
        return result;
    }

    cpp_dbc::expected<int64_t, DBException> FirebirdDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("FB2B3C4D5E6F", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getLong(std::nothrow, it->second);
    }

    cpp_dbc::expected<double, DBException> FirebirdDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("WW48OXWIIVBF", "Connection lost");
        FIREBIRD_DEBUG("getDouble(columnIndex=%zu)", columnIndex);
        auto valResult = getColumnValue(std::nothrow, columnIndex);
        if (!valResult.has_value())
        {
            return cpp_dbc::unexpected(valResult.error());
        }
        const std::string &value = valResult.value();
        FIREBIRD_DEBUG("  getColumnValue returned: '%s'", value.c_str());
        if (value.empty())
        {
            return 0.0;
        }
        double result = 0.0;
        auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), result);
        if (ec != std::errc{})
        {
            return cpp_dbc::unexpected(DBException("I9E0B1D3F4C8",
                "Invalid double value: " + value, system_utils::captureCallStack()));
        }
        return result;
    }

    cpp_dbc::expected<double, DBException> FirebirdDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("FB3C4D5E6F7A", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getDouble(std::nothrow, it->second);
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("FEKRO46JEMTL", "Connection lost");
        return getColumnValue(std::nothrow, columnIndex);
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getString(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("FB4D5E6F7A8B", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getString(std::nothrow, it->second);
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("C9VHRLKU1UYP", "Connection lost");
        auto valResult = getColumnValue(std::nothrow, columnIndex);
        if (!valResult.has_value())
        {
            return cpp_dbc::unexpected(valResult.error());
        }
        const std::string &value = valResult.value();
        if (value.empty())
        {
            return false;
        }

        if (value == "1" || value == "true" || value == "TRUE" || value == "T" || value == "t" || value == "Y" || value == "y")
        {
            return true;
        }
        return false;
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::getBoolean(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("FB5E6F7A8B9C", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return getBoolean(std::nothrow, it->second);
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
    {
        FIREBIRD_STMT_LOCK_OR_RETURN("ICSJV6F47KZD", "Connection lost");
        if (columnIndex >= m_fieldCount)
        {
            return cpp_dbc::unexpected(DBException("B4C0D6E2F9A5", "Column index out of range: " + std::to_string(columnIndex),
                                                   system_utils::captureCallStack()));
        }
        return m_nullIndicators[columnIndex] < 0;
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isNull(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected(DBException("FB6F7A8B9C0D", "Column not found: " + columnName, system_utils::captureCallStack()));
        }
        return isNull(std::nothrow, it->second);
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
