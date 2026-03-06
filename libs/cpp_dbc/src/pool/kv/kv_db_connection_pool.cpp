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
 * getKVDBConnection, factory methods, and the full
 * KVPooledDBConnection + Redis subclass implementations.
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
                                           int initialSize,
                                           int maxSize,
                                           int minIdle,
                                           long maxWaitMillis,
                                           long validationTimeoutMillis,
                                           long idleTimeoutMillis,
                                           long maxLifetimeMillis,
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

    KVDBConnectionPool::~KVDBConnectionPool()
    {
        // DBConnectionPoolBase destructor handles close() and cleanup
    }

    // ── Private helpers ─────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException>
    KVDBConnectionPool::createDBConnection(std::nothrow_t) noexcept
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
                                                                                                   int initialSize,
                                                                                                   int maxSize,
                                                                                                   int minIdle,
                                                                                                   long maxWaitMillis,
                                                                                                   long validationTimeoutMillis,
                                                                                                   long idleTimeoutMillis,
                                                                                                   long maxLifetimeMillis,
                                                                                                   bool testOnBorrow,
                                                                                                   bool testOnReturn,
                                                                                                   TransactionIsolationLevel transactionIsolation) noexcept
    {
        try
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
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("5REJXCVLLMX9", "Failed to create connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("HGU9G2E3MGVE", "Failed to create connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<KVDBConnectionPool>, DBException> KVDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        try
        {
            auto pool = std::make_shared<KVDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }

            return pool;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("5REJXCVLLMX9", "Failed to create connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("HGU9G2E3MGVE", "Failed to create connection pool: unknown error", system_utils::captureCallStack()));
        }
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
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive)
    {
        // m_creationTime, m_lastUsedTimeNs, m_active, m_closed use in-class initializers
    }

    bool KVPooledDBConnection::isPoolValid(std::nothrow_t) const noexcept
    {
        return m_poolAlive && m_poolAlive->load(std::memory_order_acquire) && !m_pool.expired();
    }

    KVPooledDBConnection::~KVPooledDBConnection()
    {
        if (!m_closed.load(std::memory_order_acquire) && m_conn)
        {
            // CRITICAL: Never call returnToPool() or shared_from_this() from destructor.
            // When the destructor runs, refcount is already 0, so shared_from_this()
            // throws bad_weak_ptr. Instead, do direct cleanup:
            // 1. Decrement active counter if this connection was active
            // 2. Close the underlying physical connection
            if (m_active.load(std::memory_order_acquire))
            {
                if (auto pool = m_pool.lock())
                {
                    pool->decrementActiveCount(std::nothrow);
                }
                m_active.store(false, std::memory_order_release);
            }

            auto closeResult = m_conn->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("KVPooledDBConnection::~KVPooledDBConnection - close failed: %s", closeResult.error().what_s().data());
            }
        }
    }

    cpp_dbc::expected<void, DBException> KVPooledDBConnection::close(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the close
        bool expected = false;
        if (m_closed.compare_exchange_strong(expected, true))
        {
            try
            {
                // Return to pool instead of actually closing
                updateLastUsedTime(std::nothrow);

                // Check if pool is still alive using the shared atomic flag
                if (isPoolValid(std::nothrow))
                {
                    // Try to obtain a shared_ptr from the weak_ptr
                    if (auto poolShared = m_pool.lock())
                    {
                        // ============================================================================
                        // CRITICAL FIX: Race Condition in Connection Pool Return Flow
                        // ============================================================================
                        //
                        // BUG DESCRIPTION:
                        // ----------------
                        // A race condition existed where m_closed was reset to false AFTER
                        // returnConnection() completed. This created a window where another thread
                        // could obtain the connection from the idle queue while m_closed was still
                        // true, causing spurious "Connection is closed" errors.
                        //
                        // SEQUENCE OF EVENTS (BUG):
                        // -------------------------
                        // Thread A (returning connection):
                        //   1. close() sets m_closed = true (compare_exchange)
                        //   2. returnConnection() is called
                        //   3. Inside returnConnection():
                        //      - LOCK(m_mutexIdleConnections)
                        //      - m_idleConnections.push(conn)  <-- Connection available with m_closed=TRUE
                        //      - UNLOCK(m_mutexIdleConnections) <-- Race window opens here
                        //   4. m_closed.store(false)  <-- Too late if Thread B already got the connection
                        //
                        // Thread B (acquiring connection) - executes between steps 3 and 4:
                        //   1. getIdleDBConnection() acquires m_mutexIdleConnections
                        //   2. Pops connection from idle queue (m_closed is still TRUE)
                        //   3. Returns connection to caller
                        //   4. Caller calls ping() -> sees m_closed=true -> throws "Connection is closed"
                        //
                        // FIX:
                        // ----
                        // Reset m_closed to false BEFORE calling returnConnection(). This ensures
                        // that when the connection becomes available in the idle queue, m_closed
                        // is already false and any thread that obtains it will see the correct state.
                        //
                        // If returnConnection() fails or throws an exception, the catch blocks below
                        // will close the underlying connection, maintaining correct error handling.
                        // ============================================================================
                        m_closed.store(false, std::memory_order_release);
                        auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<DBConnectionPooled>(shared_from_this()));
                        if (!returnResult.has_value())
                        {
                            CP_DEBUG("KVPooledDBConnection::close - returnConnection failed: %s", returnResult.error().what_s().data());
                            return returnResult;
                        }
                    }
                }
                else if (m_conn)
                {
                    // If pool is invalid, actually close the connection
                    auto result = m_conn->close(std::nothrow);
                    if (!result.has_value())
                    {
                        CP_DEBUG("KVPooledDBConnection::close - Failed to close underlying connection: %s", result.error().what_s().data());
                        return result;
                    }
                }

                return {};
            }
            catch (const std::bad_weak_ptr &ex)
            {
                // shared_from_this failed, just close the connection
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    auto result = m_conn->close(std::nothrow);
                    if (!result.has_value())
                    {
                        return result;
                    }
                }
                return cpp_dbc::unexpected(DBException("UAT5CCJP43TT",
                                                       "Failed to obtain shared_from_this: " + std::string(ex.what()),
                                                       system_utils::captureCallStack()));
            }
            catch (const DBException &ex)
            {
                // DBException from underlying connection, just return it
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                // Any other exception, close the connection and return error
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("YTAQIZFLMMUA",
                                                       "Exception in close: " + std::string(ex.what()),
                                                       system_utils::captureCallStack()));
            }
            catch (...) // NOSONAR - Catch-all to ensure m_closed is always set correctly on any exception
            {
                CP_DEBUG("KVPooledDBConnection::close - Unknown exception caught");
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("3M08ISR1QG57",
                                                       "Unknown exception in close",
                                                       system_utils::captureCallStack()));
            }
        }

        return {};
    }

