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
        DB_DRIVER_LOCK_GUARD(m_connMutex);

        if (m_closed.load(std::memory_order_acquire) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("7E8F9G0H1I2J", "Connection is closed", system_utils::captureCallStack()));
        }

        // If no transaction is active, nothing to commit
        if (!m_transactionActive)
        {
            return {};
        }

        PGresultHandle result(PQexec(m_conn.get(), "COMMIT"));
        if (PQresultStatus(result.get()) != PGRES_COMMAND_OK)
        {
            std::string error = PQresultErrorMessage(result.get());
            return cpp_dbc::unexpected<DBException>(DBException("3K4L5M6N7O8P", "Commit failed: " + error, system_utils::captureCallStack()));
        }

        m_transactionActive = false;
        m_autoCommit = true;

        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::rollback(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);

        if (m_closed.load(std::memory_order_acquire) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5W6X7Y8Z9A0B", "Connection is closed", system_utils::captureCallStack()));
        }

        // If no transaction is active, nothing to rollback
        if (!m_transactionActive)
        {
            return {};
        }

        PGresultHandle result(PQexec(m_conn.get(), "ROLLBACK"));
        if (PQresultStatus(result.get()) != PGRES_COMMAND_OK)
        {
            std::string error = PQresultErrorMessage(result.get());
            return cpp_dbc::unexpected<DBException>(DBException("1C2D3E4F5G6H", "Rollback failed: " + error, system_utils::captureCallStack()));
        }

        m_transactionActive = false;
        m_autoCommit = true;

        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);

        if (m_closed.load(std::memory_order_acquire) || !m_conn)
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

        {
            PGresultHandle result(PQexec(m_conn.get(), query.c_str()));
            if (PQresultStatus(result.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result.get());
                return cpp_dbc::unexpected<DBException>(DBException("3Q37JJHOWQJE", "Failed to set transaction isolation level: " + error, system_utils::captureCallStack()));
            }
        }

        this->m_isolationLevel = level;

        // If we're in a transaction (autoCommit = false), we need to restart it
        // for the new isolation level to take effect
        if (!m_autoCommit)
        {
            PGresultHandle commitResult(PQexec(m_conn.get(), "COMMIT"));
            if (PQresultStatus(commitResult.get()) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(commitResult.get());
                return cpp_dbc::unexpected<DBException>(DBException("1G2H3I4J5K6L", "Failed to commit transaction: " + error, system_utils::captureCallStack()));
            }

            // For SERIALIZABLE isolation, we need special handling
            if (m_isolationLevel == TRANSACTION_SERIALIZABLE)
            {
                std::string beginCmd = "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                PGresultHandle beginResult(PQexec(m_conn.get(), beginCmd.c_str()));
                if (PQresultStatus(beginResult.get()) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(beginResult.get());
                    return cpp_dbc::unexpected<DBException>(DBException("V8W9X0Y1Z2A3", "Failed to start SERIALIZABLE transaction: " + error, system_utils::captureCallStack()));
                }

                // Force snapshot acquisition with a dummy query
                PGresultHandle snapshotResult(PQexec(m_conn.get(), "SELECT 1"));
                if (PQresultStatus(snapshotResult.get()) != PGRES_TUPLES_OK)
                {
                    std::string error = PQresultErrorMessage(snapshotResult.get());
                    return cpp_dbc::unexpected<DBException>(DBException("3S4T5U6V7W8X", "Failed to acquire snapshot: " + error, system_utils::captureCallStack()));
                }
            }
            else
            {
                // Standard BEGIN for other isolation levels
                PGresultHandle beginResult(PQexec(m_conn.get(), "BEGIN"));
                if (PQresultStatus(beginResult.get()) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(beginResult.get());
                    return cpp_dbc::unexpected<DBException>(DBException("9Y0Z1A2B3C4D", "Failed to start transaction: " + error, system_utils::captureCallStack()));
                }
            }
        }

        return {};
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException> PostgreSQLDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);

        if (m_closed.load(std::memory_order_acquire) || !m_conn)
        {
            return cpp_dbc::unexpected<DBException>(DBException("5E6F7G8H9I0J", "Connection is closed", system_utils::captureCallStack()));
        }

        // Query the current isolation level
        PGresultHandle result(PQexec(m_conn.get(), "SHOW transaction_isolation"));
        if (PQresultStatus(result.get()) != PGRES_TUPLES_OK)
        {
            std::string error = PQresultErrorMessage(result.get());
            return cpp_dbc::unexpected<DBException>(DBException("3W4X5Y6Z7A8B", "Failed to get transaction isolation level: " + error, system_utils::captureCallStack()));
        }

        if (PQntuples(result.get()) == 0)
        {
            return cpp_dbc::unexpected<DBException>(DBException("9C0D1E2F3G4H", "Failed to fetch transaction isolation level", system_utils::captureCallStack()));
        }

        std::string level = PQgetvalue(result.get(), 0, 0);

        // Convert the string value to the enum - handle both formats
        std::string levelLower = level;
        // Convert to lowercase for case-insensitive comparison
        std::ranges::transform(levelLower, levelLower.begin(),
                               [](unsigned char c)
                               { return std::tolower(c); });

        if (levelLower == "read uncommitted" || levelLower == "read_uncommitted")
        {
            return TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED;
        }
        else if (levelLower == "read committed" || levelLower == "read_committed")
        {
            return TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;
        }
        else if (levelLower == "repeatable read" || levelLower == "repeatable_read")
        {
            return TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ;
        }
        else if (levelLower == "serializable")
        {
            return TransactionIsolationLevel::TRANSACTION_SERIALIZABLE;
        }
        else
        {
            return TransactionIsolationLevel::TRANSACTION_NONE;
        }
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::close(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);

        if (!m_closed.load(std::memory_order_acquire) && m_conn)
        {
            // Close all active result sets and statements before closing the connection
            // This ensures deallocation while we have exclusive access
            [[maybe_unused]] auto closeRsResult = closeAllResultSets(std::nothrow);
            [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

            // Sleep for 25ms to avoid problems with concurrency
            std::this_thread::sleep_for(std::chrono::milliseconds(25));

            // shared_ptr will automatically call PQfinish via PGconnDeleter
            m_conn.reset();
            m_closed.store(true, std::memory_order_release);

            // Unregister from the driver registry so getConnectionAlive() reflects
            // actual live connections. The owner_less m_self weak_ptr is used for
            // set lookup — raw 'this' would not match the set's comparator.
            PostgreSQLDBDriver::unregisterConnection(std::nothrow, m_self);
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::reset(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);

        if (m_closed.load(std::memory_order_acquire) || !m_conn)
        {
            return {}; // Nothing to reset if already closed
        }

        // Close all active result sets and statements
        [[maybe_unused]] auto closeRsResult = closeAllResultSets(std::nothrow);
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

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        // CRITICAL: Close all active result sets and statements BEFORE making connection available
        // closeAllResultSets/closeAllStatements acquire m_connMutex internally
        [[maybe_unused]] auto closeRsResult = closeAllResultSets(std::nothrow);
        [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

        // 2026-03-12T12:00:00Z
        // Bug: m_autoCommit is a plain bool (non-atomic) modified under m_connMutex in
        // setAutoCommit/beginTransaction/commit/rollback. Reading it without the lock
        // is a data race (undefined behavior) in multi-threaded builds.
        // Solution: Acquire m_connMutex before reading m_autoCommit. Safe with
        // recursive_mutex since setAutoCommit also acquires m_connMutex internally.
        DB_DRIVER_LOCK_GUARD(m_connMutex);

        // Restore autocommit for the next user of this connection
        if (!m_autoCommit)
        {
            setAutoCommit(std::nothrow, true);
        }

        return {};
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    // No try/catch: the only possible throw is std::bad_alloc from the
    // std::string copy, which is a death-sentence exception — no meaningful
    // recovery is possible, so std::terminate is the correct response.
    cpp_dbc::expected<std::string, DBException> PostgreSQLDBConnection::getURI(std::nothrow_t) const noexcept
    {
        return m_uri;
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

    // 2026-03-08T21:00:00Z
    // Bug: PQserverVersion() for PostgreSQL 10+ encodes as major*10000 + minor
    // (e.g. 160004 = version 16.4), not major*10000 + minor*100 + patch.
    // The old code produced "16.0.4" instead of "16.4".
    // Solution: Use two-component decoding (major.minor) for versions >= 10.
    std::string PostgreSQLDBConnection::formatServerVersion(std::nothrow_t, int version) const noexcept
    {
        if (version >= 100000)
        {
            int major = version / 10000;
            int minor = version % 10000;
            return std::to_string(major) + "." + std::to_string(minor);
        }
        // Pre-10: major*10000 + minor*100 + patch
        int major = version / 10000;
        int minor = (version / 100) % 100;
        int patch = version % 100;
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }

    cpp_dbc::expected<std::string, DBException> PostgreSQLDBConnection::getServerVersion(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);

        if (m_closed.load(std::memory_order_acquire) || !m_conn)
        {
            return cpp_dbc::unexpected(DBException(
                "36M3T260UN4J",
                "Connection is closed",
                system_utils::captureCallStack()));
        }

        return formatServerVersion(std::nothrow, PQserverVersion(m_conn.get()));
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> PostgreSQLDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);

        if (m_closed.load(std::memory_order_acquire) || !m_conn)
        {
            return cpp_dbc::unexpected(DBException(
                "IB9HNOSWLXD7",
                "Connection is closed",
                system_utils::captureCallStack()));
        }

        std::map<std::string, std::string> info;

        int version = PQserverVersion(m_conn.get());
        info["ServerVersion"] = formatServerVersion(std::nothrow, version);
        info["ServerVersionNumeric"] = std::to_string(version);

        int protocolVersion = PQprotocolVersion(m_conn.get());
        info["ProtocolVersion"] = std::to_string(protocolVersion);

        const char *serverEncoding = PQparameterStatus(m_conn.get(), "server_encoding");
        if (serverEncoding)
        {
            info["ServerEncoding"] = serverEncoding;
        }

        const char *clientEncoding = PQparameterStatus(m_conn.get(), "client_encoding");
        if (clientEncoding)
        {
            info["ClientEncoding"] = clientEncoding;
        }

        const char *timeZone = PQparameterStatus(m_conn.get(), "TimeZone");
        if (timeZone)
        {
            info["TimeZone"] = timeZone;
        }

        const char *intDateTimes = PQparameterStatus(m_conn.get(), "integer_datetimes");
        if (intDateTimes)
        {
            info["IntegerDatetimes"] = intDateTimes;
        }

        const char *stdConformingStrings = PQparameterStatus(m_conn.get(), "standard_conforming_strings");
        if (stdConformingStrings)
        {
            info["StandardConformingStrings"] = stdConformingStrings;
        }

        return info;
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
