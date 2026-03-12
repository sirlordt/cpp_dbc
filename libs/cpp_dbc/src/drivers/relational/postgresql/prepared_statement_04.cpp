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
        PG_STMT_LOCK_OR_RETURN("83S78M5O9PFH", "Statement closed");

        // Get PGconn* safely through the connection
        auto connResult = getPGConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected<DBException>(connResult.error());
        }
        PGconn *pgConn = connResult.value();

        // Prepare the statement if not already prepared
        if (!m_prepared)
        {
            PGresult *prepareResult = PQprepare(pgConn, m_stmtName.c_str(), m_sql.c_str(), static_cast<int>(m_paramValues.size()), m_paramTypes.data());
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
            pgConn,
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
            return cpp_dbc::unexpected<DBException>(DBException("7ZYR9G76KRKT", "Failed to execute query: " + error, system_utils::captureCallStack()));
        }

        // Create ResultSet with connection reference for lifecycle management
        auto rsResult = PostgreSQLDBResultSet::create(std::nothrow, m_connection, result);
        if (!rsResult.has_value())
        {
            return cpp_dbc::unexpected(rsResult.error());
        }

        // Close the statement after execution (single-use)
        // This is safe because PQexecPrepared() copies all data to the PGresult
        [[maybe_unused]] auto closeResult = close(std::nothrow);

        return cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>{rsResult.value()};
    }

    cpp_dbc::expected<uint64_t, DBException> PostgreSQLDBPreparedStatement::executeUpdate(std::nothrow_t) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("83S78M5O9PFH", "Statement closed");

        // Get PGconn* safely through the connection
        auto connResult = getPGConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected<DBException>(connResult.error());
        }
        PGconn *pgConn = connResult.value();

        // Prepare the statement if not already prepared
        if (!m_prepared)
        {
            PGresult *prepareResult = PQprepare(pgConn, m_stmtName.c_str(), m_sql.c_str(), static_cast<int>(m_paramValues.size()), m_paramTypes.data());
            if (PQresultStatus(prepareResult) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(prepareResult);
                PQclear(prepareResult);
                return cpp_dbc::unexpected<DBException>(DBException("LV8VBI4QT5XS", "Failed to prepare statement: " + error, system_utils::captureCallStack()));
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
            pgConn,
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
        [[maybe_unused]] auto closeResult = close(std::nothrow);

        return rowCount;
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBPreparedStatement::execute(std::nothrow_t) noexcept
    {
        PG_STMT_LOCK_OR_RETURN("83S78M5O9PFH", "Statement closed");

        // Get PGconn* safely through the connection
        auto connResult = getPGConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected<DBException>(connResult.error());
        }
        PGconn *pgConn = connResult.value();

        // Prepare the statement if not already prepared
        if (!m_prepared)
        {
            PGresult *prepareResult = PQprepare(pgConn, m_stmtName.c_str(), m_sql.c_str(), static_cast<int>(m_paramValues.size()), m_paramTypes.data());
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
            pgConn,
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

    cpp_dbc::expected<void, DBException> PostgreSQLDBPreparedStatement::close(std::nothrow_t) noexcept
    {
        // CRITICAL: Must hold the shared connection mutex to prevent race conditions.
        // PQexec(DEALLOCATE) uses the PGconn* connection, so concurrent access from
        // another thread (e.g., connection pool validation) causes protocol errors.
        PG_STMT_LOCK_OR_RETURN("83S78M5O9PFH", "Statement closed");

        if (m_prepared)
        {
            // Try to deallocate the prepared statement if connection is still valid
            auto conn = m_connection.lock();
            if (conn && conn->m_conn)
            {
                std::string deallocateSQL = "DEALLOCATE " + m_stmtName;
                PGresult *res = PQexec(conn->m_conn.get(), deallocateSQL.c_str());
                if (res)
                {
                    PQclear(res);
                }
            }
            m_prepared = false;
        }

        return {};
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
