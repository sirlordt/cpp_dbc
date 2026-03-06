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
 * @file relational_db_connection_pool.cpp
 * @brief Thin relational pool — delegates all pool infrastructure to DBConnectionPoolBase.
 *
 * Only contains: constructors, createDBConnection, createPooledDBConnection,
 * getRelationalDBConnection, factory methods, and the relational-specific
 * RelationalPooledDBConnection methods (prepareStatement, executeQuery, etc.).
 *
 * Common pooled-connection logic (close, returnToPool, destructor, pool metadata)
 * is inherited from PooledDBConnectionBase via CRTP.
 */

#include "cpp_dbc/pool/relational/relational_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "../connection_pool_internal.hpp"

namespace cpp_dbc
{
    // ── RelationalDBConnectionPool ────────────────────────────────────────────

    // ── Constructors (delegate to base) ──────────────────────────────────────

    RelationalDBConnectionPool::RelationalDBConnectionPool(DBConnectionPool::ConstructorTag,
                                                           const std::string &url,
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
        : DBConnectionPoolBase(DBConnectionPool::ConstructorTag{}, url, username, password, options,
                               initialSize, maxSize, minIdle, maxWaitMillis, validationTimeoutMillis,
                               idleTimeoutMillis, maxLifetimeMillis, testOnBorrow, testOnReturn,
                               transactionIsolation)
    {
        // All initialization handled by DBConnectionPoolBase
    }

    RelationalDBConnectionPool::RelationalDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : DBConnectionPoolBase(DBConnectionPool::ConstructorTag{}, config)
    {
        // All initialization handled by DBConnectionPoolBase
    }

    // ── Destructor ───────────────────────────────────────────────────────────

    // DBConnectionPoolBase destructor handles close() and cleanup
    RelationalDBConnectionPool::~RelationalDBConnectionPool() = default;

    // ── Private helpers ──────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>
    RelationalDBConnectionPool::createDBConnection(std::nothrow_t) const noexcept
    {
        auto dbConnResult = DriverManager::getDBConnection(std::nothrow, getUrl(), getUsername(), getPassword(), getOptions());
        if (!dbConnResult.has_value())
        {
            return cpp_dbc::unexpected(dbConnResult.error());
        }

        auto relationalConn = std::dynamic_pointer_cast<RelationalDBConnection>(dbConnResult.value());
        if (!relationalConn)
        {
            return cpp_dbc::unexpected(DBException("B3K9L4M1N2P5", "Connection pool only supports relational database connections", system_utils::captureCallStack()));
        }
        return relationalConn;
    }

    cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
    RelationalDBConnectionPool::createPooledDBConnection(std::nothrow_t) noexcept
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
        // because RelationalPooledDBConnection stores weak_ptr<RelationalDBConnectionPool>.
        std::weak_ptr<RelationalDBConnectionPool> weakPool;
        try
        {
            weakPool = std::static_pointer_cast<RelationalDBConnectionPool>(shared_from_this());
        }
        catch (const std::bad_weak_ptr &ex)
        {
            // Pool not managed by shared_ptr — pooled connection would have no way to return to pool
            CP_DEBUG("RelationalDBConnectionPool::createPooledDBConnection - Pool not managed by shared_ptr: %s", ex.what());
            return cpp_dbc::unexpected(DBException("RXS8PODA5LMG",
                                                   "Pool is not managed by shared_ptr, cannot create pooled connection",
                                                   system_utils::captureCallStack()));
        }

        return std::static_pointer_cast<DBConnectionPooled>(
            std::make_shared<RelationalPooledDBConnection>(conn, weakPool, getPoolAliveFlag()));
    }

    // ── Static factories ─────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnectionPool>, DBException> RelationalDBConnectionPool::create(std::nothrow_t,
                                                                                                                   const std::string &url,
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
        auto pool = std::make_shared<RelationalDBConnectionPool>(
            DBConnectionPool::ConstructorTag{}, url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, transactionIsolation);

        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }

        return pool;
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnectionPool>, DBException> RelationalDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        auto pool = std::make_shared<RelationalDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }

        return pool;
    }

    // ── Family-specific getter ───────────────────────────────────────────────

