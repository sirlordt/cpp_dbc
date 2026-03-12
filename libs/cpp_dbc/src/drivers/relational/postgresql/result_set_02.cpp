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
 @brief PostgreSQL database driver implementation - PostgreSQLDBResultSet nothrow methods (part 1)

*/

#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"

#if USE_POSTGRESQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <charconv>

#include "postgresql_internal.hpp"

namespace cpp_dbc::PostgreSQL
{

    // Nothrow API implementation for PostgreSQLDBResultSet
    cpp_dbc::expected<void, DBException> PostgreSQLDBResultSet::close(std::nothrow_t) noexcept
    {
        PG_STMT_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        if (m_result)
        {
            // Smart pointer will automatically call PQclear via PGresultDeleter
            m_result.reset();
            m_rowPosition = 0;
            m_rowCount = 0;
            m_fieldCount = 0;
        }

        m_closed.store(true, std::memory_order_release);
        return {};
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::isEmpty(std::nothrow_t) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");
        return m_rowCount == 0;
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::next(std::nothrow_t) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");

        if (!m_result || m_rowPosition >= m_rowCount)
        {
            return false;
        }

        m_rowPosition++;
        return true;
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
    {
        return m_rowPosition == 0;
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        return m_result && m_rowPosition > m_rowCount;
    }

    cpp_dbc::expected<uint64_t, DBException> PostgreSQLDBResultSet::getRow(std::nothrow_t) noexcept
    {
        return m_rowPosition;
    }

    cpp_dbc::expected<int, DBException> PostgreSQLDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");

        if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
        {
            return cpp_dbc::unexpected<DBException>(DBException("H3NT10D8LP66", "Invalid column index or row position", system_utils::captureCallStack()));
        }

        // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
        auto idx = static_cast<int>(columnIndex - 1);
        int row = m_rowPosition - 1;

        if (PQgetisnull(m_result.get(), row, idx))
        {
            return 0; // Return 0 for NULL values (similar to JDBC)
        }

        const char *value = PQgetvalue(m_result.get(), row, idx);
        int result = 0;
        auto [ptr, ec] = std::from_chars(value, value + std::strlen(value), result);
        if (ec != std::errc{})
        {
            return cpp_dbc::unexpected<DBException>(DBException("GV1IE638SARF", "Failed to convert value to int", system_utils::captureCallStack()));
        }
        return result;
    }

    cpp_dbc::expected<int, DBException> PostgreSQLDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected<DBException>(DBException("LFNW4BOER18E", "Column not found: " + columnName, system_utils::captureCallStack()));
        }

        return getInt(std::nothrow, it->second + 1); // +1 because getInt(int) is 1-based
    }

    cpp_dbc::expected<int64_t, DBException> PostgreSQLDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");

        if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
        {
            return cpp_dbc::unexpected<DBException>(DBException("1ZO5W2I6K57A", "Invalid column index or row position", system_utils::captureCallStack()));
        }

        auto idx = static_cast<int>(columnIndex - 1);
        int row = m_rowPosition - 1;

        if (PQgetisnull(m_result.get(), row, idx))
        {
            return 0;
        }

        const char *value = PQgetvalue(m_result.get(), row, idx);
        int64_t result = 0;
        auto [ptr, ec] = std::from_chars(value, value + std::strlen(value), result);
        if (ec != std::errc{})
        {
            return cpp_dbc::unexpected<DBException>(DBException("PRTK87X1YSDK", "Failed to convert value to int64", system_utils::captureCallStack()));
        }
        return result;
    }

    cpp_dbc::expected<int64_t, DBException> PostgreSQLDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected<DBException>(DBException("7C8D9E0F1G2H", "Column not found: " + columnName, system_utils::captureCallStack()));
        }

        return getLong(std::nothrow, it->second + 1);
    }

    cpp_dbc::expected<double, DBException> PostgreSQLDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");

        if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3I4J5K6L7M8N", "Invalid column index or row position", system_utils::captureCallStack()));
        }

        auto idx = static_cast<int>(columnIndex - 1);
        int row = m_rowPosition - 1;

        if (PQgetisnull(m_result.get(), row, idx))
        {
            return 0.0;
        }

        const char *value = PQgetvalue(m_result.get(), row, idx);
        double result = 0.0;
        auto [ptr, ec] = std::from_chars(value, value + std::strlen(value), result);
        if (ec != std::errc{})
        {
            return cpp_dbc::unexpected<DBException>(DBException("9O0P1Q2R3S4T", "Failed to convert value to double", system_utils::captureCallStack()));
        }
        return result;
    }

    cpp_dbc::expected<double, DBException> PostgreSQLDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
    {
        auto it = m_columnMap.find(columnName);
        if (it == m_columnMap.end())
        {
            return cpp_dbc::unexpected<DBException>(DBException("5U6V7W8X9Y0Z", "Column not found: " + columnName, system_utils::captureCallStack()));
        }

        return getDouble(std::nothrow, it->second + 1);
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
