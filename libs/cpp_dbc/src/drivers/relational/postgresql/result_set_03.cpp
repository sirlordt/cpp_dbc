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
 @brief PostgreSQL database driver implementation - PostgreSQLDBResultSet nothrow methods (part 2)

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

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");

            if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                return cpp_dbc::unexpected<DBException>(DBException("UBNZW9NY218W", "Invalid column index or row position", system_utils::captureCallStack()));
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
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("8F4D2E6B9A3C", "Unknown error in PostgreSQLDBResultSet::getString", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBResultSet::getString(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("01ROBDP3R86H", "Column not found: " + columnName, system_utils::captureCallStack()));
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
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("8C4E2B6F1A9D", "Unknown error in PostgreSQLDBResultSet::getString", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");

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
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("1C5E8B3D7F2A", "Unknown error in PostgreSQLDBResultSet::getBoolean", system_utils::captureCallStack()));
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
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("2D5F8A3C7B1E", "Unknown error in PostgreSQLDBResultSet::getBoolean", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");

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
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("1B7C3D9E5F2A", "Unknown error in PostgreSQLDBResultSet::isNull", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBResultSet::isNull(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("9S0T1U2V3W4X", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return isNull(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4E2B8C7F1D9A", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("3A5D1F8B2E7C", "Unknown error in PostgreSQLDBResultSet::isNull", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBResultSet::getDate(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");

            if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                return cpp_dbc::unexpected<DBException>(DBException("RDEMHHV22EAB", "Invalid column index or row position", system_utils::captureCallStack()));
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
            return cpp_dbc::unexpected<DBException>(DBException("B1PBVRJNEU5K", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("S7Z4TK049LW3", "Unknown error in PostgreSQLDBResultSet::getDate", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBResultSet::getDate(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("MN1JH9883LNF", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getDate(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("014EI7LWFDH3", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("JS4WK77TSA1R", "Unknown error in PostgreSQLDBResultSet::getDate", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBResultSet::getTimestamp(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");

            if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                return cpp_dbc::unexpected<DBException>(DBException("9ZQ22DP6MLDU", "Invalid column index or row position", system_utils::captureCallStack()));
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
            return cpp_dbc::unexpected<DBException>(DBException("H9FNOH6H3K3S", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("NI3XBYW2DEB0", "Unknown error in PostgreSQLDBResultSet::getTimestamp", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBResultSet::getTimestamp(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("VQGH4QZNZY5Q", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getTimestamp(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("YTCDE3QIRVXR", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("HM795W2OS4EF", "Unknown error in PostgreSQLDBResultSet::getTimestamp", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBResultSet::getTime(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            PG_STMT_LOCK_OR_RETURN("PV38KVY6QHYV", "ResultSet closed");

            if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                return cpp_dbc::unexpected<DBException>(DBException("MHWEPBV3PB0Y", "Invalid column index or row position", system_utils::captureCallStack()));
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
            return cpp_dbc::unexpected<DBException>(DBException("QDTK2KI58XQM", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("K0NPWTJWFTA8", "Unknown error in PostgreSQLDBResultSet::getTime", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBResultSet::getTime(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected<DBException>(DBException("8HSKSVPD3D4T", "Column not found: " + columnName, system_utils::captureCallStack()));
            }

            return getTime(std::nothrow, it->second + 1);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("GQP8ZBSVJ1NT", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("1UUP3WSEXFU5", "Unknown error in PostgreSQLDBResultSet::getTime", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> PostgreSQLDBResultSet::getColumnNames(std::nothrow_t) noexcept
    {
        return m_columnNames;
    }

    cpp_dbc::expected<size_t, DBException> PostgreSQLDBResultSet::getColumnCount(std::nothrow_t) noexcept
    {
        return m_fieldCount;
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