#ifdef __cpp_exceptions

    std::shared_ptr<RelationalDBConnection> RelationalDBConnectionPool::getRelationalDBConnection()
    {
        auto result = getRelationalDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>
    RelationalDBConnectionPool::getRelationalDBConnection(std::nothrow_t) noexcept
    {
        auto result = acquireConnection(std::nothrow);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        // acquireConnection() returns shared_ptr<DBConnectionPooled>. The underlying object
        // is a RelationalPooledDBConnection which inherits from both DBConnectionPooled and
        // RelationalDBConnection. dynamic_pointer_cast navigates the diamond correctly.
        auto conn = std::dynamic_pointer_cast<RelationalDBConnection>(result.value());
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("H3WZ5N5DLVDY",
                                                   "Failed to cast pooled connection to RelationalDBConnection",
                                                   system_utils::captureCallStack()));
        }
        return conn;
    }

    // ════════════════════════════════════════════════════════════════════════
    // RelationalPooledDBConnection implementation
    // ════════════════════════════════════════════════════════════════════════

    RelationalPooledDBConnection::RelationalPooledDBConnection(
        std::shared_ptr<RelationalDBConnection> connection,
        std::weak_ptr<RelationalDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive) noexcept
        : Base(std::move(connection), std::move(connectionPool), std::move(poolAlive))
    {
        // All members initialized by CRTP base
    }

    // ── Relational-specific throwing methods ─────────────────────────────────

