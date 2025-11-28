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
 @brief Tests for PostgreSQL database operations

*/

#include "cpp_dbc/drivers/driver_postgresql.hpp"
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

        // PostgreSQLResultSet implementation
        PostgreSQLResultSet::PostgreSQLResultSet(PGresult *res) : m_result(res)
        {
            if (m_result)
            {
                m_rowCount = PQntuples(m_result);
                m_fieldCount = PQnfields(m_result);

                // Store column names and create column name to index mapping
                for (int i = 0; i < m_fieldCount; i++)
                {
                    std::string name = PQfname(m_result, i);
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

        PostgreSQLResultSet::~PostgreSQLResultSet()
        {
            this->close();
        }

        bool PostgreSQLResultSet::next()
        {
            if (!m_result || m_rowPosition >= m_rowCount)
            {
                return false;
            }

            m_rowPosition++;
            return true;
        }

        bool PostgreSQLResultSet::isBeforeFirst()
        {
            return m_rowPosition == 0;
        }

        bool PostgreSQLResultSet::isAfterLast()
        {
            return m_result && m_rowPosition > m_rowCount;
        }

        uint64_t PostgreSQLResultSet::getRow()
        {
            return m_rowPosition;
        }

        int PostgreSQLResultSet::getInt(int columnIndex)
        {
            if (!m_result || columnIndex < 1 || columnIndex > m_fieldCount || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                throw DBException("1S2T3U4V5W6X", "Invalid column index or row position", system_utils::captureCallStack());
            }

            // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = columnIndex - 1;
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result, row, idx))
            {
                return 0; // Return 0 for NULL values (similar to JDBC)
            }

            const char *value = PQgetvalue(m_result, row, idx);
            try
            {
                return std::stoi(value);
            }
            catch (const std::exception &e)
            {
                throw DBException("7Y8Z9A0B1C2D", "Failed to convert value to int", system_utils::captureCallStack());
            }
        }

        int PostgreSQLResultSet::getInt(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("3E4F5G6H7I8J", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getInt(it->second + 1); // +1 because getInt(int) is 1-based
        }

        long PostgreSQLResultSet::getLong(int columnIndex)
        {
            if (!m_result || columnIndex < 1 || columnIndex > m_fieldCount || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                throw DBException("9K0L1M2N3O4P", "Invalid column index or row position", system_utils::captureCallStack());
            }

            int idx = columnIndex - 1;
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result, row, idx))
            {
                return 0;
            }

            const char *value = PQgetvalue(m_result, row, idx);
            try
            {
                return std::stol(value);
            }
            catch (const std::exception &e)
            {
                throw DBException("1W2X3Y4Z5A6B", "Failed to convert value to long", system_utils::captureCallStack());
            }
        }

        long PostgreSQLResultSet::getLong(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("7C8D9E0F1G2H", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getLong(it->second + 1);
        }

        double PostgreSQLResultSet::getDouble(int columnIndex)
        {
            if (!m_result || columnIndex < 1 || columnIndex > m_fieldCount || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                throw DBException("3I4J5K6L7M8N", "Invalid column index or row position", system_utils::captureCallStack());
            }

            int idx = columnIndex - 1;
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result, row, idx))
            {
                return 0.0;
            }

            const char *value = PQgetvalue(m_result, row, idx);
            try
            {
                return std::stod(value);
            }
            catch (const std::exception &e)
            {
                throw DBException("9O0P1Q2R3S4T", "Failed to convert value to double", system_utils::captureCallStack());
            }
        }

        double PostgreSQLResultSet::getDouble(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("5U6V7W8X9Y0Z", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getDouble(it->second + 1);
        }

        std::string PostgreSQLResultSet::getString(int columnIndex)
        {
            if (!m_result || columnIndex < 1 || columnIndex > m_fieldCount || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                throw DBException("1A2B3C4D5E6F", "Invalid column index or row position", system_utils::captureCallStack());
            }

            int idx = columnIndex - 1;
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result, row, idx))
            {
                return "";
            }

            return std::string(PQgetvalue(m_result, row, idx));
        }

        std::string PostgreSQLResultSet::getString(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("32DF0933F6D5", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getString(it->second + 1);
        }

        bool PostgreSQLResultSet::getBoolean(int columnIndex)
        {
            std::string value = getString(columnIndex);
            return (value == "t" || value == "true" || value == "1" || value == "TRUE" || value == "True");
        }

        bool PostgreSQLResultSet::getBoolean(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("7G8H9I0J1K2L", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getBoolean(it->second + 1);
        }

        bool PostgreSQLResultSet::isNull(int columnIndex)
        {
            if (!m_result || columnIndex < 1 || columnIndex > m_fieldCount || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                throw DBException("3M4N5O6P7Q8R", "Invalid column index or row position", system_utils::captureCallStack());
            }

            int idx = columnIndex - 1;
            int row = m_rowPosition - 1;

            return PQgetisnull(m_result, row, idx);
        }

        bool PostgreSQLResultSet::isNull(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("9S0T1U2V3W4X", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return isNull(it->second + 1);
        }

        std::vector<std::string> PostgreSQLResultSet::getColumnNames()
        {
            return m_columnNames;
        }

        size_t PostgreSQLResultSet::getColumnCount()
        {
            return m_fieldCount;
        }

        void PostgreSQLResultSet::close()
        {
            if (m_result)
            {
                PQclear(m_result);
                m_result = nullptr;
                m_rowPosition = 0;
                m_rowCount = 0;
                m_fieldCount = 0;
            }
        }

        // PostgreSQLPreparedStatement implementation
        // Helper method to process SQL and count parameters
        int PostgreSQLPreparedStatement::processSQL(std::string &sqlQuery) const
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

        PostgreSQLPreparedStatement::PostgreSQLPreparedStatement(PGconn *conn_handle, const std::string &sql_stmt, const std::string &stmt_name)
            : m_conn(conn_handle), m_sql(sql_stmt), m_stmtName(stmt_name)
        {
            if (!m_conn)
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

        PostgreSQLPreparedStatement::~PostgreSQLPreparedStatement()
        {
            close();
        }

        void PostgreSQLPreparedStatement::close()
        {
            if (m_prepared)
            {
                // Deallocate the prepared statement
                std::string deallocateSQL = "DEALLOCATE " + m_stmtName;
                PGresult *res = PQexec(m_conn, deallocateSQL.c_str());
                if (res)
                {
                    PQclear(res);
                }
                m_prepared = false;
            }
        }

        void PostgreSQLPreparedStatement::notifyConnClosing()
        {
            // Connection is closing, invalidate the statement without calling mysql_stmt_close
            // since the connection is already being destroyed
            this->close();
        }

        void PostgreSQLPreparedStatement::setInt(int parameterIndex, int value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("5Y6Z7A8B9C0D", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = std::to_string(value);
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 23;  // INT4OID
        }

        void PostgreSQLPreparedStatement::setLong(int parameterIndex, long value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("1E2F3G4H5I6J", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = std::to_string(value);
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 20;  // INT8OID
        }

        void PostgreSQLPreparedStatement::setDouble(int parameterIndex, double value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("7K8L9M0N1O2P", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = std::to_string(value);
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 701; // FLOAT8OID
        }

        void PostgreSQLPreparedStatement::setString(int parameterIndex, const std::string &value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("3Q4R5S6T7U8V", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = value;
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 25;  // TEXTOID
        }

        void PostgreSQLPreparedStatement::setBoolean(int parameterIndex, bool value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("9W0X1Y2Z3A4B", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = value ? "t" : "f"; // PostgreSQL uses 't' and 'f' for boolean values
            m_paramLengths[idx] = 1;
            m_paramFormats[idx] = 0; // Text format
            m_paramTypes[idx] = 16;  // BOOLOID
        }

        void PostgreSQLPreparedStatement::setNull(int parameterIndex, Types type)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("4A049129B485", "Invalid parameter index", system_utils::captureCallStack());
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
        }

        void PostgreSQLPreparedStatement::setDate(int parameterIndex, const std::string &value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("5C6D7E8F9G0H", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = value;
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0;  // Text format
            m_paramTypes[idx] = 1082; // DATEOID
        }

        void PostgreSQLPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("1I2J3K4L5M6N", "Invalid parameter index", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;
            m_paramValues[idx] = value;
            m_paramLengths[idx] = m_paramValues[idx].length();
            m_paramFormats[idx] = 0;  // Text format
            m_paramTypes[idx] = 1114; // TIMESTAMPOID
        }

        std::shared_ptr<ResultSet> PostgreSQLPreparedStatement::executeQuery()
        {
            if (!m_conn)
            {
                throw DBException("7O8P9Q0R1S2T", "Connection is invalid", system_utils::captureCallStack());
            }

            // Prepare the statement if not already prepared
            if (!m_prepared)
            {
                PGresult *prepareResult = PQprepare(m_conn, m_stmtName.c_str(), m_sql.c_str(), static_cast<int>(m_paramValues.size()), m_paramTypes.data());
                if (PQresultStatus(prepareResult) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(prepareResult);
                    PQclear(prepareResult);
                    throw DBException("3U4V5W6X7Y8Z", "Failed to prepare statement: " + error, system_utils::captureCallStack());
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
                m_conn,
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
                throw DBException("9A0B1C2D3E4F", "Failed to execute query: " + error, system_utils::captureCallStack());
            }

            auto resultSet = std::make_shared<PostgreSQLResultSet>(result);

            // Close the statement after execution (single-use)
            // This is safe because PQexecPrepared() copies all data to the PGresult
            close();

            return resultSet;
        }

        uint64_t PostgreSQLPreparedStatement::executeUpdate()
        {
            if (!m_conn)
            {
                throw DBException("5G6H7I8J9K0L", "Connection is invalid", system_utils::captureCallStack());
            }

            // Prepare the statement if not already prepared
            if (!m_prepared)
            {
                PGresult *prepareResult = PQprepare(m_conn, m_stmtName.c_str(), m_sql.c_str(), static_cast<int>(m_paramValues.size()), m_paramTypes.data());
                if (PQresultStatus(prepareResult) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(prepareResult);
                    PQclear(prepareResult);
                    throw DBException("1M2N3O4P5Q6R", "Failed to prepare statement: " + error, system_utils::captureCallStack());
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
                m_conn,
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
                throw DBException("7S8T9U0V1W2X", "Failed to execute update: " + error, system_utils::captureCallStack());
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

        bool PostgreSQLPreparedStatement::execute()
        {
            if (!m_conn)
            {
                throw DBException("3Y4Z5A6B7C8D", "Connection is invalid", system_utils::captureCallStack());
            }

            // Prepare the statement if not already prepared
            if (!m_prepared)
            {
                PGresult *prepareResult = PQprepare(m_conn, m_stmtName.c_str(), m_sql.c_str(), static_cast<int>(m_paramValues.size()), m_paramTypes.data());
                if (PQresultStatus(prepareResult) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(prepareResult);
                    PQclear(prepareResult);
                    throw DBException("9E0F1G2H3I4J", "Failed to prepare statement: " + error, system_utils::captureCallStack());
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
                m_conn,
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
                throw DBException("5K6L7M8N9O0P", "Failed to execute statement: " + error, system_utils::captureCallStack());
            }

            PQclear(result);
            return hasResultSet;
        }

        // BLOB support methods for PostgreSQLResultSet
        std::shared_ptr<Blob> PostgreSQLResultSet::getBlob(int columnIndex)
        {
            if (!m_result || columnIndex < 1 || columnIndex > m_fieldCount || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                throw DBException("5K6L7M8N9O0P", "Invalid column index or row position for getBlob", system_utils::captureCallStack());
            }

            // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = columnIndex - 1;
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result, row, idx))
            {
                return std::make_shared<PostgreSQLBlob>(nullptr);
            }

            // Check if the column is a bytea type
            Oid type = PQftype(m_result, idx);
            if (type != 17) // BYTEAOID
            {
                throw DBException("EA04B0D9155C", "Column is not a BLOB/bytea type", system_utils::captureCallStack());
            }

            // Get the binary data
            // const char *value = PQgetvalue(m_result, row, idx);
            // int length = PQgetlength(m_result, row, idx);

            // Create a vector with the data using our getBytes method
            // This will properly handle the hex format
            std::vector<uint8_t> data = getBytes(columnIndex);

            // Create a PostgreSQLBlob with the data
            return std::make_shared<PostgreSQLBlob>(nullptr, data);
        }

        std::shared_ptr<Blob> PostgreSQLResultSet::getBlob(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("392BEAA07684", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getBlob(it->second + 1); // +1 because getBlob(int) is 1-based
        }

        std::shared_ptr<InputStream> PostgreSQLResultSet::getBinaryStream(int columnIndex)
        {
            if (!m_result || columnIndex < 1 || columnIndex > m_fieldCount || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                throw DBException("FC94875EDF73", "Invalid column index or row position for getBinaryStream", system_utils::captureCallStack());
            }

            // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = columnIndex - 1;
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result, row, idx))
            {
                // Return an empty stream
                return std::make_shared<PostgreSQLInputStream>("", 0);
            }

            // Get the binary data using our getBytes method
            // This will properly handle the hex format
            std::vector<uint8_t> data = getBytes(columnIndex);

            // Create a new input stream with the data
            if (data.empty())
            {
                return std::make_shared<PostgreSQLInputStream>("", 0);
            }
            else
            {
                return std::make_shared<PostgreSQLInputStream>(
                    reinterpret_cast<const char *>(data.data()),
                    data.size());
            }
        }

        std::shared_ptr<InputStream> PostgreSQLResultSet::getBinaryStream(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("27EF08AD722D", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getBinaryStream(it->second + 1); // +1 because getBinaryStream(int) is 1-based
        }

        std::vector<uint8_t> PostgreSQLResultSet::getBytes(int columnIndex)
        {
            if (!m_result || columnIndex < 1 || columnIndex > m_fieldCount || m_rowPosition < 1 || m_rowPosition > m_rowCount)
            {
                throw DBException("D5E8D5D3A7A4", "Invalid column index or row position for getBytes", system_utils::captureCallStack());
            }

            // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = columnIndex - 1;
            int row = m_rowPosition - 1;

            if (PQgetisnull(m_result, row, idx))
            {
                return {};
            }

            // Get the binary data
            const char *value = PQgetvalue(m_result, row, idx);
            int length = PQgetlength(m_result, row, idx);

            // Check if the column is a bytea type
            Oid type = PQftype(m_result, idx);

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

        std::vector<uint8_t> PostgreSQLResultSet::getBytes(const std::string &columnName)
        {
            auto it = m_columnMap.find(columnName);
            if (it == m_columnMap.end())
            {
                throw DBException("599349A7DAA4", "Column not found: " + columnName, system_utils::captureCallStack());
            }

            return getBytes(it->second + 1); // +1 because getBytes(int) is 1-based
        }

        // BLOB support methods for PostgreSQLPreparedStatement
        void PostgreSQLPreparedStatement::setBlob(int parameterIndex, std::shared_ptr<Blob> x)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("3C2333857671", "Invalid parameter index for setBlob", system_utils::captureCallStack());
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
                return;
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
        }

        void PostgreSQLPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("D182B9C3A9CC", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack());
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
                return;
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
        }

        void PostgreSQLPreparedStatement::setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("13B0690421E5", "Invalid parameter index for setBinaryStream", system_utils::captureCallStack());
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
                return;
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
        }

        void PostgreSQLPreparedStatement::setBytes(int parameterIndex, const std::vector<uint8_t> &x)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("D6EC2CC8C12C", "Invalid parameter index for setBytes", system_utils::captureCallStack());
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
        }

        void PostgreSQLPreparedStatement::setBytes(int parameterIndex, const uint8_t *x, size_t length)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(m_paramValues.size()))
            {
                throw DBException("D8D28AD75097", "Invalid parameter index for setBytes", system_utils::captureCallStack());
            }

            int idx = parameterIndex - 1;

            if (!x)
            {
                // Set to NULL
                m_paramValues[idx] = "";
                m_paramLengths[idx] = 0;
                m_paramFormats[idx] = 0; // Text format
                m_paramTypes[idx] = 17;  // BYTEAOID
                return;
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
        }

        // PostgreSQLConnection implementation
        PostgreSQLConnection::PostgreSQLConnection(const std::string &host,
                                                   int port,
                                                   const std::string &database,
                                                   const std::string &user,
                                                   const std::string &password,
                                                   const std::map<std::string, std::string> &options)
            : m_conn(nullptr), m_closed(false), m_autoCommit(true), m_statementCounter(0),
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

            // Connect to the database
            m_conn = PQconnectdb(conninfo.str().c_str());
            if (PQstatus(m_conn) != CONNECTION_OK)
            {
                std::string error = PQerrorMessage(m_conn);
                PQfinish(m_conn);
                m_conn = nullptr;
                throw DBException("1Q2R3S4T5U6V", "Failed to connect to PostgreSQL: " + error, system_utils::captureCallStack());
            }

            // Set up a notice processor to suppress NOTICE messages
            PQsetNoticeProcessor(m_conn, [](void *, const char *)
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

        PostgreSQLConnection::~PostgreSQLConnection()
        {
            close();
        }

        void PostgreSQLConnection::close()
        {
            if (!m_closed && m_conn)
            {

                // Notify all active statements that connection is closing
                {
                    std::lock_guard<std::mutex> lock(m_statementsMutex);
                    for (auto &stmt : m_activeStatements)
                    {
                        // if (auto stmt = weakStmt.lock())
                        if (stmt)
                        {
                            stmt->notifyConnClosing();
                        }
                    }
                    m_activeStatements.clear();
                }

                PQfinish(m_conn);
                m_conn = nullptr;
                m_closed = true;

                // Sleep for 5ms to avoid problems with concurrency and memory stability
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        bool PostgreSQLConnection::isClosed()
        {
            return m_closed;
        }

        void PostgreSQLConnection::returnToPool()
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

        bool PostgreSQLConnection::isPooled()
        {
            return false;
        }

        std::shared_ptr<PreparedStatement> PostgreSQLConnection::prepareStatement(const std::string &sql)
        {
            if (m_closed || !m_conn)
            {
                throw DBException("7W8X9Y0Z1A2B", "Connection is closed", system_utils::captureCallStack());
            }

            // Generate a unique statement name and pass it to the prepared statement
            std::string stmtName = generateStatementName();
            auto stmt = std::make_shared<PostgreSQLPreparedStatement>(m_conn, sql, stmtName);

            registerStatement(stmt);

            return stmt;
        }

        std::shared_ptr<ResultSet> PostgreSQLConnection::executeQuery(const std::string &sql)
        {
            if (m_closed || !m_conn)
            {
                throw DBException("3C4D5E6F7G8H", "Connection is closed", system_utils::captureCallStack());
            }

            PGresult *result = PQexec(m_conn, sql.c_str());
            if (PQresultStatus(result) != PGRES_TUPLES_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw DBException("9I0J1K2L3M4N", "Query failed: " + error, system_utils::captureCallStack());
            }

            return std::make_shared<PostgreSQLResultSet>(result);
        }

        uint64_t PostgreSQLConnection::executeUpdate(const std::string &sql)
        {
            if (m_closed || !m_conn)
            {
                throw DBException("5O6P7Q8R9S0T", "Connection is closed", system_utils::captureCallStack());
            }

            PGresult *result = PQexec(m_conn, sql.c_str());
            if (PQresultStatus(result) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw DBException("1U2V3W4X5Y6Z", "Update failed: " + error, system_utils::captureCallStack());
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

        void PostgreSQLConnection::setAutoCommit(bool autoCommitFlag)
        {
            if (m_closed || !m_conn)
            {
                throw DBException("7A8B9C0D1E2F", "Connection is closed", system_utils::captureCallStack());
            }

            // PostgreSQL: BEGIN starts a transaction, COMMIT ends it
            // If autoCommit is true, we don't need to do anything special
            // If autoCommit is false, we need to start a transaction if we're not already in one
            if (this->m_autoCommit && !autoCommitFlag)
            {
                // Start a transaction
                std::string beginCmd = "BEGIN";

                // For SERIALIZABLE isolation, we need to ensure the snapshot is acquired immediately
                // by using a READ ONLY DEFERRABLE transaction when possible
                if (m_isolationLevel == TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
                {
                    beginCmd = "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE";

                    // Execute a dummy SELECT to force snapshot acquisition immediately
                    PGresult *dummyResult = PQexec(m_conn, beginCmd.c_str());
                    if (PQresultStatus(dummyResult) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(dummyResult);
                        PQclear(dummyResult);
                        throw DBException("3G4H5I6J7K8L", "Failed to start SERIALIZABLE transaction: " + error, system_utils::captureCallStack());
                    }
                    PQclear(dummyResult);

                    // Force snapshot acquisition with a dummy query
                    PGresult *snapshotResult = PQexec(m_conn, "SELECT 1");
                    if (PQresultStatus(snapshotResult) != PGRES_TUPLES_OK)
                    {
                        std::string error = PQresultErrorMessage(snapshotResult);
                        PQclear(snapshotResult);
                        throw DBException("9M0N1O2P3Q4R", "Failed to acquire snapshot: " + error, system_utils::captureCallStack());
                    }
                    PQclear(snapshotResult);
                    return; // We've already started the transaction and acquired the snapshot
                }
                else
                {
                    // Standard BEGIN for other isolation levels
                    PGresult *result = PQexec(m_conn, beginCmd.c_str());
                    if (PQresultStatus(result) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(result);
                        PQclear(result);
                        throw DBException("5S6T7U8V9W0X", "Failed to start transaction: " + error, system_utils::captureCallStack());
                    }
                    PQclear(result);
                }
            }
            else if (!this->m_autoCommit && autoCommitFlag)
            {
                // Commit the current transaction
                PGresult *result = PQexec(m_conn, "COMMIT");
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    throw DBException("1Y2Z3A4B5C6D", "Failed to commit transaction: " + error, system_utils::captureCallStack());
                }
                PQclear(result);
            }

            this->m_autoCommit = autoCommitFlag;
        }

        bool PostgreSQLConnection::getAutoCommit()
        {
            return m_autoCommit;
        }

        void PostgreSQLConnection::commit()
        {
            if (m_closed || !m_conn)
            {
                throw DBException("7E8F9G0H1I2J", "Connection is closed", system_utils::captureCallStack());
            }

            PGresult *result = PQexec(m_conn, "COMMIT");
            if (PQresultStatus(result) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw DBException("3K4L5M6N7O8P", "Commit failed: " + error, system_utils::captureCallStack());
            }
            PQclear(result);

            // Start a new transaction if auto-commit is disabled
            if (!m_autoCommit)
            {
                result = PQexec(m_conn, "BEGIN");
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    throw DBException("9Q0R1S2T3U4V", "Failed to start transaction: " + error, system_utils::captureCallStack());
                }
                PQclear(result);
            }
        }

        void PostgreSQLConnection::rollback()
        {
            if (m_closed || !m_conn)
            {
                throw DBException("5W6X7Y8Z9A0B", "Connection is closed", system_utils::captureCallStack());
            }

            PGresult *result = PQexec(m_conn, "ROLLBACK");
            if (PQresultStatus(result) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw DBException("1C2D3E4F5G6H", "Rollback failed: " + error, system_utils::captureCallStack());
            }
            PQclear(result);

            // Start a new transaction if auto-commit is disabled
            if (!m_autoCommit)
            {
                result = PQexec(m_conn, "BEGIN");
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    throw DBException("7I8J9K0L1M2N", "Failed to start transaction: " + error, system_utils::captureCallStack());
                }
                PQclear(result);
            }
        }

        void PostgreSQLConnection::setTransactionIsolation(TransactionIsolationLevel level)
        {
            if (m_closed || !m_conn)
            {
                throw DBException("3O4P5Q6R7S8T", "Connection is closed", system_utils::captureCallStack());
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
                throw DBException("9U0V1W2X3Y4Z", "Unsupported transaction isolation level", system_utils::captureCallStack());
            }

            PGresult *result = PQexec(m_conn, query.c_str());
            if (PQresultStatus(result) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw DBException("5A6B7C8D9E0F", "Failed to set transaction isolation level: " + error, system_utils::captureCallStack());
            }
            PQclear(result);

            this->m_isolationLevel = level;

            // If we're in a transaction (autoCommit = false), we need to restart it
            // for the new isolation level to take effect
            if (!m_autoCommit)
            {
                result = PQexec(m_conn, "COMMIT");
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    throw DBException("1G2H3I4J5K6L", "Failed to commit transaction: " + error, system_utils::captureCallStack());
                }
                PQclear(result);

                // For SERIALIZABLE isolation, we need special handling
                if (m_isolationLevel == TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
                {
                    std::string beginCmd = "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                    result = PQexec(m_conn, beginCmd.c_str());
                    if (PQresultStatus(result) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(result);
                        PQclear(result);
                        throw DBException("7M8N9O0P1Q2R", "Failed to start SERIALIZABLE transaction: " + error, system_utils::captureCallStack());
                    }
                    PQclear(result);

                    // Force snapshot acquisition with a dummy query
                    PGresult *snapshotResult = PQexec(m_conn, "SELECT 1");
                    if (PQresultStatus(snapshotResult) != PGRES_TUPLES_OK)
                    {
                        std::string error = PQresultErrorMessage(snapshotResult);
                        PQclear(snapshotResult);
                        throw DBException("3S4T5U6V7W8X", "Failed to acquire snapshot: " + error, system_utils::captureCallStack());
                    }
                    PQclear(snapshotResult);
                }
                else
                {
                    // Standard BEGIN for other isolation levels
                    result = PQexec(m_conn, "BEGIN");
                    if (PQresultStatus(result) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(result);
                        PQclear(result);
                        throw DBException("9Y0Z1A2B3C4D", "Failed to start transaction: " + error, system_utils::captureCallStack());
                    }
                    PQclear(result);
                }
            }
        }

        TransactionIsolationLevel PostgreSQLConnection::getTransactionIsolation()
        {
            if (m_closed || !m_conn)
            {
                throw DBException("5E6F7G8H9I0J", "Connection is closed", system_utils::captureCallStack());
            }

            // Query the current isolation level
            PGresult *result = PQexec(m_conn, "SHOW transaction_isolation");
            if (PQresultStatus(result) != PGRES_TUPLES_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw DBException("3W4X5Y6Z7A8B", "Failed to get transaction isolation level: " + error, system_utils::captureCallStack());
            }

            if (PQntuples(result) == 0)
            {
                PQclear(result);
                throw DBException("9C0D1E2F3G4H", "Failed to fetch transaction isolation level", system_utils::captureCallStack());
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

        std::string PostgreSQLConnection::generateStatementName()
        {
            std::stringstream ss;
            ss << "stmt_" << m_statementCounter++;
            return ss.str();
        }

        void PostgreSQLConnection::registerStatement(std::shared_ptr<PostgreSQLPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            // m_activeStatements.insert(std::weak_ptr<MySQLPreparedStatement>(stmt));
            m_activeStatements.insert(stmt);
        }

        void PostgreSQLConnection::unregisterStatement(std::shared_ptr<PostgreSQLPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(m_statementsMutex);
            // m_activeStatements.erase(std::weak_ptr<MySQLPreparedStatement>(stmt));
            m_activeStatements.erase(stmt);
        }

        std::string PostgreSQLConnection::getURL() const
        {
            return m_url;
        }

        // PostgreSQLDriver implementation
        PostgreSQLDriver::PostgreSQLDriver()
        {
            // PostgreSQL doesn't require explicit initialization like MySQL
        }

        PostgreSQLDriver::~PostgreSQLDriver()
        {
            // Also call PQfinish with nullptr as a fallback
            PQfinish(nullptr);

            // Sleep a bit more to ensure all resources are properly released
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        std::shared_ptr<Connection> PostgreSQLDriver::connect(const std::string &url,
                                                              const std::string &user,
                                                              const std::string &password,
                                                              const std::map<std::string, std::string> &options)
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
                    throw DBException("1K2L3M4N5O6P", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack());
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
                            throw DBException("7Q8R9S0T1U2V", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack());
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
                            throw DBException("5I6J7K8L9M0N", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack());
                        }

                        std::string portStr = temp.substr(hostEnd + 1, portEnd - hostEnd - 1);
                        try
                        {
                            port = std::stoi(portStr);
                        }
                        catch (...)
                        {
                            throw DBException("1O2P3Q4R5S6T", "Invalid port in URL: " + url, system_utils::captureCallStack());
                        }

                        // Extract database
                        database = temp.substr(portEnd + 1);
                    }
                }
                else
                {
                    throw DBException("7U8V9W0X1Y2Z", "Invalid PostgreSQL connection URL: " + url, system_utils::captureCallStack());
                }
            }

            return std::make_shared<PostgreSQLConnection>(host, port, database, user, password, options);
        }

        bool PostgreSQLDriver::acceptsURL(const std::string &url)
        {
            return url.substr(0, 21) == "cpp_dbc:postgresql://";
        }

        bool PostgreSQLDriver::parseURL(const std::string &url,
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
    } // namespace PostgreSQL
} // namespace cpp_dbc
