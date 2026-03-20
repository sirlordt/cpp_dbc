/**

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

 @file connection_04.cpp
 @brief Firebird database driver implementation - FirebirdDBConnection protected overrides and nothrow methods (group 3)

*/

#include "cpp_dbc/drivers/relational/driver_firebird.hpp"

#if USE_FIREBIRD

#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <cmath>

#include "firebird_internal.hpp"

namespace cpp_dbc::Firebird
{

    // ============================================================================
    // Protected overrides — pool lifecycle
    // ============================================================================

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::prepareForPoolReturn(
        std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        // Reset connection state: close all statements/resultsets and rollback
        auto resetResult = reset(std::nothrow);
        if (!resetResult.has_value())
        {
            FIREBIRD_DEBUG("prepareForPoolReturn(nothrow): Failed to reset: %s",
                           resetResult.error().what_s().data());
            // Continue anyway - try to at least reset autoCommit
        }

        // Reset auto-commit to true (default state for pooled connections)
        [[maybe_unused]] auto setResult = setAutoCommit(std::nothrow, true);

        // Restore transaction isolation level if requested by the pool
        if (isolationLevel != TransactionIsolationLevel::TRANSACTION_NONE)
        {
            auto isoResult = getTransactionIsolation(std::nothrow);
            if (!isoResult.has_value())
            {
                FIREBIRD_DEBUG("prepareForPoolReturn(nothrow): Failed to get isolation: %s",
                               isoResult.error().what_s().data());
                return cpp_dbc::unexpected(isoResult.error());
            }
            if (isoResult.value() != isolationLevel)
            {
                auto setIsoResult = setTransactionIsolation(std::nothrow, isolationLevel);
                if (!setIsoResult.has_value())
                {
                    FIREBIRD_DEBUG("prepareForPoolReturn(nothrow): Failed to restore isolation: %s",
                                   setIsoResult.error().what_s().data());
                    return setIsoResult;
                }
            }
        }

        return {};
    }

    // 2026-03-07T00:00:00Z
    // Bug: Firebird MVCC snapshots are taken when a transaction starts. Pooled
    // connections keep stale snapshots from pool-creation time, so DDL changes
    // committed later (RECREATE TABLE, etc.) are invisible → "table id not defined".
    // Solution: In prepareForBorrow(), rollback the current transaction and start
    // a fresh one to acquire an up-to-date MVCC snapshot. The error is caused by
    // stale snapshots, not by closeAllStatements() in prepareForPoolReturn().
    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("HR0QTJ2GVXFT", "Connection closed");

        if (m_closed.load(std::memory_order_seq_cst))
        {
            return {};
        }

        // Refresh the MVCC snapshot by rolling back and starting a new transaction.
        // This ensures the borrowed connection can see all DDL changes committed
        // after the previous transaction started.
        if (m_tr && m_autoCommit)
        {
            FIREBIRD_DEBUG("FirebirdConnection::prepareForBorrow(nothrow) - Refreshing MVCC snapshot");
            [[maybe_unused]] auto rollbackResult = rollback(std::nothrow);
        }

