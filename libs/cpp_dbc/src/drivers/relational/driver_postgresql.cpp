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

 @file driver_postgresql.cpp
 @brief PostgreSQL database driver implementation

*/

#include "cpp_dbc/drivers/relational/driver_postgresql.hpp"
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

namespace cpp_dbc
{
    namespace PostgreSQL
    {

        // PostgreSQLDBResultSet implementation
        PostgreSQLDBResultSet::PostgreSQLDBResultSet(PGresult *res) : m_result(res)
        {
            if (m_result)
            {
                m_rowCount = PQntuples(m_result.get());
                m_fieldCount = PQnfields(m_result.get());

                // Store column names and create column name to index mapping
                for (int i = 0; i < m_fieldCount; i++)
                {
                    std::string name = PQfname(m_result.get(), i);
                    m_columnNames.push_back(name);
                    m_columnMap[name] = i;
                }
            }
            else
            {
                m_rowCount = 0;
                m_fieldCount = 0;
            }
        }

        PostgreSQLDBResultSet::~PostgreSQLDBResultSet()
        {
            this->close();
        }

        bool PostgreSQLDBResultSet::next()
        {
            auto result = this->next(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        bool PostgreSQLDBResultSet::isBeforeFirst()
        {
            auto result = this->isBeforeFirst(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        bool PostgreSQLDBResultSet::isAfterLast()
        {
            auto result = this->isAfterLast(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        uint64_t PostgreSQLDBResultSet::getRow()
        {
            auto result = this->getRow(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        int PostgreSQLDBResultSet::getInt(size_t columnIndex)
        {
            auto result = this->getInt(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        int PostgreSQLDBResultSet::getInt(const std::string &columnName)
        {
            auto result = this->getInt(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        long PostgreSQLDBResultSet::getLong(size_t columnIndex)
        {
            auto result = this->getLong(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        long PostgreSQLDBResultSet::getLong(const std::string &columnName)
        {
            auto result = this->getLong(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        double PostgreSQLDBResultSet::getDouble(size_t columnIndex)
        {
            auto result = this->getDouble(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        double PostgreSQLDBResultSet::getDouble(const std::string &columnName)
        {
            auto result = this->getDouble(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        std::string PostgreSQLDBResultSet::getString(size_t columnIndex)
        {
            auto result = this->getString(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        std::string PostgreSQLDBResultSet::getString(const std::string &columnName)
        {
            auto result = this->getString(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        bool PostgreSQLDBResultSet::getBoolean(size_t columnIndex)
        {
            auto result = this->getBoolean(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        bool PostgreSQLDBResultSet::getBoolean(const std::string &columnName)
        {
            auto result = this->getBoolean(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        bool PostgreSQLDBResultSet::isNull(size_t columnIndex)
        {
            auto result = this->isNull(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        bool PostgreSQLDBResultSet::isNull(const std::string &columnName)
        {
            auto result = this->isNull(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        std::vector<std::string> PostgreSQLDBResultSet::getColumnNames()
        {
            auto result = this->getColumnNames(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        size_t PostgreSQLDBResultSet::getColumnCount()
        {
            auto result = this->getColumnCount(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        void PostgreSQLDBResultSet::close()
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            if (m_result)
            {
                // Smart pointer will automatically call PQclear via PGresultDeleter
                m_result.reset();
                m_rowPosition = 0;
                m_rowCount = 0;
                m_fieldCount = 0;
            }
        }

        bool PostgreSQLDBResultSet::isEmpty()
        {

            DB_DRIVER_LOCK_GUARD(m_mutex);

            return m_rowCount == 0;
        }

        // PostgreSQLDBPreparedStatement implementation
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
                    pos = 0;
                    size_t lastPos = 0;
                    int paramIdx = 1;

                    while ((pos = sqlQuery.find("?", lastPos)) != std::string::npos)
                    {
                        newSql.append(sqlQuery, lastPos, pos - lastPos);
#ifdef USE_STD_FORMAT
                        newSql.append(std::format("${}", paramIdx++)); // C++20 required
#else
                        newSql.append("$" + std::to_string(paramIdx++));
#endif
                        lastPos = pos + 1;
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

        PostgreSQLDBPreparedStatement::PostgreSQLDBPreparedStatement(std::weak_ptr<PGconn> conn_handle, const std::string &sql_stmt, const std::string &stmt_name)
            : m_conn(conn_handle), m_sql(sql_stmt), m_stmtName(stmt_name)
        {
            // Verify connection is valid by trying to lock it
            PGconn *connPtr = getPGConnection();
            if (!connPtr)
            {
                throw DBException("5Q6R7S8T9U0V", "Invalid PostgreSQL connection", system_utils::captureCallStack());
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
            close(std::nothrow);
        }

        void PostgreSQLDBPreparedStatement::close()
        {
            auto result = this->close(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
        }

        void PostgreSQLDBPreparedStatement::notifyConnClosing()
        {
            // Connection is closing, invalidate the statement without calling mysql_stmt_close
            // since the connection is already being destroyed
            this->close();
        }

        void PostgreSQLDBPreparedStatement::setInt(int parameterIndex, int value)
        {
            auto result = this->setInt(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        void PostgreSQLDBPreparedStatement::setLong(int parameterIndex, long value)
        {
            auto result = this->setLong(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        void PostgreSQLDBPreparedStatement::setDouble(int parameterIndex, double value)
        {
            auto result = this->setDouble(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        void PostgreSQLDBPreparedStatement::setString(int parameterIndex, const std::string &value)
        {
            auto result = this->setString(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("5B9C3F1D7E2A", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("4E7B2D9A3F8C", "Unknown error in PostgreSQLDBPreparedStatement::setString", system_utils::captureCallStack()));
            }
        }

        void PostgreSQLDBPreparedStatement::setBoolean(int parameterIndex, bool value)
        {
            auto result = this->setBoolean(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2D7F9A3E8C1B", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("6B2A5F9D3C7E", "Unknown error in PostgreSQLDBPreparedStatement::setBoolean", system_utils::captureCallStack()));
            }
        }

        void PostgreSQLDBPreparedStatement::setNull(int parameterIndex, Types type)
        {
            auto result = this->setNull(std::nothrow, parameterIndex, type);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setNull(std::nothrow_t, int parameterIndex, Types type) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
                {
                    return cpp_dbc::unexpected<DBException>(DBException("4A049129B485", "Invalid parameter index", system_utils::captureCallStack()));
                }

                int idx = parameterIndex - 1;

                // Set the value to NULL
                m_paramValues[idx] = "";

                // Set the OID type based on our Types enum
                Oid pgType;
                switch (type)
                {
                case Types::INTEGER:
                    pgType = 23; // INT4OID
                    break;
                case Types::FLOAT:
                    pgType = 700; // FLOAT4OID
                    break;
                case Types::DOUBLE:
                    pgType = 701; // FLOAT8OID
                    break;
                case Types::VARCHAR:
                    pgType = 25; // TEXTOID
                    break;
                case Types::DATE:
                    pgType = 1082; // DATEOID
                    break;
                case Types::TIMESTAMP:
                    pgType = 1114; // TIMESTAMPOID
                    break;
                case Types::BOOLEAN:
                    pgType = 16; // BOOLOID
                    break;
                case Types::BLOB:
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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("8D2C7F3A9E5B", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("1F6E3B9D4A7C", "Unknown error in PostgreSQLDBPreparedStatement::setNull", system_utils::captureCallStack()));
            }
        }

        void PostgreSQLDBPreparedStatement::setDate(int parameterIndex, const std::string &value)
        {
            auto result = this->setDate(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        void PostgreSQLDBPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
        {
            auto result = this->setTimestamp(std::nothrow, parameterIndex, value);
            if (!result)
            {
                throw result.error();
            }
        }

        std::shared_ptr<RelationalDBResultSet> PostgreSQLDBPreparedStatement::executeQuery()
        {
            auto result = this->executeQuery(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> PostgreSQLDBPreparedStatement::executeQuery(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                // Get the connection safely
                auto connPtr = m_conn.lock();
                if (!connPtr)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("4EB26050A94C", "PostgreSQL connection has been closed", system_utils::captureCallStack()));
                }

                // Prepare the statement if not already prepared
                if (!m_prepared)
                {
                    PGresult *prepareResult = PQprepare(connPtr.get(), m_stmtName.c_str(), m_sql.c_str(), static_cast<int>(m_paramValues.size()), m_paramTypes.data());
                    if (PQresultStatus(prepareResult) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(prepareResult);
                        PQclear(prepareResult);
                        return cpp_dbc::unexpected<DBException>(DBException("3U4V5W6X7Y8Z", "Failed to prepare statement: " + error, system_utils::captureCallStack()));
                    }
                    PQclear(prepareResult);
                    m_prepared = true;
                }

                // Convert parameter values to C-style array of char pointers
                std::vector<const char *> paramValuePtrs(m_paramValues.size());
                for (size_t i = 0; i < m_paramValues.size(); i++)
                {
                    paramValuePtrs[i] = m_paramValues[i].empty() ? nullptr : m_paramValues[i].c_str();
                }

                // Execute the prepared statement
                // Create temporary vector of ints for parameter lengths
                std::vector<int> paramLengthsInt(m_paramLengths.begin(), m_paramLengths.end());

                PGresult *result = PQexecPrepared(
                    connPtr.get(),
                    m_stmtName.c_str(),
                    static_cast<int>(m_paramValues.size()),
                    paramValuePtrs.data(),
                    paramLengthsInt.data(),
                    m_paramFormats.data(),
                    0 // Result format (0 = text)
                );

                if (PQresultStatus(result) != PGRES_TUPLES_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    return cpp_dbc::unexpected<DBException>(DBException("9A0B1C2D3E4F", "Failed to execute query: " + error, system_utils::captureCallStack()));
                }

                auto resultSet = std::make_shared<PostgreSQLDBResultSet>(result);

                // Close the statement after execution (single-use)
                // This is safe because PQexecPrepared() copies all data to the PGresult
                close();

                return cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>{resultSet};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7D2E9B4F1C8A", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3F8C6A5D2B9E", "Unknown error in PostgreSQLDBPreparedStatement::executeQuery", system_utils::captureCallStack()));
            }
        }

        uint64_t PostgreSQLDBPreparedStatement::executeUpdate()
        {
            auto result = this->executeUpdate(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        cpp_dbc::expected<uint64_t, DBException> PostgreSQLDBPreparedStatement::executeUpdate(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                // Get the connection safely
                auto connPtr = m_conn.lock();
                if (!connPtr)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("4EB26050A94C", "PostgreSQL connection has been closed", system_utils::captureCallStack()));
                }

                // Prepare the statement if not already prepared
                if (!m_prepared)
                {
                    PGresult *prepareResult = PQprepare(connPtr.get(), m_stmtName.c_str(), m_sql.c_str(), static_cast<int>(m_paramValues.size()), m_paramTypes.data());
                    if (PQresultStatus(prepareResult) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(prepareResult);
                        PQclear(prepareResult);
                        return cpp_dbc::unexpected<DBException>(DBException("1M2N3O4P5Q6R", "Failed to prepare statement: " + error, system_utils::captureCallStack()));
                    }
                    PQclear(prepareResult);
                    m_prepared = true;
                }

                // Convert parameter values to C-style array of char pointers
                std::vector<const char *> paramValuePtrs(m_paramValues.size());
                for (size_t i = 0; i < m_paramValues.size(); i++)
                {
                    paramValuePtrs[i] = m_paramValues[i].empty() ? nullptr : m_paramValues[i].c_str();
                }

                // Execute the prepared statement
                // Create temporary vector of ints for parameter lengths
                std::vector<int> paramLengthsInt(m_paramLengths.begin(), m_paramLengths.end());

                PGresult *result = PQexecPrepared(
                    connPtr.get(),
                    m_stmtName.c_str(),
                    static_cast<int>(m_paramValues.size()),
                    paramValuePtrs.data(),
                    paramLengthsInt.data(),
                    m_paramFormats.data(),
                    0 // Result format (0 = text)
                );

                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    return cpp_dbc::unexpected<DBException>(DBException("7S8T9U0V1W2X", "Failed to execute update: " + error, system_utils::captureCallStack()));
                }

                // Get the number of affected rows
                char *affectedRows = PQcmdTuples(result);
                uint64_t rowCount = 0;
                if (affectedRows && affectedRows[0] != '\0')
                {
                    std::from_chars(affectedRows, affectedRows + strlen(affectedRows), rowCount);
                }

                PQclear(result);

                // Close the statement after execution (single-use)
                close();

                return rowCount;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("9E2D7F5A3B8C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("6F1A9D3C7E2B", "Unknown error in PostgreSQLDBPreparedStatement::executeUpdate", system_utils::captureCallStack()));
            }
        }

        bool PostgreSQLDBPreparedStatement::execute()
        {
            auto result = this->execute(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        // BLOB support methods for PostgreSQLDBResultSet
        std::shared_ptr<Blob> PostgreSQLDBResultSet::getBlob(size_t columnIndex)
        {
            auto result = this->getBlob(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        std::shared_ptr<Blob> PostgreSQLDBResultSet::getBlob(const std::string &columnName)
        {
            auto result = this->getBlob(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        std::shared_ptr<InputStream> PostgreSQLDBResultSet::getBinaryStream(size_t columnIndex)
        {
            auto result = this->getBinaryStream(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        std::shared_ptr<InputStream> PostgreSQLDBResultSet::getBinaryStream(const std::string &columnName)
        {
            auto result = this->getBinaryStream(std::nothrow, columnName);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        std::vector<uint8_t> PostgreSQLDBResultSet::getBytes(size_t columnIndex)
        {
            auto result = this->getBytes(std::nothrow, columnIndex);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        std::vector<uint8_t> PostgreSQLDBResultSet::getBytes(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("599349A7DAA4", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getBytes(it->second + 1); // +1 because getBytes(int) is 1-based
        }

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
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("11AD78E9C72F", e.what(), system_utils::captureCallStack()));
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
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("A9CD0E3B6241", e.what(), system_utils::captureCallStack()));
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
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("27DB83E591AF", e.what(), system_utils::captureCallStack()));
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
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("F5E21DA897B6", e.what(), system_utils::captureCallStack()));
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
                    return cpp_dbc::unexpected<DBException>(DBException("1S2T3U4V5W6X", "Invalid column index or row position", system_utils::captureCallStack()));
                }

                // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
                int idx = static_cast<int>(columnIndex - 1);
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
                catch (const std::exception &e)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("7Y8Z9A0B1C2D", "Failed to convert value to int", system_utils::captureCallStack()));
                }
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("D28E47A9FC35", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("0B4C5A8D76E3", "Unknown error in PostgreSQLDBResultSet::getInt", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<long, DBException> PostgreSQLDBResultSet::getLong(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("9K0L1M2N3O4P", "Invalid column index or row position", system_utils::captureCallStack()));
                }

                int idx = static_cast<int>(columnIndex - 1);
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
                catch (const std::exception &e)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("1W2X3Y4Z5A6B", "Failed to convert value to long", system_utils::captureCallStack()));
                }
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7D9E2F8A1B3C", e.what(), system_utils::captureCallStack()));
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

                int idx = static_cast<int>(columnIndex - 1);
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
                catch (const std::exception &e)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("9O0P1Q2R3S4T", "Failed to convert value to double", system_utils::captureCallStack()));
                }
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7B2E8F4D9A3C", e.what(), system_utils::captureCallStack()));
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

                int idx = static_cast<int>(columnIndex - 1);
                int row = m_rowPosition - 1;

                if (PQgetisnull(m_result.get(), row, idx))
                {
                    return std::string("");
                }

                return std::string(PQgetvalue(m_result.get(), row, idx));
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3E7C9A1B5D2F", e.what(), system_utils::captureCallStack()));
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

                int idx = static_cast<int>(columnIndex - 1);
                int row = m_rowPosition - 1;

                if (PQgetisnull(m_result.get(), row, idx))
                {
                    return false;
                }

                std::string value = std::string(PQgetvalue(m_result.get(), row, idx));
                return (value == "t" || value == "true" || value == "1" || value == "TRUE" || value == "True");
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("4B9D3F7A2E8C", e.what(), system_utils::captureCallStack()));
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

                int idx = static_cast<int>(columnIndex - 1);
                int row = m_rowPosition - 1;

                return PQgetisnull(m_result.get(), row, idx);
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("5D9F2A7B3E8C", e.what(), system_utils::captureCallStack()));
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
                    return cpp_dbc::unexpected<DBException>(DBException("3E4F5G6H7I8J", "Column not found: " + columnName, system_utils::captureCallStack()));
                }

                return getInt(std::nothrow, it->second + 1); // +1 because getInt(int) is 1-based
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("FE573C284D9A", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("18CA4D3B2E07", "Unknown error in PostgreSQLDBResultSet::getInt", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<long, DBException> PostgreSQLDBResultSet::getLong(std::nothrow_t, const std::string &columnName) noexcept
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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3D9B7C5E8F2A", e.what(), system_utils::captureCallStack()));
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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2C8A1E9D3B5F", e.what(), system_utils::captureCallStack()));
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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("1F3D5A7C9B2E", e.what(), system_utils::captureCallStack()));
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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("9F3A7B2D5E8C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2D5F8A3C7B1E", "Unknown error in PostgreSQLDBResultSet::getBoolean", system_utils::captureCallStack()));
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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("4E2B8C7F1D9A", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3A5D1F8B2E7C", "Unknown error in PostgreSQLDBResultSet::isNull", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::vector<std::string>, DBException> PostgreSQLDBResultSet::getColumnNames(std::nothrow_t) noexcept
        {
            try
            {
                return m_columnNames;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7B3E9C2D5F8A", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("4D2A8F6E9B3C", "Unknown error in PostgreSQLDBResultSet::getColumnNames", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<size_t, DBException> PostgreSQLDBResultSet::getColumnCount(std::nothrow_t) noexcept
        {
            try
            {
                return m_fieldCount;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2F9E4B7A3D8C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("5A8D3F1C6E9B", "Unknown error in PostgreSQLDBResultSet::getColumnCount", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::shared_ptr<Blob>, DBException> PostgreSQLDBResultSet::getBlob(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("5K6L7M8N9O0P", "Invalid column index or row position for getBlob", system_utils::captureCallStack()));
                }

                // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
                int idx = static_cast<int>(columnIndex - 1);
                int row = m_rowPosition - 1;

                if (PQgetisnull(m_result.get(), row, idx))
                {
                    // Return an empty blob with no connection (data is already loaded)
                    return cpp_dbc::expected<std::shared_ptr<Blob>, DBException>(std::make_shared<PostgreSQLBlob>(std::shared_ptr<PGconn>()));
                }

                // Check if the column is a bytea type
                Oid type = PQftype(m_result.get(), idx);
                if (type != 17) // BYTEAOID
                {
                    return cpp_dbc::unexpected<DBException>(DBException("EA04B0D9155C", "Column is not a BLOB/bytea type", system_utils::captureCallStack()));
                }

                // Get the binary data using our getBytes method
                auto bytesResult = getBytes(std::nothrow, columnIndex);
                if (!bytesResult)
                {
                    return cpp_dbc::unexpected<DBException>(bytesResult.error());
                }
                std::vector<uint8_t> data = bytesResult.value();

                // Create a PostgreSQLBlob with the data
                // Note: We pass an empty shared_ptr because the data is already loaded
                // and the blob won't need to query the database
                return cpp_dbc::expected<std::shared_ptr<Blob>, DBException>{std::make_shared<PostgreSQLBlob>(std::shared_ptr<PGconn>(), data)};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("F93D7A2B6E1C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("4B8C2E5A1F9D", "Unknown error in PostgreSQLDBResultSet::getBlob", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::shared_ptr<Blob>, DBException> PostgreSQLDBResultSet::getBlob(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected<DBException>(DBException("392BEAA07684", "Column not found: " + columnName, system_utils::captureCallStack()));
                }

                return getBlob(std::nothrow, it->second + 1); // +1 because getBlob(int) is 1-based
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("5A1D9F8E3B7C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2C6E8A4B9D1F", "Unknown error in PostgreSQLDBResultSet::getBlob", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> PostgreSQLDBResultSet::getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("FC94875EDF73", "Invalid column index or row position for getBinaryStream", system_utils::captureCallStack()));
                }

                // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
                int idx = static_cast<int>(columnIndex - 1);
                int row = m_rowPosition - 1;

                if (PQgetisnull(m_result.get(), row, idx))
                {
                    // Return an empty stream
                    return cpp_dbc::expected<std::shared_ptr<InputStream>, DBException>{std::make_shared<PostgreSQLInputStream>("", 0)};
                }

                // Get the binary data using our getBytes method
                auto bytesResult = getBytes(std::nothrow, columnIndex);
                if (!bytesResult)
                {
                    return cpp_dbc::unexpected<DBException>(bytesResult.error());
                }
                std::vector<uint8_t> data = bytesResult.value();

                // Create a new input stream with the data
                if (data.empty())
                {
                    return cpp_dbc::expected<std::shared_ptr<InputStream>, DBException>{std::make_shared<PostgreSQLInputStream>("", 0)};
                }
                else
                {
                    return cpp_dbc::expected<std::shared_ptr<InputStream>, DBException>{
                        std::make_shared<PostgreSQLInputStream>(
                            reinterpret_cast<const char *>(data.data()),
                            data.size())};
                }
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3A7C9E5F1D2B", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("8D4B2F6E9A1C", "Unknown error in PostgreSQLDBResultSet::getBinaryStream", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> PostgreSQLDBResultSet::getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected<DBException>(DBException("27EF08AD722D", "Column not found: " + columnName, system_utils::captureCallStack()));
                }

                return getBinaryStream(std::nothrow, it->second + 1); // +1 because getBinaryStream(int) is 1-based
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7B2E9A4F1D5C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3C8F5E1A9D7B", "Unknown error in PostgreSQLDBResultSet::getBinaryStream", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::vector<uint8_t>, DBException> PostgreSQLDBResultSet::getBytes(std::nothrow_t, size_t columnIndex) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (!m_result || columnIndex < 1 || columnIndex > static_cast<size_t>(m_fieldCount) || m_rowPosition < 1 || m_rowPosition > m_rowCount)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("D5E8D5D3A7A4", "Invalid column index or row position for getBytes", system_utils::captureCallStack()));
                }

                // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
                int idx = static_cast<int>(columnIndex - 1);
                int row = m_rowPosition - 1;

                if (PQgetisnull(m_result.get(), row, idx))
                {
                    return std::vector<uint8_t>{};
                }

                // Get the binary data
                const char *value = PQgetvalue(m_result.get(), row, idx);
                int length = PQgetlength(m_result.get(), row, idx);

                // Check if the column is a bytea type
                Oid type = PQftype(m_result.get(), idx);

                // Create a vector with the data
                std::vector<uint8_t> data;

                if (length > 0)
                {
                    // For BYTEA type, we need to handle the hex format
                    if (type == 17) // BYTEAOID
                    {
                        // Check if the data is in hex format (starts with \x)
                        if (length >= 2 && value[0] == '\\' && value[1] == 'x')
                        {
                            // Skip the \x prefix
                            const char *hexData = value + 2;
                            int hexLength = length - 2;

                            // Each byte is represented by 2 hex characters
                            data.reserve(hexLength / 2);

                            for (int i = 0; i < hexLength; i += 2)
                            {
                                if (i + 1 < hexLength)
                                {
                                    // Convert hex pair to byte
                                    char hexPair[3] = {hexData[i], hexData[i + 1], 0};
                                    uint8_t byte = static_cast<uint8_t>(strtol(hexPair, nullptr, 16));
                                    data.push_back(byte);
                                }
                            }
                        }
                        else
                        {
                            // Use PQunescapeBytea for other bytea formats
                            size_t unescapedLength;
                            unsigned char *unescapedData = PQunescapeBytea(reinterpret_cast<const unsigned char *>(value), &unescapedLength);

                            if (unescapedData)
                            {
                                data.resize(unescapedLength);
                                std::memcpy(data.data(), unescapedData, unescapedLength);
                                PQfreemem(unescapedData);
                            }
                        }
                    }
                    else
                    {
                        // For non-BYTEA types, just copy the raw data
                        data.resize(length);
                        std::memcpy(data.data(), value, length);
                    }
                }

                return data;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7F2E9D3B8C5A", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("4A1D8B5F2E7C", "Unknown error in PostgreSQLDBResultSet::getBytes", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::vector<uint8_t>, DBException> PostgreSQLDBResultSet::getBytes(std::nothrow_t, const std::string &columnName) noexcept
        {
            try
            {
                auto it = m_columnMap.find(columnName);
                if (it == m_columnMap.end())
                {
                    return cpp_dbc::unexpected<DBException>(DBException("599349A7DAA4", "Column not found: " + columnName, system_utils::captureCallStack()));
                }

                return getBytes(std::nothrow, it->second + 1);
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3D8B7E2F5A9C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("6F1C8A3B9D2E", "Unknown error in PostgreSQLDBResultSet::getBytes", system_utils::captureCallStack()));
            }
        }

        // BLOB support methods for PostgreSQLDBPreparedStatement
        void PostgreSQLDBPreparedStatement::setBlob(int parameterIndex, std::shared_ptr<Blob> x)
        {
            auto result = this->setBlob(std::nothrow, parameterIndex, x);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
                {
                    return cpp_dbc::unexpected<DBException>(DBException("3C2333857671", "Invalid parameter index for setBlob", system_utils::captureCallStack()));
                }

                int idx = parameterIndex - 1;

                // Store the blob object to keep it alive
                m_blobObjects[idx] = x;

                if (!x)
                {
                    // Set to NULL
                    m_paramValues[idx] = "";
                    m_paramLengths[idx] = 0;
                    m_paramFormats[idx] = 0; // Text format
                    m_paramTypes[idx] = 17;  // BYTEAOID
                    return {};
                }

                // Get the blob data
                std::vector<uint8_t> data = x->getBytes(0, x->length());

                // Store the data in our vector to keep it alive
                m_blobValues[idx] = std::move(data);

                // Use binary format for BYTEA data
                m_paramValues[idx].resize(m_blobValues[idx].size());
                std::memcpy(&m_paramValues[idx][0], m_blobValues[idx].data(), m_blobValues[idx].size());

                m_paramLengths[idx] = m_blobValues[idx].size();
                m_paramFormats[idx] = 1; // Binary format
                m_paramTypes[idx] = 17;  // BYTEAOID

                return {};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7D9E3B5F8A2C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("4B6A9C1E8D3F", "Unknown error in PostgreSQLDBPreparedStatement::setBlob", system_utils::captureCallStack()));
            }
        }

        void PostgreSQLDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
        {
            auto result = this->setBinaryStream(std::nothrow, parameterIndex, x);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
                {
                    return cpp_dbc::unexpected<DBException>(DBException("D182B9C3A9CC", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack()));
                }

                int idx = parameterIndex - 1;

                // Store the stream object to keep it alive
                m_streamObjects[idx] = x;

                if (!x)
                {
                    // Set to NULL
                    m_paramValues[idx] = "";
                    m_paramLengths[idx] = 0;
                    m_paramFormats[idx] = 0; // Text format
                    m_paramTypes[idx] = 17;  // BYTEAOID
                    return {};
                }

                // Read all data from the stream
                std::vector<uint8_t> data;
                uint8_t buffer[4096];
                int bytesRead;
                while ((bytesRead = x->read(buffer, sizeof(buffer))) > 0)
                {
                    data.insert(data.end(), buffer, buffer + bytesRead);
                }

                // Store the data in our vector to keep it alive
                m_blobValues[idx] = std::move(data);

                // Use binary format for BYTEA data
                m_paramValues[idx].resize(m_blobValues[idx].size());
                std::memcpy(&m_paramValues[idx][0], m_blobValues[idx].data(), m_blobValues[idx].size());

                m_paramLengths[idx] = m_blobValues[idx].size();
                m_paramFormats[idx] = 1; // Binary format
                m_paramTypes[idx] = 17;  // BYTEAOID

                return {};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2A6D9F4B7E3C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("8C5E1B7A2D9F", "Unknown error in PostgreSQLDBPreparedStatement::setBinaryStream", system_utils::captureCallStack()));
            }
        }

        void PostgreSQLDBPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
        {
            auto result = this->setBinaryStream(std::nothrow, parameterIndex, x, length);
            if (!result)
            {
                throw result.error();
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
                {
                    return cpp_dbc::unexpected<DBException>(DBException("13B0690421E5", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack()));
                }

                int idx = parameterIndex - 1;

                // Store the stream object to keep it alive
                m_streamObjects[idx] = x;

                if (!x)
                {
                    // Set to NULL
                    m_paramValues[idx] = "";
                    m_paramLengths[idx] = 0;
                    m_paramFormats[idx] = 0; // Text format
                    m_paramTypes[idx] = 17;  // BYTEAOID
                    return {};
                }

                // Read up to 'length' bytes from the stream
                std::vector<uint8_t> data;
                data.reserve(length);
                uint8_t buffer[4096];
                size_t totalBytesRead = 0;
                int bytesRead;
                while (totalBytesRead < length && (bytesRead = x->read(buffer, std::min(sizeof(buffer), length - totalBytesRead))) > 0)
                {
                    data.insert(data.end(), buffer, buffer + bytesRead);
                    totalBytesRead += bytesRead;
                }

                // Store the data in our vector to keep it alive
                m_blobValues[idx] = std::move(data);

                // Use binary format for BYTEA data
                m_paramValues[idx].resize(m_blobValues[idx].size());
                std::memcpy(&m_paramValues[idx][0], m_blobValues[idx].data(), m_blobValues[idx].size());

                m_paramLengths[idx] = m_blobValues[idx].size();
                m_paramFormats[idx] = 1; // Binary format
                m_paramTypes[idx] = 17;  // BYTEAOID

                return {};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("5D7F3A2E9B1C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("8C2E6B9A4F1D", "Unknown error in PostgreSQLDBPreparedStatement::setBinaryStream", system_utils::captureCallStack()));
            }
        }

        void PostgreSQLDBPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
        {
            auto result = this->setBytes(std::nothrow, parameterIndex, x);
            if (!result)
            {
                throw result.error();
            }
        }

        void PostgreSQLDBPreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
        {
            auto result = this->setBytes(std::nothrow, parameterIndex, x, length);
            if (!result)
            {
                throw result.error();
            }
        }

        // Nothrow API implementation for PostgreSQLDBPreparedStatement
        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setInt(std::nothrow_t, int parameterIndex, int value) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("9D4C7B1E8F3A", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("6E2A5F8C1B9D", "Unknown error in PostgreSQLDBPreparedStatement::setInt", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setLong(std::nothrow_t, int parameterIndex, long value) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("8E2F5A9C3B7D", e.what(), system_utils::captureCallStack()));
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
                DB_DRIVER_LOCK_GUARD(m_mutex);

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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2D9F5C7B1A3E", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("8A4E6B2F9D3C", "Unknown error in PostgreSQLDBPreparedStatement::setDouble", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("9E7D3F1A8C2B", e.what(), system_utils::captureCallStack()));
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
                DB_DRIVER_LOCK_GUARD(m_mutex);

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
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7B3F8A2D9C5E", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2E4D6F8A1B9C", "Unknown error in PostgreSQLDBPreparedStatement::setTimestamp", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
                {
                    return cpp_dbc::unexpected<DBException>(DBException("D6EC2CC8C12C", "Invalid parameter index for setBytes", system_utils::captureCallStack()));
                }

                int idx = parameterIndex - 1;

                // Store the data in our vector to keep it alive
                m_blobValues[idx] = x;

                // Use binary format for BYTEA data
                m_paramValues[idx].resize(m_blobValues[idx].size());
                std::memcpy(&m_paramValues[idx][0], m_blobValues[idx].data(), m_blobValues[idx].size());

                m_paramLengths[idx] = m_blobValues[idx].size();
                m_paramFormats[idx] = 1; // Binary format
                m_paramTypes[idx] = 17;  // BYTEAOID

                return {};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3A7F9D2E5B8C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("6E1B9D4A7C2F", "Unknown error in PostgreSQLDBPreparedStatement::setBytes", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
                {
                    return cpp_dbc::unexpected<DBException>(DBException("D8D28AD75097", "Invalid parameter index for setBytes", system_utils::captureCallStack()));
                }

                int idx = parameterIndex - 1;

                if (!x)
                {
                    // Set to NULL
                    m_paramValues[idx] = "";
                    m_paramLengths[idx] = 0;
                    m_paramFormats[idx] = 0; // Text format
                    m_paramTypes[idx] = 17;  // BYTEAOID
                    return {};
                }

                // Store the data in our vector to keep it alive
                m_blobValues[idx].resize(length);
                std::memcpy(m_blobValues[idx].data(), x, length);

                // Use binary format for BYTEA data
                m_paramValues[idx].resize(m_blobValues[idx].size());
                std::memcpy(&m_paramValues[idx][0], m_blobValues[idx].data(), m_blobValues[idx].size());

                m_paramLengths[idx] = m_blobValues[idx].size();
                m_paramFormats[idx] = 1; // Binary format
                m_paramTypes[idx] = 17;  // BYTEAOID

                return {};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7D9F2B5E3A8C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("4E8B3F9A2D6C", "Unknown error in PostgreSQLDBPreparedStatement::setBytes", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<bool, DBException> PostgreSQLDBPreparedStatement::execute(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_mutex);

                // Get the connection safely
                auto connPtr = m_conn.lock();
                if (!connPtr)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("4EB26050A94C", "PostgreSQL connection has been closed", system_utils::captureCallStack()));
                }

                // Prepare the statement if not already prepared
                if (!m_prepared)
                {
                    PGresult *prepareResult = PQprepare(connPtr.get(), m_stmtName.c_str(), m_sql.c_str(), static_cast<int>(m_paramValues.size()), m_paramTypes.data());
                    if (PQresultStatus(prepareResult) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(prepareResult);
                        PQclear(prepareResult);
                        return cpp_dbc::unexpected<DBException>(DBException("9E0F1G2H3I4J", "Failed to prepare statement: " + error, system_utils::captureCallStack()));
                    }
                    PQclear(prepareResult);
                    m_prepared = true;
                }

                // Convert parameter values to C-style array of char pointers
                std::vector<const char *> paramValuePtrs(m_paramValues.size());
                for (size_t i = 0; i < m_paramValues.size(); i++)
                {
                    paramValuePtrs[i] = m_paramValues[i].empty() ? nullptr : m_paramValues[i].c_str();
                }

                // Execute the prepared statement
                // Create temporary vector of ints for parameter lengths
                std::vector<int> paramLengthsInt(m_paramLengths.begin(), m_paramLengths.end());

                PGresult *result = PQexecPrepared(
                    connPtr.get(),
                    m_stmtName.c_str(),
                    static_cast<int>(m_paramValues.size()),
                    paramValuePtrs.data(),
                    paramLengthsInt.data(),
                    m_paramFormats.data(),
                    0 // Result format (0 = text)
                );

                ExecStatusType status = PQresultStatus(result);
                bool hasResultSet = (status == PGRES_TUPLES_OK);

                // Clean up if there was an error
                if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    return cpp_dbc::unexpected<DBException>(DBException("5K6L7M8N9O0P", "Failed to execute statement: " + error, system_utils::captureCallStack()));
                }

                PQclear(result);
                return hasResultSet;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7A9C5E2B8D3F", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("1F4B8E3D7A2C", "Unknown error in PostgreSQLDBPreparedStatement::execute", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::close(std::nothrow_t) noexcept
        {
            try
            {
                if (m_prepared)
                {
                    // Try to deallocate the prepared statement if connection is still valid
                    auto conn = m_conn.lock();
                    if (conn)
                    {
                        std::string deallocateSQL = "DEALLOCATE " + m_stmtName;
                        PGresult *res = PQexec(conn.get(), deallocateSQL.c_str());
                        if (res)
                        {
                            PQclear(res);
                        }
                    }
                    m_prepared = false;
                }

                return {};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3D8E5F2A9B7C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("6B9C2D7E4F1A", "Unknown error in PostgreSQLDBPreparedStatement::close", system_utils::captureCallStack()));
            }
        }

        // PostgreSQLDBConnection implementation
        PostgreSQLDBConnection::PostgreSQLDBConnection(const std::string &host,
                                                       int port,
                                                       const std::string &database,
                                                       const std::string &user,
                                                       const std::string &password,
                                                       const std::map<std::string, std::string> &options)
            : m_closed(false), m_autoCommit(true), m_transactionActive(false), m_statementCounter(0),
              m_isolationLevel(TransactionIsolationLevel::TRANSACTION_READ_COMMITTED) // PostgreSQL default
        {
            // Build connection string
            std::stringstream conninfo;
            conninfo << "host=" << host << " ";
            conninfo << "port=" << port << " ";
            conninfo << "dbname=" << database << " ";
            conninfo << "user=" << user << " ";
            conninfo << "password=" << password << " ";

            for (const auto &option : options)
            {
                if (option.first.starts_with("query__") == false)
                {
                    conninfo << option.first << "=" << option.second << " ";
                }
            }

            if (options.find("gssencmode") == options.end())
            {
                conninfo << "gssencmode=disable";
            }

            // Connect to the database - create shared_ptr with custom deleter
            PGconn *rawConn = PQconnectdb(conninfo.str().c_str());
            if (PQstatus(rawConn) != CONNECTION_OK)
            {
                std::string error = PQerrorMessage(rawConn);
                PQfinish(rawConn);
                throw DBException("1Q2R3S4T5U6V", "Failed to connect to PostgreSQL: " + error, system_utils::captureCallStack());
            }

            // Wrap in shared_ptr with custom deleter
            m_conn = std::shared_ptr<PGconn>(rawConn, PGconnDeleter());

            // Set up a notice processor to suppress NOTICE messages
            PQsetNoticeProcessor(m_conn.get(), [](void *, const char *)
                                 {
                                     // Do nothing with the notice message
                                 },
                                 nullptr);

            // Set auto-commit mode
            setAutoCommit(true);

            // Initialize URL string once
            std::stringstream urlBuilder;
            urlBuilder << "cpp_dbc:postgresql://" << host << ":" << port;
            if (!database.empty())
            {
                urlBuilder << "/" << database;
            }
            m_url = urlBuilder.str();
        }

        PostgreSQLDBConnection::~PostgreSQLDBConnection()
        {
            close();
        }

        void PostgreSQLDBConnection::close()
        {
            if (!m_closed && m_conn)
            {

                // Notify all active statements that connection is closing
                {
                    std::lock_guard<std::mutex> lock(m_statementsMutex);
                    for (auto &stmt : m_activeStatements)
                    {
                        if (stmt)
                        {
                            stmt->notifyConnClosing();
                        }
                    }
                    m_activeStatements.clear();
                }

                // Sleep for 25ms to avoid problems with concurrency
                std::this_thread::sleep_for(std::chrono::milliseconds(25));

                // shared_ptr will automatically call PQfinish via PGconnDeleter
                m_conn.reset();
                m_closed = true;
            }
        }

        bool PostgreSQLDBConnection::isClosed()
        {
            return m_closed;
        }

        void PostgreSQLDBConnection::returnToPool()
        {
            // Don't physically close the connection, just mark it as available
            // so it can be reused by the pool

            // Reset the connection state if necessary
            try
            {
                // Make sure autocommit is enabled for the next time the connection is used
                if (!m_autoCommit)
                {
                    setAutoCommit(true);
                }

                // We don't set m_closed = true because we want to keep the connection open
                // Just mark it as available for reuse
            }
            catch (...)
            {
                // Ignore errors during cleanup
            }
        }

        bool PostgreSQLDBConnection::isPooled()
        {
            return false;
        }

        std::shared_ptr<RelationalDBPreparedStatement> PostgreSQLDBConnection::prepareStatement(const std::string &sql)
        {
            auto result = prepareStatement(std::nothrow, sql);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        std::shared_ptr<RelationalDBResultSet> PostgreSQLDBConnection::executeQuery(const std::string &sql)
        {
            auto result = executeQuery(std::nothrow, sql);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        uint64_t PostgreSQLDBConnection::executeUpdate(const std::string &sql)
        {
            auto result = executeUpdate(std::nothrow, sql);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        bool PostgreSQLDBConnection::beginTransaction()
        {
            auto result = this->beginTransaction(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        bool PostgreSQLDBConnection::transactionActive()
        {
            auto result = this->transactionActive(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        void PostgreSQLDBConnection::setAutoCommit(bool autoCommitFlag)
        {
            auto result = setAutoCommit(std::nothrow, autoCommitFlag);
            if (!result)
            {
                throw result.error();
            }
        }

        bool PostgreSQLDBConnection::getAutoCommit()
        {
            auto result = getAutoCommit(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        void PostgreSQLDBConnection::commit()
        {
            auto result = this->commit(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
        }

        void PostgreSQLDBConnection::rollback()
        {
            auto result = this->rollback(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
        }

        void PostgreSQLDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
        {
            auto result = this->setTransactionIsolation(std::nothrow, level);
            if (!result)
            {
                throw result.error();
            }
        }

        TransactionIsolationLevel PostgreSQLDBConnection::getTransactionIsolation()
        {
            auto result = this->getTransactionIsolation(std::nothrow);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        std::string PostgreSQLDBConnection::generateStatementName()
        {
            std::stringstream ss;
            ss << "stmt_" << m_statementCounter++;
            return ss.str();
        }

        void PostgreSQLDBConnection::registerStatement(std::shared_ptr<PostgreSQLDBPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            // m_activeStatements.insert(std::weak_ptr<MySQLPreparedStatement>(stmt));
            m_activeStatements.insert(stmt);
        }

        void PostgreSQLDBConnection::unregisterStatement(std::shared_ptr<PostgreSQLDBPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            // m_activeStatements.erase(std::weak_ptr<MySQLPreparedStatement>(stmt));
            m_activeStatements.erase(stmt);
        }

        std::string PostgreSQLDBConnection::getURL() const
        {
            return m_url;
        }

        // PostgreSQLDBDriver implementation
        PostgreSQLDBDriver::PostgreSQLDBDriver()
        {
            // PostgreSQL doesn't require explicit initialization like MySQL
        }

        PostgreSQLDBDriver::~PostgreSQLDBDriver()
        {
            // Sleep a bit to ensure all resources are properly released
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        std::shared_ptr<RelationalDBConnection> PostgreSQLDBDriver::connectRelational(const std::string &url,
                                                                                      const std::string &user,
                                                                                      const std::string &password,
                                                                                      const std::map<std::string, std::string> &options)
        {
            auto result = connectRelational(std::nothrow, url, user, password, options);
            if (!result)
            {
                throw result.error();
            }
            return result.value();
        }

        bool PostgreSQLDBDriver::acceptsURL(const std::string &url)
        {
            return url.substr(0, 21) == "cpp_dbc:postgresql://";
        }

        bool PostgreSQLDBDriver::parseURL(const std::string &url,
                                          std::string &host,
                                          int &port,
                                          std::string &database)
        {
            // Parse URL of format: cpp_dbc:postgresql://host:port/database
            if (!acceptsURL(url))
            {
                return false;
            }

            // Extract host, port, and database
            std::string temp = url.substr(21); // Remove "cpp_dbc:postgresql://"

            // Find host:port separator
            size_t hostEnd = temp.find(":");
            if (hostEnd == std::string::npos)
            {
                // Try to find database separator if no port is specified
                hostEnd = temp.find("/");
                if (hostEnd == std::string::npos)
                {
                    return false;
                }

                host = temp.substr(0, hostEnd);
                port = 5432; // Default PostgreSQL port
            }
            else
            {
                host = temp.substr(0, hostEnd);

                // Find port/database separator
                size_t portEnd = temp.find("/", hostEnd + 1);
                if (portEnd == std::string::npos)
                {
                    return false;
                }

                std::string portStr = temp.substr(hostEnd + 1, portEnd - hostEnd - 1);
                try
                {
                    port = std::stoi(portStr);
                }
                catch (...)
                {
                    return false;
                }

                temp = temp.substr(portEnd);
            }

            // Extract database name (remove leading '/')
            database = temp.substr(1);

            return true;
        }

        // Nothrow API implementation
        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> PostgreSQLDBDriver::connectRelational(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options) noexcept
        {
            try
            {
                std::string host;
                int port;
                std::string database;

                // Check if the URL is in the expected format
                if (acceptsURL(url))
                {
                    // URL is in the format cpp_dbc:postgresql://host:port/database
                    if (!parseURL(url, host, port, database))
                    {
                        return cpp_dbc::unexpected<DBException>(DBException("1K2L3M4N5O6P", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack()));
                    }
                }
                else
                {
                    // Try to extract host, port, and database directly
                    size_t hostStart = url.find("://");
                    if (hostStart != std::string::npos)
                    {
                        std::string temp = url.substr(hostStart + 3);

                        // Find host:port separator
                        size_t hostEnd = temp.find(":");
                        if (hostEnd == std::string::npos)
                        {
                            // Try to find database separator if no port is specified
                            hostEnd = temp.find("/");
                            if (hostEnd == std::string::npos)
                            {
                                return cpp_dbc::unexpected<DBException>(DBException("7Q8R9S0T1U2V", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack()));
                            }

                            host = temp.substr(0, hostEnd);
                            port = 5432; // Default PostgreSQL port
                        }
                        else
                        {
                            host = temp.substr(0, hostEnd);

                            // Find port/database separator
                            size_t portEnd = temp.find("/", hostEnd + 1);
                            if (portEnd == std::string::npos)
                            {
                                return cpp_dbc::unexpected<DBException>(DBException("5I6J7K8L9M0N", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack()));
                            }

                            std::string portStr = temp.substr(hostEnd + 1, portEnd - hostEnd - 1);
                            try
                            {
                                port = std::stoi(portStr);
                            }
                            catch (...)
                            {
                                return cpp_dbc::unexpected<DBException>(DBException("1O2P3Q4R5S6T", "Invalid port in URL: " + url, system_utils::captureCallStack()));
                            }

                            // Extract database
                            database = temp.substr(portEnd + 1);
                        }
                    }
                    else
                    {
                        return cpp_dbc::unexpected<DBException>(DBException("7U8V9W0X1Y2Z", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack()));
                    }
                }

                auto connection = std::make_shared<PostgreSQLDBConnection>(host, port, database, user, password, options);
                return cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>(std::static_pointer_cast<RelationalDBConnection>(connection));
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2A3B4C5D6E7F", std::string("Exception in connectRelational: ") + e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("8G9H0I1J2K3L", "Unknown exception in connectRelational", system_utils::captureCallStack()));
            }
        }

        std::string PostgreSQLDBDriver::getName() const noexcept
        {
            return "postgresql";
        }

        // Nothrow API implementation for PostgreSQLDBConnection
        cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> PostgreSQLDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("7W8X9Y0Z1A2B", "Connection is closed", system_utils::captureCallStack()));
                }

                // Generate a unique statement name and pass weak_ptr to the prepared statement
                // so it can safely detect when connection is closed
                std::string stmtName = generateStatementName();
                auto stmt = std::make_shared<PostgreSQLDBPreparedStatement>(std::weak_ptr<PGconn>(m_conn), sql, stmtName);

                registerStatement(stmt);

                return cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>(std::static_pointer_cast<RelationalDBPreparedStatement>(stmt));
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("1D3F5A7C9E2B", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("8B4D6F2A9C3E", "Unknown error in PostgreSQLDBConnection::prepareStatement", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> PostgreSQLDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("3C4D5E6F7G8H", "Connection is closed", system_utils::captureCallStack()));
                }

                PGresult *result = PQexec(m_conn.get(), sql.c_str());
                if (PQresultStatus(result) != PGRES_TUPLES_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    return cpp_dbc::unexpected<DBException>(DBException("9I0J1K2L3M4N", "Query failed: " + error, system_utils::captureCallStack()));
                }

                auto resultSet = std::make_shared<PostgreSQLDBResultSet>(result);
                return cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>(std::static_pointer_cast<RelationalDBResultSet>(resultSet));
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2D7F9A3E8C1B", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("6B2A5F9D3C7E", "Unknown error in PostgreSQLDBConnection::executeQuery", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<uint64_t, DBException> PostgreSQLDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("5O6P7Q8R9S0T", "Connection is closed", system_utils::captureCallStack()));
                }

                PGresult *result = PQexec(m_conn.get(), sql.c_str());
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    return cpp_dbc::unexpected<DBException>(DBException("1U2V3W4X5Y6Z", "Update failed: " + error, system_utils::captureCallStack()));
                }

                // Get the number of affected rows
                char *affectedRows = PQcmdTuples(result);
                uint64_t rowCount = 0;
                if (affectedRows && affectedRows[0] != '\0')
                {
                    std::from_chars(affectedRows, affectedRows + strlen(affectedRows), rowCount);
                }

                PQclear(result);
                return rowCount;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("9A7C3E5B2D8F", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("4F1D8B6E2A9C", "Unknown error in PostgreSQLDBConnection::executeUpdate", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::setAutoCommit(std::nothrow_t, bool autoCommitFlag) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("7A8B9C0D1E2F", "Connection is closed", system_utils::captureCallStack()));
                }

                // Only take action if the flag is actually changing
                if (this->m_autoCommit != autoCommitFlag)
                {
                    if (!autoCommitFlag)
                    {
                        // Si estamos desactivando autoCommit, iniciar una transacción explícita
                        // PostgreSQL está siempre en modo autocommit hasta que se inicia una transacción
                        this->m_autoCommit = false;

                        auto result = beginTransaction(std::nothrow);
                        if (!result)
                        {
                            return cpp_dbc::unexpected<DBException>(result.error());
                        }
                    }
                    else
                    {
                        // If we're enabling autocommit, we must ensure any active transaction is ended
                        if (m_transactionActive)
                        {
                            auto result = commit(std::nothrow);
                            if (!result)
                            {
                                return cpp_dbc::unexpected<DBException>(result.error());
                            }
                        }

                        this->m_autoCommit = true;
                    }
                }

                return {};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("5D7F9A2E8B3C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("1B6E9C4A8D2F", "Unknown error in PostgreSQLDBConnection::setAutoCommit", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::getAutoCommit(std::nothrow_t) noexcept
        {
            try
            {
                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("9D3F5A7C2E8B", "Connection is closed", system_utils::captureCallStack()));
                }

                return m_autoCommit;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("1B7D9E5F3A2C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("8E4C2A6F9B3D", "Unknown error in PostgreSQLDBConnection::getAutoCommit", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::beginTransaction(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("7A8B9C0D1E2F", "Connection is closed", system_utils::captureCallStack()));
                }

                // If transaction is already active, just return true
                if (m_transactionActive)
                {
                    return true;
                }

                // Start a transaction
                std::string beginCmd = "BEGIN";

                // For SERIALIZABLE isolation, we need special handling
                if (m_isolationLevel == TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
                {
                    beginCmd = "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE";

                    // Execute the BEGIN command
                    PGresult *dummyResult = PQexec(m_conn.get(), beginCmd.c_str());
                    if (PQresultStatus(dummyResult) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(dummyResult);
                        PQclear(dummyResult);
                        return cpp_dbc::unexpected<DBException>(DBException("3G4H5I6J7K8L", "Failed to start SERIALIZABLE transaction: " + error, system_utils::captureCallStack()));
                    }
                    PQclear(dummyResult);

                    // Force snapshot acquisition with a dummy query
                    PGresult *snapshotResult = PQexec(m_conn.get(), "SELECT 1");
                    if (PQresultStatus(snapshotResult) != PGRES_TUPLES_OK)
                    {
                        std::string error = PQresultErrorMessage(snapshotResult);
                        PQclear(snapshotResult);
                        return cpp_dbc::unexpected<DBException>(DBException("9M0N1O2P3Q4R", "Failed to acquire snapshot: " + error, system_utils::captureCallStack()));
                    }
                    PQclear(snapshotResult);
                }
                else
                {
                    // Standard BEGIN for other isolation levels
                    PGresult *result = PQexec(m_conn.get(), beginCmd.c_str());
                    if (PQresultStatus(result) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(result);
                        PQclear(result);
                        return cpp_dbc::unexpected<DBException>(DBException("5S6T7U8V9W0X", "Failed to start transaction: " + error, system_utils::captureCallStack()));
                    }
                    PQclear(result);
                }

                m_autoCommit = false;
                m_transactionActive = true;
                return true;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("4B8D2F6A9E3C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7E1C9A5F2D8B", "Unknown error in PostgreSQLDBConnection::beginTransaction", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::transactionActive(std::nothrow_t) noexcept
        {
            try
            {
                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("5F8B2E9A7D3C", "Connection is closed", system_utils::captureCallStack()));
                }

                return m_transactionActive;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3A7D9B5E2C8F", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("1D6F8B3E9A2C", "Unknown error in PostgreSQLDBConnection::transactionActive", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::commit(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("7E8F9G0H1I2J", "Connection is closed", system_utils::captureCallStack()));
                }

                // If no transaction is active, nothing to commit
                if (!m_transactionActive)
                {
                    return {};
                }

                PGresult *result = PQexec(m_conn.get(), "COMMIT");
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    return cpp_dbc::unexpected<DBException>(DBException("3K4L5M6N7O8P", "Commit failed: " + error, system_utils::captureCallStack()));
                }
                PQclear(result);

                m_transactionActive = false;
                m_autoCommit = true;

                return {};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("9D2E7F5A3C8B", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("6A1B4C8D3E7F", "Unknown error in PostgreSQLDBConnection::commit", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::rollback(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("5W6X7Y8Z9A0B", "Connection is closed", system_utils::captureCallStack()));
                }

                // If no transaction is active, nothing to rollback
                if (!m_transactionActive)
                {
                    return {};
                }

                PGresult *result = PQexec(m_conn.get(), "ROLLBACK");
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    return cpp_dbc::unexpected<DBException>(DBException("1C2D3E4F5G6H", "Rollback failed: " + error, system_utils::captureCallStack()));
                }
                PQclear(result);

                m_transactionActive = false;
                m_autoCommit = true;

                return {};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("8D2A7F4E9B3C", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("5C9A1E7B3D8F", "Unknown error in PostgreSQLDBConnection::rollback", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("3O4P5Q6R7S8T", "Connection is closed", system_utils::captureCallStack()));
                }

                std::string query;
                switch (level)
                {
                case TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED:
                    // PostgreSQL treats READ UNCOMMITTED the same as READ COMMITTED
                    query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
                    break;
                case TransactionIsolationLevel::TRANSACTION_READ_COMMITTED:
                    query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ COMMITTED";
                    break;
                case TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ:
                    query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL REPEATABLE READ";
                    break;
                case TransactionIsolationLevel::TRANSACTION_SERIALIZABLE:
                    query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                    break;
                default:
                    return cpp_dbc::unexpected<DBException>(DBException("9U0V1W2X3Y4Z", "Unsupported transaction isolation level", system_utils::captureCallStack()));
                }

                PGresult *result = PQexec(m_conn.get(), query.c_str());
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    return cpp_dbc::unexpected<DBException>(DBException("5A6B7C8D9E0F", "Failed to set transaction isolation level: " + error, system_utils::captureCallStack()));
                }
                PQclear(result);

                this->m_isolationLevel = level;

                // If we're in a transaction (autoCommit = false), we need to restart it
                // for the new isolation level to take effect
                if (!m_autoCommit)
                {
                    result = PQexec(m_conn.get(), "COMMIT");
                    if (PQresultStatus(result) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(result);
                        PQclear(result);
                        return cpp_dbc::unexpected<DBException>(DBException("1G2H3I4J5K6L", "Failed to commit transaction: " + error, system_utils::captureCallStack()));
                    }
                    PQclear(result);

                    // For SERIALIZABLE isolation, we need special handling
                    if (m_isolationLevel == TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
                    {
                        std::string beginCmd = "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                        result = PQexec(m_conn.get(), beginCmd.c_str());
                        if (PQresultStatus(result) != PGRES_COMMAND_OK)
                        {
                            std::string error = PQresultErrorMessage(result);
                            PQclear(result);
                            return cpp_dbc::unexpected<DBException>(DBException("7M8N9O0P1Q2R", "Failed to start SERIALIZABLE transaction: " + error, system_utils::captureCallStack()));
                        }
                        PQclear(result);

                        // Force snapshot acquisition with a dummy query
                        PGresult *snapshotResult = PQexec(m_conn.get(), "SELECT 1");
                        if (PQresultStatus(snapshotResult) != PGRES_TUPLES_OK)
                        {
                            std::string error = PQresultErrorMessage(snapshotResult);
                            PQclear(snapshotResult);
                            return cpp_dbc::unexpected<DBException>(DBException("3S4T5U6V7W8X", "Failed to acquire snapshot: " + error, system_utils::captureCallStack()));
                        }
                        PQclear(snapshotResult);
                    }
                    else
                    {
                        // Standard BEGIN for other isolation levels
                        result = PQexec(m_conn.get(), "BEGIN");
                        if (PQresultStatus(result) != PGRES_COMMAND_OK)
                        {
                            std::string error = PQresultErrorMessage(result);
                            PQclear(result);
                            return cpp_dbc::unexpected<DBException>(DBException("9Y0Z1A2B3C4D", "Failed to start transaction: " + error, system_utils::captureCallStack()));
                        }
                        PQclear(result);
                    }
                }

                return {};
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7D1B9E3C5A8F", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2E4C6A9D1F7B", "Unknown error in PostgreSQLDBConnection::setTransactionIsolation", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<TransactionIsolationLevel, DBException> PostgreSQLDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
        {
            try
            {
                DB_DRIVER_LOCK_GUARD(m_connMutex);

                if (m_closed || !m_conn)
                {
                    return cpp_dbc::unexpected<DBException>(DBException("5E6F7G8H9I0J", "Connection is closed", system_utils::captureCallStack()));
                }

                // Query the current isolation level
                PGresult *result = PQexec(m_conn.get(), "SHOW transaction_isolation");
                if (PQresultStatus(result) != PGRES_TUPLES_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    return cpp_dbc::unexpected<DBException>(DBException("3W4X5Y6Z7A8B", "Failed to get transaction isolation level: " + error, system_utils::captureCallStack()));
                }

                if (PQntuples(result) == 0)
                {
                    PQclear(result);
                    return cpp_dbc::unexpected<DBException>(DBException("9C0D1E2F3G4H", "Failed to fetch transaction isolation level", system_utils::captureCallStack()));
                }

                std::string level = PQgetvalue(result, 0, 0);
                PQclear(result);

                // Convert the string value to the enum - handle both formats
                std::string levelLower = level;
                // Convert to lowercase for case-insensitive comparison
                std::transform(levelLower.begin(), levelLower.end(), levelLower.begin(),
                               [](unsigned char c)
                               { return std::tolower(c); });

                if (levelLower == "read uncommitted" || levelLower == "read_uncommitted")
                    return TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED;
                else if (levelLower == "read committed" || levelLower == "read_committed")
                    return TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;
                else if (levelLower == "repeatable read" || levelLower == "repeatable_read")
                    return TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ;
                else if (levelLower == "serializable")
                    return TransactionIsolationLevel::TRANSACTION_SERIALIZABLE;
                else
                    return TransactionIsolationLevel::TRANSACTION_NONE;
            }
            catch (const DBException &e)
            {
                return cpp_dbc::unexpected<DBException>(e);
            }
            catch (const std::exception &e)
            {
                return cpp_dbc::unexpected<DBException>(DBException("2D7E9C4B8A3F", e.what(), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected<DBException>(DBException("5F1C8D3A6B9E", "Unknown error in PostgreSQLDBConnection::getTransactionIsolation", system_utils::captureCallStack()));
            }
        }

    } // namespace PostgreSQL
} // namespace cpp_dbc
