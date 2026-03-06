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
 * @file kv_db_connection_pool.cpp
 * @brief Thin KV pool — delegates all pool infrastructure to DBConnectionPoolBase.
 *
 * Only contains: constructors, createDBConnection, createPooledDBConnection,
 * getKVDBConnection, factory methods, and the KV-specific
 * KVPooledDBConnection methods (setString, getString, etc.).
 *
 * Common pooled-connection logic (close, returnToPool, destructor, pool metadata)
 * is inherited from PooledDBConnectionBase via CRTP.
 */

#include "cpp_dbc/pool/kv/kv_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "../connection_pool_internal.hpp"

namespace cpp_dbc
{
    // ── KVDBConnectionPool ──────────────────────────────────────────────────

    // ── Constructors (delegate to base) ─────────────────────────────────────

    KVDBConnectionPool::KVDBConnectionPool(DBConnectionPool::ConstructorTag,
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

    KVDBConnectionPool::KVDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : DBConnectionPoolBase(DBConnectionPool::ConstructorTag{}, config)
    {
        // All initialization handled by DBConnectionPoolBase
    }

    // ── Destructor ──────────────────────────────────────────────────────────

    // DBConnectionPoolBase destructor handles close() and cleanup
    KVDBConnectionPool::~KVDBConnectionPool() = default;

    // ── Private helpers ─────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException>
    KVDBConnectionPool::createDBConnection(std::nothrow_t) const noexcept
    {
        auto dbConnResult = DriverManager::getDBConnection(std::nothrow, getUrl(), getUsername(), getPassword(), getOptions());
        if (!dbConnResult.has_value())
        {
            return cpp_dbc::unexpected(dbConnResult.error());
        }

        auto kvConn = std::dynamic_pointer_cast<KVDBConnection>(dbConnResult.value());
        if (!kvConn)
        {
            return cpp_dbc::unexpected(DBException("96CAD6FDCDD9", "Connection pool only supports key-value database connections", system_utils::captureCallStack()));
        }
        return kvConn;
    }

    cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
    KVDBConnectionPool::createPooledDBConnection(std::nothrow_t) noexcept
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
        // because KVPooledDBConnection stores weak_ptr<KVDBConnectionPool>.
        std::weak_ptr<KVDBConnectionPool> weakPool;
        try
        {
            weakPool = std::static_pointer_cast<KVDBConnectionPool>(shared_from_this());
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("KVDBConnectionPool::createPooledDBConnection - Pool not managed by shared_ptr: %s", ex.what());
        }

        return std::static_pointer_cast<DBConnectionPooled>(
            std::make_shared<KVPooledDBConnection>(conn, weakPool, getPoolAliveFlag()));
    }

    // ── Static factories ────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<KVDBConnectionPool>, DBException> KVDBConnectionPool::create(std::nothrow_t,
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
        auto pool = std::make_shared<KVDBConnectionPool>(
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

    cpp_dbc::expected<std::shared_ptr<KVDBConnectionPool>, DBException> KVDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        auto pool = std::make_shared<KVDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }

        return pool;
    }

    // ── Family-specific getter ──────────────────────────────────────────────