#ifdef __cpp_exceptions

    void KVPooledDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool KVPooledDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return true;
        }
        return m_conn->isClosed(std::nothrow);
    }

    std::chrono::time_point<std::chrono::steady_clock> KVPooledDBConnection::getCreationTime(std::nothrow_t) const noexcept
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> KVPooledDBConnection::getLastUsedTime(std::nothrow_t) const noexcept
    {
        return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{m_lastUsedTimeNs.load(std::memory_order_acquire)}};
    }

    cpp_dbc::expected<void, DBException> KVPooledDBConnection::setActive(std::nothrow_t, bool isActive) noexcept
    {
        m_active.store(isActive, std::memory_order_release);
        return {};
    }

    bool KVPooledDBConnection::isActive(std::nothrow_t) const noexcept
    {
        return m_active.load(std::memory_order_acquire);
    }

    void KVPooledDBConnection::markPoolClosed(std::nothrow_t, bool closed) noexcept
    {
        m_closed.store(closed, std::memory_order_release);
    }

    bool KVPooledDBConnection::isPoolClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_acquire);
    }

    void KVPooledDBConnection::updateLastUsedTime(std::nothrow_t) noexcept
    {
        m_lastUsedTimeNs.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
    }

    cpp_dbc::expected<void, DBException> KVPooledDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the return
        bool expected = false;
        if (!m_closed.compare_exchange_strong(expected, true))
        {
            return {};
        }

        try
        {
            // Return to pool instead of actually closing
            updateLastUsedTime(std::nothrow);

            // Check if pool is still alive using the shared atomic flag
            // Use qualified call to avoid virtual dispatch issues when called from destructor
            if (KVPooledDBConnection::isPoolValid(std::nothrow))
            {
                // Try to obtain a shared_ptr from the weak_ptr
                if (auto poolShared = m_pool.lock())
                {
                    // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                    // (see close(nothrow) method for full explanation of the bug)
                    m_closed.store(false, std::memory_order_release);
                    auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<DBConnectionPooled>(this->shared_from_this()));
                    if (!returnResult.has_value())
                    {
                        CP_DEBUG("KVPooledDBConnection::returnToPool - returnConnection failed: %s", returnResult.error().what_s().data());
                        return returnResult;
                    }
                }
            }
            return {};
        }
        catch (const std::exception &ex)
        {
            // Only shared_from_this() can throw here (std::bad_weak_ptr).
            // All other inner calls are nothrow. Close the connection and return error.
            CP_DEBUG("KVPooledDBConnection::returnToPool - shared_from_this failed: %s", ex.what());
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("6JNF0M82POI3",
                                                   "Exception in returnToPool: " + std::string(ex.what()),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions
        {
            CP_DEBUG("KVPooledDBConnection::returnToPool - Unknown exception caught");
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("COMQDQ0I0RYA",
                                                   "Unknown exception in returnToPool",
                                                   system_utils::captureCallStack()));
        }
    }

