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
 @brief SQLite database driver implementation - SQLiteDBConnection nothrow methods

*/

#include "cpp_dbc/drivers/relational/driver_sqlite.hpp"
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <climits> // Para INT_MAX
#include <cstdlib> // Para getenv
#include <fstream> // Para std::ifstream
#include <charconv>
#include "sqlite_internal.hpp"

#if USE_SQLITE

namespace cpp_dbc::SQLite
{

    // Nothrow API

    // No try/catch: all inner calls are nothrow (sqlite3_interrupt is a C function,
    // closeAllResultSets/closeAllStatements/rollback/setAutoCommit/transactionActive
    // all take std::nothrow_t). The only possible throws are death-sentence exceptions
    // (std::bad_alloc, mutex std::system_error) with no meaningful recovery path.
    cpp_dbc::expected<void, cpp_dbc::DBException> SQLiteDBConnection::reset(std::nothrow_t) noexcept
    {
        SQLITE_CONNECTION_LOCK_OR_RETURN("JPDLCGSJGAY2", "Cannot reset connection");

        // 2026-02-15T00:00:00Z
        // Bug: executeQuery() failure with an invalid column leaves the connection in an
        // inconsistent state (WAL locks, partial transactions). Subsequent reset() calls
        // rollback() → executeUpdate("ROLLBACK") which fails with "attempt to write a
        // readonly database" (R4Z5A6B7C8D9), permanently corrupting the connection.
        // Solution: Call sqlite3_interrupt() to forcibly cancel pending operations and
        // clear locks before attempting cleanup, allowing subsequent operations to succeed.
        sqlite3_interrupt(m_conn.get());

        // Close all result sets first, then statements
        auto closeRsResult = closeAllResultSets(std::nothrow);
        if (!closeRsResult.has_value())
        {
            SQLITE_DEBUG("  reset: closeAllResultSets failed: %s", closeRsResult.error().what_s().data());
        }
        auto closeStmtsResult = closeAllStatements(std::nothrow);
        if (!closeStmtsResult.has_value())
        {
            SQLITE_DEBUG("  reset: closeAllStatements failed: %s", closeStmtsResult.error().what_s().data());
        }

        // Rollback any active transaction
        auto txActive = transactionActive(std::nothrow);
        if (txActive.has_value() && txActive.value())
        {
            auto rbResult = rollback(std::nothrow);
            if (!rbResult.has_value())
            {
                SQLITE_DEBUG("  reset: rollback failed: %s", rbResult.error().what_s().data());
            }
        }

        // Reset auto-commit to true
        auto acResult = setAutoCommit(std::nothrow, true);
        if (!acResult.has_value())
        {
            SQLITE_DEBUG("  reset: setAutoCommit failed: %s", acResult.error().what_s().data());
        }

        return {};
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::ping(std::nothrow_t) noexcept
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

    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> SQLiteDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected(DBException("R0Z1A2B3C4D5", "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        auto stmtResult = SQLiteDBPreparedStatement::create(std::nothrow, std::weak_ptr<sqlite3>(m_conn), weak_from_this(), sql);
        if (!stmtResult.has_value())
        {
            return cpp_dbc::unexpected(stmtResult.error());
        }
        auto stmt = stmtResult.value();
        // If registration fails the statement is NOT returned — destructor closes it.
        auto regResult = registerStatement(std::nothrow, std::weak_ptr<SQLiteDBPreparedStatement>(stmt));
        if (!regResult.has_value())
        {
            return cpp_dbc::unexpected(regResult.error());
        }
        return std::shared_ptr<RelationalDBPreparedStatement>(stmt);
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> SQLiteDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected(DBException("R1Z2A3B4C5D6", "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        sqlite3_stmt *stmt = nullptr;
        int result = sqlite3_prepare_v2(m_conn.get(), sql.c_str(), -1, &stmt, nullptr);
        if (result != SQLITE_OK)
        {
            std::string errorMsg = sqlite3_errmsg(m_conn.get());
            return cpp_dbc::unexpected(DBException("R2Z3A4B5C6D7", "Failed to prepare query: " + errorMsg,
                                                   system_utils::captureCallStack()));
        }

        if (!stmt)
        {
            return cpp_dbc::unexpected(DBException("1DEA86F65A95", "Statement is null after successful preparation",
                                                   system_utils::captureCallStack()));
        }

        auto self = std::dynamic_pointer_cast<SQLiteDBConnection>(shared_from_this());
        auto rsResult = SQLiteDBResultSet::create(std::nothrow, stmt, true, self, nullptr);
        if (!rsResult.has_value())
        {
            return cpp_dbc::unexpected(rsResult.error());
        }
        return std::shared_ptr<RelationalDBResultSet>(rsResult.value());
    }

    cpp_dbc::expected<uint64_t, DBException> SQLiteDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected(DBException("R3Z4A5B6C7D8", "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        char *errmsg = nullptr;
        int result = sqlite3_exec(m_conn.get(), sql.c_str(), nullptr, nullptr, &errmsg);

        if (result != SQLITE_OK)
        {
            std::string error = errmsg ? errmsg : "Unknown error";
            sqlite3_free(errmsg);
            return cpp_dbc::unexpected(DBException("R4Z5A6B7C8D9", "Failed to execute update: " + error,
                                                   system_utils::captureCallStack()));
        }

        return static_cast<uint64_t>(sqlite3_changes(m_conn.get()));
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::setAutoCommit(std::nothrow_t, bool autoCommit) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected(DBException("R5Z6A7B8C9D0", "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        // Only change the state if we're actually changing the mode
        if (m_autoCommit.load(std::memory_order_seq_cst) != autoCommit)
        {
            if (autoCommit)
            {
                // Enabling autocommit - commit any active transaction first
                if (m_transactionActive.load(std::memory_order_seq_cst))
                {
                    auto commitResult = commit(std::nothrow);
                    if (!commitResult.has_value())
                    {
                        return cpp_dbc::unexpected(commitResult.error());
                    }
                }

                m_autoCommit.store(true, std::memory_order_seq_cst);
                m_transactionActive.store(false, std::memory_order_seq_cst);
            }
            else
            {
                // Disabling autocommit - begin a transaction automatically (like MySQL driver)
                auto beginResult = beginTransaction(std::nothrow);
                if (!beginResult.has_value())
                {
                    return cpp_dbc::unexpected(beginResult.error());
                }
            }
        }
        return {};
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        return m_autoCommit.load(std::memory_order_seq_cst);
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected(DBException("FD82C45A3E09", "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        if (m_transactionActive.load(std::memory_order_seq_cst))
        {
            return false;
        }

        auto updateResult = executeUpdate(std::nothrow, "BEGIN TRANSACTION");
        if (!updateResult.has_value())
        {
            return cpp_dbc::unexpected(updateResult.error());
        }

        m_autoCommit.store(false, std::memory_order_seq_cst);
        m_transactionActive.store(true, std::memory_order_seq_cst);
        return true;
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        return m_transactionActive.load(std::memory_order_seq_cst);
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::commit(std::nothrow_t) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected(DBException("R6Z7A8B9C0D1", "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        auto updateResult = executeUpdate(std::nothrow, "COMMIT");
        if (!updateResult.has_value())
        {
            return cpp_dbc::unexpected(updateResult.error());
        }

        m_transactionActive.store(false, std::memory_order_seq_cst);
        m_autoCommit.store(true, std::memory_order_seq_cst);
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::rollback(std::nothrow_t) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected(DBException("R7Z8A9B0C1D2", "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        auto updateResult = executeUpdate(std::nothrow, "ROLLBACK");
        if (!updateResult.has_value())
        {
            return cpp_dbc::unexpected(updateResult.error());
        }

        m_transactionActive.store(false, std::memory_order_seq_cst);
        m_autoCommit.store(true, std::memory_order_seq_cst);
        return {};
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);

        if (m_closed.load(std::memory_order_seq_cst) || !m_conn)
        {
            return cpp_dbc::unexpected(DBException("R8Z9A0B1C2D3", "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        // SQLite only supports SERIALIZABLE isolation level
        if (level != TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
        {
            return cpp_dbc::unexpected(DBException("R9Z0A1B2C3D4", "SQLite only supports SERIALIZABLE isolation level",
                                                   system_utils::captureCallStack()));
        }

        m_isolationLevel = level;
        return {};
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException> SQLiteDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        std::scoped_lock globalLock(*m_globalFileMutex);
        return m_isolationLevel;
    }

    // No try/catch: all inner calls are nothrow (std::nothrow_t methods, C API calls,
    // atomic stores, shared_ptr::reset). The only possible throws are std::bad_alloc
    // and mutex std::system_error — both are death-sentence exceptions with no
    // meaningful recovery path. Catching them would hide a catastrophic failure.
    cpp_dbc::expected<void, DBException> SQLiteDBConnection::close(std::nothrow_t) noexcept
    {
        SQLITE_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        // Close all result sets FIRST (before statements)
        [[maybe_unused]] auto closeRsResult = closeAllResultSets(std::nothrow);

        // Close all prepared statements properly
        // closeAllStatements() includes safety net for leaked statements
        [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

        // Call sqlite3_release_memory to free up caches and unused memory
        [[maybe_unused]]
        int releasedMemory = sqlite3_release_memory(1000000);
        SQLITE_DEBUG("Released %d bytes of SQLite memory", releasedMemory);

        // Smart pointer will automatically call sqlite3_close_v2 via SQLiteDbDeleter
        m_conn.reset();
        m_closed.store(true, std::memory_order_seq_cst);

        // Unregister from the driver registry so getConnectionAlive() reflects
        // actual live connections. The owner_less m_self weak_ptr is used for
        // set lookup — raw 'this' would not match the set's comparator.
        SQLiteDBDriver::unregisterConnection(std::nothrow, m_self);

        return {};
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_seq_cst);
    }

    // No try/catch: reset(std::nothrow) is noexcept and all inner operations are
    // nothrow. The only possible throws are death-sentence exceptions (std::bad_alloc,
    // mutex std::system_error) with no meaningful recovery path.
    cpp_dbc::expected<void, DBException> SQLiteDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        // Reset the connection state for the next borrower
        // reset() closes result sets, statements, rolls back and resets autocommit
        auto resetResult = reset(std::nothrow);
        if (!resetResult.has_value())
        {
            SQLITE_DEBUG("returnToPool(nothrow): reset failed: %s",
                         resetResult.error().what_s().data());
            // Continue - try to at least restore autocommit
        }

        return {};
    }

    cpp_dbc::expected<bool, DBException> SQLiteDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    // No try/catch: the only possible throw is std::bad_alloc from the
    // std::string copy, which is a death-sentence exception — no meaningful
    // recovery is possible, so std::terminate is the correct response.
    cpp_dbc::expected<std::string, DBException> SQLiteDBConnection::getURI(std::nothrow_t) const noexcept
    {
        return m_uri;
    }

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::prepareForPoolReturn(
        std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        // Delegate to reset() which closes result sets, statements, rolls back, and resets autocommit
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

    cpp_dbc::expected<void, DBException> SQLiteDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        // No-op for SQLite: no MVCC snapshot refresh needed
        return {};
    }

    cpp_dbc::expected<std::string, DBException> SQLiteDBConnection::getServerVersion(std::nothrow_t) noexcept
    {
        return std::string(sqlite3_libversion());
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> SQLiteDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        std::map<std::string, std::string> info;

        info["ServerVersion"] = sqlite3_libversion();
        info["ServerVersionNumeric"] = std::to_string(sqlite3_libversion_number());
        info["SourceId"] = sqlite3_sourceid();
        info["ThreadSafe"] = std::to_string(sqlite3_threadsafe());

        return info;
    }

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
