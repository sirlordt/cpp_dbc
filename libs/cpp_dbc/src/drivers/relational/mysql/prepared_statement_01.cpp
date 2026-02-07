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

 @file prepared_statement_01.cpp
 @brief MySQL database driver implementation - MySQLDBPreparedStatement (constructor, destructor, throwing methods)

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

    // MySQLDBPreparedStatement implementation

    void MySQLDBPreparedStatement::notifyConnClosing()
    {
        // Connection is closing, invalidate the statement without calling mysql_stmt_close
        // since the connection is already being destroyed
        auto result = this->close(std::nothrow);
        if (!result.has_value())
        {
            // Log the error but don't throw - connection is already closing
            MYSQL_DEBUG("Failed to close prepared statement during connection shutdown: " << result.error().what_s());
        }
    }

    MYSQL *MySQLDBPreparedStatement::getMySQLConnection() const
    {
        auto conn = m_mysql.lock();
        if (!conn)
        {
            throw DBException("471F2E35F961", "MySQL connection has been closed", system_utils::captureCallStack());
        }
        return conn.get();
    }

#if DB_DRIVER_THREAD_SAFE
    MySQLDBPreparedStatement::MySQLDBPreparedStatement(std::weak_ptr<MYSQL> mysql_conn, SharedConnMutex connMutex, const std::string &sql_stmt)
        : m_mysql(mysql_conn), m_sql(sql_stmt), m_connMutex(std::move(connMutex))
    {
#else
    MySQLDBPreparedStatement::MySQLDBPreparedStatement(std::weak_ptr<MYSQL> mysql_conn, const std::string &sql_stmt)
        : m_mysql(mysql_conn), m_sql(sql_stmt)
    {
#endif
        MYSQL *mysqlPtr = getMySQLConnection();

        m_stmt.reset(mysql_stmt_init(mysqlPtr));
        if (!m_stmt)
        {
            throw DBException("3Y4Z5A6B7C8D", "Failed to initialize statement", system_utils::captureCallStack());
        }

        if (mysql_stmt_prepare(m_stmt.get(), m_sql.c_str(), m_sql.length()) != 0)
        {
            std::string error = mysql_stmt_error(m_stmt.get());
            m_stmt.reset(); // Smart pointer will call mysql_stmt_close via deleter
            throw DBException("P0Z1A2B3C4D5", "Failed to prepare statement: " + error, system_utils::captureCallStack());
        }

        // Count parameters (question marks) in the SQL
        unsigned long paramCount = mysql_stmt_param_count(m_stmt.get());
        m_binds.resize(paramCount);
        memset(m_binds.data(), 0, sizeof(MYSQL_BIND) * paramCount);

        // Initialize string values vector
        m_stringValues.resize(paramCount);

        // Initialize parameter values vector for query reconstruction
        m_parameterValues.resize(paramCount);

        // Initialize numeric value vectors
        m_intValues.resize(paramCount);
        m_longValues.resize(paramCount);
        m_doubleValues.resize(paramCount);
        m_nullFlags.resize(paramCount);

        // Initialize BLOB-related vectors
        m_blobValues.resize(paramCount);
        m_blobObjects.resize(paramCount);
        m_streamObjects.resize(paramCount);
    }

    MySQLDBPreparedStatement::~MySQLDBPreparedStatement()
    {
        // Call close and log errors, but don't throw from destructor
        auto result = MySQLDBPreparedStatement::close(std::nothrow);
        if (!result.has_value())
        {
            // Log the error but don't throw from destructor
            MYSQL_DEBUG("Failed to close prepared statement in destructor: " << result.error().what_s());
        }
    }

    void MySQLDBPreparedStatement::setInt(int parameterIndex, int value)
    {
        auto result = setInt(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setLong(int parameterIndex, int64_t value)
    {
        auto result = setLong(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setDouble(int parameterIndex, double value)
    {
        auto result = setDouble(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setString(int parameterIndex, const std::string &value)
    {
        auto result = setString(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setBoolean(int parameterIndex, bool value)
    {
        auto result = setBoolean(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setNull(int parameterIndex, Types type)
    {
        auto result = setNull(std::nothrow, parameterIndex, type);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setDate(int parameterIndex, const std::string &value)
    {
        auto result = setDate(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
    {
        auto result = setTimestamp(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setTime(int parameterIndex, const std::string &value)
    {
        auto result = setTime(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    // BLOB support methods
    void MySQLDBPreparedStatement::setBlob(int parameterIndex, std::shared_ptr<Blob> x)
    {
        auto result = setBlob(std::nothrow, parameterIndex, x);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
    {
        auto result = setBinaryStream(std::nothrow, parameterIndex, x);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
    {
        auto result = setBinaryStream(std::nothrow, parameterIndex, x, length);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
    {
        auto result = setBytes(std::nothrow, parameterIndex, x);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void MySQLDBPreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
    {
        auto result = setBytes(std::nothrow, parameterIndex, x, length);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::shared_ptr<RelationalDBResultSet> MySQLDBPreparedStatement::executeQuery()
    {
        auto result = executeQuery(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    uint64_t MySQLDBPreparedStatement::executeUpdate()
    {
        auto result = executeUpdate(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool MySQLDBPreparedStatement::execute()
    {
        auto result = execute(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void MySQLDBPreparedStatement::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
