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
 * @file columnar_db_connection_pool.cpp
 * @brief Thin columnar pool — delegates all pool infrastructure to DBConnectionPoolBase.
 *
 * Only contains: constructors, createDBConnection, createPooledDBConnection,
 * getColumnarDBConnection, factory methods, and the columnar-specific
 * ColumnarPooledDBConnection methods (prepareStatement, executeQuery, etc.).
 *
 * Common pooled-connection logic (close, returnToPool, destructor, pool metadata)
 * is inherited from PooledDBConnectionBase via CRTP.
 */

#include "cpp_dbc/pool/columnar/columnar_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "../connection_pool_internal.hpp"

namespace cpp_dbc
{
    // ── ColumnarDBConnectionPool ─────────────────────────────────────────────

    // ── Constructors (delegate to base) ──────────────────────────────────────

    ColumnarDBConnectionPool::ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag,
                                                       const std::string &uri,
                                                       const std::string &username,
                                                       const std::string &password,
                                                       const std::map<std::string, std::string> &options,
                                                       size_t initialSize,
                                                       size_t maxSize,
                                                       size_t minIdle,
                                                       size_t maxWaitMillis,
                                                       size_t validationTimeoutMillis,
                                                       size_t idleTimeoutMillis,
                                                       size_t maxLifetimeMillis,
                                                       bool testOnBorrow,
                                                       bool testOnReturn,
                                                       TransactionIsolationLevel transactionIsolation) noexcept
        : DBConnectionPoolBase(DBConnectionPool::ConstructorTag{}, uri, username, password, options,
                               initialSize, maxSize, minIdle, maxWaitMillis, validationTimeoutMillis,
                               idleTimeoutMillis, maxLifetimeMillis, testOnBorrow, testOnReturn,
                               transactionIsolation)
    {
        // All initialization handled by DBConnectionPoolBase
    }

    ColumnarDBConnectionPool::ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : DBConnectionPoolBase(DBConnectionPool::ConstructorTag{}, config)
    {
        // All initialization handled by DBConnectionPoolBase
    }

    // ── Destructor ───────────────────────────────────────────────────────────

    // DBConnectionPoolBase destructor handles close() and cleanup
    ColumnarDBConnectionPool::~ColumnarDBConnectionPool() = default;

    // ── Private helpers ──────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException>
    ColumnarDBConnectionPool::createDBConnection(std::nothrow_t) const noexcept
    {
        auto dbConnResult = DriverManager::getDBConnection(std::nothrow, getUri(), getUsername(), getPassword(), getOptions());
        if (!dbConnResult.has_value())
        {
            return cpp_dbc::unexpected(dbConnResult.error());
        }

        auto columnarConn = std::dynamic_pointer_cast<ColumnarDBConnection>(dbConnResult.value());
        if (!columnarConn)
        {
            return cpp_dbc::unexpected(DBException("TP7X7T9LHSDV", "Connection pool only supports columnar database connections", system_utils::captureCallStack()));
        }
        return columnarConn;
    }

    cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
    ColumnarDBConnectionPool::createPooledDBConnection(std::nothrow_t) noexcept
    {
        auto connResult = createDBConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        auto conn = connResult.value();

        // Set transaction isolation level on the new connection
        auto isoResult = conn->setTransactionIsolation(std::nothrow, getTransactionIsolation());
        if (!isoResult.has_value())
        {
            return cpp_dbc::unexpected(isoResult.error());
        }

        // Create pooled connection with weak_ptr to this pool.
        // shared_from_this() returns shared_ptr<DBConnectionPoolBase>; downcast to the concrete type
        // because ColumnarPooledDBConnection stores weak_ptr<ColumnarDBConnectionPool>.
        std::weak_ptr<ColumnarDBConnectionPool> weakPool;
        try
        {
            weakPool = std::static_pointer_cast<ColumnarDBConnectionPool>(shared_from_this());
        }
        catch (const std::bad_weak_ptr &ex)
        {
            // Pool not managed by shared_ptr — pooled connection would have no way to return to pool
            CP_DEBUG("ColumnarDBConnectionPool::createPooledDBConnection - Pool not managed by shared_ptr: %s", ex.what());
            return cpp_dbc::unexpected(DBException("768A1PHQK011",
                                                   "Pool is not managed by shared_ptr, cannot create pooled connection",
                                                   system_utils::captureCallStack()));
        }

        return std::static_pointer_cast<DBConnectionPooled>(
            std::make_shared<ColumnarPooledDBConnection>(conn, weakPool, getPoolAliveFlag()));
    }

    // ── Static factories ─────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnectionPool>, DBException> ColumnarDBConnectionPool::create(std::nothrow_t,
                                                                                                               const std::string &uri,
                                                                                                               const std::string &username,
                                                                                                               const std::string &password,
                                                                                                               const std::map<std::string, std::string> &options,
                                                                                                               size_t initialSize,
                                                                                                               size_t maxSize,
                                                                                                               size_t minIdle,
                                                                                                               size_t maxWaitMillis,
                                                                                                               size_t validationTimeoutMillis,
                                                                                                               size_t idleTimeoutMillis,
                                                                                                               size_t maxLifetimeMillis,
                                                                                                               bool testOnBorrow,
                                                                                                               bool testOnReturn,
                                                                                                               TransactionIsolationLevel transactionIsolation) noexcept
    {
        auto pool = std::make_shared<ColumnarDBConnectionPool>(
            DBConnectionPool::ConstructorTag{}, uri, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, transactionIsolation);

        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }

        return pool;
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnectionPool>, DBException> ColumnarDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        auto pool = std::make_shared<ColumnarDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }

        return pool;
    }

    // ── Family-specific getter ───────────────────────────────────────────────

