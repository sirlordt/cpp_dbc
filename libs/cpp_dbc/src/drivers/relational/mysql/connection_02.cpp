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
 @brief MySQL database driver implementation - MySQLDBConnection nothrow methods (part 1 - query and autocommit)

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

    // Nothrow API implementations

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
        // try/catch is needed here: shared_from_this() can throw std::bad_weak_ptr
        try
        {
            MYSQL_CONNECTION_LOCK_OR_RETURN("DK8K315FW3Y5", "Cannot prepare statement");

            // Pass weak_ptr<MySQLDBConnection> to the PreparedStatement
            // The statement accesses the MYSQL* and mutex through the connection reference
            auto stmtResult = MySQLDBPreparedStatement::create(std::nothrow, std::weak_ptr<MySQLDBConnection>(shared_from_this()), sql);
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
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("5PFOVDU347UD",
                                                   std::string("prepareStatement failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected(DBException("P4AGG0BAVQIP",
                                                   "prepareStatement failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> MySQLDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        // try/catch is needed here: shared_from_this() can throw std::bad_weak_ptr
        try
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

            auto rsResult = MySQLDBResultSet::create(std::nothrow, result, shared_from_this());
            if (!rsResult.has_value())
            {
                return cpp_dbc::unexpected(rsResult.error());
            }
            auto rs = rsResult.value();

            // Registration with the connection is done inside create() → initialize()
            return std::shared_ptr<RelationalDBResultSet>(rs);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("8RBHAJMYREPU",
                                                   std::string("executeQuery failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions after typed catch above
        {
            return cpp_dbc::unexpected(DBException("8T29618FJ2VI",
                                                   "executeQuery failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

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
        DB_DRIVER_LOCK_GUARD(*m_connMutex);

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
