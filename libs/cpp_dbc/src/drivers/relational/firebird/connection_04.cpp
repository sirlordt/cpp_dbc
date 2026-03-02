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
 @brief Firebird database driver implementation - FirebirdDBConnection nothrow methods (group 3: setTransactionIsolation, getTransactionIsolation, prepareForPoolReturn, prepareForBorrow)

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
    // NOTHROW API — group 3: setTransactionIsolation, getTransactionIsolation,
    //               prepareForPoolReturn, prepareForBorrow
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

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::prepareForPoolReturn(std::nothrow_t) noexcept
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

        return {};
    }

    // ============================================================================
    // MVCC (Multi-Version Concurrency Control) Fix for Connection Pooling
    // ============================================================================
    //
    // PROBLEM:
    // Firebird uses MVCC, where each transaction sees a "snapshot" of the database
    // taken at the moment the transaction starts. This snapshot is immutable for
    // the lifetime of the transaction.
    //
    // In a connection pool scenario:
    // 1. Pool creates N connections at initialization time (e.g., T=0)
    // 2. Each connection starts a transaction immediately (auto-commit mode)
    // 3. These transactions capture a snapshot of the database at T=0
    // 4. Later (e.g., T=10), a test runs RECREATE TABLE or other DDL
    // 5. When a pooled connection is borrowed, its transaction still has the T=0 snapshot
    // 6. The T=0 snapshot doesn't "see" the table created at T=10
    // 7. Result: "SQLCODE -219: table id not defined" errors
    //
    // SYMPTOMS:
    // - Intermittent "table id not defined" errors in pool tests
    // - First connection works (gets fresh transaction), subsequent ones fail
    // - Errors appear after DDL operations (CREATE, DROP, ALTER, RECREATE TABLE)
    // - Pool timeout errors may also appear as a secondary effect
    //
    // SOLUTION:
    // When borrowing a connection from the pool, we rollback the current transaction
    // and start a new one. This forces Firebird to take a fresh MVCC snapshot that
    // includes all committed DDL changes up to that moment.
    //
    // ============================================================================
    // WARNING: COMMON MISDIAGNOSIS - READ THIS BEFORE MODIFYING
    // ============================================================================
    //
    // When debugging pool issues with symptoms like "timeout" + "table id not defined",
    // it may seem logical to blame closeAllActivePreparedStatements() in
    // prepareForPoolReturn(), thinking:
    //
    //   "Statement closing is slow under Valgrind → causes timeouts → connections
    //    with stale transactions get borrowed → table errors"
    //
    // THIS IS A FALSE DIAGNOSIS. The actual cause is the MVCC snapshot issue described
    // above. The statement closing and MVCC are COMPLETELY UNRELATED concerns:
    //
    // +---------------------------+----------------------------------------+
    // | Statement Closing         | MVCC Refresh                           |
    // +---------------------------+----------------------------------------+
    // | Resource management       | Database visibility                    |
    // | Releases handles/memory   | Determines what tables/data are seen   |
    // | Done in prepareForReturn  | Done in prepareForBorrow               |
    // | Can be slow under Valgrind| Always fast (just rollback+start tx)   |
    // | Does NOT cause MVCC issues| FIXES the MVCC visibility problem      |
    // +---------------------------+----------------------------------------+
    //
    // If you see "table id not defined" errors in pool tests, the fix is HERE in
    // prepareForBorrow(), NOT in removing statement closing from prepareForPoolReturn().
    //
    // This was verified experimentally: with prepareForBorrow() active, both
    // active statement closing AND passive clearing work correctly. The MVCC
    // refresh is the essential fix.
    //
    // ============================================================================
    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("HR0QTJ2GVXFT", "Connection closed");

        if (m_closed.load(std::memory_order_acquire))
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

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