#ifdef __cpp_exceptions

    void KVPooledDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool KVPooledDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string KVPooledDBConnection::getURL() const
    {
        auto result = getURL(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void KVPooledDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
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

    cpp_dbc::expected<void, DBException>
    KVPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("XYRS4F9L0GJN", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->setTransactionIsolation(std::nothrow, level);
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    KVPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("YX9CAJLI0F0N", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getTransactionIsolation(std::nothrow);
    }

    cpp_dbc::expected<void, DBException>
    KVPooledDBConnection::prepareForPoolReturn(std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("T9L6PF5ZIEZ9",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForPoolReturn(std::nothrow, isolationLevel);
    }

    cpp_dbc::expected<void, DBException> KVPooledDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("QL405DKSNBIQ",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForBorrow(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return !m_active.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::getURL(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("WDBWCPX3AUDK", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> KVPooledDBConnection::reset(std::nothrow_t) noexcept
    {
        if (!m_conn)
        {
            return cpp_dbc::unexpected(DBException("V7KTKPSZ9P38",
                                                   "Underlying connection is null",
                                                   system_utils::captureCallStack()));
        }

        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("Z4A54D26J5G0",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        updateLastUsedTime(std::nothrow);
        return m_conn->reset(std::nothrow);
    }

    std::shared_ptr<DBConnection> KVPooledDBConnection::getUnderlyingConnection(std::nothrow_t) noexcept
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

#ifdef __cpp_exceptions
    // Forward all KVDBConnection methods to underlying connection

    bool KVPooledDBConnection::setString(const std::string &key, const std::string &value,
                                         std::optional<int64_t> expirySeconds)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setString(key, value, expirySeconds);
    }

    std::string KVPooledDBConnection::getString(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->getString(key);
    }

    bool KVPooledDBConnection::exists(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->exists(key);
    }

    bool KVPooledDBConnection::deleteKey(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->deleteKey(key);
    }

    int64_t KVPooledDBConnection::deleteKeys(const std::vector<std::string> &keys)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->deleteKeys(keys);
    }

    bool KVPooledDBConnection::expire(const std::string &key, int64_t seconds)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->expire(key, seconds);
    }

    int64_t KVPooledDBConnection::getTTL(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->getTTL(key);
    }

    int64_t KVPooledDBConnection::increment(const std::string &key, int64_t by)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->increment(key, by);
    }

    int64_t KVPooledDBConnection::decrement(const std::string &key, int64_t by)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->decrement(key, by);
    }

    int64_t KVPooledDBConnection::listPushLeft(const std::string &key, const std::string &value)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listPushLeft(key, value);
    }

    int64_t KVPooledDBConnection::listPushRight(const std::string &key, const std::string &value)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listPushRight(key, value);
    }

    std::string KVPooledDBConnection::listPopLeft(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listPopLeft(key);
    }

    std::string KVPooledDBConnection::listPopRight(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listPopRight(key);
    }

    std::vector<std::string> KVPooledDBConnection::listRange(const std::string &key, int64_t start, int64_t stop)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listRange(key, start, stop);
    }

    int64_t KVPooledDBConnection::listLength(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listLength(key);
    }

    bool KVPooledDBConnection::hashSet(const std::string &key, const std::string &field, const std::string &value)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashSet(key, field, value);
    }

    std::string KVPooledDBConnection::hashGet(const std::string &key, const std::string &field)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashGet(key, field);
    }

    bool KVPooledDBConnection::hashDelete(const std::string &key, const std::string &field)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashDelete(key, field);
    }

    bool KVPooledDBConnection::hashExists(const std::string &key, const std::string &field)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashExists(key, field);
    }

    std::map<std::string, std::string> KVPooledDBConnection::hashGetAll(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashGetAll(key);
    }

    int64_t KVPooledDBConnection::hashLength(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashLength(key);
    }

    bool KVPooledDBConnection::setAdd(const std::string &key, const std::string &member)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setAdd(key, member);
    }

    bool KVPooledDBConnection::setRemove(const std::string &key, const std::string &member)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setRemove(key, member);
    }

    bool KVPooledDBConnection::setIsMember(const std::string &key, const std::string &member)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setIsMember(key, member);
    }

    std::vector<std::string> KVPooledDBConnection::setMembers(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setMembers(key);
    }

    int64_t KVPooledDBConnection::setSize(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setSize(key);
    }

    bool KVPooledDBConnection::sortedSetAdd(const std::string &key, double score, const std::string &member)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->sortedSetAdd(key, score, member);
    }

    bool KVPooledDBConnection::sortedSetRemove(const std::string &key, const std::string &member)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->sortedSetRemove(key, member);
    }

    std::optional<double> KVPooledDBConnection::sortedSetScore(const std::string &key, const std::string &member)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->sortedSetScore(key, member);
    }

    std::vector<std::string> KVPooledDBConnection::sortedSetRange(const std::string &key, int64_t start, int64_t stop)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->sortedSetRange(key, start, stop);
    }

    int64_t KVPooledDBConnection::sortedSetSize(const std::string &key)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->sortedSetSize(key);
    }

    std::vector<std::string> KVPooledDBConnection::scanKeys(const std::string &pattern, int64_t count)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->scanKeys(pattern, count);
    }

    std::string KVPooledDBConnection::executeCommand(const std::string &command, const std::vector<std::string> &args)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->executeCommand(command, args);
    }

    bool KVPooledDBConnection::flushDB(bool async)
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->flushDB(async);
    }

    bool KVPooledDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::map<std::string, std::string> KVPooledDBConnection::getServerInfo()
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerInfo();
    }
