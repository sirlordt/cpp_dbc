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

 @file prepared_statement_04.cpp
 @brief Firebird database driver implementation - FirebirdDBPreparedStatement nothrow methods (part 3)

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

    cpp_dbc::expected<uint64_t, DBException> FirebirdDBPreparedStatement::executeUpdate(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate(nothrow) - Starting");

        FIREBIRD_LOCK_OR_RETURN("OVWWOPF0F4Y4", "Connection lost");

        FIREBIRD_DEBUG("  m_closed: %s", (m_closed.load(std::memory_order_acquire) ? "true" : "false"));
        FIREBIRD_DEBUG("  m_stmt: %p", (void*)(uintptr_t)m_stmt);
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("F6A2B8C4D1E7", "Statement is closed", system_utils::captureCallStack()));
        }

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_acquire))
        {
            FIREBIRD_DEBUG("  Statement was invalidated by DDL operation!");
            return cpp_dbc::unexpected(DBException("SRK90BKB8M9D", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
        }

        // FIX #1: Access transaction handle safely via m_connection
        auto conn = m_connection.lock();
        if (!conn)
        {
            FIREBIRD_DEBUG("  Connection destroyed!");
            return cpp_dbc::unexpected(DBException("FB9D2E3F4A5D", "Connection destroyed while executing update", system_utils::captureCallStack()));
        }

        ISC_STATUS_ARRAY status;

        // Execute the statement - now using conn->m_tr instead of m_trPtr
        FIREBIRD_DEBUG("  Calling isc_dsql_execute...");
        FIREBIRD_DEBUG("    conn->m_tr=%ld", (long)conn->m_tr);
        FIREBIRD_DEBUG("    &m_stmt=%p, m_stmt=%p", (void*)&m_stmt, (void*)(uintptr_t)m_stmt);
        if (isc_dsql_execute(status, &(conn->m_tr), &m_stmt, SQL_DIALECT_V6, m_inputSqlda.get()))
        {
            // Save the error message BEFORE calling any other Firebird API functions
            // because they will overwrite the status vector
            std::string errorMsg = interpretStatusVector(status);
            FIREBIRD_DEBUG("  isc_dsql_execute failed: %s", errorMsg.c_str());

            // If autocommit is enabled, rollback the failed statement using retaining mode.
            // isc_rollback_retaining() discards the failed statement's changes but keeps
            // the transaction handle (m_tr) alive, consistent with isc_commit_retaining()
            // used on the success path. This avoids the heavy endTransaction() cycle
            // (closeAllActiveResultSets + startTransaction) for a transient error.
            if (const auto ac = conn->getAutoCommit(std::nothrow); ac.has_value() && ac.value())
            {
                FIREBIRD_DEBUG("  AutoCommit is enabled, rolling back failed statement with isc_rollback_retaining()");
                ISC_STATUS_ARRAY rollbackStatus;
                isc_rollback_retaining(rollbackStatus, &conn->m_tr);
                FIREBIRD_DEBUG("  isc_rollback_retaining completed");
            }

            return cpp_dbc::unexpected(DBException("A7B3C9D5E2F8", "Failed to execute update: " + errorMsg,
                                                   system_utils::captureCallStack()));
        }
        FIREBIRD_DEBUG("  isc_dsql_execute succeeded");

        // Get affected rows count
        char infoBuffer[64] = {0}; // Initialize to zero to avoid valgrind warnings
        char resultBuffer[64] = {0};
        infoBuffer[0] = isc_info_sql_records;
        infoBuffer[1] = isc_info_end;

        if (isc_dsql_sql_info(status, &m_stmt, sizeof(infoBuffer), infoBuffer, sizeof(resultBuffer), resultBuffer))
        {
            FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate(nothrow) - Failed to get sql_info, checking autocommit");
            if (const auto ac = conn->getAutoCommit(std::nothrow); ac.has_value() && ac.value())
            {
                // Use isc_commit_retaining() instead of conn->commit() for autocommit mode.
                //
                // isc_commit_retaining() persists the data immediately (like a checkpoint)
                // BUT keeps the transaction handle (m_tr) alive and active. This means:
                //   - No need to close active ResultSets/cursors before committing
                //   - No need to call startTransaction() afterwards
                //   - No heavy endTransaction() cycle (closeAllActiveResultSets + sleep_for)
                //
                // This mirrors how SQLite handles autocommit: sqlite3_exec() persists data
                // atomically without a separate commit step or cursor lifecycle management.
                //
                // Contrast with autoCommit=false: when the user calls conn->commit()
                // explicitly, it goes through endTransaction(true) → isc_commit_transaction()
                // which properly terminates the transaction and starts a new one.
                FIREBIRD_DEBUG("  AutoCommit is enabled, calling isc_commit_retaining()");
                ISC_STATUS_ARRAY commitStatus;
                isc_commit_retaining(commitStatus, &conn->m_tr);
                FIREBIRD_DEBUG("  isc_commit_retaining completed");
            }
            FIREBIRD_DEBUG("  Returning 0 (unable to get count)");
            return 0; // Unable to get count, return 0
        }

        // Parse the result buffer to get the count
        uint64_t count = 0;
        char *p = resultBuffer;
        while (*p != isc_info_end)
        {
            char item = *p++;
            short len = static_cast<short>(isc_vax_integer(p, 2));
            p += 2;

            if (item == isc_info_sql_records)
            {
                while (*p != isc_info_end)
                {
                    char subItem = *p++;
                    short subLen = static_cast<short>(isc_vax_integer(p, 2));
                    p += 2;

                    if (subItem == isc_info_req_update_count ||
                        subItem == isc_info_req_delete_count ||
                        subItem == isc_info_req_insert_count)
                    {
                        count += static_cast<uint64_t>(isc_vax_integer(p, subLen));
                    }
                    p += subLen;
                }
            }
            else
            {
                p += len;
            }
        }

        FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate(nothrow) - Checking autocommit");
        if (const auto ac = conn->getAutoCommit(std::nothrow); ac.has_value() && ac.value())
        {
            // Use isc_commit_retaining() instead of conn->commit() for autocommit mode.
            //
            // isc_commit_retaining() persists the data immediately (like a checkpoint)
            // BUT keeps the transaction handle (m_tr) alive and active. This means:
            //   - No need to close active ResultSets/cursors before committing
            //   - No need to call startTransaction() afterwards
            //   - No heavy endTransaction() cycle (closeAllActiveResultSets + sleep_for)
            //
            // This mirrors how SQLite handles autocommit: sqlite3_exec() persists data
            // atomically without a separate commit step or cursor lifecycle management.
            //
            // Contrast with autoCommit=false: when the user calls conn->commit()
            // explicitly, it goes through endTransaction(true) → isc_commit_transaction()
            // which properly terminates the transaction and starts a new one.
            FIREBIRD_DEBUG("  AutoCommit is enabled, calling isc_commit_retaining()");
            ISC_STATUS_ARRAY commitStatus;
            isc_commit_retaining(commitStatus, &conn->m_tr);
            FIREBIRD_DEBUG("  isc_commit_retaining completed");
        }
        else
        {
            FIREBIRD_DEBUG("  AutoCommit is disabled, data stays in active transaction until explicit commit()");
        }

        FIREBIRD_DEBUG("FirebirdPreparedStatement::executeUpdate(nothrow) - Done, returning count=%llu", (unsigned long long)count);
        return count;
    }

    cpp_dbc::expected<bool, DBException> FirebirdDBPreparedStatement::execute(std::nothrow_t) noexcept
    {
        FIREBIRD_LOCK_OR_RETURN("4EPRL2TOTZ0P", "Connection lost");

        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("B8C4D0E6F3A9", "Statement is closed", system_utils::captureCallStack()));
        }

        // Check if statement was invalidated by connection due to DDL operation
        if (m_invalidated.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("T967KCIH016C", "Statement was invalidated due to DDL operation (DROP/ALTER/CREATE). Please create a new prepared statement.", system_utils::captureCallStack()));
        }

        // FIX #1: Access transaction handle safely via m_connection
        auto conn = m_connection.lock();
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("FB9D2E3F4A5F", "Connection destroyed while executing statement", system_utils::captureCallStack()));
        }

        ISC_STATUS_ARRAY status;

        // Execute the statement - now using conn->m_tr instead of m_trPtr
        if (isc_dsql_execute(status, &(conn->m_tr), &m_stmt, SQL_DIALECT_V6, m_inputSqlda.get()))
        {
            std::string errorMsg = interpretStatusVector(status);
            return cpp_dbc::unexpected(DBException("FBCX4Y5Z6A7B", "Failed to execute statement: " + errorMsg,
                                                   system_utils::captureCallStack()));
        }

        return m_outputSqlda->sqld > 0; // Returns true if there are result columns
    }

    cpp_dbc::expected<void, DBException> FirebirdDBPreparedStatement::close(std::nothrow_t) noexcept
    {
        // Use special macro for close() - returns success if already closed (idempotent)
        FIREBIRD_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        FIREBIRD_DEBUG("FirebirdPreparedStatement::close(nothrow) - Starting");
        FIREBIRD_DEBUG("  m_closed: %s", (m_closed.load(std::memory_order_acquire) ? "true" : "false"));
        FIREBIRD_DEBUG("  m_stmt: %p", (void*)(uintptr_t)m_stmt);

        // Unregister from connection if connection is still alive AND not in reset()
        // During reset(), closeAllActivePreparedStatements() already holds the lock and clears the list
        auto conn = m_connection.lock();
        if (conn && !conn->isResetting())
        {
            [[maybe_unused]] auto unregResult = conn->unregisterStatement(std::nothrow, weak_from_this());
        }

        ISC_STATUS_ARRAY status;

        if (m_stmt)
        {
            FIREBIRD_DEBUG("  Freeing statement with DSQL_drop...");
            // Only try to free if the handle is valid
            if (m_stmt != 0)
            {
                // Create a local copy of the statement handle
                isc_stmt_handle localStmt = m_stmt;

                // Free the statement using the local copy
                isc_dsql_free_statement(status, &localStmt, DSQL_drop);
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
                FIREBIRD_DEBUG("  Statement freed");
            }
            m_stmt = 0;
        }

        // Smart pointers will handle cleanup automatically
        FIREBIRD_DEBUG("  Resetting smart pointers");
        m_inputSqlda.reset();
        m_outputSqlda.reset();

        m_closed.store(true, std::memory_order_release);
        FIREBIRD_DEBUG("FirebirdPreparedStatement::close(nothrow) - Done");
        return {};
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
