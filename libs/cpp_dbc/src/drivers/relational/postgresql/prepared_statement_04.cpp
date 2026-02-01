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

 @file prepared_statement_04.cpp
 @brief PostgreSQL database driver implementation - PostgreSQLDBPreparedStatement nothrow methods (part 3 - execute and close)

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

    // Nothrow execute methods for PostgreSQLDBPreparedStatement
    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> PostgreSQLDBPreparedStatement::executeQuery(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_mutex);

            // Get the connection safely
            auto connPtr = m_conn.lock();
            if (!connPtr)
            {
                return cpp_dbc::unexpected<DBException>(DBException("J6K7L8M9N0O1", "PostgreSQL connection has been closed", system_utils::captureCallStack()));
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7D2E9B4F1C8A", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3F8C6A5D2B9E", "Unknown error in PostgreSQLDBPreparedStatement::executeQuery", system_utils::captureCallStack()));
        }
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
                return cpp_dbc::unexpected<DBException>(DBException("K7L8M9N0O1P2", "PostgreSQL connection has been closed", system_utils::captureCallStack()));
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
            const char *affectedRows = PQcmdTuples(result);
            uint64_t rowCount = 0;
            if (affectedRows && affectedRows[0] != '\0')
            {
                std::string_view affectedRowsView(affectedRows);
                std::from_chars(affectedRowsView.data(), affectedRowsView.data() + affectedRowsView.size(), rowCount);
            }

            PQclear(result);

            // Close the statement after execution (single-use)
            close();

            return rowCount;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9E2D7F5A3B8C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("6F1A9D3C7E2B", "Unknown error in PostgreSQLDBPreparedStatement::executeUpdate", system_utils::captureCallStack()));
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
                return cpp_dbc::unexpected<DBException>(DBException("L8M9N0O1P2Q3", "PostgreSQL connection has been closed", system_utils::captureCallStack()));
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7A9C5E2B8D3F", ex.what(), system_utils::captureCallStack()));
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3D8E5F2A9B7C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("6B9C2D7E4F1A", "Unknown error in PostgreSQLDBPreparedStatement::close", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