#ifdef __cpp_exceptions

    std::shared_ptr<ColumnarDBConnection> ColumnarDBConnectionPool::getColumnarDBConnection()
    {
        auto result = getColumnarDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException>
    ColumnarDBConnectionPool::getColumnarDBConnection(std::nothrow_t) noexcept
    {
        auto result = acquireConnection(std::nothrow);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        // acquireConnection() returns shared_ptr<DBConnectionPooled>. The underlying object
        // is a ColumnarPooledDBConnection which inherits from both DBConnectionPooled and
        // ColumnarDBConnection. dynamic_pointer_cast navigates the diamond correctly.
        auto conn = std::dynamic_pointer_cast<ColumnarDBConnection>(result.value());
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("JK047VF8Z7P6",
                                                   "Failed to cast pooled connection to ColumnarDBConnection",
                                                   system_utils::captureCallStack()));
        }
        return conn;
    }

    // ════════════════════════════════════════════════════════════════════════
    // ColumnarPooledDBConnection implementation
    // ════════════════════════════════════════════════════════════════════════

    ColumnarPooledDBConnection::ColumnarPooledDBConnection(
        std::shared_ptr<ColumnarDBConnection> connection,
        std::weak_ptr<ColumnarDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive) noexcept
        : Base(std::move(connection), std::move(connectionPool), std::move(poolAlive))
    {
        // All members initialized by CRTP base
    }

    // ── Columnar-specific throwing methods ────────────────────────────────────

#ifdef __cpp_exceptions

    std::shared_ptr<ColumnarDBPreparedStatement> ColumnarPooledDBConnection::prepareStatement(const std::string &query)
    {
        auto result = prepareStatement(std::nothrow, query);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<ColumnarDBResultSet> ColumnarPooledDBConnection::executeQuery(const std::string &query)
    {
        auto result = executeQuery(std::nothrow, query);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t ColumnarPooledDBConnection::executeUpdate(const std::string &query)
    {
        auto result = executeUpdate(std::nothrow, query);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool ColumnarPooledDBConnection::beginTransaction()
    {
        auto result = beginTransaction(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void ColumnarPooledDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ColumnarPooledDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ColumnarPooledDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel ColumnarPooledDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    // ── Columnar-specific nothrow methods ─────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> ColumnarPooledDBConnection::prepareStatement(std::nothrow_t, const std::string &query) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("T3SWMWQ4CMKE",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->prepareStatement(std::nothrow, query);
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> ColumnarPooledDBConnection::executeQuery(std::nothrow_t, const std::string &query) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("PTUV21WCDWB7",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->executeQuery(std::nothrow, query);
    }

    cpp_dbc::expected<uint64_t, DBException> ColumnarPooledDBConnection::executeUpdate(std::nothrow_t, const std::string &query) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("5OY4J9KQ4RWE",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->executeUpdate(std::nothrow, query);
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("XJ11KQTI03DD",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->beginTransaction(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::commit(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("N7OGZJNMWCQ4",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->commit(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::rollback(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("PEY7OGNBFRQE",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->rollback(std::nothrow);
    }

    cpp_dbc::expected<void, DBException>
    ColumnarPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("WQQNTF57OIB3",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setTransactionIsolation(std::nothrow, level);
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    ColumnarPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("RL7UNKR8KXVW",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getTransactionIsolation(std::nothrow);
    }

} // namespace cpp_dbc

// ScyllaDB connection pool implementation
namespace cpp_dbc::ScyllaDB
{
    ScyllaDBConnectionPool::ScyllaDBConnectionPool(DBConnectionPool::ConstructorTag,
                                                   const std::string &uri,
                                                   const std::string &username,
                                                   const std::string &password) noexcept
        : ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag{}, uri, username, password)
    {
        // Scylla-specific initialization if needed
    }

    ScyllaDBConnectionPool::ScyllaDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag{}, config)
    {
        // Scylla-specific initialization if needed
    }

#ifdef __cpp_exceptions

    std::shared_ptr<ScyllaDBConnectionPool> ScyllaDBConnectionPool::create(const std::string &uri,
                                                                           const std::string &username,
                                                                           const std::string &password)
    {
        auto result = create(std::nothrow, uri, username, password);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<ScyllaDBConnectionPool> ScyllaDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto result = create(std::nothrow, config);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<ScyllaDBConnectionPool>, DBException> ScyllaDBConnectionPool::create(std::nothrow_t,
                                                                                                           const std::string &uri,
                                                                                                           const std::string &username,
                                                                                                           const std::string &password) noexcept
    {
        auto pool = std::make_shared<ScyllaDBConnectionPool>(DBConnectionPool::ConstructorTag{}, uri, username, password);
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return pool;
    }

    cpp_dbc::expected<std::shared_ptr<ScyllaDBConnectionPool>, DBException> ScyllaDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        auto pool = std::make_shared<ScyllaDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return pool;
    }
} // namespace cpp_dbc::ScyllaDB
