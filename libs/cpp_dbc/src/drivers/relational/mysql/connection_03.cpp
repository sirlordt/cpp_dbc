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
 @brief MySQL database driver implementation - MySQLDBConnection nothrow methods (part 2 - transactions)

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

    cpp_dbc::expected<void, DBException> MySQLDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("47FCEE77D4F3", "Cannot set transaction isolation");

        using enum TransactionIsolationLevel;
        std::string query;
        switch (level)
        {
        case TRANSACTION_READ_UNCOMMITTED:
        {
            query = "SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
            break;
        }
        case TRANSACTION_READ_COMMITTED:
        {
            query = "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED";
            break;
        }
        case TRANSACTION_REPEATABLE_READ:
        {
            query = "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ";
            break;
        }
        case TRANSACTION_SERIALIZABLE:
        {
            query = "SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE";
            break;
        }
        default:
        {
            return cpp_dbc::unexpected(DBException("IFRBRHGBEVGR", "Unsupported transaction isolation level", system_utils::captureCallStack()));
        }
        }

        if (mysql_query(m_mysql.get(), query.c_str()) != 0)
        {
            return cpp_dbc::unexpected(DBException("KMGSGPBOPDPV", std::string("Failed to set transaction isolation level: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
        }

        // Verify that the isolation level was actually set
        auto actualLevelResult = getTransactionIsolation(std::nothrow);
        if (!actualLevelResult.has_value())
        {
            return cpp_dbc::unexpected(actualLevelResult.error());
        }
        TransactionIsolationLevel actualLevel = *actualLevelResult;

        if (actualLevel != level)
        {
            // Some MySQL configurations might not allow certain isolation levels
            // In this case, we'll update our internal state to match what MySQL actually set
            this->m_isolationLevel = actualLevel;
        }
        else
        {
            this->m_isolationLevel = level;
        }
        return {};
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException> MySQLDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("UTJ1TFQIYZ5D", "Cannot get transaction isolation");

        // If we're being called from setTransactionIsolation, return the cached value
        // to avoid potential infinite recursion
        if (m_inGetTransactionIsolation)
        {
            return this->m_isolationLevel;
        }

        m_inGetTransactionIsolation = true;

        // Query the current isolation level (try newer syntax first, then fall back to older MySQL versions)
        if (mysql_query(m_mysql.get(), "SELECT @@transaction_isolation") != 0 &&
            mysql_query(m_mysql.get(), "SELECT @@tx_isolation") != 0)
        {
            m_inGetTransactionIsolation = false;
            return cpp_dbc::unexpected(DBException("I0JKHM4RHV1G", std::string("Failed to get transaction isolation level: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
        }

        MySQLResHandle resultHandle(mysql_store_result(m_mysql.get()));
        if (!resultHandle)
        {
            m_inGetTransactionIsolation = false;
            return cpp_dbc::unexpected(DBException("DB6UZR9SYKMQ", std::string("Failed to get result set: ") + mysql_error(m_mysql.get()), system_utils::captureCallStack()));
        }

        MYSQL_ROW row = mysql_fetch_row(resultHandle.get());
        if (!row)
        {
            m_inGetTransactionIsolation = false;
            return cpp_dbc::unexpected(DBException("DJ3HAJ9PH240", "Failed to fetch transaction isolation level", system_utils::captureCallStack()));
        }

        std::string level = row[0];

        // Convert the string value to the enum
        TransactionIsolationLevel isolationResult;
        if (level == "READ-UNCOMMITTED" || level == "READ_UNCOMMITTED")
        {
            isolationResult = TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED;
        }
        else if (level == "READ-COMMITTED" || level == "READ_COMMITTED")
        {
            isolationResult = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;
        }
        else if (level == "REPEATABLE-READ" || level == "REPEATABLE_READ")
        {
            isolationResult = TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ;
        }
        else if (level == "SERIALIZABLE")
        {
            isolationResult = TransactionIsolationLevel::TRANSACTION_SERIALIZABLE;
        }
        else
        {
            isolationResult = TransactionIsolationLevel::TRANSACTION_NONE;
        }

        // Update our cached value
        this->m_isolationLevel = isolationResult;
        m_inGetTransactionIsolation = false;
        return isolationResult;
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::close(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        // Close all active result sets and statements before closing the connection
        // This ensures mysql_stmt_close() is called while we have exclusive access
        [[maybe_unused]] auto closeRsResult = closeAllActiveResultSets(std::nothrow);
        [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

        // 2026-03-07T00:00:00Z
        // Bug: Destroying the MYSQL handle immediately after closing all statements and result
        // sets can race with in-flight MySQL C API calls on other threads that still reference
        // the handle, causing use-after-free crashes.
        // Solution: A 25ms delay gives in-flight operations time to complete before the handle
        // is destroyed. This is a pragmatic workaround pending a refcount-based solution.
        std::this_thread::sleep_for(std::chrono::milliseconds(25));

        m_mysql.reset();
        m_closed.store(true, std::memory_order_release);
        return {};
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::reset(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN_SUCCESS_IF_CLOSED();

        // RAII guard to set m_resetting flag during reset operation
        // This prevents ResultSet/PreparedStatement from calling unregister during closeAll*()
        cpp_dbc::system_utils::AtomicGuard<bool> resettingGuard(m_resetting, true, false);

        // Close all active result sets and statements
        [[maybe_unused]] auto closeRsResult = closeAllActiveResultSets(std::nothrow);
        [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

        // Rollback any active transaction
        auto txActive = transactionActive(std::nothrow);
        if (txActive.has_value() && txActive.value())
        {
            [[maybe_unused]] auto rbResult = rollback(std::nothrow);
        }

        // Reset auto-commit to true
        [[maybe_unused]] auto acResult = setAutoCommit(std::nothrow, true);

        return {};
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        // CRITICAL: Close all active result sets and statements BEFORE making connection available
        // closeAllStatements() acquires m_connMutex internally
        [[maybe_unused]] auto closeRsResult = closeAllActiveResultSets(std::nothrow);
        [[maybe_unused]] auto closeStmtsResult = closeAllStatements(std::nothrow);

        // Restore autocommit for the next user of this connection
        if (!m_autoCommit)
        {
            setAutoCommit(std::nothrow, true);
        }

        return {};
    }

    cpp_dbc::expected<bool, DBException> MySQLDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBConnection::getURL(std::nothrow_t) const noexcept
    {
        return m_url;
    }

    cpp_dbc::expected<void, DBException> MySQLDBConnection::prepareForPoolReturn(
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

    cpp_dbc::expected<void, DBException> MySQLDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        // No-op for MySQL: no MVCC snapshot refresh needed
        return {};
    }

    cpp_dbc::expected<std::string, DBException> MySQLDBConnection::getServerVersion(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("DSU8478ZA6RH", "Connection closed");

        const char *version = mysql_get_server_info(m_mysql.get());
        if (!version)
        {
            return cpp_dbc::unexpected(DBException(
                "LO4E9S7LYL2N",
                "Failed to get MySQL server version",
                system_utils::captureCallStack()));
        }

        return std::string(version);
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> MySQLDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("SPNJEMWFRIKJ", "Connection closed");

        std::map<std::string, std::string> info;

        const char *version = mysql_get_server_info(m_mysql.get());
        if (version)
        {
            info["ServerVersion"] = version;
        }

        unsigned long serverVersion = mysql_get_server_version(m_mysql.get());
        info["ServerVersionNumeric"] = std::to_string(serverVersion);

        const char *hostInfo = mysql_get_host_info(m_mysql.get());
        if (hostInfo)
        {
            info["HostInfo"] = hostInfo;
        }

        unsigned long protocolVersion = mysql_get_proto_info(m_mysql.get());
        info["ProtocolVersion"] = std::to_string(protocolVersion);

        const char *charset = mysql_character_set_name(m_mysql.get());
        if (charset)
        {
            info["CharacterSet"] = charset;
        }

        unsigned long threadId = mysql_thread_id(m_mysql.get());
        info["ThreadId"] = std::to_string(threadId);

        const char *stat = mysql_stat(m_mysql.get());
        if (stat)
        {
            info["ServerStatus"] = stat;
        }

        return info;
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
