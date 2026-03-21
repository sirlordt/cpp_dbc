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
 @brief MySQL database driver implementation - MySQLDBConnection protected overrides and nothrow methods (part 2 - transactions and server info)

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
    // Protected overrides — pool lifecycle
    // ============================================================================

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

    // ============================================================================
    // Nothrow API — part 2: transaction isolation and server info
    // ============================================================================

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

        if (mysql_query(m_conn.get(), query.c_str()) != 0)
        {
            return cpp_dbc::unexpected(DBException("KMGSGPBOPDPV", std::string("Failed to set transaction isolation level: ") + mysql_error(m_conn.get()), system_utils::captureCallStack()));
        }

        // Verify that the isolation level was actually set
        auto actualLevelResult = getTransactionIsolation(std::nothrow);
        if (!actualLevelResult.has_value())
        {
            return cpp_dbc::unexpected(actualLevelResult.error());
        }
        TransactionIsolationLevel actualLevel = *actualLevelResult;

        // Update cached state to reflect what the server actually applied
        this->m_isolationLevel = actualLevel;

        if (actualLevel != level)
        {
            return cpp_dbc::unexpected(DBException("HDRX22G5JGOI",
                "Transaction isolation level mismatch: requested " +
                    std::string(toStringView(level)) + " but server applied " +
                    std::string(toStringView(actualLevel)),
                system_utils::captureCallStack()));
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
        if (mysql_query(m_conn.get(), "SELECT @@transaction_isolation") != 0 &&
            mysql_query(m_conn.get(), "SELECT @@tx_isolation") != 0)
        {
            m_inGetTransactionIsolation = false;
            return cpp_dbc::unexpected(DBException("I0JKHM4RHV1G", std::string("Failed to get transaction isolation level: ") + mysql_error(m_conn.get()), system_utils::captureCallStack()));
        }

        MySQLResHandle resultHandle(mysql_store_result(m_conn.get()));
        if (!resultHandle)
        {
            m_inGetTransactionIsolation = false;
            return cpp_dbc::unexpected(DBException("DB6UZR9SYKMQ", std::string("Failed to get result set: ") + mysql_error(m_conn.get()), system_utils::captureCallStack()));
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

    cpp_dbc::expected<std::string, DBException> MySQLDBConnection::getServerVersion(std::nothrow_t) noexcept
    {
        MYSQL_CONNECTION_LOCK_OR_RETURN("DSU8478ZA6RH", "Connection closed");

        const char *version = mysql_get_server_info(m_conn.get());
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

        const char *version = mysql_get_server_info(m_conn.get());
        if (version)
        {
            info["ServerVersion"] = version;
        }

        unsigned long serverVersion = mysql_get_server_version(m_conn.get());
        info["ServerVersionNumeric"] = std::to_string(serverVersion);

        const char *hostInfo = mysql_get_host_info(m_conn.get());
        if (hostInfo)
        {
            info["HostInfo"] = hostInfo;
        }

        unsigned long protocolVersion = mysql_get_proto_info(m_conn.get());
        info["ProtocolVersion"] = std::to_string(protocolVersion);

        const char *charset = mysql_character_set_name(m_conn.get());
        if (charset)
        {
            info["CharacterSet"] = charset;
        }

        unsigned long threadId = mysql_thread_id(m_conn.get());
        info["ThreadId"] = std::to_string(threadId);

        const char *stat = mysql_stat(m_conn.get());
        if (stat)
        {
            info["ServerStatus"] = stat;
        }

        return info;
    }

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
