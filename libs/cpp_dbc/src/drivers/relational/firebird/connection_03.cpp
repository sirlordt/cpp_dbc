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

 @file connection_03.cpp
 @brief Firebird database driver implementation - FirebirdDBConnection nothrow methods (group 2: prepareStatement, executeQuery, executeUpdate, setAutoCommit, getAutoCommit, beginTransaction, transactionActive, commit, rollback)

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
    // NOTHROW API — group 2: prepareStatement, executeQuery, executeUpdate,
    //               setAutoCommit, getAutoCommit, beginTransaction, transactionActive,
    //               commit, rollback
    // ============================================================================

    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>
    FirebirdDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        // try/catch is needed here: shared_from_this() can throw std::bad_weak_ptr
        // (a recoverable exception, not a death-sentence) if the object is not owned
        // by a shared_ptr. All other inner calls are noexcept.
        try
        {
            FIREBIRD_CONNECTION_LOCK_OR_RETURN("11Z6HRLRBNND", "Connection closed");

            FIREBIRD_DEBUG("FirebirdConnection::prepareStatement(nothrow) - Starting");
            FIREBIRD_DEBUG("  SQL: %s", sql.c_str());
            FIREBIRD_DEBUG("  m_closed: %s", (m_closed.load(std::memory_order_acquire) ? "true" : "false"));
            FIREBIRD_DEBUG("  m_tr: %ld", (long)m_tr);

            if (m_closed.load(std::memory_order_acquire))
            {
                FIREBIRD_DEBUG("  Connection is closed!");
                return cpp_dbc::unexpected(DBException("C5DB7C0E1EE3", "Connection is closed", system_utils::captureCallStack()));
            }

            if (!m_tr)
            {
                FIREBIRD_DEBUG("  No active transaction, starting one...");
                // Use startTransaction() directly to avoid corrupting m_autoCommit.
                // beginTransaction() sets m_autoCommit=false as a side effect, which
                // prevents isc_commit_retaining() from being called in executeUpdate(),
                // causing all pool-managed connections (re-borrowed after reset()) to
                // silently roll back their INSERTs instead of committing them.
                auto txResult = startTransaction(std::nothrow);
                if (!txResult.has_value())
                {
                    return cpp_dbc::unexpected(txResult.error());
                }
            }

            FIREBIRD_DEBUG("  Creating FirebirdDBPreparedStatement...");
            FIREBIRD_DEBUG("    m_db.get()=%p, *m_db.get()=%ld", (void*)m_db.get(), (m_db.get() ? (long)*m_db.get() : 0L));
            FIREBIRD_DEBUG("    m_tr=%ld", (long)m_tr);
            // PreparedStatement no longer receives m_connMutex as parameter
            // It will access the mutex through m_connection weak_ptr when needed
            auto stmtResult = FirebirdDBPreparedStatement::create(
                std::nothrow, std::weak_ptr<isc_db_handle>(m_db), sql, shared_from_this());
            if (!stmtResult.has_value())
            {
                return cpp_dbc::unexpected(stmtResult.error());
            }
            auto stmt = stmtResult.value();

            FIREBIRD_DEBUG("FirebirdConnection::prepareStatement(nothrow) - Done");
            return cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>(std::static_pointer_cast<RelationalDBPreparedStatement>(stmt));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("2791E5F8FC3C", std::string("Exception in prepareStatement: ") + ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D87AA3FA1250", "Unknown exception in prepareStatement", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>
    FirebirdDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("NVCJZFQZP6C1", "Connection closed");

        FIREBIRD_DEBUG("FirebirdConnection::executeQuery(nothrow) - Starting");
        FIREBIRD_DEBUG("  SQL: %s", sql.c_str());

        // First, prepare the statement using the nothrow version
        auto stmtResult = prepareStatement(std::nothrow, sql);
        if (!stmtResult.has_value())
        {
            return cpp_dbc::unexpected(stmtResult.error());
        }

        // Now execute the query using the nothrow version
        auto stmt = stmtResult.value();
        auto resultSetResult = stmt->executeQuery(std::nothrow);
        if (!resultSetResult.has_value())
        {
            return cpp_dbc::unexpected(resultSetResult.error());
        }

        FIREBIRD_DEBUG("FirebirdConnection::executeQuery(nothrow) - Done");
        return resultSetResult.value();
    }

    cpp_dbc::expected<uint64_t, DBException>
    FirebirdDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("PHRJ2XOXAVCQ", "Connection closed");

        FIREBIRD_DEBUG("FirebirdConnection::executeUpdate(nothrow) - Starting");
        FIREBIRD_DEBUG("  SQL: %s", sql.c_str());

        // Check if this is a CREATE DATABASE statement
        // CREATE DATABASE requires special handling with isc_dsql_execute_immediate
        std::string upperSql = sql;
        std::transform(upperSql.begin(), upperSql.end(), upperSql.begin(), ::toupper);

        // Remove leading whitespace for comparison
        size_t start = upperSql.find_first_not_of(" \t\n\r");
        if (start != std::string::npos)
        {
            upperSql = upperSql.substr(start);
        }

        if (upperSql.find("CREATE DATABASE") == 0 || upperSql.find("CREATE SCHEMA") == 0)
        {
            FIREBIRD_DEBUG("FirebirdConnection::executeUpdate(nothrow) - Detected CREATE DATABASE statement");
            return executeCreateDatabase(std::nothrow, sql);
        }

        // Check if this is a DDL statement that requires metadata lock cleanup
        // DDL operations like DROP, ALTER, CREATE, RECREATE need exclusive metadata locks
        // If there are active prepared statements holding metadata locks, we get deadlock
        bool isDDL = (upperSql.find("DROP ") == 0 ||
                      upperSql.find("ALTER ") == 0 ||
                      upperSql.find("CREATE ") == 0 ||
                      upperSql.find("RECREATE ") == 0);

        if (isDDL)
        {
            FIREBIRD_DEBUG("FirebirdConnection::executeUpdate(nothrow) - Detected DDL statement, cleaning up metadata locks");

            // Close all active prepared statements to release metadata locks
            closeAllActivePreparedStatements(std::nothrow);

            // Commit current transaction to ensure all metadata locks are released
            if (m_tr)
            {
                FIREBIRD_DEBUG("  Committing current transaction before DDL");
                auto endResult = endTransaction(std::nothrow, true);
                if (!endResult.has_value())
                {
                    FIREBIRD_DEBUG("  Commit before DDL failed: %s", endResult.error().what_s().data());
                    return cpp_dbc::unexpected(endResult.error());
                }
                auto startResult = startTransaction(std::nothrow);
                if (!startResult.has_value())
                {
                    FIREBIRD_DEBUG("  Start transaction after DDL commit failed: %s", startResult.error().what_s().data());
                    return cpp_dbc::unexpected(startResult.error());
                }
            }

            FIREBIRD_DEBUG("  Metadata locks cleanup completed");
        }

        // First, prepare the statement using the nothrow version
        auto stmtResult = prepareStatement(std::nothrow, sql);
        if (!stmtResult.has_value())
        {
            return cpp_dbc::unexpected(stmtResult.error());
        }

        // Now execute the update using the nothrow version
        auto stmt = stmtResult.value();
        auto updateResult = stmt->executeUpdate(std::nothrow);
        if (!updateResult.has_value())
        {
            return cpp_dbc::unexpected(updateResult.error());
        }

        FIREBIRD_DEBUG("FirebirdConnection::executeUpdate(nothrow) - Done");
        return updateResult.value();
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::setAutoCommit(std::nothrow_t, bool autoCommit) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("Z8AZGGGFAGTC", "Connection closed");

        FIREBIRD_DEBUG("FirebirdConnection::setAutoCommit(nothrow) - Starting");
        FIREBIRD_DEBUG("  Current autoCommit: %s", (m_autoCommit ? "true" : "false"));
        FIREBIRD_DEBUG("  New autoCommit: %s", (autoCommit ? "true" : "false"));

        if (m_autoCommit == autoCommit)
        {
            FIREBIRD_DEBUG("  No change needed, returning");
            return {};
        }

        if (m_autoCommit && !autoCommit)
        {
            // Switching from auto-commit to manual
            // Commit any pending transaction
            if (m_tr)
            {
                FIREBIRD_DEBUG("  Switching from auto-commit to manual, committing pending transaction");
                auto commitResult = commit(std::nothrow);
                if (!commitResult.has_value())
                {
                    return cpp_dbc::unexpected(commitResult.error());
                }
            }
        }

        m_autoCommit = autoCommit;
        FIREBIRD_DEBUG("  AutoCommit set to: %s", (m_autoCommit ? "true" : "false"));

        if (m_autoCommit && !m_tr)
        {
            FIREBIRD_DEBUG("  AutoCommit enabled but no transaction, starting one...");
            auto txResult = startTransaction(std::nothrow);
            if (!txResult.has_value())
            {
                FIREBIRD_DEBUG("  startTransaction failed: %s", txResult.error().what_s().data());
                return cpp_dbc::unexpected(txResult.error());
            }
        }

        FIREBIRD_DEBUG("FirebirdConnection::setAutoCommit(nothrow) - Done");
        return {};
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("3JLUB904QYU4", "Connection closed");

        FIREBIRD_DEBUG("FirebirdConnection::getAutoCommit(nothrow) - Returning %s", (m_autoCommit ? "true" : "false"));
        return m_autoCommit;
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::beginTransaction(nothrow) - Starting");
        FIREBIRD_DEBUG("  m_autoCommit before: %s", (m_autoCommit ? "true" : "false"));
        FIREBIRD_DEBUG("  m_transactionActive: %s", (m_transactionActive ? "true" : "false"));

        FIREBIRD_CONNECTION_LOCK_OR_RETURN("BW40SKTB21R9", "Connection closed");

        // Disable autocommit when beginning a manual transaction
        // This prevents executeUpdate from auto-committing
        m_autoCommit = false;
        FIREBIRD_DEBUG("  m_autoCommit after: %s", (m_autoCommit ? "true" : "false"));

        // If transaction is already active, just return true (like MySQL)
        if (m_transactionActive)
        {
            FIREBIRD_DEBUG("FirebirdConnection::beginTransaction(nothrow) - Transaction already active, returning true");
            return true;
        }

        auto txResult = startTransaction(std::nothrow);
        if (!txResult.has_value())
        {
            return cpp_dbc::unexpected(txResult.error());
        }
        FIREBIRD_DEBUG("FirebirdConnection::beginTransaction(nothrow) - Done");
        return true;
    }

    cpp_dbc::expected<bool, DBException>
    FirebirdDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("9J7OLUOJAWKB", "Connection closed");

        FIREBIRD_DEBUG("FirebirdConnection::transactionActive(nothrow) - Returning %s", (m_transactionActive ? "true" : "false"));
        return m_transactionActive;
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::commit(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::commit(nothrow) - Starting");

        FIREBIRD_DEBUG("  Acquiring lock...");
        FIREBIRD_CONNECTION_LOCK_OR_RETURN("WXETVZN5SFDN", "Connection closed");
        FIREBIRD_DEBUG("  Lock acquired");

        FIREBIRD_DEBUG("  Calling endTransaction(true)...");
        auto endResult = endTransaction(std::nothrow, true);
        if (!endResult.has_value())
        {
            return cpp_dbc::unexpected(endResult.error());
        }
        FIREBIRD_DEBUG("  endTransaction completed");

        if (m_autoCommit)
        {
            FIREBIRD_DEBUG("  AutoCommit is enabled, calling startTransaction()...");
            auto startResult = startTransaction(std::nothrow);
            if (!startResult.has_value())
            {
                return cpp_dbc::unexpected(startResult.error());
            }
            FIREBIRD_DEBUG("  startTransaction completed");
        }

        FIREBIRD_DEBUG("FirebirdConnection::commit(nothrow) - Done");
        return {};
    }

    cpp_dbc::expected<void, DBException>
    FirebirdDBConnection::rollback(std::nothrow_t) noexcept
    {
        FIREBIRD_DEBUG("FirebirdConnection::rollback(nothrow) - Starting");

        FIREBIRD_CONNECTION_LOCK_OR_RETURN("FYUWP3CPT23W", "Connection closed");

        FIREBIRD_DEBUG("  Calling endTransaction(false)...");
        auto endResult = endTransaction(std::nothrow, false);
        if (!endResult.has_value())
        {
            return cpp_dbc::unexpected(endResult.error());
        }
        FIREBIRD_DEBUG("  endTransaction completed");

        if (m_autoCommit)
        {
            FIREBIRD_DEBUG("  AutoCommit is enabled, calling startTransaction()...");
            auto startResult = startTransaction(std::nothrow);
            if (!startResult.has_value())
            {
                return cpp_dbc::unexpected(startResult.error());
            }
            FIREBIRD_DEBUG("  startTransaction completed");
        }

        FIREBIRD_DEBUG("FirebirdConnection::rollback(nothrow) - Done");
        return {};
    }

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
