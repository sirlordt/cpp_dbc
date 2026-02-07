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

 @file prepared_statement_02.cpp
 @brief SQLite database driver implementation - SQLiteDBPreparedStatement nothrow methods (part 1 - basic type setters)

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

    // Nothrow API
    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setInt(std::nothrow_t, int parameterIndex, int value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("S0Z1A2B3C4D5", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                return cpp_dbc::unexpected(DBException("S1Z2A3B4C5D6", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                       system_utils::captureCallStack()));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                return cpp_dbc::unexpected(DBException("S2Z3A4B5C6D7", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                       system_utils::captureCallStack()));
            }

            int result = sqlite3_bind_int(m_stmt.get(), parameterIndex, value);
            if (result != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("S3Z4A5B6C7D8", "Failed to bind integer parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value=" + std::to_string(value) + ", result=" + std::to_string(result) + ")",
                                                       system_utils::captureCallStack()));
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SI1A1B2C3D4E",
                                                   std::string("setInt failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SI1A1B2C3D4F",
                                                   "setInt failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setLong(std::nothrow_t, int parameterIndex, int64_t value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("S4Z5A6B7C8D9", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                return cpp_dbc::unexpected(DBException("S5Z6A7B8C9D0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                       system_utils::captureCallStack()));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                return cpp_dbc::unexpected(DBException("NHNWDRFVQG77", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                       system_utils::captureCallStack()));
            }

            int result = sqlite3_bind_int64(m_stmt.get(), parameterIndex, value);
            if (result != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("S6Z7A8B9C0D1", "Failed to bind long parameter: " + std::string(sqlite3_errmsg(dbPtr)),
                                                       system_utils::captureCallStack()));
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SL1A1B2C3D4E",
                                                   std::string("setLong failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SL1A1B2C3D4F",
                                                   "setLong failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setDouble(std::nothrow_t, int parameterIndex, double value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("S7Z8A9B0C1D2", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                return cpp_dbc::unexpected(DBException("7M8N9O0P1Q2R", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                       system_utils::captureCallStack()));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                return cpp_dbc::unexpected(DBException("S8Z9A0B1C2D3", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                       system_utils::captureCallStack()));
            }

            int result = sqlite3_bind_double(m_stmt.get(), parameterIndex, value);
            if (result != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("S9Z0A1B2C3D4", "Failed to bind double parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value=" + std::to_string(value) + ", result=" + std::to_string(result) + ")",
                                                       system_utils::captureCallStack()));
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SD1A1B2C3D4E",
                                                   std::string("setDouble failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SD1A1B2C3D4F",
                                                   "setDouble failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("T0Z1A2B3C4D5", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                return cpp_dbc::unexpected(DBException("T1Z2A3B4C5D6", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                       system_utils::captureCallStack()));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                return cpp_dbc::unexpected(DBException("T2Z3A4B5C6D7", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                       system_utils::captureCallStack()));
            }

            // SQLITE_TRANSIENT tells SQLite to make its own copy of the data
            int result = sqlite3_bind_text(m_stmt.get(), parameterIndex, value.c_str(), -1, SQLITE_TRANSIENT);
            if (result != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("T3Z4A5B6C7D8", "Failed to bind string parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value='" + value + "'" + ", result=" + std::to_string(result) + ")",
                                                       system_utils::captureCallStack()));
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SS1A1B2C3D4E",
                                                   std::string("setString failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SS1A1B2C3D4F",
                                                   "setString failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("T4Z5A6B7C8D9", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                return cpp_dbc::unexpected(DBException("T5Z6A7B8C9D0", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                       system_utils::captureCallStack()));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                return cpp_dbc::unexpected(DBException("T6Z7A8B9C0D1", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                       system_utils::captureCallStack()));
            }

            int intValue = value ? 1 : 0;
            int result = sqlite3_bind_int(m_stmt.get(), parameterIndex, intValue);
            if (result != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("T7Z8A9B0C1D2", "Failed to bind boolean parameter: " + std::string(sqlite3_errmsg(dbPtr)) + " (index=" + std::to_string(parameterIndex) + ", value=" + (value ? "true" : "false") + ", result=" + std::to_string(result) + ")",
                                                       system_utils::captureCallStack()));
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SB1A1B2C3D4E",
                                                   std::string("setBoolean failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SB1A1B2C3D4F",
                                                   "setBoolean failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setNull(std::nothrow_t, int parameterIndex, [[maybe_unused]] Types type) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_stmt)
            {
                return cpp_dbc::unexpected(DBException("T8Z9A0B1C2D3", "Statement is closed",
                                                       system_utils::captureCallStack()));
            }

            // Get the SQLite connection safely (throws if connection is closed)
            sqlite3 *dbPtr = getSQLiteConnection();

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                return cpp_dbc::unexpected(DBException("JIHL7D5UH98N", "Invalid parameter index: " + std::to_string(parameterIndex),
                                                       system_utils::captureCallStack()));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(m_stmt.get());
            if (parameterIndex > paramCount)
            {
                return cpp_dbc::unexpected(DBException("CSB01AH6AD9Z", "Parameter index out of range: " + std::to_string(parameterIndex) + " (statement has " + std::to_string(paramCount) + " parameters)",
                                                       system_utils::captureCallStack()));
            }

            int result = sqlite3_bind_null(m_stmt.get(), parameterIndex);
            if (result != SQLITE_OK)
            {
                return cpp_dbc::unexpected(DBException("T9Z0A1B2C3D4", "Failed to bind null parameter: " + std::string(sqlite3_errmsg(dbPtr)),
                                                       system_utils::captureCallStack()));
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("SN1A1B2C3D4E",
                                                   std::string("setNull failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("SN1A1B2C3D4F",
                                                   "setNull failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        return setString(std::nothrow, parameterIndex, value);
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        return setString(std::nothrow, parameterIndex, value);
    }

    cpp_dbc::expected<void, DBException> SQLiteDBPreparedStatement::setTime(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        return setString(std::nothrow, parameterIndex, value);
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
