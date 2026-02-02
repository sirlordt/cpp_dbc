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
 * @file connection_01.cpp
 * @brief ScyllaDB database driver implementation - ScyllaDBConnection class (constructor, destructor, all methods)
 */

#include "cpp_dbc/drivers/columnar/driver_scylladb.hpp"

#if USE_SCYLLADB

#include <array>
#include <cctype>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include "cpp_dbc/common/system_utils.hpp"
#include "scylladb_internal.hpp"

namespace cpp_dbc::ScyllaDB
{
    // ====================================================================
    // ScyllaDBConnection
    // ====================================================================

    ScyllaDBConnection::ScyllaDBConnection(const std::string &host, int port, const std::string &keyspace,
                                           const std::string &user, const std::string &password,
                                           const std::map<std::string, std::string> &)
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::constructor - Connecting to " << host << ":" << port);
        m_cluster = std::shared_ptr<CassCluster>(cass_cluster_new(), CassClusterDeleter());
        cass_cluster_set_contact_points(m_cluster.get(), host.c_str());
        cass_cluster_set_port(m_cluster.get(), port);

        if (!user.empty())
        {
            SCYLLADB_DEBUG("ScyllaDBConnection::constructor - Setting credentials for user: " << user);
            cass_cluster_set_credentials(m_cluster.get(), user.c_str(), password.c_str());
        }

        m_session = std::shared_ptr<CassSession>(cass_session_new(), CassSessionDeleter());

        // Connect
        SCYLLADB_DEBUG("ScyllaDBConnection::constructor - Connecting to cluster");
        CassFutureHandle connect_future(cass_session_connect(m_session.get(), m_cluster.get()));

        if (cass_future_error_code(connect_future.get()) != CASS_OK)
        {
            const char *message;
            size_t length;
            cass_future_error_message(connect_future.get(), &message, &length);
            std::string errorMsg(message, length);
            SCYLLADB_DEBUG("ScyllaDBConnection::constructor - Connection failed: " << errorMsg);
            throw DBException("Q8R9S0T1U2V3", errorMsg, system_utils::captureCallStack());
        }

        SCYLLADB_DEBUG("ScyllaDBConnection::constructor - Connected successfully");

        // Use keyspace if provided
        if (!keyspace.empty())
        {
            SCYLLADB_DEBUG("ScyllaDBConnection::constructor - Using keyspace: " << keyspace);
            std::string query = "USE " + keyspace;
            CassStatementHandle statement(cass_statement_new(query.c_str(), 0));
            CassFutureHandle future(cass_session_execute(m_session.get(), statement.get()));
            if (cass_future_error_code(future.get()) != CASS_OK)
            {
                SCYLLADB_DEBUG("ScyllaDBConnection::constructor - Failed to use keyspace: " << keyspace);
                throw DBException("R9S0T1U2V3W4", "Failed to use keyspace " + keyspace, system_utils::captureCallStack());
            }
        }

        m_closed = false;
        m_url = "scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;
        SCYLLADB_DEBUG("ScyllaDBConnection::constructor - Connection established");
    }

    ScyllaDBConnection::~ScyllaDBConnection()
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::destructor - Destroying connection");
        ScyllaDBConnection::close();
    }

    void ScyllaDBConnection::close()
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::close - Closing connection");
        DB_DRIVER_LOCK_GUARD(m_connMutex);
        if (!m_closed && m_session)
        {
            CassFutureHandle close_future(cass_session_close(m_session.get()));
            cass_future_wait(close_future.get());
            m_session.reset();
            m_cluster.reset();
            m_closed = true;
            SCYLLADB_DEBUG("ScyllaDBConnection::close - Connection closed");
        }
    }

    bool ScyllaDBConnection::isClosed() const
    {
        return m_closed;
    }

    void ScyllaDBConnection::returnToPool()
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::returnToPool - No-op");
        // No-op for now
    }

    bool ScyllaDBConnection::isPooled()
    {
        return false;
    }

    std::string ScyllaDBConnection::getURL() const
    {
        return m_url;
    }

    std::shared_ptr<ColumnarDBPreparedStatement> ScyllaDBConnection::prepareStatement(const std::string &query)
    {
        auto result = prepareStatement(std::nothrow, query);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::shared_ptr<ColumnarDBResultSet> ScyllaDBConnection::executeQuery(const std::string &query)
    {
        auto result = executeQuery(std::nothrow, query);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    uint64_t ScyllaDBConnection::executeUpdate(const std::string &query)
    {
        auto result = executeUpdate(std::nothrow, query);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    bool ScyllaDBConnection::beginTransaction()
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::beginTransaction - Transactions not supported in ScyllaDB driver");
        // Scylla/Cassandra doesn't support ACID transactions in the traditional sense
        // Lightweight Transactions (LWT) exist but are different.
        return false;
    }

    void ScyllaDBConnection::commit()
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::commit - No-op");
    }

    void ScyllaDBConnection::rollback()
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::rollback - No-op");
    }

    // Nothrow API

    cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> ScyllaDBConnection::prepareStatement(std::nothrow_t, const std::string &query) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::prepareStatement - Preparing query: " << query);
        DB_DRIVER_LOCK_GUARD(m_connMutex);
        if (m_closed || !m_session)
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
        return std::shared_ptr<ColumnarDBPreparedStatement>(std::make_shared<ScyllaDBPreparedStatement>(m_session, query, prepared));
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> ScyllaDBConnection::executeQuery(std::nothrow_t, const std::string &query) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::executeQuery - Executing query: " << query);
        DB_DRIVER_LOCK_GUARD(m_connMutex);
        if (m_closed || !m_session)
        {
            SCYLLADB_DEBUG("ScyllaDBConnection::executeQuery - Connection closed");
            return cpp_dbc::unexpected(DBException("8A350B08A3B3", "Connection closed", system_utils::captureCallStack()));
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
            return cpp_dbc::unexpected(DBException("772E10871903", errorMsg, system_utils::captureCallStack()));
        }

        const CassResult *result = cass_future_get_result(future.get());
        SCYLLADB_DEBUG("ScyllaDBConnection::executeQuery - Query executed successfully");
        return std::shared_ptr<ColumnarDBResultSet>(std::make_shared<ScyllaDBResultSet>(result));
    }

    cpp_dbc::expected<uint64_t, DBException> ScyllaDBConnection::executeUpdate(std::nothrow_t, const std::string &query) noexcept
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::executeUpdate - Executing update: " << query);
        auto res = executeQuery(std::nothrow, query);
        if (!res)
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

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
