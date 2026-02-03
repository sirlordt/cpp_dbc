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
 @brief Firebird database driver implementation - Part 6: FirebirdDBResultSet nothrow methods (part 1)

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{

    // ============================================================================
    // FirebirdDBResultSet - NOTHROW METHODS (part 2)
    // ============================================================================

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isAfterLast(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
            return !m_hasData && m_rowPosition > 0;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A168B6F5CED0", std::string("Exception in isAfterLast: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B279C7A6DFE1", "Unknown exception in isAfterLast", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> FirebirdDBResultSet::getRow(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
            return m_rowPosition;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C38AD8B7E0F2", std::string("Exception in getRow: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D49BE9C8F1A3", "Unknown exception in getRow", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> FirebirdDBResultSet::getInt(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
            std::string value = getColumnValue(columnIndex);
            return value.empty() ? 0 : std::stoi(value);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E5ACFAD9B0C4", std::string("Exception in getInt: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F6BDE0EAC1D5", "Unknown exception in getInt", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<long, DBException> FirebirdDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
            std::string value = getColumnValue(columnIndex);
            return value.empty() ? 0L : std::stol(value);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("G7CEFBE1D2A6", std::string("Exception in getLong: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("H8DFA0C2E3B7", "Unknown exception in getLong", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<double, DBException> FirebirdDBResultSet::getDouble(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
            FIREBIRD_DEBUG("getDouble(columnIndex=" << columnIndex << ")");
            std::string value = getColumnValue(columnIndex);
            FIREBIRD_DEBUG("  getColumnValue returned: '" << value << "'");
            return value.empty() ? 0.0 : std::stod(value);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("I9E0B1D3F4C8", std::string("Exception in getDouble: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("J0F1C2E4A5D9", "Unknown exception in getDouble", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getString(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
            return getColumnValue(columnIndex);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("K1A2D3F5B6E0", std::string("Exception in getString: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("L2B3E4A6C7F1", "Unknown exception in getString", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::getBoolean(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
            std::string value = getColumnValue(columnIndex);
            if (value.empty())
                return false;

            if (value == "1" || value == "true" || value == "TRUE" || value == "T" || value == "t" || value == "Y" || value == "y")
                return true;
            return false;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("M3C4E5A7B8F2", std::string("Exception in getBoolean: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("N4D5F6B8C9A3", "Unknown exception in getBoolean", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isNull(std::nothrow_t, size_t columnIndex) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
            if (columnIndex >= m_fieldCount)
            {
                return cpp_dbc::unexpected(DBException("B4C0D6E2F9A5", "Column index out of range: " + std::to_string(columnIndex),
                                                       system_utils::captureCallStack()));
            }
            return m_nullIndicators[columnIndex] < 0;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("F5C63D7F7AE5", std::string("Exception in isNull: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("09B6905763F3", "Unknown exception in isNull", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<int, DBException> FirebirdDBResultSet::getInt(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("FB1A2B3C4D5E", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getInt(std::nothrow, it->second);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("Q7D4F8C0E1A9", std::string("Exception in getInt(string): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("R8E5A9D1F2B0", "Unknown exception in getInt(string)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<long, DBException> FirebirdDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("FB2B3C4D5E6F", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getLong(std::nothrow, it->second);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("S9F6B0E2A3C1", std::string("Exception in getLong(string): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("T0A7C1F3B4D2", "Unknown exception in getLong(string)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<double, DBException> FirebirdDBResultSet::getDouble(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("FB3C4D5E6F7A", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getDouble(std::nothrow, it->second);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("U1B8D2A4C5E3", std::string("Exception in getDouble(string): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("V2C9E3B5D6F4", "Unknown exception in getDouble(string)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBResultSet::getString(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("FB4D5E6F7A8B", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getString(std::nothrow, it->second);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("W3D0F4C6E7A5", std::string("Exception in getString(string): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("X4E1A5D7F8B6", "Unknown exception in getString(string)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::getBoolean(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("FB5E6F7A8B9C", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return getBoolean(std::nothrow, it->second);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("Y5F2B6E8A9C7", std::string("Exception in getBoolean(string): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("Z6A3C7F9B0D8", "Unknown exception in getBoolean(string)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBResultSet::isNull(std::nothrow_t, const std::string &columnName) noexcept
    {
        try
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                return cpp_dbc::unexpected(DBException("FB6F7A8B9C0D", "Column not found: " + columnName, system_utils::captureCallStack()));
            }
            return isNull(std::nothrow, it->second);
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("A7B4D8A0C1E9", std::string("Exception in isNull(string): ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B8C5E9B1D2F0", "Unknown exception in isNull(string)", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> FirebirdDBResultSet::getColumnNames(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
            return m_columnNames;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("C9D6F0B2E8A1", std::string("Exception in getColumnNames: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D0E7A1C3F9B2", "Unknown exception in getColumnNames", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<size_t, DBException> FirebirdDBResultSet::getColumnCount(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);
            return m_fieldCount;
        }
        catch (const DBException &e)
        {
            return cpp_dbc::unexpected(e);
        }
        catch (const std::exception &e)
        {
            return cpp_dbc::unexpected(DBException("E1F8B2D4A0C3", std::string("Exception in getColumnCount: ") + e.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F2A9C3E5B1D4", "Unknown exception in getColumnCount", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
