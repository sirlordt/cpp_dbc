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
 @brief MySQL database driver implementation - MySQLDBPreparedStatement nothrow methods (part 1 - basic type setters)

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

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setInt(std::nothrow_t, int parameterIndex, int value) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("5K6L7M8N9O0P", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the value in our vector
            m_intValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_LONG;
            m_binds[idx].buffer = &m_intValues[idx];
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = std::to_string(value);
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("A0B6C2D8E5F1",
                                                   std::string("setInt failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("A0B6C2D8E5F2",
                                                   "setInt failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setLong(std::nothrow_t, int parameterIndex, long value) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("1Q2R3S4T5U6V", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the value in our vector
            m_longValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
            m_binds[idx].buffer = &m_longValues[idx];
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = std::to_string(value);
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("B1C7D3E9F6A2",
                                                   std::string("setLong failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B1C7D3E9F6A3",
                                                   "setLong failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setDouble(std::nothrow_t, int parameterIndex, double value) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("7W8X9Y0Z1A2B", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the value in our vector
            m_doubleValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_DOUBLE;
            m_binds[idx].buffer = &m_doubleValues[idx];
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = std::to_string(value);
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C2D8E4F0A7B3",
                                                   std::string("setDouble failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C2D8E4F0A7B4",
                                                   "setDouble failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("3C4D5E6F7G8H", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the string in our vector to keep it alive
            m_stringValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_STRING;
            m_binds[idx].buffer = m_stringValues[idx].data();
            m_binds[idx].buffer_length = m_stringValues[idx].length();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction (with proper escaping)
            std::string escapedValue = value;
            // Simple escape for single quotes
            size_t pos = 0;
            while ((pos = escapedValue.find("'", pos)) != std::string::npos)
            {
                escapedValue.replace(pos, 1, "''");
                pos += 2;
            }
            m_parameterValues[idx] = "'" + escapedValue + "'";
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D3E9F5A1B8C4",
                                                   std::string("setString failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D3E9F5A1B8C5",
                                                   "setString failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("9I0J1K2L3M4N", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store as int value
            m_intValues[idx] = value ? 1 : 0;

            m_binds[idx].buffer_type = MYSQL_TYPE_LONG;
            m_binds[idx].buffer = &m_intValues[idx];
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            m_parameterValues[idx] = std::to_string(m_intValues[idx]);
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("E4F0A6B2C9D5",
                                                   std::string("setBoolean failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E4F0A6B2C9D6",
                                                   "setBoolean failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setNull(std::nothrow_t, int parameterIndex, Types type) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("5O6P7Q8R9S0T", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Setup MySQL type based on our Types enum
            using enum Types;
            enum_field_types mysqlType;
            switch (type)
            {
            case INTEGER:
                mysqlType = MYSQL_TYPE_LONG;
                break;
            case FLOAT:
                mysqlType = MYSQL_TYPE_FLOAT;
                break;
            case DOUBLE:
                mysqlType = MYSQL_TYPE_DOUBLE;
                break;
            case VARCHAR:
                mysqlType = MYSQL_TYPE_STRING;
                break;
            case DATE:
                mysqlType = MYSQL_TYPE_DATE;
                break;
            case TIMESTAMP:
                mysqlType = MYSQL_TYPE_TIMESTAMP;
                break;
            case BOOLEAN:
                mysqlType = MYSQL_TYPE_TINY;
                break;
            case BLOB:
                mysqlType = MYSQL_TYPE_BLOB;
                break;
            default:
                mysqlType = MYSQL_TYPE_NULL;
            }

            // Store the null flag in our vector (1 for true, 0 for false)
            m_nullFlags[idx] = 1;

            m_binds[idx].buffer_type = mysqlType;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - Required for MySQL C API interop
            m_binds[idx].is_null = reinterpret_cast<bool *>(&m_nullFlags[idx]); // NOSONAR
            m_binds[idx].buffer = nullptr;
            m_binds[idx].buffer_length = 0;
            m_binds[idx].length = nullptr;
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F5A1B7C3D0E6",
                                                   std::string("setNull failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F5A1B7C3D0E7",
                                                   "setNull failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("1U2V3W4X5Y6Z", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the date string in our vector to keep it alive
            m_stringValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_DATE;
            m_binds[idx].buffer = m_stringValues[idx].data();
            m_binds[idx].buffer_length = m_stringValues[idx].length();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("A6B2C8D4E1F7",
                                                   std::string("setDate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("A6B2C8D4E1F8",
                                                   "setDate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("7A8B9C0D1E2F", "Invalid parameter index", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the timestamp string in our vector to keep it alive
            m_stringValues[idx] = value;

            m_binds[idx].buffer_type = MYSQL_TYPE_TIMESTAMP;
            m_binds[idx].buffer = m_stringValues[idx].data();
            m_binds[idx].buffer_length = m_stringValues[idx].length();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("B7C3D9E5F2A8",
                                                   std::string("setTimestamp failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B7C3D9E5F2A9",
                                                   "setTimestamp failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
            {
                return cpp_dbc::unexpected(DBException("D1E2F3G4H5I6", "Invalid parameter index for setBlob", system_utils::captureCallStack()));
            }

            int idx = parameterIndex - 1;

            // Store the blob object to keep it alive
            m_blobObjects[idx] = x;

            if (!x)
            {
                auto nullResult = setNull(std::nothrow, parameterIndex, Types::BLOB);
                if (!nullResult.has_value())
                {
                    return cpp_dbc::unexpected(nullResult.error());
                }
                return {};
            }

            // Get the blob data
            std::vector<uint8_t> data = x->getBytes(0, x->length());

            // Store the data in our vector to keep it alive
            m_blobValues[idx] = std::move(data);

            m_binds[idx].buffer_type = MYSQL_TYPE_BLOB;
            m_binds[idx].buffer = m_blobValues[idx].data();
            m_binds[idx].buffer_length = m_blobValues[idx].size();
            m_binds[idx].is_null = nullptr;
            m_binds[idx].length = nullptr;

            // Store parameter value for query reconstruction (with proper escaping)
            m_parameterValues[idx] = "BINARY DATA";
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C8D4E0F6A3B9",
                                                   std::string("setBlob failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C8D4E0F6A3BA",
                                                   "setBlob failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
