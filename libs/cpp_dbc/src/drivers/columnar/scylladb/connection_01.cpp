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

    // Nothrow constructor — contains all connection logic.
    // Never throws: any failure is recorded in m_initFailed/m_initError so that
    // the delegating throwing constructor and the create() factory can both
    // reuse this code path without duplication.
    // Public for std::make_shared access, but effectively private via PrivateCtorTag.
    ScyllaDBConnection::ScyllaDBConnection(ScyllaDBConnection::PrivateCtorTag,
                                           std::nothrow_t,
                                           const std::string &host,
                                           int port,
                                           const std::string &keyspace,
                                           const std::string &user,
                                           const std::string &password,
                                           [[maybe_unused]] const std::map<std::string, std::string> &options)
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::constructor(nothrow) - Connecting to " << host << ":" << port);
        m_cluster = std::shared_ptr<CassCluster>(cass_cluster_new(), CassClusterDeleter());
        cass_cluster_set_contact_points(m_cluster.get(), host.c_str());
        cass_cluster_set_port(m_cluster.get(), port);

        if (!user.empty())
        {
            SCYLLADB_DEBUG("ScyllaDBConnection::constructor(nothrow) - Setting credentials for user: " << user);
            cass_cluster_set_credentials(m_cluster.get(), user.c_str(), password.c_str());
        }

        m_session = std::shared_ptr<CassSession>(cass_session_new(), CassSessionDeleter());

        SCYLLADB_DEBUG("ScyllaDBConnection::constructor(nothrow) - Connecting to cluster");
        CassFutureHandle connect_future(cass_session_connect(m_session.get(), m_cluster.get()));

        if (cass_future_error_code(connect_future.get()) != CASS_OK)
        {
            const char *message;
            size_t length;
            cass_future_error_message(connect_future.get(), &message, &length);
            std::string errorMsg(message, length);
            SCYLLADB_DEBUG("ScyllaDBConnection::constructor(nothrow) - Connection failed: " << errorMsg);
            m_initFailed = true;
            m_initError = std::make_unique<DBException>("Q8R9S0T1U2V3", errorMsg, system_utils::captureCallStack());
            return;
        }

        SCYLLADB_DEBUG("ScyllaDBConnection::constructor(nothrow) - Connected successfully");

        if (!keyspace.empty())
        {
            // Validate keyspace name to prevent CQL injection.
            // Keyspace names should only contain alphanumeric characters and underscores.
            bool isValidKeyspace = std::ranges::all_of(keyspace, [](unsigned char c)
                                                       { return std::isalnum(c) || c == '_'; });
            if (!isValidKeyspace)
            {
                m_initFailed = true;
                m_initError = std::make_unique<DBException>("7A3F9E2B5C8D", "Invalid keyspace name: " + keyspace, system_utils::captureCallStack());
                return;
            }

            SCYLLADB_DEBUG("ScyllaDBConnection::constructor(nothrow) - Using keyspace: " << keyspace);
            std::string query = "USE " + keyspace;
            CassStatementHandle statement(cass_statement_new(query.c_str(), 0));
            CassFutureHandle future(cass_session_execute(m_session.get(), statement.get()));
            if (cass_future_error_code(future.get()) != CASS_OK)
            {
                SCYLLADB_DEBUG("ScyllaDBConnection::constructor(nothrow) - Failed to use keyspace: " << keyspace);
                m_initFailed = true;
                m_initError = std::make_unique<DBException>("R9S0T1U2V3W4", "Failed to use keyspace " + keyspace, system_utils::captureCallStack());
                return;
            }
        }

        m_closed = false;

        // Cache the full URI including the cpp_dbc: prefix (reuse centralized builder for IPv6 bracket handling)
        m_uri = system_utils::buildDBURI("cpp_dbc:scylladb://", host, port, keyspace);

        SCYLLADB_DEBUG("ScyllaDBConnection::constructor(nothrow) - Connection established");
    }

    ScyllaDBConnection::~ScyllaDBConnection()
    {
        SCYLLADB_DEBUG("ScyllaDBConnection::destructor - Destroying connection");
        ScyllaDBConnection::close(std::nothrow);
    }

#ifdef __cpp_exceptions
    // DBConnection interface — throwing wrappers

    void ScyllaDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool ScyllaDBConnection::isClosed() const
    {
        return m_closed.load(std::memory_order_acquire);
    }

    void ScyllaDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool ScyllaDBConnection::isPooled() const
    {
        return false;
    }

    std::string ScyllaDBConnection::getURI() const
    {
        return m_uri;
    }

    void ScyllaDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool ScyllaDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    // ColumnarDBConnection interface — throwing wrappers (same order as in connection.hpp)

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
        auto result = beginTransaction(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    void ScyllaDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ScyllaDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ScyllaDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel ScyllaDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string ScyllaDBConnection::getServerVersion()
    {
        auto result = getServerVersion(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

    std::map<std::string, std::string> ScyllaDBConnection::getServerInfo()
    {
        auto result = getServerInfo(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }

#endif // __cpp_exceptions

} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