        return {};
    }

    // ============================================================================
    // NOTHROW API — group 3: setTransactionIsolation, getTransactionIsolation,
    //               getServerVersion, getServerInfo
    // ============================================================================

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::setTransactionIsolation(nothrow) - Starting");
        FIREBIRD_DEBUG("  Current level: %d", static_cast<int>(m_isolationLevel));
        FIREBIRD_DEBUG("  New level: %d", static_cast<int>(level));

        FIREBIRD_CONNECTION_LOCK_OR_RETURN("67HASSXYPRLT", "Connection closed");

        // If the isolation level is already set to the requested level, do nothing
        if (m_isolationLevel == level)
        {
            FIREBIRD_DEBUG("  No change needed, returning");
            return {};
        }

        // If a transaction is active, we need to end it first, change the isolation level,
        // and restart the transaction with the new isolation level
        bool hadActiveTransaction = m_transactionActive;
        if (m_transactionActive)
        {
            FIREBIRD_DEBUG("  Transaction is active, ending it first");
            // Commit the current transaction (or rollback if autocommit is off)
            if (m_autoCommit)
            {
                FIREBIRD_DEBUG("  AutoCommit is enabled, committing current transaction");
                auto endResult = endTransaction(std::nothrow, true);
                if (!endResult.has_value())
                {
                    return cpp_dbc::unexpected(endResult.error());
                }
            }
            else
            {
                FIREBIRD_DEBUG("  AutoCommit is disabled, rolling back current transaction");
                auto endResult = endTransaction(std::nothrow, false);
                if (!endResult.has_value())
                {
                    return cpp_dbc::unexpected(endResult.error());
                }
            }
        }

        m_isolationLevel = level;
        FIREBIRD_DEBUG("  Isolation level set to: %d", static_cast<int>(m_isolationLevel));

        // Restart the transaction if we had one active and autocommit is on
        if (hadActiveTransaction && m_autoCommit)
        {
            FIREBIRD_DEBUG("  Restarting transaction with new isolation level");
            auto startResult = startTransaction(std::nothrow);
            if (!startResult.has_value())
            {
                FIREBIRD_DEBUG("  Failed to restart transaction: %s", startResult.error().what_s().data());
                return cpp_dbc::unexpected(startResult.error());
            }
        }

        FIREBIRD_DEBUG("FirebirdConnection::setTransactionIsolation(nothrow) - Done");
        return {};
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    FirebirdDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("W27RDYMQ9TXH", "Connection closed");

        FIREBIRD_DEBUG("FirebirdConnection::getTransactionIsolation(nothrow) - Returning %d", static_cast<int>(m_isolationLevel));
        return m_isolationLevel;
    }

    cpp_dbc::expected<std::string, DBException> FirebirdDBConnection::getServerVersion(std::nothrow_t) noexcept
    {
        auto queryResult = executeQuery(std::nothrow,
            "SELECT RDB$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') FROM RDB$DATABASE");
        if (!queryResult.has_value())
        {
            return cpp_dbc::unexpected(queryResult.error());
        }

        auto &rs = queryResult.value();
        auto nextResult = rs->next(std::nothrow);
        if (!nextResult.has_value() || !nextResult.value())
        {
            auto closeResult = rs->close(std::nothrow);
            return cpp_dbc::unexpected(DBException(
                "J23T9D56C0OJ",
                "Failed to retrieve Firebird server version",
                system_utils::captureCallStack()));
        }

        auto versionResult = rs->getString(std::nothrow, 0);
        auto closeResult = rs->close(std::nothrow);

        if (!versionResult.has_value())
        {
            return cpp_dbc::unexpected(versionResult.error());
        }

        return versionResult.value();
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> FirebirdDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        std::map<std::string, std::string> info;

        auto versionResult = getServerVersion(std::nothrow);
        if (versionResult.has_value())
        {
            info["ServerVersion"] = versionResult.value();
        }

        auto queryResult = executeQuery(std::nothrow,
            "SELECT MON$ODS_MAJOR, MON$ODS_MINOR, MON$PAGE_SIZE, MON$SQL_DIALECT "
            "FROM MON$DATABASE");
        if (queryResult.has_value())
        {
            auto &rs = queryResult.value();
            auto nextResult = rs->next(std::nothrow);
            if (nextResult.has_value() && nextResult.value())
            {
                auto odsMajor = rs->getString(std::nothrow, 0);
                if (odsMajor.has_value())
                {
                    info["ODSMajorVersion"] = odsMajor.value();
                }
                auto odsMinor = rs->getString(std::nothrow, 1);
                if (odsMinor.has_value())
                {
                    info["ODSMinorVersion"] = odsMinor.value();
                }
                auto pageSize = rs->getString(std::nothrow, 2);
                if (pageSize.has_value())
                {
                    info["PageSize"] = pageSize.value();
                }
                auto sqlDialect = rs->getString(std::nothrow, 3);
                if (sqlDialect.has_value())
                {
                    info["SQLDialect"] = sqlDialect.value();
                }
            }
            [[maybe_unused]] auto closeResult = rs->close(std::nothrow);
        }

        return info;
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
