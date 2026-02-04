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
    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::next(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_result || m_rowPosition >= m_rowCount)
            {
                return false;
            }

            m_rowPosition++;
            return true;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("11AD78E9C72F", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("69BA39113CCA", "Unknown error in PostgreSQLDBResultSet::next", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::isBeforeFirst(std::nothrow_t) noexcept
    {
        try
        {
            return m_rowPosition == 0;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("A9CD0E3B6241", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("C3BE49F7A8D2", "Unknown error in PostgreSQLDBResultSet::isBeforeFirst", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        try
        {
            return m_result && m_rowPosition > m_rowCount;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("27DB83E591AF", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("B94C03EA75D1", "Unknown error in PostgreSQLDBResultSet::isAfterLast", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> PostgreSQLDBResultSet::getRow(std::nothrow_t) noexcept
    {
        try
        {
            return m_rowPosition;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("F5E21DA897B6", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9A4E73C0D8F2", "Unknown error in PostgreSQLDBResultSet::getRow", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> PostgreSQLDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

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
            try
            {
                return std::stoi(value);
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                return cpp_dbc::unexpected<DBException>(DBException("GV1IE638SARF", "Failed to convert value to int", system_utils::captureCallStack()));
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("D28E47A9FC35", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("0B4C5A8D76E3", "Unknown error in PostgreSQLDBResultSet::getInt", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int64_t, DBException> PostgreSQLDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

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
            try
            {
                return std::stol(value);
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                return cpp_dbc::unexpected<DBException>(DBException("PRTK87X1YSDK", "Failed to convert value to long", system_utils::captureCallStack()));
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7D9E2F8A1B3C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5C6A2D3E7F1B", "Unknown error in PostgreSQLDBResultSet::getLong", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<double, DBException> PostgreSQLDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

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
            try
            {
                return std::stod(value);
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                return cpp_dbc::unexpected<DBException>(DBException("9O0P1Q2R3S4T", "Failed to convert value to double", system_utils::captureCallStack()));
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7B2E8F4D9A3C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5D1F7E3B9C2A", "Unknown error in PostgreSQLDBResultSet::getDouble", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                return cpp_dbc::unexpected<DBException>(DBException("1A2B3C4D5E6F", "Invalid column index or row position", system_utils::captureCallStack()));
            }

            auto idx = static_cast<int>(columnIndex - 1);
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result.get(), row, idx))
            {
                return std::string("");
            }

            return std::string(PQgetvalue(m_result.get(), row, idx));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3E7C9A1B5D2F", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8F4D2E6B9A3C", "Unknown error in PostgreSQLDBResultSet::getString", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                return cpp_dbc::unexpected<DBException>(DBException("F7096FE7EDFC", "Invalid column index or row position", system_utils::captureCallStack()));
            }

            auto idx = static_cast<int>(columnIndex - 1);
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result.get(), row, idx))
            {
                return false;
            }

            auto value = std::string(PQgetvalue(m_result.get(), row, idx));
            return (value == "t" || value == "true" || value == "1" || value == "TRUE" || value == "True");
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4B9D3F7A2E8C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("1C5E8B3D7F2A", "Unknown error in PostgreSQLDBResultSet::getBoolean", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3M4N5O6P7Q8R", "Invalid column index or row position", system_utils::captureCallStack()));
            }

            auto idx = static_cast<int>(columnIndex - 1);
            int row = m_rowPosition - 1;

            return PQgetisnull(m_result.get(), row, idx);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5D9F2A7B3E8C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("1B7C3D9E5F2A", "Unknown error in PostgreSQLDBResultSet::isNull", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> PostgreSQLDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("LFNW4BOER18E", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getInt(std::nothrow, it->second + 1); // +1 because getInt(int) is 1-based
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("FE573C284D9A", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("18CA4D3B2E07", "Unknown error in PostgreSQLDBResultSet::getInt", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int64_t, DBException> PostgreSQLDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("7C8D9E0F1G2H", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getLong(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3D9B7C5E8F2A", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("6A2E9F1D7B4C", "Unknown error in PostgreSQLDBResultSet::getLong", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<double, DBException> PostgreSQLDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("5U6V7W8X9Y0Z", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getDouble(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2C8A1E9D3B5F", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4D7F6E2A8B1C", "Unknown error in PostgreSQLDBResultSet::getDouble", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBResultSet::getString(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("32DF0933F6D5", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getString(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("1F3D5A7C9B2E", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8C4E2B6F1A9D", "Unknown error in PostgreSQLDBResultSet::getString", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::getBoolean(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("7G8H9I0J1K2L", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getBoolean(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9F3A7B2D5E8C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2D5F8A3C7B1E", "Unknown error in PostgreSQLDBResultSet::getBoolean", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