#ifdef __cpp_exceptions

    std::shared_ptr<KVDBConnection> KVDBConnectionPool::getKVDBConnection()
    {
        auto result = getKVDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException>
    KVDBConnectionPool::getKVDBConnection(std::nothrow_t) noexcept
    {
        auto result = acquireConnection(std::nothrow);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        // acquireConnection() returns shared_ptr<DBConnectionPooled>. The underlying object
        // is a KVPooledDBConnection which inherits from both DBConnectionPooled and
        // KVDBConnection. dynamic_pointer_cast navigates the diamond correctly.
        return std::dynamic_pointer_cast<KVDBConnection>(result.value());
    }

    // ════════════════════════════════════════════════════════════════════════
    // KVPooledDBConnection implementation
    // ════════════════════════════════════════════════════════════════════════

    KVPooledDBConnection::KVPooledDBConnection(
        std::shared_ptr<KVDBConnection> connection,
        std::weak_ptr<KVDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive) noexcept
        : Base(std::move(connection), std::move(connectionPool), std::move(poolAlive))
    {
        // All members initialized by CRTP base
    }

    // ── KV-specific throwing methods ─────────────────────────────────────────

#ifdef __cpp_exceptions

    bool KVPooledDBConnection::setString(const std::string &key, const std::string &value,
                                         std::optional<int64_t> expirySeconds)
    {
        auto result = setString(std::nothrow, key, value, expirySeconds);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string KVPooledDBConnection::getString(const std::string &key)
    {
        auto result = getString(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::exists(const std::string &key)
    {
        auto result = exists(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::deleteKey(const std::string &key)
    {
        auto result = deleteKey(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t KVPooledDBConnection::deleteKeys(const std::vector<std::string> &keys)
    {
        auto result = deleteKeys(std::nothrow, keys);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::expire(const std::string &key, int64_t seconds)
    {
        auto result = expire(std::nothrow, key, seconds);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t KVPooledDBConnection::getTTL(const std::string &key)
    {
        auto result = getTTL(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t KVPooledDBConnection::increment(const std::string &key, int64_t by)
    {
        auto result = increment(std::nothrow, key, by);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t KVPooledDBConnection::decrement(const std::string &key, int64_t by)
    {
        auto result = decrement(std::nothrow, key, by);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t KVPooledDBConnection::listPushLeft(const std::string &key, const std::string &value)
    {
        auto result = listPushLeft(std::nothrow, key, value);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t KVPooledDBConnection::listPushRight(const std::string &key, const std::string &value)
    {
        auto result = listPushRight(std::nothrow, key, value);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string KVPooledDBConnection::listPopLeft(const std::string &key)
    {
        auto result = listPopLeft(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string KVPooledDBConnection::listPopRight(const std::string &key)
    {
        auto result = listPopRight(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> KVPooledDBConnection::listRange(const std::string &key, int64_t start, int64_t stop)
    {
        auto result = listRange(std::nothrow, key, start, stop);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t KVPooledDBConnection::listLength(const std::string &key)
    {
        auto result = listLength(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::hashSet(const std::string &key, const std::string &field, const std::string &value)
    {
        auto result = hashSet(std::nothrow, key, field, value);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string KVPooledDBConnection::hashGet(const std::string &key, const std::string &field)
    {
        auto result = hashGet(std::nothrow, key, field);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::hashDelete(const std::string &key, const std::string &field)
    {
        auto result = hashDelete(std::nothrow, key, field);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::hashExists(const std::string &key, const std::string &field)
    {
        auto result = hashExists(std::nothrow, key, field);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::map<std::string, std::string> KVPooledDBConnection::hashGetAll(const std::string &key)
    {
        auto result = hashGetAll(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t KVPooledDBConnection::hashLength(const std::string &key)
    {
        auto result = hashLength(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::setAdd(const std::string &key, const std::string &member)
    {
        auto result = setAdd(std::nothrow, key, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::setRemove(const std::string &key, const std::string &member)
    {
        auto result = setRemove(std::nothrow, key, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::setIsMember(const std::string &key, const std::string &member)
    {
        auto result = setIsMember(std::nothrow, key, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> KVPooledDBConnection::setMembers(const std::string &key)
    {
        auto result = setMembers(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t KVPooledDBConnection::setSize(const std::string &key)
    {
        auto result = setSize(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::sortedSetAdd(const std::string &key, double score, const std::string &member)
    {
        auto result = sortedSetAdd(std::nothrow, key, score, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::sortedSetRemove(const std::string &key, const std::string &member)
    {
        auto result = sortedSetRemove(std::nothrow, key, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::optional<double> KVPooledDBConnection::sortedSetScore(const std::string &key, const std::string &member)
    {
        auto result = sortedSetScore(std::nothrow, key, member);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> KVPooledDBConnection::sortedSetRange(const std::string &key, int64_t start, int64_t stop)
    {
        auto result = sortedSetRange(std::nothrow, key, start, stop);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    int64_t KVPooledDBConnection::sortedSetSize(const std::string &key)
    {
        auto result = sortedSetSize(std::nothrow, key);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> KVPooledDBConnection::scanKeys(const std::string &pattern, int64_t count)
    {
        auto result = scanKeys(std::nothrow, pattern, count);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string KVPooledDBConnection::executeCommand(const std::string &command, const std::vector<std::string> &args)
    {
        auto result = executeCommand(std::nothrow, command, args);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool KVPooledDBConnection::flushDB(bool async)
    {
        auto result = flushDB(std::nothrow, async);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::map<std::string, std::string> KVPooledDBConnection::getServerInfo()
    {
        auto result = getServerInfo(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void KVPooledDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel KVPooledDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    // ── KV-specific nothrow methods ──────────────────────────────────────────

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setString(
        std::nothrow_t,
        const std::string &key,
        const std::string &value,
        std::optional<int64_t> expirySeconds) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("4VPQD2H5ZYLA", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setString(std::nothrow, key, value, expirySeconds);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::getString(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("8B3GILMXWV04", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getString(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::exists(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("9JLXR5KHJL1V", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->exists(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::deleteKey(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("WRL9V4IUCC5N", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->deleteKey(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::deleteKeys(
        std::nothrow_t, const std::vector<std::string> &keys) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("YYSBQ8253EEW", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->deleteKeys(std::nothrow, keys);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::expire(
        std::nothrow_t, const std::string &key, int64_t seconds) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("WUO2USW00Z4X", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->expire(std::nothrow, key, seconds);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::getTTL(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("U96MMF4JYOY2", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getTTL(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::increment(
        std::nothrow_t, const std::string &key, int64_t by) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("YVGAFS6KDQ0Y", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->increment(std::nothrow, key, by);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::decrement(
        std::nothrow_t, const std::string &key, int64_t by) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("QX8JAUTJ0U7N", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->decrement(std::nothrow, key, by);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listPushLeft(
        std::nothrow_t, const std::string &key, const std::string &value) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("1FXK5A8EP0XR", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->listPushLeft(std::nothrow, key, value);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listPushRight(
        std::nothrow_t, const std::string &key, const std::string &value) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("H9TBOJPVMWOC", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->listPushRight(std::nothrow, key, value);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::listPopLeft(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("4WY3QQRYDPBY", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->listPopLeft(std::nothrow, key);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::listPopRight(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("F41VSGJSI98I", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->listPopRight(std::nothrow, key);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::listRange(
        std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("580M5J29HAZ4", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->listRange(std::nothrow, key, start, stop);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("G4L088U2OLBK", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->listLength(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashSet(
        std::nothrow_t,
        const std::string &key,
        const std::string &field,
        const std::string &value) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("E6APDEZ3U5UH", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->hashSet(std::nothrow, key, field, value);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::hashGet(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("U4Q7V2NXS4XS", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->hashGet(std::nothrow, key, field);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashDelete(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("98FB529Z0HU1", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->hashDelete(std::nothrow, key, field);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashExists(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("GW2I9BUNKUV3", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->hashExists(std::nothrow, key, field);
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> KVPooledDBConnection::hashGetAll(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("L7G2AIKNB7KR", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->hashGetAll(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::hashLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("O0SQQ9SBCEBR", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->hashLength(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setAdd(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("6WUCWGQ9DOU0", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setAdd(std::nothrow, key, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("2DEHI6SV7BVW", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setRemove(std::nothrow, key, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setIsMember(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("4GDRRINM61ZR", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setIsMember(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::setMembers(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("ZU4C0BH7E6NR", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setMembers(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::setSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("0071SDPBSFEL", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setSize(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::sortedSetAdd(
        std::nothrow_t, const std::string &key, double score, const std::string &member) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("QHQ4YRFSNEE3", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->sortedSetAdd(std::nothrow, key, score, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::sortedSetRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("HW98B32OUCP8", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->sortedSetRemove(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::optional<double>, DBException> KVPooledDBConnection::sortedSetScore(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("74M3RKYNIDSV", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->sortedSetScore(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::sortedSetRange(
        std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("K3H8GVNKF8R3", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->sortedSetRange(std::nothrow, key, start, stop);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::sortedSetSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("5WW77792M8EW", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->sortedSetSize(std::nothrow, key);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::scanKeys(
        std::nothrow_t, const std::string &pattern, int64_t count) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("ZIBW4FMSIRQU", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->scanKeys(std::nothrow, pattern, count);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::executeCommand(
        std::nothrow_t,
        const std::string &command,
        const std::vector<std::string> &args) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("V5PYNMSCNNZB", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->executeCommand(std::nothrow, command, args);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::flushDB(
        std::nothrow_t, bool async) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("C9F1QO8X1RC1", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->flushDB(std::nothrow, async);
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> KVPooledDBConnection::getServerInfo(
        std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("Q4UUV0ULFRE4", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getServerInfo(std::nothrow);
    }

    cpp_dbc::expected<void, DBException>
    KVPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("XYRS4F9L0GJN", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setTransactionIsolation(std::nothrow, level);
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    KVPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("YX9CAJLI0F0N", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getTransactionIsolation(std::nothrow);
    }

} // namespace cpp_dbc

// ── RedisDBConnectionPool ───────────────────────────────────────────────

namespace cpp_dbc::Redis
{
    RedisDBConnectionPool::RedisDBConnectionPool(DBConnectionPool::ConstructorTag,
                                                 const std::string &url,
                                                 const std::string &username,
                                                 const std::string &password) noexcept
        : KVDBConnectionPool(DBConnectionPool::ConstructorTag{}, url, username, password, {}, 5, 20, 3, 5000, 5000, 300000, 1800000, true, false)
    {
    }

    RedisDBConnectionPool::RedisDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : KVDBConnectionPool(DBConnectionPool::ConstructorTag{}, config)
    {
        // Redis-specific initialization if needed
    }

#ifdef __cpp_exceptions

    std::shared_ptr<RedisDBConnectionPool> RedisDBConnectionPool::create(const std::string &url,
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

    std::shared_ptr<RedisDBConnectionPool> RedisDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto result = create(std::nothrow, config);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<RedisDBConnectionPool>, DBException> RedisDBConnectionPool::create(std::nothrow_t,
                                                                                                         const std::string &url,
                                                                                                         const std::string &username,
                                                                                                         const std::string &password) noexcept
    {
        auto pool = std::make_shared<RedisDBConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return pool;
    }

    cpp_dbc::expected<std::shared_ptr<RedisDBConnectionPool>, DBException> RedisDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        auto pool = std::make_shared<RedisDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return pool;
    }

} // namespace cpp_dbc::Redis
