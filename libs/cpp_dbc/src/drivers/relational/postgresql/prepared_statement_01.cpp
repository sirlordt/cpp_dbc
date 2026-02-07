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
 @brief PostgreSQL database driver implementation - PostgreSQLDBPreparedStatement (constructor, destructor, throwing methods)

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

#if defined(__cpp_lib_format) && __cpp_lib_format >= 201907L
#include <format>
#define USE_STD_FORMAT
#endif

#include "postgresql_internal.hpp"

namespace cpp_dbc::PostgreSQL
{

    // PostgreSQLDBPreparedStatement implementation

    // Private methods (in order of declaration in .hpp)
    void PostgreSQLDBPreparedStatement::notifyConnClosing()
    {
        // Connection is closing, invalidate the statement without calling mysql_stmt_close
        // since the connection is already being destroyed
        auto result = this->close(std::nothrow);
        if (!result.has_value())
        {
            // Log the error but don't throw - connection is already closing
            PG_DEBUG("Failed to close prepared statement: " << result.error().what_s());
        }
    }

    // Helper method to process SQL and count parameters
    int PostgreSQLDBPreparedStatement::processSQL(std::string &sqlQuery) const
    {
        // Count parameters (using $1, $2, etc. or ? placeholders)
        int paramCount = 0;

        // First, check for $n style parameters
        size_t pos = 0;
        while ((pos = sqlQuery.find("$", pos)) != std::string::npos)
        {
            pos++;
            if (pos < sqlQuery.length() && isdigit(sqlQuery[pos]))
            {
                int paramIdx = 0;
                while (pos < sqlQuery.length() && isdigit(sqlQuery[pos]))
                {
                    paramIdx = paramIdx * 10 + (sqlQuery[pos] - '0');
                    pos++;
                }
                if (paramIdx > paramCount)
                {
                    paramCount = paramIdx;
                }
            }
        }

        // If no $n parameters were found, check for ? style parameters
        if (paramCount == 0)
        {
            pos = 0;
            while ((pos = sqlQuery.find("?", pos)) != std::string::npos)
            {
                paramCount++;
                pos++;
            }

            // If we found ? parameters, we need to convert the SQL to use $n parameters
            if (paramCount > 0)
            {
                std::string newSql;
                size_t lastPos = 0;
                int paramIdx = 1;

                pos = sqlQuery.find("?", lastPos);
                while (pos != std::string::npos)
                {
                    newSql.append(sqlQuery, lastPos, pos - lastPos);
#ifdef USE_STD_FORMAT
                    newSql.append(std::format("${}", paramIdx)); // C++20 required
#else
                    newSql.append("$" + std::to_string(paramIdx));
#endif
                    paramIdx++;
                    lastPos = pos + 1;
                    pos = sqlQuery.find("?", lastPos);
                }

                // Append the rest of the SQL
                if (lastPos < sqlQuery.length())
                {
                    newSql.append(sqlQuery, lastPos, sqlQuery.length() - lastPos);
                }

                // Replace the original SQL with the converted one
                sqlQuery = newSql;
            }
        }

        return paramCount;
    }

    // Helper method to get PGconn* safely, throws if connection is closed
    PGconn *PostgreSQLDBPreparedStatement::getPGConnection() const
    {
        auto conn = m_conn.lock();
        if (!conn)
        {
            throw DBException("4EB26050A94C", "PostgreSQL connection has been closed", system_utils::captureCallStack());
        }
        return conn.get();
    }

#if DB_DRIVER_THREAD_SAFE
    PostgreSQLDBPreparedStatement::PostgreSQLDBPreparedStatement(std::weak_ptr<PGconn> conn_handle, SharedConnMutex connMutex, const std::string &sql_stmt, const std::string &stmt_name)
        : m_conn(conn_handle), m_sql(sql_stmt), m_stmtName(stmt_name), m_connMutex(std::move(connMutex))
    {
#else
    PostgreSQLDBPreparedStatement::PostgreSQLDBPreparedStatement(std::weak_ptr<PGconn> conn_handle, const std::string &sql_stmt, const std::string &stmt_name)
        : m_conn(conn_handle), m_sql(sql_stmt), m_stmtName(stmt_name)
    {
#endif
        // Verify connection is valid by trying to lock it
        const PGconn *connPtr = getPGConnection();
        if (!connPtr)
        {
            throw DBException("E2L06693IILH", "Invalid PostgreSQL connection", system_utils::captureCallStack());
        }

        // Process SQL and count parameters
        int paramCount = processSQL(m_sql);

        // Initialize parameter arrays
        m_paramValues.resize(paramCount);
        m_paramLengths.resize(paramCount);
        m_paramFormats.resize(paramCount);
        m_paramTypes.resize(paramCount);

        // Initialize BLOB-related vectors
        m_blobValues.resize(paramCount);
        m_blobObjects.resize(paramCount);
        m_streamObjects.resize(paramCount);

        // Default to text format for all parameters
        for (int i = 0; i < paramCount; i++)
        {
            m_paramValues[i] = "";
            m_paramLengths[i] = 0;
            m_paramFormats[i] = 0; // 0 = text, 1 = binary
            m_paramTypes[i] = 0;   // 0 = let server guess
        }
    }

    PostgreSQLDBPreparedStatement::~PostgreSQLDBPreparedStatement()
    {
        auto result = PostgreSQLDBPreparedStatement::close(std::nothrow);
        if (!result.has_value())
        {
            // Log the error but don't throw - in destructor
            PG_DEBUG("Failed to close prepared statement: " << result.error().what_s());
        }
    }

    void PostgreSQLDBPreparedStatement::setInt(int parameterIndex, int value)
    {
        auto result = this->setInt(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setLong(int parameterIndex, int64_t value)
    {
        auto result = this->setLong(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setDouble(int parameterIndex, double value)
    {
        auto result = this->setDouble(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setString(int parameterIndex, const std::string &value)
    {
        auto result = this->setString(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setBoolean(int parameterIndex, bool value)
    {
        auto result = this->setBoolean(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setNull(int parameterIndex, Types type)
    {
        auto result = this->setNull(std::nothrow, parameterIndex, type);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setDate(int parameterIndex, const std::string &value)
    {
        auto result = this->setDate(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
    {
        auto result = this->setTimestamp(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setTime(int parameterIndex, const std::string &value)
    {
        auto result = this->setTime(std::nothrow, parameterIndex, value);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    // BLOB support methods
    void PostgreSQLDBPreparedStatement::setBlob(int parameterIndex, std::shared_ptr<Blob> x)
    {
        auto result = this->setBlob(std::nothrow, parameterIndex, x);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
    {
        auto result = this->setBinaryStream(std::nothrow, parameterIndex, x);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
    {
        auto result = this->setBinaryStream(std::nothrow, parameterIndex, x, length);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
    {
        auto result = this->setBytes(std::nothrow, parameterIndex, x);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void PostgreSQLDBPreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
    {
        auto result = this->setBytes(std::nothrow, parameterIndex, x, length);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::shared_ptr<RelationalDBResultSet> PostgreSQLDBPreparedStatement::executeQuery()
    {
        auto result = this->executeQuery(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t PostgreSQLDBPreparedStatement::executeUpdate()
    {
        auto result = this->executeUpdate(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool PostgreSQLDBPreparedStatement::execute()
    {
        auto result = this->execute(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void PostgreSQLDBPreparedStatement::close()
    {
        auto result = this->close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
