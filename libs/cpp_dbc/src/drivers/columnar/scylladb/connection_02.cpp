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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file connection_02.cpp
 * @brief ScyllaDB ScyllaDBConnection - Part 2 (nothrow versions: DBConnection and ColumnarDBConnection interface)
 */

#include "cpp_dbc/drivers/columnar/driver_scylladb.hpp"

#if USE_SCYLLADB

#include <sstream>
#include "cpp_dbc/common/system_utils.hpp"
#include "scylladb_internal.hpp"

namespace cpp_dbc::ScyllaDB
{

    // ============================================================================
    // ScyllaDBConnection - DBConnection nothrow interface implementations
    // ============================================================================

    cpp_dbc::expected<void, DBException> ScyllaDBConnection::close(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);
        if (m_closed.load(std::memory_order_acquire))
        {
            return {};
        }

        SCYLLADB_DEBUG("ScyllaDBConnection::close(nothrow) - Closing connection");

        if (m_session)
        {
            CassFutureHandle close_future(cass_session_close(m_session.get()));
            cass_future_wait(close_future.get());
            m_session.reset();
        }
        m_cluster.reset();
        m_closed.store(true, std::memory_order_release);

        // Release live connection count for cleanup() guard.
        // Safe against double-decrement: the m_closed check above returns early
        // if already closed, so this line executes exactly once.
        ScyllaDBDriver::s_liveConnectionCount.fetch_sub(1, std::memory_order_release);