#endif // __cpp_exceptions

    // ============================================================================
    // KVPooledDBConnection - NOTHROW IMPLEMENTATIONS (delegate to underlying connection)
    // ============================================================================

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setString(
        std::nothrow_t,
        const std::string &key,
        const std::string &value,
        std::optional<int64_t> expirySeconds) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setString(std::nothrow, key, value, expirySeconds);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::getString(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->getString(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::exists(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->exists(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::deleteKey(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->deleteKey(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::deleteKeys(
        std::nothrow_t, const std::vector<std::string> &keys) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->deleteKeys(std::nothrow, keys);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::expire(
        std::nothrow_t, const std::string &key, int64_t seconds) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->expire(std::nothrow, key, seconds);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::getTTL(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->getTTL(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::increment(
        std::nothrow_t, const std::string &key, int64_t by) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->increment(std::nothrow, key, by);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::decrement(
        std::nothrow_t, const std::string &key, int64_t by) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->decrement(std::nothrow, key, by);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listPushLeft(
        std::nothrow_t, const std::string &key, const std::string &value) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listPushLeft(std::nothrow, key, value);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listPushRight(
        std::nothrow_t, const std::string &key, const std::string &value) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listPushRight(std::nothrow, key, value);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::listPopLeft(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listPopLeft(std::nothrow, key);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::listPopRight(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listPopRight(std::nothrow, key);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::listRange(
        std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listRange(std::nothrow, key, start, stop);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->listLength(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashSet(
        std::nothrow_t,
        const std::string &key,
        const std::string &field,
        const std::string &value) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashSet(std::nothrow, key, field, value);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::hashGet(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashGet(std::nothrow, key, field);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashDelete(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashDelete(std::nothrow, key, field);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashExists(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashExists(std::nothrow, key, field);
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> KVPooledDBConnection::hashGetAll(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashGetAll(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::hashLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->hashLength(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setAdd(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setAdd(std::nothrow, key, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setRemove(std::nothrow, key, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setIsMember(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setIsMember(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::setMembers(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setMembers(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::setSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->setSize(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::sortedSetAdd(
        std::nothrow_t, const std::string &key, double score, const std::string &member) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->sortedSetAdd(std::nothrow, key, score, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::sortedSetRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->sortedSetRemove(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::optional<double>, DBException> KVPooledDBConnection::sortedSetScore(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->sortedSetScore(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::sortedSetRange(
        std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->sortedSetRange(std::nothrow, key, start, stop);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::sortedSetSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->sortedSetSize(std::nothrow, key);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::scanKeys(
        std::nothrow_t, const std::string &pattern, int64_t count) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->scanKeys(std::nothrow, pattern, count);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::executeCommand(
        std::nothrow_t,
        const std::string &command,
        const std::vector<std::string> &args) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->executeCommand(std::nothrow, command, args);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::flushDB(
        std::nothrow_t, bool async) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->flushDB(std::nothrow, async);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::ping(std::nothrow_t) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->ping(std::nothrow);
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> KVPooledDBConnection::getServerInfo(
        std::nothrow_t) noexcept
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerInfo(std::nothrow);
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
        try
        {
            auto pool = std::make_shared<RedisDBConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("Q52SK76UE7I1", "Failed to create Redis connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("6RMCJA19VK7Q", "Failed to create Redis connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RedisDBConnectionPool>, DBException> RedisDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        try
        {
            auto pool = std::make_shared<RedisDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("Q52SK76UE7I1", "Failed to create Redis connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("6RMCJA19VK7Q", "Failed to create Redis connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::Redis
