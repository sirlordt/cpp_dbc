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

 @file connection_03.cpp
 @brief PostgreSQL database driver implementation - PostgreSQLDBConnection nothrow methods (part 2 - transactions)

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

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::commit(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9D2E7F5A3C8B", ex.what(), system_utils::captureCallStack()));
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
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("8D2A7F4E9B3C", ex.what(), system_utils::captureCallStack()));
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
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_conn)
            {
                return cpp_dbc::unexpected<DBException>(DBException("3O4P5Q6R7S8T", "Connection is closed", system_utils::captureCallStack()));
            }

            using enum TransactionIsolationLevel;
            std::string query;
            switch (level)
            {
            case TRANSACTION_READ_UNCOMMITTED:
                // PostgreSQL treats READ UNCOMMITTED the same as READ COMMITTED
                query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
                break;
            case TRANSACTION_READ_COMMITTED:
                query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ COMMITTED";
                break;
            case TRANSACTION_REPEATABLE_READ:
                query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL REPEATABLE READ";
                break;
            case TRANSACTION_SERIALIZABLE:
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
                return cpp_dbc::unexpected<DBException>(DBException("3Q37JJHOWQJE", "Failed to set transaction isolation level: " + error, system_utils::captureCallStack()));
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
                if (m_isolationLevel == TRANSACTION_SERIALIZABLE)
                {
                    std::string beginCmd = "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                    result = PQexec(m_conn.get(), beginCmd.c_str());
                    if (PQresultStatus(result) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(result);
                        PQclear(result);
                        return cpp_dbc::unexpected<DBException>(DBException("V8W9X0Y1Z2A3", "Failed to start SERIALIZABLE transaction: " + error, system_utils::captureCallStack()));
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7D1B9E3C5A8F", ex.what(), system_utils::captureCallStack()));
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
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

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
            std::ranges::transform(levelLower, levelLower.begin(),
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected<DBException>(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected<DBException>(DBException("2D7E9C4B8A3F", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5F1C8D3A6B9E", "Unknown error in PostgreSQLDBConnection::getTransactionIsolation", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::close(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (!m_closed && m_conn)
            {
                // Close all active statements before closing the connection
                // This ensures statement deallocation while we have exclusive access
                [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

                // Sleep for 25ms to avoid problems with concurrency
                std::this_thread::sleep_for(std::chrono::milliseconds(25));

                // shared_ptr will automatically call PQfinish via PGconnDeleter
                m_conn.reset();
                m_closed = true;
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("IT035Z2NF0PP",
                                                   std::string("close failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("TCL2VGCUPB11",
                                                   "close failed with unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::reset(std::nothrow_t) noexcept
    {
        try
        {
            DB_DRIVER_LOCK_GUARD(*m_connMutex);

            if (m_closed || !m_conn)
            {
                return {}; // Nothing to reset if already closed
            }

            // Close all active statements
            [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

            // Rollback any active transaction
            auto txActive = transactionActive(std::nothrow);
            if (txActive.has_value() && txActive.value())
            {
                rollback(std::nothrow);
            }

            // Reset auto-commit to true
            setAutoCommit(std::nothrow, true);

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("Z0V8CQDUCOSY",
                                                   std::string("reset failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("R0EWEV8IUFL0",
                                                   "reset failed with unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed;
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        try
        {
            // CRITICAL: Close all active statements BEFORE making connection available
            // closeAllStatements() acquires m_connMutex internally
            [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

            // Restore autocommit for the next user of this connection
            if (!m_autoCommit)
            {
                setAutoCommit(std::nothrow, true);
            }

            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("1LMUYDKRLMIT",
                                                   std::string("returnToPool failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("RLOO527LNW3S",
                                                   "returnToPool failed with unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBConnection::getURL(std::nothrow_t) const noexcept
    {
        return m_url;
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::prepareForPoolReturn(
        std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        // Delegate to reset() which closes statements, rolls back, and resets autocommit
        auto resetResult = reset(std::nothrow);
        if (!resetResult.has_value())
        {
            return resetResult;
        }

        // Restore transaction isolation level if requested by the pool
        if (isolationLevel != TransactionIsolationLevel::TRANSACTION_NONE)
        {
            auto isoResult = getTransactionIsolation(std::nothrow);
            if (!isoResult.has_value())
            {
                return cpp_dbc::unexpected(isoResult.error());
            }
            if (isoResult.value() != isolationLevel)
            {
                auto setResult = setTransactionIsolation(std::nothrow, isolationLevel);
                if (!setResult.has_value())
                {
                    return setResult;
                }
            }
        }

        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        // No-op for PostgreSQL: no MVCC snapshot refresh needed
        return {};
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