        SCYLLADB_DEBUG("ScyllaDBConnection::close(nothrow) - Connection closed");
        return {};
    }

    cpp_dbc::expected<bool, DBException> ScyllaDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<void, DBException> ScyllaDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        return reset(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> ScyllaDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return false;
    }

    cpp_dbc::expected<std::string, DBException> ScyllaDBConnection::getURI(std::nothrow_t) const noexcept
    {
        return m_uri;
    }

    cpp_dbc::expected<void, DBException> ScyllaDBConnection::reset(std::nothrow_t) noexcept
    {
        return prepareForPoolReturn(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> ScyllaDBConnection::ping(std::nothrow_t) noexcept
    {
        auto result = executeQuery(std::nothrow, "SELECT release_version FROM system.local");
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

    // ============================================================================
    // ScyllaDBConnection - ColumnarDBConnection nothrow interface implementations
    // ============================================================================

    cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException>
    ScyllaDBConnection::prepareStatement(std::nothrow_t, const std::string &query) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::prepareStatement - Preparing query: " << query);
        DB_DRIVER_LOCK_GUARD(m_connMutex);
        if (m_closed.load(std::memory_order_acquire) || !m_session)
        {
            SCYLLADB_DEBUG("ScyllaDBConnection::prepareStatement - Connection closed");
            return cpp_dbc::unexpected(DBException("S0T1U2V3W4X5", "Connection closed", system_utils::captureCallStack()));
        }

        CassFutureHandle future(cass_session_prepare(m_session.get(), query.c_str()));

        if (cass_future_error_code(future.get()) != CASS_OK)
        {
            const char *message;
            size_t length;
            cass_future_error_message(future.get(), &message, &length);
            std::string errorMsg(message, length);
            SCYLLADB_DEBUG("ScyllaDBConnection::prepareStatement - Prepare failed: " << errorMsg);
            return cpp_dbc::unexpected(DBException("T1U2V3W4X5Y6", errorMsg, system_utils::captureCallStack()));
        }

        const CassPrepared *prepared = cass_future_get_prepared(future.get());
        SCYLLADB_DEBUG("ScyllaDBConnection::prepareStatement - Query prepared successfully");
        auto stmtResult = ScyllaDBPreparedStatement::create(std::nothrow, m_session, query, prepared);
        if (!stmtResult.has_value())
        {
            return cpp_dbc::unexpected(stmtResult.error());
        }
        return std::shared_ptr<ColumnarDBPreparedStatement>(stmtResult.value());
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException>
    ScyllaDBConnection::executeQuery(std::nothrow_t, const std::string &query) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::executeQuery - Executing query: " << query);
        DB_DRIVER_LOCK_GUARD(m_connMutex);
        if (m_closed.load(std::memory_order_acquire) || !m_session)
        {
            SCYLLADB_DEBUG("ScyllaDBConnection::executeQuery - Connection closed");
            return cpp_dbc::unexpected(DBException("1KNEN1MHOTOA", "Connection closed", system_utils::captureCallStack()));
        }

        CassStatementHandle statement(cass_statement_new(query.c_str(), 0));
        CassFutureHandle future(cass_session_execute(m_session.get(), statement.get()));

        if (cass_future_error_code(future.get()) != CASS_OK)
        {
            const char *message;
            size_t length;
            cass_future_error_message(future.get(), &message, &length);
            std::string errorMsg(message, length);
            SCYLLADB_DEBUG("ScyllaDBConnection::executeQuery - Execution failed: " << errorMsg);
            return cpp_dbc::unexpected(DBException("13JPWFTYAH1G", errorMsg, system_utils::captureCallStack()));
        }

        const CassResult *result = cass_future_get_result(future.get());
        SCYLLADB_DEBUG("ScyllaDBConnection::executeQuery - Query executed successfully");
        return std::shared_ptr<ColumnarDBResultSet>(std::make_shared<ScyllaDBResultSet>(result));
    }

    cpp_dbc::expected<uint64_t, DBException>
    ScyllaDBConnection::executeUpdate(std::nothrow_t, const std::string &query) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::executeUpdate - Executing update: " << query);
        auto res = executeQuery(std::nothrow, query);
        if (!res.has_value())
        {
            SCYLLADB_DEBUG("ScyllaDBConnection::executeUpdate - Update failed");
            return cpp_dbc::unexpected(res.error());
        }

        // Use shared helper for consistent heuristic-based estimation
        // (Cassandra/ScyllaDB doesn't provide actual affected row counts)
        return estimateAffectedRows(query);
    }

    cpp_dbc::expected<bool, DBException> ScyllaDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::beginTransaction - Transactions not supported");
        return false;
    }

    cpp_dbc::expected<void, DBException> ScyllaDBConnection::commit(std::nothrow_t) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::commit - No-op");
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBConnection::rollback(std::nothrow_t) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::rollback - No-op");
        return {};
    }

    cpp_dbc::expected<void, DBException>
    ScyllaDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);
        m_transactionIsolation = level;
        return {};
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    ScyllaDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);
        return m_transactionIsolation;
    }

    cpp_dbc::expected<void, DBException>
    ScyllaDBConnection::prepareForPoolReturn(std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        DB_DRIVER_LOCK_GUARD(m_connMutex);
        SCYLLADB_DEBUG("ScyllaDBConnection::prepareForPoolReturn(nothrow) - Rolling back any active transaction");
        // ScyllaDB/Cassandra has no ACID transactions; cleanup is a no-op.
        // Restore isolation level if requested (store-only, no DB command).
        if (isolationLevel != TransactionIsolationLevel::TRANSACTION_NONE)
        {
            m_transactionIsolation = isolationLevel;
        }
        return {};
    }

    cpp_dbc::expected<void, DBException> ScyllaDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        // ScyllaDB/Cassandra connections are stateless; no refresh needed on borrow
        return {};
    }

    cpp_dbc::expected<std::string, DBException> ScyllaDBConnection::getServerVersion(std::nothrow_t) noexcept
    {
        auto queryResult = executeQuery(std::nothrow, "SELECT release_version FROM system.local");
        if (!queryResult.has_value())
        {
            return cpp_dbc::unexpected(queryResult.error());
        }

        auto &rs = queryResult.value();
        auto nextResult = rs->next(std::nothrow);
        if (!nextResult.has_value() || !nextResult.value())
        {
            return cpp_dbc::unexpected(DBException(
                "X1Z9CGTWP1HA",
                "Failed to retrieve ScyllaDB server version",
                system_utils::captureCallStack()));
        }

        auto versionResult = rs->getString(std::nothrow, 1);
        if (!versionResult.has_value())
        {
            return cpp_dbc::unexpected(versionResult.error());
        }

        return versionResult.value();
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> ScyllaDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        std::map<std::string, std::string> info;

        auto queryResult = executeQuery(std::nothrow,
            "SELECT release_version, cluster_name, data_center, rack, partitioner, cql_version "
            "FROM system.local");
        if (!queryResult.has_value())
        {
            return cpp_dbc::unexpected(queryResult.error());
        }

        auto &rs = queryResult.value();
        auto nextResult = rs->next(std::nothrow);
        if (nextResult.has_value() && nextResult.value())
        {
            auto releaseVersion = rs->getString(std::nothrow, 1);
            if (releaseVersion.has_value())
            {
                info["ServerVersion"] = releaseVersion.value();
            }
            auto clusterName = rs->getString(std::nothrow, 2);
            if (clusterName.has_value())
            {
                info["ClusterName"] = clusterName.value();
            }
            auto dataCenter = rs->getString(std::nothrow, 3);
            if (dataCenter.has_value())
            {
                info["DataCenter"] = dataCenter.value();
            }
            auto rack = rs->getString(std::nothrow, 4);
            if (rack.has_value())
            {
                info["Rack"] = rack.value();
            }
            auto partitioner = rs->getString(std::nothrow, 5);
            if (partitioner.has_value())
            {
                info["Partitioner"] = partitioner.value();
            }
            auto cqlVersion = rs->getString(std::nothrow, 6);
            if (cqlVersion.has_value())
            {
                info["CQLVersion"] = cqlVersion.value();
            }
        }

        return info;
    }

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
