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

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::close(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        // Close all active result sets and statements before closing the connection
        // This ensures deallocation while we have exclusive access
        [[maybe_unused]] auto closeRsResult = closeAllResultSets(std::nothrow);
        [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

        // 2026-03-19T00:00:00Z
        // Bug: Immediate teardown after closing statements/result sets races with
        // concurrent PostgreSQL connection cleanup paths (e.g., pool validation pings),
        // causing intermittent close-time failures under Helgrind/ThreadSanitizer.
        // Solution: Brief delay allows in-flight cleanup to finish before the native
        // PGconn handle is destroyed via m_conn.reset().
        std::this_thread::sleep_for(std::chrono::milliseconds(25));

        // shared_ptr will automatically call PQfinish via PGconnDeleter
        m_conn.reset();
        m_closed.store(true, std::memory_order_seq_cst);

        // Unregister from the driver registry so getConnectionAlive() reflects
        // actual live connections. The owner_less m_self weak_ptr is used for
        // set lookup — raw 'this' would not match the set's comparator.
        PostgreSQLDBDriver::unregisterConnection(std::nothrow, m_self);

        return {};
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::reset(std::nothrow_t) noexcept
    {
        POSTGRESQL_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        // Close all active result sets and statements
        auto closeRsResult = closeAllResultSets(std::nothrow);
        if (!closeRsResult.has_value())
        {
            POSTGRESQL_DEBUG("  reset: closeAllResultSets failed: %s", closeRsResult.error().what_s().data());
        }
        auto closeStmtsResult = closeAllStatements(std::nothrow);
        if (!closeStmtsResult.has_value())
        {
            POSTGRESQL_DEBUG("  reset: closeAllStatements failed: %s", closeStmtsResult.error().what_s().data());
        }

        // Rollback any active transaction
        auto txActive = transactionActive(std::nothrow);
        if (txActive.has_value() && txActive.value())
        {
            auto rbResult = rollback(std::nothrow);
            if (!rbResult.has_value())
            {
                POSTGRESQL_DEBUG("  reset: rollback failed: %s", rbResult.error().what_s().data());
            }
        }

        // Reset auto-commit to true
        auto acResult = setAutoCommit(std::nothrow, true);
        if (!acResult.has_value())
        {
            POSTGRESQL_DEBUG("  reset: setAutoCommit failed: %s", acResult.error().what_s().data());
        }

        return {};
    }

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_seq_cst);
    }

    cpp_dbc::expected<void, DBException> PostgreSQLDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        // CRITICAL: Close all active result sets and statements BEFORE making connection available
        // closeAllResultSets/closeAllStatements acquire m_connMutex internally
        auto closeRsResult = closeAllResultSets(std::nothrow);
        if (!closeRsResult.has_value())
        {
            POSTGRESQL_DEBUG("  returnToPool: closeAllResultSets failed: %s", closeRsResult.error().what_s().data());
        }
        auto closeStmtsResult = closeAllStatements(std::nothrow);
        if (!closeStmtsResult.has_value())
        {
            POSTGRESQL_DEBUG("  returnToPool: closeAllStatements failed: %s", closeStmtsResult.error().what_s().data());
        }

        // 2026-03-12T12:00:00Z
        // Bug: m_autoCommit is a plain bool (non-atomic) modified under m_connMutex in
        // setAutoCommit/beginTransaction/commit/rollback. Reading it without the lock
        // is a data race (undefined behavior) in multi-threaded builds.
        // Solution: Acquire m_connMutex before reading m_autoCommit. Safe with
        // recursive_mutex since setAutoCommit also acquires m_connMutex internally.
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

        // Restore autocommit for the next user of this connection
        if (!m_autoCommit)
        {
            auto acResult = setAutoCommit(std::nothrow, true);
            if (!acResult.has_value())
            {
                POSTGRESQL_DEBUG("  returnToPool: setAutoCommit failed: %s", acResult.error().what_s().data());
            }
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

    cpp_dbc::expected<bool, DBException> PostgreSQLDBConnection::ping(std::nothrow_t) noexcept
    {
        auto result = executeQuery(std::nothrow, "SELECT 1");
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        auto closeResult = result.value()->close(std::nothrow);
        if (!closeResult.has_value())
        {
            return cpp_dbc::unexpected(closeResult.error());
        }
        return true;
    }

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
