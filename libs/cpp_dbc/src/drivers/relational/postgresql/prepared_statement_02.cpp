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
 @brief PostgreSQL database driver implementation - PostgreSQLDBPreparedStatement nothrow methods (part 1 - basic type setters)

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

    // Nothrow API implementation for PostgreSQLDBPreparedStatement
    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setInt(std::nothrow_t, int parameterIndex, int value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("5Y6Z7A8B9C0D", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = std::to_string(value);
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 23;  // INT4OID

            return {}; // Return empty/success for void expected
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9D4C7B1E8F3A", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("6E2A5F8C1B9D", "Unknown error in PostgreSQLDBPreparedStatement::setInt", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setLong(std::nothrow_t, int parameterIndex, int64_t value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("1E2F3G4H5I6J", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = std::to_string(value);
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 20;  // INT8OID

            return {}; // Return empty/success for void expected
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8E2F5A9C3B7D", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("1C6E9F3B8D2A", "Unknown error in PostgreSQLDBPreparedStatement::setLong", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setDouble(std::nothrow_t, int parameterIndex, double value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("7K8L9M0N1O2P", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = std::to_string(value);
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 701; // FLOAT8OID

            return {}; // Return empty/success for void expected
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2D9F5C7B1A3E", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8A4E6B2F9D3C", "Unknown error in PostgreSQLDBPreparedStatement::setDouble", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("3Q4R5S6T7U8V", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = value;
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 25;  // TEXTOID

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5B9C3F1D7E2A", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4E7B2D9A3F8C", "Unknown error in PostgreSQLDBPreparedStatement::setString", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("9W0X1Y2Z3A4B", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = value ? "t" : "f"; // PostgreSQL uses 't' and 'f' for boolean values
            m_paramLengths[idx] = 1;
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 16;  // BOOLOID

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2D7F9A3E8C1B", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("6B2A5F9D3C7E", "Unknown error in PostgreSQLDBPreparedStatement::setBoolean", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setNull(std::nothrow_t, int parameterIndex, Types type) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("4A049129B485", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Set the value to NULL
            m_paramValues[idx] = "";

            // Set the OID type based on our Types enum
            using enum Types;
            Oid pgType;
            switch (type)
            {
            case INTEGER:
                pgType = 23; // INT4OID
                break;
            case FLOAT:
                pgType = 700; // FLOAT4OID
                break;
            case DOUBLE:
                pgType = 701; // FLOAT8OID
                break;
            case VARCHAR:
                pgType = 25; // TEXTOID
                break;
            case DATE:
                pgType = 1082; // DATEOID
                break;
            case TIMESTAMP:
                pgType = 1114; // TIMESTAMPOID
                break;
            case BOOLEAN:
                pgType = 16; // BOOLOID
                break;
            case BLOB:
                pgType = 17; // BYTEAOID
                break;
            default:
                pgType = 0; // Let the server guess
            }

            m_paramTypes[idx] = pgType;
            m_paramLengths[idx] = 0;
            m_paramFormats[idx] = 0; // Text format

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8D2C7F3A9E5B", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("1F6E3B9D4A7C", "Unknown error in PostgreSQLDBPreparedStatement::setNull", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("5C6D7E8F9G0H", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = value;
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0;  // Text format
            m_paramTypes[idx] = 1082; // DATEOID

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9E7D3F1A8C2B", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4A6B2C8D3E7F", "Unknown error in PostgreSQLDBPreparedStatement::setDate", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                return cpp_dbc::unexpected<DBException>(DBException("1I2J3K4L5M6N", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = value;
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0;  // Text format
            m_paramTypes[idx] = 1114; // TIMESTAMPOID

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7B3F8A2D9C5E", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2E4D6F8A1B9C", "Unknown error in PostgreSQLDBPreparedStatement::setTimestamp", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
