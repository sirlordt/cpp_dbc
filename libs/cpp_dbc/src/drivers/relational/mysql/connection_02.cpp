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

#include "cpp_dbc/drivers/relational/mysql_blob.hpp"
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

    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> MySQLDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed || !m_mysql)
            {
                return cpp_dbc::unexpected(DBException("M1Y2S3Q4L5C6", "Connection is closed", system_utils::captureCallStack()));
            }

            // Pass weak_ptr to the PreparedStatement so it can safely detect when connection is closed
            auto stmt = std::make_shared<MySQLDBPreparedStatement>(std::weak_ptr<MYSQL>(m_mysql), sql);

            // Register the statement in our registry
            registerStatement(stmt);

            return std::shared_ptr<RelationalDBPreparedStatement>(stmt);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("B4C0D6E2F9AC",
                                                   std::string("prepareStatement failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B4C0D6E2F9AD",
                                                   "prepareStatement failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> MySQLDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed || !m_mysql)
            {
                return cpp_dbc::unexpected(DBException("7I8J9K0L1M2N", "Connection is closed", system_utils::captureCallStack()));
            }

            if (mysql_query(m_mysql.get(), sql.c_str()) != 0)
            {
                return cpp_dbc::unexpected(DBException("M2Y3S4Q5L6C7", std::string("Query failed: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
            }

            MYSQL_RES *result = mysql_store_result(m_mysql.get());
            if (!result && mysql_field_count(m_mysql.get()) > 0)
            {
                return cpp_dbc::unexpected(DBException("M3Y4S5Q6L7C8", std::string("Failed to get result set: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
            }

            return std::shared_ptr<RelationalDBResultSet>(std::make_shared<MySQLDBResultSet>(result));
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C5D1E7F3A0BE",
                                                   std::string("executeQuery failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C5D1E7F3A0BF",
                                                   "executeQuery failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<uint64_t, DBException> MySQLDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed || !m_mysql)
            {
                return cpp_dbc::unexpected(DBException("M4Y5S6Q7L8C9", "Connection is closed", system_utils::captureCallStack()));
            }

            if (mysql_query(m_mysql.get(), sql.c_str()) != 0)
            {
                return cpp_dbc::unexpected(DBException("M5Y6S7Q8L9C0", std::string("Update failed: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
            }

            return mysql_affected_rows(m_mysql.get());
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D6E2F8A4B1CF",
                                                   std::string("executeUpdate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D6E2F8A4B1D0",
                                                   "executeUpdate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::setAutoCommit(std::nothrow_t, bool autoCommitFlag) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed || !m_mysql)
            {
                return cpp_dbc::unexpected(DBException("M6Y7S8Q9L0C1", "Connection is closed", system_utils::captureCallStack()));
            }

            // Only change the SQL state if we're actually changing the mode
            if (this->m_autoCommit != autoCommitFlag)
            {
                if (autoCommitFlag)
                {
                    // Habilitar autocommit (1) y desactivar transacciones
                    std::string query = "SET autocommit=1";
                    if (mysql_query(m_mysql.get(), query.c_str()) != 0)
                    {
                        return cpp_dbc::unexpected(DBException("M7Y8S9Q0L1C2", std::string("Failed to set autocommit mode: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
                    }

                    this->m_autoCommit = true;
                    this->m_transactionActive = false;
                }
                else
                {
                    // Si estamos desactivando autocommit, usar beginTransaction para iniciar la transacción
                    auto beginResult = beginTransaction(std::nothrow);
                    if (!beginResult.has_value())
                    {
                        return cpp_dbc::unexpected(beginResult.error());
                    }
                }
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("E7F3A9B5C2DB",
                                                   std::string("setAutoCommit failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E7F3A9B5C2DC",
                                                   "setAutoCommit failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            return m_autoCommit;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F8A4B0C6D3EC",
                                                   std::string("getAutoCommit failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F8A4B0C6D3ED",
                                                   "getAutoCommit failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed || !m_mysql)
            {
                return cpp_dbc::unexpected(DBException("U7V8W9X0Y1Z2", "Connection is closed", system_utils::captureCallStack()));
            }

            // If transaction is already active, just return true
            if (m_transactionActive)
            {
                return true;
            }

            // Start transaction by disabling autocommit
            std::string query = "SET autocommit=0";
            if (mysql_query(m_mysql.get(), query.c_str()) != 0)
            {
                return cpp_dbc::unexpected(DBException("M8Y9S0Q1L2C3", std::string("Failed to begin transaction: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
            }

            m_autoCommit = false;
            m_transactionActive = true;
            return true;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("A9B5C1D7E4F9",
                                                   std::string("beginTransaction failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("A9B5C1D7E4FA",
                                                   "beginTransaction failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            return m_transactionActive;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("B0C6D2E8F5AB",
                                                   std::string("transactionActive failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B0C6D2E8F5AC",
                                                   "transactionActive failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::commit(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed || !m_mysql)
            {
                return cpp_dbc::unexpected(DBException("M9Y0S1Q2L3C4", "Connection is closed", system_utils::captureCallStack()));
            }

            // No transaction active, nothing to commit
            if (!m_transactionActive)
            {
                return {};
            }

            if (mysql_query(m_mysql.get(), "COMMIT") != 0)
            {
                return cpp_dbc::unexpected(DBException("N0Y1S2Q3L4C5", std::string("Commit failed: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
            }

            m_transactionActive = false;

            // If autoCommit is still false, a new transaction starts automatically in MySQL
            if (!m_autoCommit)
            {
                m_transactionActive = true;
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C1D7E3F9A6BB",
                                                   std::string("commit failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C1D7E3F9A6BC",
                                                   "commit failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::rollback(std::nothrow_t) noexcept
    {
        try
        {

            DB_DRIVER_LOCK_GUARD(m_connMutex);

            if (m_closed || !m_mysql)
            {
                return cpp_dbc::unexpected(DBException("N1Y2S3Q4L5C6", "Connection is closed", system_utils::captureCallStack()));
            }

            // No transaction active, nothing to rollback
            if (!m_transactionActive)
            {
                return {};
            }

            if (mysql_query(m_mysql.get(), "ROLLBACK") != 0)
            {
                return cpp_dbc::unexpected(DBException("N2Y3S4Q5L6C7", std::string("Rollback failed: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
            }

            m_transactionActive = false;

            // If autoCommit is still false, a new transaction starts automatically in MySQL
            if (!m_autoCommit)
            {
                m_transactionActive = true;
            }
            return {};
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("D2E8F4A0B6CB",
                                                   std::string("rollback failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("D2E8F4A0B6CC",
                                                   "rollback failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
