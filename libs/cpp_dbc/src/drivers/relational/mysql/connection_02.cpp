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
 @brief MySQL database driver implementation - MySQLDBConnection nothrow methods (part 1 - lifecycle, query, and autocommit)

*/

#include "cpp_dbc/drivers/relational/driver_mysql.hpp"

#if USE_MYSQL

#include <array>
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include "cpp_dbc/common/system_utils.hpp"
#include "mysql_internal.hpp"

namespace cpp_dbc::MySQL
{

    // ============================================================================
    // Nothrow API — matching header declaration order
    // ============================================================================

    cpp_dbc::expected<void, DBException> MySQLDBConnection::close(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        // Close all active result sets and statements before closing the connection
        // This ensures mysql_stmt_close() is called while we have exclusive access
        [[maybe_unused]] auto closeRsResult = closeAllResultSets(std::nothrow);
        [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

        // 2026-03-07T00:00:00Z
        // Bug: Destroying the MYSQL handle immediately after closing all statements and result
        // sets can race with in-flight MySQL C API calls on other threads that still reference
        // the handle, causing use-after-free crashes.
        // Solution: A 25ms delay gives in-flight operations time to complete before the handle
        // is destroyed. This is a pragmatic workaround pending a refcount-based solution.
        std::this_thread::sleep_for(std::chrono::milliseconds(25));

        m_conn.reset();
        m_closed.store(true, std::memory_order_seq_cst);

        // Remove this connection from the driver's registry.
        // m_self holds a weak_ptr with the same control block as the original shared_ptr,
        // so owner_less comparison works correctly even if the shared_ptr refcount is 0.
        // Safe against double-unregister: MYSQL_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED
        // above returns early if already closed, so this executes exactly once.
        MySQLDBDriver::unregisterConnection(std::nothrow, m_self);

        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::reset(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        // RAII guard to set m_resetting flag during reset operation
        // This prevents ResultSet/PreparedStatement from calling unregister during closeAll*()
        cpp_dbc::system_utils::AtomicGuard resettingGuard(m_resetting, true, false);

        // Close all active result sets and statements
        auto closeRsResult = closeAllResultSets(std::nothrow);
        if (!closeRsResult.has_value())
        {
            MYSQL_DEBUG("  reset: closeAllResultSets failed: %s", closeRsResult.error().what_s().data());
        }
        auto closeStmtsResult = closeAllStatements(std::nothrow);
        if (!closeStmtsResult.has_value())
        {
            MYSQL_DEBUG("  reset: closeAllStatements failed: %s", closeStmtsResult.error().what_s().data());
        }

        // Rollback any active transaction
        auto txActive = transactionActive(std::nothrow);
        if (txActive.has_value() && txActive.value())
        {
            auto rbResult = rollback(std::nothrow);
            if (!rbResult.has_value())
            {
                MYSQL_DEBUG("  reset: rollback failed: %s", rbResult.error().what_s().data());
            }
        }

        // Reset auto-commit to true
        auto acResult = setAutoCommit(std::nothrow, true);
        if (!acResult.has_value())
        {
            MYSQL_DEBUG("  reset: setAutoCommit failed: %s", acResult.error().what_s().data());
        }

        return {};
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_seq_cst);
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        // CRITICAL: Close all active result sets and statements BEFORE making connection available
        // closeAllStatements() acquires m_connMutex internally
        auto closeRsResult = closeAllResultSets(std::nothrow);
        if (!closeRsResult.has_value())
        {
            MYSQL_DEBUG("  returnToPool: closeAllResultSets failed: %s", closeRsResult.error().what_s().data());
        }
        auto closeStmtsResult = closeAllStatements(std::nothrow);
        if (!closeStmtsResult.has_value())
        {
            MYSQL_DEBUG("  returnToPool: closeAllStatements failed: %s", closeStmtsResult.error().what_s().data());
        }

        // Restore autocommit for the next user of this connection
        if (!m_autoCommit)
        {
            auto acResult = setAutoCommit(std::nothrow, true);
            if (!acResult.has_value())
            {
                MYSQL_DEBUG("  returnToPool: setAutoCommit failed: %s", acResult.error().what_s().data());
            }
        }

        return {};
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    // No try/catch: the only possible throw is std::bad_alloc from the
    // std::string copy, which is a death-sentence exception — no meaningful
    // recovery is possible, so std::terminate is the correct response.
    cpp_dbc::expected<std::string, DBException> MySQLDBConnection::getURI(std::nothrow_t) const noexcept
    {
        return m_uri;
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::ping(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("QRN556B0TNNX", "Cannot ping");

        if (mysql_ping(m_conn.get()) != 0)
        {
            return false;
        }
        return true;
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> MySQLDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("DK8K315FW3Y5", "Cannot prepare statement");

        // Pass weak_ptr<MySQLDBConnection> to the PreparedStatement
        // weak_from_this() is noexcept — no try/catch needed
        auto stmtResult = MySQLDBPreparedStatement::create(std::nothrow, weak_from_this(), sql);
            if (!stmtResult.has_value())
            {
                return cpp_dbc::unexpected(stmtResult.error());
            }
            auto stmt = stmtResult.value();

            // Register the statement as weak_ptr — allows natural destruction when user releases reference.
            // Statements will be explicitly closed in returnToPool() or close() before connection reuse.
            // If registration fails the statement is NOT returned — destructor closes it.
        auto regResult = registerStatement(std::nothrow, std::weak_ptr<MySQLDBPreparedStatement>(stmt));
        if (!regResult.has_value())
        {
            return cpp_dbc::unexpected(regResult.error());
        }

        return std::shared_ptr<RelationalDBPreparedStatement>(stmt);
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> MySQLDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("0KKYQGC8X7KF", "Cannot execute query");

        if (mysql_query(m_conn.get(), sql.c_str()) != 0)
        {
            return cpp_dbc::unexpected(DBException("LDUQOJQ0UO5C", std::string("Query failed: ") + mysql_error(m_conn.get()), system_utils::captureCallStack()));
        }

        MYSQL_RES *result = mysql_store_result(m_conn.get());
        if (!result && mysql_field_count(m_conn.get()) > 0)
        {
            return cpp_dbc::unexpected(DBException("VJU5PI2HJ7DB", std::string("Failed to get result set: ") + mysql_error(m_conn.get()), system_utils::captureCallStack()));
        }

        // weak_from_this().lock() is noexcept — no try/catch needed
        auto self = weak_from_this().lock();
        if (!self)
        {
            if (result)
            {
                mysql_free_result(result);
            }
            return cpp_dbc::unexpected(DBException("8RBHAJMYREPU",
                "Connection is not owned by shared_ptr",
                system_utils::captureCallStack()));
        }

        auto rsResult = MySQLDBResultSet::create(std::nothrow, result, self);
        if (!rsResult.has_value())
        {
            return cpp_dbc::unexpected(rsResult.error());
        }
        auto rs = rsResult.value();

        // Registration with the connection is done inside create() → initialize()
        return std::shared_ptr<RelationalDBResultSet>(rs);
    }

    cpp_dbc::expected<uint64_t, DBException> MySQLDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("KXRO97HR1PLJ", "Cannot execute update");

        if (mysql_query(m_conn.get(), sql.c_str()) != 0)
        {
            return cpp_dbc::unexpected(DBException("OAKJBNYP41SP", std::string("Update failed: ") + mysql_error(m_conn.get()), system_utils::captureCallStack()));
        }

        return mysql_affected_rows(m_conn.get());
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::setAutoCommit(std::nothrow_t, bool autoCommitFlag) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("T5WPK768DCXS", "Cannot set autocommit");

        // Only change the SQL state if we're actually changing the mode
        if (this->m_autoCommit != autoCommitFlag)
        {
            if (autoCommitFlag)
            {
                // Enable autocommit (1) and deactivate transactions
                std::string query = "SET autocommit=1";
                if (mysql_query(m_conn.get(), query.c_str()) != 0)
                {
                    return cpp_dbc::unexpected(DBException("G69BR0Y3T54B", std::string("Failed to set autocommit mode: ") + mysql_error(m_conn.get()), system_utils::captureCallStack()));
                }

                this->m_autoCommit = true;
                this->m_transactionActive = false;
            }
            else
            {
                // If disabling autocommit, use beginTransaction to start a new transaction
                auto beginResult = beginTransaction(std::nothrow);
                if (!beginResult.has_value())
                {
                    return cpp_dbc::unexpected(beginResult.error());
                }
            }
        }
        return {};
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("KPZZGSPSLJPH", "Cannot get autocommit");

        return m_autoCommit;
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("29T0BN4MPKDZ", "Cannot begin transaction");

        // If transaction is already active, just return true
        if (m_transactionActive)
        {
            return true;
        }

        // Start transaction by disabling autocommit
        std::string query = "SET autocommit=0";
        if (mysql_query(m_conn.get(), query.c_str()) != 0)
        {
            return cpp_dbc::unexpected(DBException("797I0D2WFCWY", std::string("Failed to begin transaction: ") + mysql_error(m_conn.get()), system_utils::captureCallStack()));
        }

        m_autoCommit = false;
        m_transactionActive = true;
        return true;
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("S8O6SSTSMX4U", "Cannot check transaction state");

        return m_transactionActive;
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::commit(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("GSMACMDU9ILQ", "Cannot commit");

        // No transaction active, nothing to commit
        if (!m_transactionActive)
        {
            return {};
        }

        if (mysql_query(m_conn.get(), "COMMIT") != 0)
        {
            return cpp_dbc::unexpected(DBException("EE9LVJATQS6L", std::string("Commit failed: ") + mysql_error(m_conn.get()), system_utils::captureCallStack()));
        }

        m_transactionActive = false;

        // If autoCommit is still false, a new transaction starts automatically in MySQL
        if (!m_autoCommit)
        {
            m_transactionActive = true;
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::rollback(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("TY8TOENT80W3", "Cannot rollback");

        // No transaction active, nothing to rollback
        if (!m_transactionActive)
        {
            return {};
        }

        if (mysql_query(m_conn.get(), "ROLLBACK") != 0)
        {
            return cpp_dbc::unexpected(DBException("ARRBS9Z8OOZ1", std::string("Rollback failed: ") + mysql_error(m_conn.get()), system_utils::captureCallStack()));
        }

        m_transactionActive = false;

        // If autoCommit is still false, a new transaction starts automatically in MySQL
        if (!m_autoCommit)
        {
            m_transactionActive = true;
        }
        return {};
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
