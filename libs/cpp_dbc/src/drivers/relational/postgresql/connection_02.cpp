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
        try
        {
            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed.load(std::memory_order_acquire) || !m_conn)
            {
                return cpp_dbc::unexpected<DBException>(DBException("7W8X9Y0Z1A2B", "Connection is closed", system_utils::captureCallStack()));
            }

            // Generate a unique statement name and pass weak_ptr to the connection
            // so the statement can safely detect when connection is closed
            std::string stmtName = generateStatementName();
            auto stmtResult = PostgreSQLDBPreparedStatement::create(std::nothrow,
                std::weak_ptr<PostgreSQLDBConnection>(shared_from_this()), sql, stmtName);
            if (!stmtResult.has_value())
            {
                return cpp_dbc::unexpected<DBException>(stmtResult.error());
            }
            auto stmt = stmtResult.value();

            // Register the statement as weak_ptr - allows natural destruction when user releases reference
            // Statements will be explicitly closed in returnToPool() or close() before connection reuse
            [[maybe_unused]] auto regResult = registerStatement(std::nothrow, std::weak_ptr<PostgreSQLDBPreparedStatement>(stmt));

            return cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>(std::static_pointer_cast<RelationalDBPreparedStatement>(stmt));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("1D3F5A7C9E2B", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("8B4D6F2A9C3E", "Unknown error in PostgreSQLDBConnection::prepareStatement", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> PostgreSQLDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed.load(std::memory_order_acquire) || !m_conn)
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

            auto rsResult = PostgreSQLDBResultSet::create(std::nothrow,
                std::weak_ptr<PostgreSQLDBConnection>(shared_from_this()), result);
            if (!rsResult.has_value())
            {
                return cpp_dbc::unexpected(rsResult.error());
            }
            auto rs = rsResult.value();

            // Register the result set for lifecycle management
            [[maybe_unused]] auto regResult = registerResultSet(std::nothrow, std::weak_ptr<PostgreSQLDBResultSet>(rs));

            return cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>(std::static_pointer_cast<RelationalDBResultSet>(rs));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("PG3J1K2L3M4N", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("PG4K2L3M4N5O", "Unknown error in PostgreSQLDBConnection::executeQuery", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> PostgreSQLDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed.load(std::memory_order_acquire) || !m_conn)
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
            const char *affectedRows = PQcmdTuples(result);
            uint64_t rowCount = 0;
            if (affectedRows && affectedRows[0] != '\0')
            {
                std::string_view affectedRowsView(affectedRows);
                std::from_chars(affectedRowsView.data(), affectedRowsView.data() + affectedRowsView.size(), rowCount);
            }

            PQclear(result);
            return rowCount;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9A7C3E5B2D8F", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("4F1D8B6E2A9C", "Unknown error in PostgreSQLDBConnection::executeUpdate", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::setAutoCommit(std::nothrow_t, bool autoCommitFlag) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed.load(std::memory_order_acquire) || !m_conn)
            {
                return cpp_dbc::unexpected<DBException>(DBException("R4S5T6U7V8W9", "Connection is closed", system_utils::captureCallStack()));
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
                    if (!result.has_value())
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5D7F9A2E8B3C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("1B6E9C4A8D2F", "Unknown error in PostgreSQLDBConnection::setAutoCommit", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9D3F5A7C2E8B", "Connection is closed", system_utils::captureCallStack()));
        }

        return m_autoCommit;
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed.load(std::memory_order_acquire) || !m_conn)
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("4B8D2F6A9E3C", ex.what(), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected<DBException>(DBException("7E1C9A5F2D8B", "Unknown error in PostgreSQLDBConnection::beginTransaction", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5F8B2E9A7D3C", "Connection is closed", system_utils::captureCallStack()));
        }

        return m_transactionActive;
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