#ifdef __cpp_exceptions

    std::shared_ptr<RelationalDBPreparedStatement> RelationalPooledDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<RelationalDBResultSet> RelationalPooledDBConnection::executeQuery(const std::string &sql)
    {
        auto result = executeQuery(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t RelationalPooledDBConnection::executeUpdate(const std::string &sql)
    {
        auto result = executeUpdate(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void RelationalPooledDBConnection::setAutoCommit(bool autoCommit)
    {
        auto result = setAutoCommit(std::nothrow, autoCommit);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool RelationalPooledDBConnection::getAutoCommit()
    {
        auto result = getAutoCommit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void RelationalPooledDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void RelationalPooledDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool RelationalPooledDBConnection::beginTransaction()
    {
        auto result = beginTransaction(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool RelationalPooledDBConnection::transactionActive()
    {
        auto result = transactionActive(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void RelationalPooledDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel RelationalPooledDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    // ── Relational-specific nothrow methods ──────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> RelationalPooledDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("NGFY9OKTDK8C", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->prepareStatement(std::nothrow, sql);
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> RelationalPooledDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("F9E32F56CFF4", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->executeQuery(std::nothrow, sql);
    }

    cpp_dbc::expected<uint64_t, DBException> RelationalPooledDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("JURJ0DBHVVPS", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->executeUpdate(std::nothrow, sql);
    }

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::setAutoCommit(std::nothrow_t, bool autoCommit) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("XNDX9UDCTDQF", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setAutoCommit(std::nothrow, autoCommit);
    }

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("E3DEAB8A5E6D", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getAutoCommit(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("B7C8D9E0F1G2", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->beginTransaction(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("H3I4J5K6L7M8", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->transactionActive(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::commit(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("E32DBBC7316E", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->commit(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::rollback(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("C3UAXP7DGDPI", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->rollback(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("F7A2B9C3D1E5", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setTransactionIsolation(std::nothrow, level);
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException> RelationalPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("A4B5C6D7E8F9", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getTransactionIsolation(std::nothrow);
    }

    // ════════════════════════════════════════════════════════════════════════
    // Driver-specific subclasses
    // ════════════════════════════════════════════════════════════════════════

    // MySQL connection pool implementation
    namespace MySQL
    {
        MySQLConnectionPool::MySQLConnectionPool(DBConnectionPool::ConstructorTag,
                                                 const std::string &url,
                                                 const std::string &username,
                                                 const std::string &password)
            : RelationalDBConnectionPool(DBConnectionPool::ConstructorTag{}, url, username, password)
        {
            // MySQL-specific initialization if needed
        }

        MySQLConnectionPool::MySQLConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
            : RelationalDBConnectionPool(DBConnectionPool::ConstructorTag{}, config)
        {
            // MySQL-specific initialization if needed
        }

#ifdef __cpp_exceptions

        std::shared_ptr<MySQLConnectionPool> MySQLConnectionPool::create(const std::string &url,
                                                                         const std::string &username,
                                                                         const std::string &password)
        {
            auto result = create(std::nothrow, url, username, password);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

        std::shared_ptr<MySQLConnectionPool> MySQLConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto result = create(std::nothrow, config);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

#endif // __cpp_exceptions

        cpp_dbc::expected<std::shared_ptr<MySQLConnectionPool>, DBException> MySQLConnectionPool::create(std::nothrow_t,
                                                                                                         const std::string &url,
                                                                                                         const std::string &username,
                                                                                                         const std::string &password) noexcept
        {
            auto pool = std::make_shared<MySQLConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }

        cpp_dbc::expected<std::shared_ptr<MySQLConnectionPool>, DBException> MySQLConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
        {
            auto pool = std::make_shared<MySQLConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }
    }

    // PostgreSQL connection pool implementation
    namespace PostgreSQL
    {
        PostgreSQLConnectionPool::PostgreSQLConnectionPool(DBConnectionPool::ConstructorTag,
                                                           const std::string &url,
                                                           const std::string &username,
                                                           const std::string &password)
            : RelationalDBConnectionPool(DBConnectionPool::ConstructorTag{}, url, username, password)
        {
            // PostgreSQL-specific initialization if needed
        }

        PostgreSQLConnectionPool::PostgreSQLConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
            : RelationalDBConnectionPool(DBConnectionPool::ConstructorTag{}, config)
        {
            // PostgreSQL-specific initialization if needed
        }

#ifdef __cpp_exceptions

        std::shared_ptr<PostgreSQLConnectionPool> PostgreSQLConnectionPool::create(const std::string &url,
                                                                                   const std::string &username,
                                                                                   const std::string &password)
        {
            auto result = create(std::nothrow, url, username, password);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

        std::shared_ptr<PostgreSQLConnectionPool> PostgreSQLConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto result = create(std::nothrow, config);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

#endif // __cpp_exceptions

        cpp_dbc::expected<std::shared_ptr<PostgreSQLConnectionPool>, DBException> PostgreSQLConnectionPool::create(std::nothrow_t,
                                                                                                                   const std::string &url,
                                                                                                                   const std::string &username,
                                                                                                                   const std::string &password) noexcept
        {
            auto pool = std::make_shared<PostgreSQLConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }

        cpp_dbc::expected<std::shared_ptr<PostgreSQLConnectionPool>, DBException> PostgreSQLConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
        {
            auto pool = std::make_shared<PostgreSQLConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }
    }

    // SQLite connection pool implementation
    namespace SQLite
    {
        SQLiteConnectionPool::SQLiteConnectionPool(DBConnectionPool::ConstructorTag,
                                                   const std::string &url,
                                                   const std::string &username,
                                                   const std::string &password)
            : RelationalDBConnectionPool(DBConnectionPool::ConstructorTag{}, url, username, password)
        {
            // SQLite only supports SERIALIZABLE isolation level
            // Override the isolation level from the constructor
            SQLiteConnectionPool::setPoolTransactionIsolation(std::nothrow, TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
        }

        SQLiteConnectionPool::SQLiteConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
            : RelationalDBConnectionPool(DBConnectionPool::ConstructorTag{}, config)
        {
            // SQLite only supports SERIALIZABLE isolation level
            // Override the isolation level from the config
            SQLiteConnectionPool::setPoolTransactionIsolation(std::nothrow, TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
        }

#ifdef __cpp_exceptions

        std::shared_ptr<SQLiteConnectionPool> SQLiteConnectionPool::create(const std::string &url,
                                                                           const std::string &username,
                                                                           const std::string &password)
        {
            auto result = create(std::nothrow, url, username, password);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

        std::shared_ptr<SQLiteConnectionPool> SQLiteConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto result = create(std::nothrow, config);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

#endif // __cpp_exceptions

        cpp_dbc::expected<std::shared_ptr<SQLiteConnectionPool>, DBException> SQLiteConnectionPool::create(std::nothrow_t,
                                                                                                           const std::string &url,
                                                                                                           const std::string &username,
                                                                                                           const std::string &password) noexcept
        {
            auto pool = std::make_shared<SQLiteConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }

        cpp_dbc::expected<std::shared_ptr<SQLiteConnectionPool>, DBException> SQLiteConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
        {
            auto pool = std::make_shared<SQLiteConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }
    }

    // Firebird connection pool implementation
    namespace Firebird
    {
        FirebirdConnectionPool::FirebirdConnectionPool(DBConnectionPool::ConstructorTag,
                                                       const std::string &url,
                                                       const std::string &username,
                                                       const std::string &password)
            : RelationalDBConnectionPool(DBConnectionPool::ConstructorTag{}, url, username, password)
        {
            // Firebird-specific initialization if needed
        }

        FirebirdConnectionPool::FirebirdConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
            : RelationalDBConnectionPool(DBConnectionPool::ConstructorTag{}, config)
        {
            // Firebird-specific initialization if needed
        }

#ifdef __cpp_exceptions

        std::shared_ptr<FirebirdConnectionPool> FirebirdConnectionPool::create(const std::string &url,
                                                                               const std::string &username,
                                                                               const std::string &password)
        {
            auto result = create(std::nothrow, url, username, password);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

        std::shared_ptr<FirebirdConnectionPool> FirebirdConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto result = create(std::nothrow, config);
            if (!result.has_value())
            {
                throw result.error();
            }
            return result.value();
        }

#endif // __cpp_exceptions

        cpp_dbc::expected<std::shared_ptr<FirebirdConnectionPool>, DBException> FirebirdConnectionPool::create(std::nothrow_t,
                                                                                                               const std::string &url,
                                                                                                               const std::string &username,
                                                                                                               const std::string &password) noexcept
        {
            auto pool = std::make_shared<FirebirdConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }

        cpp_dbc::expected<std::shared_ptr<FirebirdConnectionPool>, DBException> FirebirdConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
        {
            auto pool = std::make_shared<FirebirdConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }
    }

} // namespace cpp_dbc
