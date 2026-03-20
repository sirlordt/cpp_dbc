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

 @file connection_02.cpp
 @brief PostgreSQL database driver implementation - PostgreSQLDBConnection nothrow methods (part 1 - query and autocommit)

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

    // Nothrow API implementation for PostgreSQLDBConnection
    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> PostgreSQLDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7W8X9Y0Z1A2B", "Connection is closed", system_utils::captureCallStack()));
        }

        // Generate a unique statement name and pass weak_ptr to the connection
        // so the statement can safely detect when connection is closed
        std::string stmtName = generateStatementName(std::nothrow);
        auto self = weak_from_this();
        if (self.expired())
        {
            return cpp_dbc::unexpected<DBException>(DBException("7W8X9Y0Z1A2C", "Connection is not owned by shared_ptr", system_utils::captureCallStack()));
        }
        auto stmtResult = PostgreSQLDBPreparedStatement::create(std::nothrow, self, sql, stmtName);
        if (!stmtResult.has_value())
        {
            return cpp_dbc::unexpected<DBException>(stmtResult.error());
        }
        auto stmt = stmtResult.value();

        // Register the statement as weak_ptr - allows natural destruction when user releases reference.
        // Statements will be explicitly closed in returnToPool() or close() before connection reuse.
        // If registration fails the statement is NOT returned — destructor closes it.
        auto regResult = registerStatement(std::nothrow, std::weak_ptr<PostgreSQLDBPreparedStatement>(stmt));
        if (!regResult.has_value())
        {
            return cpp_dbc::unexpected(regResult.error());
        }

        return cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>(std::static_pointer_cast<RelationalDBPreparedStatement>(stmt));
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> PostgreSQLDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("3C4D5E6F7G8H", "Connection is closed", system_utils::captureCallStack()));
        }

        PGresultHandle result(PQexec(m_conn.get(), sql.c_str()));
        if (!result.get())
        {
            std::string error = PQerrorMessage(m_conn.get());
            return cpp_dbc::unexpected<DBException>(DBException("6NSV59A3RDC7", "Query failed (OOM): " + error, system_utils::captureCallStack()));
        }
        if (PQresultStatus(result.get()) != PGRES_TUPLES_OK)
        {
            std::string error = PQresultErrorMessage(result.get());
            return cpp_dbc::unexpected<DBException>(DBException("9I0J1K2L3M4N", "Query failed: " + error, system_utils::captureCallStack()));
        }

        auto selfWeak = weak_from_this();
        if (selfWeak.expired())
        {
            return cpp_dbc::unexpected<DBException>(DBException("9I0J1K2L3M4O", "Connection is not owned by shared_ptr", system_utils::captureCallStack()));
        }
        auto rsResult = PostgreSQLDBResultSet::create(std::nothrow, selfWeak, result.release());
        if (!rsResult.has_value())
        {
            return cpp_dbc::unexpected(rsResult.error());
        }
        auto rs = rsResult.value();

        // Registration with the connection is done inside create() → initialize()
        return cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>(std::static_pointer_cast<RelationalDBResultSet>(rs));
    }

    cpp_dbc::expected<uint64_t, DBException> PostgreSQLDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5O6P7Q8R9S0T", "Connection is closed", system_utils::captureCallStack()));
        }

        PGresultHandle result(PQexec(m_conn.get(), sql.c_str()));
        if (!result.get())
        {
            std::string error = PQerrorMessage(m_conn.get());
            return cpp_dbc::unexpected<DBException>(DBException("BZXECC4BPVXM", "Update failed (OOM): " + error, system_utils::captureCallStack()));
        }
        if (PQresultStatus(result.get()) != PGRES_COMMAND_OK)
        {
            std::string error = PQresultErrorMessage(result.get());
            return cpp_dbc::unexpected<DBException>(DBException("1U2V3W4X5Y6Z", "Update failed: " + error, system_utils::captureCallStack()));
        }

        // Get the number of affected rows
        const char *affectedRows = PQcmdTuples(result.get());
        uint64_t rowCount = 0;
        if (affectedRows && affectedRows[0] != '\0')
        {
            std::string_view affectedRowsView(affectedRows);
            auto [ptr, ec] = std::from_chars(affectedRowsView.data(), affectedRowsView.data() + affectedRowsView.size(), rowCount);
            if (ec != std::errc{})
            {
                return cpp_dbc::unexpected<DBException>(DBException("JH2IMLZPHQKD", "Failed to parse affected row count", system_utils::captureCallStack()));
            }
        }

        return rowCount;
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::setAutoCommit(std::nothrow_t, bool autoCommitFlag) noexcept
    {
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("R4S5T6U7V8W9", "Connection is closed", system_utils::captureCallStack()));
        }

        // Only take action if the flag is actually changing
        if (this->m_autoCommit != autoCommitFlag)
        {
            if (!autoCommitFlag)
            {
                // Disabling autoCommit — start an explicit transaction first.
                // PostgreSQL is always in autocommit mode until a transaction is started.
                // Only flip the flag after BEGIN succeeds to avoid inconsistent state.
                auto result = beginTransaction(std::nothrow);
                if (!result.has_value())
                {
                    return cpp_dbc::unexpected<DBException>(result.error());
                }

                this->m_autoCommit = false;
            }
            else
            {
                // If we're enabling autocommit, we must ensure any active transaction is ended
                if (m_transactionActive)
                {
                    auto result = commit(std::nothrow);
                    if (!result.has_value())
                    {
                        return cpp_dbc::unexpected<DBException>(result.error());
                    }
                }

                this->m_autoCommit = true;
            }
        }

        return {};
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9D3F5A7C2E8B", "Connection is closed", system_utils::captureCallStack()));
        }

        return m_autoCommit;
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("S5T6U7V8W9X0", "Connection is closed", system_utils::captureCallStack()));
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
            PGresultHandle dummyResult(PQexec(m_conn.get(), beginCmd.c_str()));
            if (!dummyResult.get())
            {
                std::string error = PQerrorMessage(m_conn.get());
                return cpp_dbc::unexpected<DBException>(DBException("M5UCTHN8L2QR", "Failed to start SERIALIZABLE transaction (OOM): " + error, system_utils::captureCallStack()));
            }
            if (PQresultStatus(dummyResult.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(dummyResult.get());
                return cpp_dbc::unexpected<DBException>(DBException("3G4H5I6J7K8L", "Failed to start SERIALIZABLE transaction: " + error, system_utils::captureCallStack()));
            }

            // Force snapshot acquisition with a dummy query
            PGresultHandle snapshotResult(PQexec(m_conn.get(), "SELECT 1"));
            if (!snapshotResult.get())
            {
                std::string error = PQerrorMessage(m_conn.get());
                [[maybe_unused]] PGresultHandle rollbackResult(PQexec(m_conn.get(), "ROLLBACK"));
                return cpp_dbc::unexpected<DBException>(DBException("VQ8V8AP5Y77Z", "Failed to acquire snapshot (OOM): " + error, system_utils::captureCallStack()));
            }
            if (PQresultStatus(snapshotResult.get()) != PGRES_TUPLES_OK)
            {
                std::string error = PQresultErrorMessage(snapshotResult.get());
                // BEGIN succeeded above, so the server is inside an open transaction.
                // Roll it back before returning to keep the server state consistent
                // with the client object (m_transactionActive is still false here).
                [[maybe_unused]] PGresultHandle rollbackResult(PQexec(m_conn.get(), "ROLLBACK"));
                return cpp_dbc::unexpected<DBException>(DBException("9M0N1O2P3Q4R", "Failed to acquire snapshot: " + error, system_utils::captureCallStack()));
            }
        }
        else
        {
            // Standard BEGIN for other isolation levels
            PGresultHandle result(PQexec(m_conn.get(), beginCmd.c_str()));
            if (!result.get())
            {
                std::string error = PQerrorMessage(m_conn.get());
                return cpp_dbc::unexpected<DBException>(DBException("H6D90ADC9XML", "Failed to start transaction (OOM): " + error, system_utils::captureCallStack()));
            }
            if (PQresultStatus(result.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result.get());
                return cpp_dbc::unexpected<DBException>(DBException("5S6T7U8V9W0X", "Failed to start transaction: " + error, system_utils::captureCallStack()));
            }
        }

        m_autoCommit = false;
        m_transactionActive = true;
        return true;
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5F8B2E9A7D3C", "Connection is closed", system_utils::captureCallStack()));
        }

        return m_transactionActive;
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
