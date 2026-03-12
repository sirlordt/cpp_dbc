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
        MYSQL_STMT_LOCK_OR_RETURN("XGRE57NSWJMQ", "setInt failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("E16M4JUNOAHM", "Invalid parameter index", system_utils::captureCallStack()));
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

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setLong(std::nothrow_t, int parameterIndex, int64_t value) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("J9TMIOACFTQ6", "setLong failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("Y4ZEGZLYZA27", "Invalid parameter index", system_utils::captureCallStack()));
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

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setDouble(std::nothrow_t, int parameterIndex, double value) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("8JO5KNH2G3PB", "setDouble failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("2K605E0TY9OX", "Invalid parameter index", system_utils::captureCallStack()));
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

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("YOSNQL0JEZGD", "setString failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("RTG84I3JPWSJ", "Invalid parameter index", system_utils::captureCallStack()));
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

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("JSCQDIWCYD90", "setBoolean failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("YTAFKAN8YTXG", "Invalid parameter index", system_utils::captureCallStack()));
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

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setNull(std::nothrow_t, int parameterIndex, Types type) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("9KTDBT153Z3G", "setNull failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("Q2HCVDXEOEIS", "Invalid parameter index", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;

        // Setup MySQL type based on our Types enum
        using enum Types;
        enum_field_types mysqlType;
        switch (type)
        {
        case INTEGER:
        {
            mysqlType = MYSQL_TYPE_LONG;
            break;
        }
        case FLOAT:
        {
            mysqlType = MYSQL_TYPE_FLOAT;
            break;
        }
        case DOUBLE:
        {
            mysqlType = MYSQL_TYPE_DOUBLE;
            break;
        }
        case VARCHAR:
        {
            mysqlType = MYSQL_TYPE_STRING;
            break;
        }
        case DATE:
        {
            mysqlType = MYSQL_TYPE_DATE;
            break;
        }
        case TIMESTAMP:
        {
            mysqlType = MYSQL_TYPE_TIMESTAMP;
            break;
        }
        case BOOLEAN:
        {
            mysqlType = MYSQL_TYPE_TINY;
            break;
        }
        case BLOB:
        {
            mysqlType = MYSQL_TYPE_BLOB;
            break;
        }
        default:
        {
            mysqlType = MYSQL_TYPE_NULL;
        }
        }

        // Store the null flag in our vector (1 for true, 0 for false)
        m_nullFlags[idx] = 1;

        m_binds[idx].buffer_type = mysqlType;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast) - Required for MySQL C API interop
        m_binds[idx].is_null = reinterpret_cast<bool *>(&m_nullFlags[idx]); // NOSONAR(cpp:S3630) — required for MySQL C API interop: is_null expects bool*
        m_binds[idx].buffer = nullptr;
        m_binds[idx].buffer_length = 0;
        m_binds[idx].length = nullptr;
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("AZ1JVKYJDANL", "setDate failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("B9M03QK7H9LB", "Invalid parameter index", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;

        // Store the date string in our vector to keep it alive
        m_stringValues[idx] = value;

        // Use MYSQL_TYPE_STRING for date strings
        // MySQL will automatically convert the string to DATE type based on the column definition
        // This matches the approach used in tests (see 20_031_test_mysql_real.cpp line 417)
        m_binds[idx].buffer_type = MYSQL_TYPE_STRING;
        m_binds[idx].buffer = m_stringValues[idx].data();
        m_binds[idx].buffer_length = m_stringValues[idx].length();
        m_binds[idx].is_null = nullptr;
        m_binds[idx].length = nullptr;
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("SPB07AEXFWLD", "setTimestamp failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("UHH8Y0JTPFNO", "Invalid parameter index", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;

        // Store the timestamp string in our vector to keep it alive
        m_stringValues[idx] = value;

        // Use MYSQL_TYPE_STRING for timestamp strings
        // MySQL will automatically convert the string to TIMESTAMP/DATETIME based on column type
        // This matches the approach used in tests (see 20_031_test_mysql_real.cpp line 418)
        m_binds[idx].buffer_type = MYSQL_TYPE_STRING;
        m_binds[idx].buffer = m_stringValues[idx].data();
        m_binds[idx].buffer_length = m_stringValues[idx].length();
        m_binds[idx].is_null = nullptr;
        m_binds[idx].length = nullptr;
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBPreparedStatement::setTime(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
    {
        MYSQL_STMT_LOCK_OR_RETURN("4J8HDIVTGZ2W", "setTime failed");

        if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_binds.size()))
        {
            return cpp_dbc::unexpected(DBException("LXZFRSP3CRQ5", "Invalid parameter index", system_utils::captureCallStack()));
        }

        int idx = parameterIndex - 1;

        // Store the time string in our vector to keep it alive
        m_stringValues[idx] = value;

        // Use MYSQL_TYPE_STRING for time strings
        // MySQL will automatically convert the string to TIME type based on the column definition
        // This matches the approach used for setDate() and setTimestamp()
        m_binds[idx].buffer_type = MYSQL_TYPE_STRING;
        m_binds[idx].buffer = m_stringValues[idx].data();
        m_binds[idx].buffer_length = m_stringValues[idx].length();
        m_binds[idx].is_null = nullptr;
        m_binds[idx].length = nullptr;
        return {};
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
