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
 * @brief Implementation of connection pool for columnar databases
 */

#include "cpp_dbc/core/columnar/columnar_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "../connection_pool_internal.hpp"
#include <algorithm>

namespace cpp_dbc
{
    // ColumnarDBConnectionPool implementation

    ColumnarDBConnectionPool::ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag,
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
        : m_url(url),
          m_username(username),
          m_password(password),
          m_options(options),
          m_initialSize(initialSize),
          m_maxSize(maxSize),
          m_minIdle(minIdle),
          m_maxWaitMillis(maxWaitMillis),
          m_validationTimeoutMillis(validationTimeoutMillis),
          m_idleTimeoutMillis(idleTimeoutMillis),
          m_maxLifetimeMillis(maxLifetimeMillis),
          m_testOnBorrow(testOnBorrow),
          m_testOnReturn(testOnReturn),
          m_transactionIsolation(transactionIsolation)
    {
        // Pool initialization will be done in the factory method
    }

    ColumnarDBConnectionPool::ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : m_url(config.getUrl()),
          m_username(config.getUsername()),
          m_password(config.getPassword()),
          m_options(config.getOptions()),
          m_initialSize(config.getInitialSize()),
          m_maxSize(config.getMaxSize()),
          m_minIdle(config.getMinIdle()),
          m_maxWaitMillis(config.getConnectionTimeout()),
          m_validationTimeoutMillis(config.getValidationInterval()),
          m_idleTimeoutMillis(config.getIdleTimeout()),
          m_maxLifetimeMillis(config.getMaxLifetimeMillis()),
          m_testOnBorrow(config.getTestOnBorrow()),
          m_testOnReturn(config.getTestOnReturn()),
          m_transactionIsolation(config.getTransactionIsolation())
    {
        // Pool initialization will be done in the factory method
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException>
    ColumnarDBConnectionPool::createDBConnection(std::nothrow_t) noexcept
    {
        auto dbConnResult = DriverManager::getDBConnection(std::nothrow, m_url, m_username, m_password, m_options);
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

    cpp_dbc::expected<std::shared_ptr<ColumnarPooledDBConnection>, DBException>
    ColumnarDBConnectionPool::createPooledDBConnection(std::nothrow_t) noexcept
    {
        auto connResult = createDBConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        auto conn = connResult.value();

        // Set transaction isolation level on the new connection
        auto isoResult = conn->setTransactionIsolation(std::nothrow, m_transactionIsolation);
        if (!isoResult.has_value())
        {
            return cpp_dbc::unexpected(isoResult.error());
        }

        // Try to create a weak_ptr from this pool using shared_from_this.
        // shared_from_this() throws std::bad_weak_ptr if the pool is not managed by a shared_ptr,
        // so it must be wrapped in try/catch — no nothrow alternative exists.
        std::weak_ptr<ColumnarDBConnectionPool> weakPool;
        try
        {
            weakPool = shared_from_this();
        }
        catch (const std::bad_weak_ptr &ex)
        {
            // Pool is not managed by shared_ptr, weakPool remains empty (stack-allocated pool)
            CP_DEBUG("ColumnarDBConnectionPool::createPooledDBConnection - Pool not managed by shared_ptr: %s", ex.what());
        }

        return std::make_shared<ColumnarPooledDBConnection>(ColumnarPooledDBConnection::ConstructorTag{}, conn, weakPool, m_poolAlive);
    }

    cpp_dbc::expected<bool, DBException>
    ColumnarDBConnectionPool::validateConnection(std::nothrow_t, std::shared_ptr<DBConnection> conn) const noexcept
    {
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("13C79W9XO68B", "validateConnection called with null connection", system_utils::captureCallStack()));
        }

        auto result = conn->ping(std::nothrow);
        if (!result.has_value())
        {
            CP_DEBUG("ColumnarDBConnectionPool::validateConnection - ping failed: %s", result.error().what_s().data());
            return cpp_dbc::unexpected(result.error());
        }
        return result.value();
    }

    void ColumnarDBConnectionPool::notifyFirstWaiter(std::nothrow_t) noexcept
    {
        if (!m_waitQueue.empty())
        {
            m_waitQueue.front()->cv.notify_one();
        }
    }

    cpp_dbc::expected<void, DBException>
    ColumnarDBConnectionPool::returnConnection(std::nothrow_t, std::shared_ptr<ColumnarPooledDBConnection> conn) noexcept
    {
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("J71WMM9C9X1P", "returnConnection called with null connection", system_utils::captureCallStack()));
        }

        if (!m_running.load(std::memory_order_acquire))
        {
            conn->setActive(std::nothrow, false);
            m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
            auto closeResult = conn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - close() during shutdown failed: %s", closeResult.error().what_s().data());
            }
            return {};
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive(std::nothrow))
        {
            CP_DEBUG("ColumnarDBConnectionPool::returnConnection - Connection is not active, SKIPPED");
            return cpp_dbc::unexpected(DBException("X9MW435J41RP", "returnConnection called on an already inactive connection (double-return bug)", system_utils::captureCallStack()));
        }

        // Additional safety check: verify connection is in allConnections (detect orphan)
        {
            std::scoped_lock lockPool(m_mutexPool);
            auto it = std::ranges::find(m_allConnections, conn);
            if (it == m_allConnections.end())
            {
                conn->setActive(std::nothrow, false);
                m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - ORPHAN: connection not in allConnections, active=%d", m_activeConnections.load(std::memory_order_acquire));
                notifyFirstWaiter(std::nothrow);
                return cpp_dbc::unexpected(DBException("21H7Z45XJ7AK", "returnConnection called with a connection not belonging to this pool (orphan)", system_utils::captureCallStack()));
            }
        }

        bool valid = true;

        // Phase 1: I/O outside lock (validation and pool-return prep)
        if (m_testOnReturn)
        {
            auto valResult = validateConnection(std::nothrow, conn->getUnderlyingConnection(std::nothrow));
            valid = valResult.has_value() && valResult.value();
        }

        if (valid && !conn->m_closed.load(std::memory_order_acquire))
        {
            auto prepReturnPoolResult = conn->getUnderlyingConnection(std::nothrow)->prepareForPoolReturn(std::nothrow, m_transactionIsolation);
            if (!prepReturnPoolResult.has_value())
            {
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - prepareForPoolReturn failed: %s", prepReturnPoolResult.error().what_s().data());
                valid = false;
            }
        }

        // Phase 2: State changes under m_mutexPool
        if (valid)
        {
            conn->updateLastUsedTime(std::nothrow);
            std::scoped_lock lockPool(m_mutexPool);
            conn->setActive(std::nothrow, false);
            m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);

            // Direct handoff: if threads are waiting, give the connection directly
            // to the first waiter instead of pushing to idle.
            if (!m_waitQueue.empty())
            {
                auto *request = m_waitQueue.front();
                m_waitQueue.pop_front();
                request->conn = conn;
                request->fulfilled = true;
                // Restore active state before handoff: setActive(false) + m_activeConnections--
                // were called unconditionally above, so they must be undone here when a waiter
                // takes the connection directly.
                conn->setActive(std::nothrow, true);
                m_activeConnections.fetch_add(1, std::memory_order_acq_rel);
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - HANDOFF: direct to waiter, active=%d, waiters=%zu, total=%zu",
                         m_activeConnections.load(std::memory_order_acquire), m_waitQueue.size(), m_allConnections.size());
                request->cv.notify_one(); // Per-waiter: only the fulfilled waiter is woken
            }
            else
            {
                m_idleConnections.push(conn);
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - VALID: pushed to idle, active=%d, idle=%zu, total=%zu",
                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                // No notification needed: waitQueue is guaranteed empty here (checked above).
                // The next borrower will find this idle connection directly in Step 1.
            }
        }
        else
        {
            // Invalid connection — remove from pool, create replacement.
            // All notifications INSIDE lock to avoid Helgrind false positives.

            // Step 1: Remove invalid from pool (under lock) + notify freed slot
            {
                std::scoped_lock lockPool(m_mutexPool);

                // CRITICAL: Mark inactive + closed BEFORE decrementing counter.
                conn->setActive(std::nothrow, false);
                conn->m_closed.store(true, std::memory_order_release);
                m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - INVALID: active=%d", m_activeConnections.load(std::memory_order_acquire));

                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    m_allConnections.erase(it);
                }
                std::queue<std::shared_ptr<ColumnarPooledDBConnection>> tempQueue;
                while (!m_idleConnections.empty())
                {
                    auto c = m_idleConnections.front();
                    m_idleConnections.pop();
                    if (c != conn)
                    {
                        tempQueue.push(c);
                    }
                }
                m_idleConnections = tempQueue;

                // Notify first waiter: freed slot allows re-check of deadline or idle
                notifyFirstWaiter(std::nothrow);
            }

            // Step 2: Close invalid connection OUTSIDE locks (I/O)
            {
                auto closeResult = conn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                if (!closeResult.has_value())
                {
                    CP_DEBUG("ColumnarDBConnectionPool::returnConnection - close failed on invalid connection: %s", closeResult.error().what_s().data());
                }
            }

            // Signal maintenance thread to create replacement asynchronously.
            // The returning thread is NOT blocked by connection creation I/O
            // (which takes 1.3-1.5s+ under Valgrind). Borrowers self-heal via
            // Step 2 in getColumnarDBConnection() if the pool is temporarily short.
            if (m_running.load(std::memory_order_acquire))
            {
                {
                    std::scoped_lock lockPool(m_mutexPool);
                    m_replenishNeeded++;
                }
                m_maintenanceCondition.notify_one();
            }
        }

        return {};
    }

    void ColumnarDBConnectionPool::maintenanceTask(std::nothrow_t) noexcept
    {
        do
        {
            // Wait 30 seconds, or wake early on close() or replenish request
            {
                std::unique_lock lock(m_mutexPool);
                m_maintenanceCondition.wait_for(lock, std::chrono::seconds(30), [this]
                                                { return !m_running.load(std::memory_order_acquire) || m_replenishNeeded > 0; });
            }

            if (!m_running.load(std::memory_order_acquire))
            {
                break;
            }

            auto now = std::chrono::steady_clock::now();

            // Expire old connections under lock (no new connection creation here)
            {
                std::scoped_lock lock(m_mutexPool);

                // Check all connections for expired ones
                for (auto it = m_allConnections.begin(); it != m_allConnections.end();)
                {
                    auto pooledConn = *it;

                    // Skip active connections
                    if (pooledConn->isActive(std::nothrow))
                    {
                        ++it;
                        continue;
                    }

                    auto idleTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - pooledConn->getLastUsedTime(std::nothrow))
                                        .count();

                    auto lifeTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - pooledConn->getCreationTime(std::nothrow))
                                        .count();

                    bool expired = (idleTime > m_idleTimeoutMillis) ||
                                   (lifeTime > m_maxLifetimeMillis);

                    if (expired && m_allConnections.size() > m_minIdle)
                    {
                        // Remove from idle queue if present
                        std::queue<std::shared_ptr<ColumnarPooledDBConnection>> tempQueue;
                        while (!m_idleConnections.empty())
                        {
                            auto conn = m_idleConnections.front();
                            m_idleConnections.pop();

                            if (conn != pooledConn)
                            {
                                tempQueue.push(conn);
                            }
                        }
                        m_idleConnections = tempQueue;

                        it = m_allConnections.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            // Replenish replacement connections requested by returnConnection() (async).
            // Each iteration: lock → check → unlock → create (I/O) → lock → register.
            // This prevents m_mutexPool from being held during the slow createPooledDBConnection()
            // which would block borrowers. The m_pendingCreations counter prevents overallocation
            // by connection-creation I/O (1.3s+ under Valgrind).
            while (m_running.load(std::memory_order_acquire)) // NOSONAR(cpp:S924) — two breaks needed: one for "nothing to replenish", one for creation failure
            {
                {
                    std::scoped_lock lock(m_mutexPool);
                    if (m_replenishNeeded == 0 || m_allConnections.size() + m_pendingCreations >= m_maxSize)
                    {
                        m_replenishNeeded = 0; // Clear if pool already at max
                        break;
                    }
                    m_pendingCreations++;
                    m_replenishNeeded--;
                }

                auto pooledResult = createPooledDBConnection(std::nothrow);

                {
                    std::scoped_lock lock(m_mutexPool);
                    m_pendingCreations--;

                    if (!pooledResult.has_value())
                    {
                        CP_DEBUG("ColumnarDBConnectionPool::maintenanceTask - Failed to create replacement: %s",
                                 pooledResult.error().what_s().data());
                        break; // Stop retrying — DB might be down
                    }

                    m_allConnections.push_back(pooledResult.value());

                    if (!m_waitQueue.empty())
                    {
                        // Direct handoff to first waiter
                        auto *req = m_waitQueue.front();
                        m_waitQueue.pop_front();
                        pooledResult.value()->setActive(std::nothrow, true);
                        m_activeConnections.fetch_add(1, std::memory_order_acq_rel);
                        req->conn = pooledResult.value();
                        req->fulfilled = true;
                        CP_DEBUG("ColumnarDBConnectionPool::maintenanceTask - REPLACEMENT HANDOFF: active=%d, waiters=%zu, total=%zu",
                                 m_activeConnections.load(std::memory_order_acquire), m_waitQueue.size(), m_allConnections.size());
                        req->cv.notify_one();
                    }
                    else
                    {
                        m_idleConnections.push(pooledResult.value());
                        CP_DEBUG("ColumnarDBConnectionPool::maintenanceTask - REPLACEMENT: pushed to idle, active=%d, idle=%zu, total=%zu",
                                 m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                    }
                }
            }

            // Replenish minIdle connections OUTSIDE lock
            // (createPooledDBConnection may acquire driver-internal locks)
            while (m_running.load(std::memory_order_acquire)) // NOSONAR(cpp:S924) — two breaks needed: one for "minIdle reached", one for creation failure
            {
                {
                    std::scoped_lock lock(m_mutexPool);
                    if (m_allConnections.size() + m_pendingCreations >= m_minIdle)
                    {
                        break;
                    }
                    m_pendingCreations++;
                }

                auto pooledResult = createPooledDBConnection(std::nothrow);

                {
                    std::scoped_lock lock(m_mutexPool);
                    m_pendingCreations--;

                    if (!pooledResult.has_value())
                    {
                        CP_DEBUG("ColumnarDBConnectionPool::maintenanceTask - Failed to create minIdle connection: %s",
                                 pooledResult.error().what_s().data());
                        break;
                    }

                    m_allConnections.push_back(pooledResult.value());

                    if (!m_waitQueue.empty())
                    {
                        // Direct handoff to first waiter
                        auto *req = m_waitQueue.front();
                        m_waitQueue.pop_front();
                        pooledResult.value()->setActive(std::nothrow, true);
                        m_activeConnections.fetch_add(1, std::memory_order_acq_rel);
                        req->conn = pooledResult.value();
                        req->fulfilled = true;
                        req->cv.notify_one();
                    }
                    else
                    {
                        m_idleConnections.push(pooledResult.value());
                    }
                }
            }
        } while (m_running.load(std::memory_order_acquire));
    }

    // ── Protected helpers ─────────────────────────────────────────────────────

    cpp_dbc::expected<void, DBException> ColumnarDBConnectionPool::initializePool(std::nothrow_t) noexcept
    {
        try
        {
            // Reserve space for connections — can throw std::bad_alloc (no nothrow overload)
            m_allConnections.reserve(m_maxSize);

            // Create initial connections
            for (int i = 0; i < m_initialSize; i++)
            {
                auto pooledResult = createPooledDBConnection(std::nothrow);
                if (!pooledResult.has_value())
                {
                    close(std::nothrow);
                    return cpp_dbc::unexpected(pooledResult.error());
                }

                // Add to idle connections and all connections lists under proper locks
                // Use scoped_lock for consistent lock ordering to prevent deadlock
                {
                    std::scoped_lock lock(m_mutexPool);
                    m_idleConnections.push(pooledResult.value());
                    m_allConnections.push_back(pooledResult.value());
                }
            }

            // Start maintenance thread — can throw std::system_error
            m_maintenanceThread = std::jthread([this]
                                               { maintenanceTask(std::nothrow); });
        }
        catch (const std::exception &ex)
        {
            close(std::nothrow);
            return cpp_dbc::unexpected(DBException("2X2QH6E74IUL", "Failed to initialize connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            close(std::nothrow);
            return cpp_dbc::unexpected(DBException("Z97SOVZ1RME9", "Failed to initialize connection pool: unknown error", system_utils::captureCallStack()));
        }
        return {};
    }

    // ── Destructor ────────────────────────────────────────────────────────────

    ColumnarDBConnectionPool::~ColumnarDBConnectionPool()
    {
        CP_DEBUG("ColumnarDBConnectionPool::~ColumnarDBConnectionPool - Starting destructor at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        auto result = ColumnarDBConnectionPool::close(std::nothrow);
        if (!result.has_value())
        {
            CP_DEBUG("ColumnarDBConnectionPool::~ColumnarDBConnectionPool - close failed: %s",
                     result.error().what_s().data());
        }

        CP_DEBUG("ColumnarDBConnectionPool::~ColumnarDBConnectionPool - Destructor completed at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

#ifdef __cpp_exceptions

    std::shared_ptr<ColumnarDBConnectionPool> ColumnarDBConnectionPool::create(const std::string &url,
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
                                                                               TransactionIsolationLevel transactionIsolation)
    {
        auto result = create(std::nothrow, url, username, password, options, initialSize, maxSize, minIdle,
                             maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
                             testOnBorrow, testOnReturn, transactionIsolation);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<ColumnarDBConnectionPool> ColumnarDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto result = create(std::nothrow, config);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DBConnection> ColumnarDBConnectionPool::getDBConnection()
    {
        auto result = getDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<ColumnarDBConnection> ColumnarDBConnectionPool::getColumnarDBConnection()
    {
        auto result = getColumnarDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t ColumnarDBConnectionPool::getActiveDBConnectionCount() const
    {
        auto result = getActiveDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t ColumnarDBConnectionPool::getIdleDBConnectionCount() const
    {
        auto result = getIdleDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t ColumnarDBConnectionPool::getTotalDBConnectionCount() const
    {
        auto result = getTotalDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void ColumnarDBConnectionPool::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool ColumnarDBConnectionPool::isRunning() const
    {
        auto result = isRunning(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnectionPool>, DBException> ColumnarDBConnectionPool::create(std::nothrow_t,
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
        auto pool = std::make_shared<ColumnarDBConnectionPool>(
            DBConnectionPool::ConstructorTag{}, url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, transactionIsolation);

        // Initialize the pool after construction (creates connections and starts maintenance thread)
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

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }

        return pool;
    }

    cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> ColumnarDBConnectionPool::getDBConnection(std::nothrow_t) noexcept
    {
        auto result = getColumnarDBConnection(std::nothrow);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        return std::static_pointer_cast<DBConnection>(result.value());
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> ColumnarDBConnectionPool::getColumnarDBConnection(std::nothrow_t) noexcept
    {
        using namespace std::chrono;
        auto deadline = steady_clock::now() + milliseconds(m_maxWaitMillis);

        while (true)
        {
            std::shared_ptr<ColumnarPooledDBConnection> result;
            bool handedOff = false;

            {
                std::unique_lock lockPool(m_mutexPool);

                if (!m_running.load(std::memory_order_acquire))
                {
                    return cpp_dbc::unexpected(DBException("6R7S8T9U0V1W", "Connection pool is closed", system_utils::captureCallStack()));
                }

                // Multiple break statements required to express mutually exclusive exit conditions
                // (idle found, result created). Refactoring would add artificial state variables.
                // Acquire loop: idle → create → wait
                while (!result) // NOSONAR(cpp:S924)
                {
                    // Step 1: Check idle connections
                    if (!m_idleConnections.empty())
                    {
                        result = m_idleConnections.front();
                        m_idleConnections.pop();
                        CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - GOT IDLE: active=%d, idle=%zu, total=%zu",
                                 m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                        break;
                    }

                    // Step 2: Create new connection if pool not full
                    if (m_allConnections.size() + m_pendingCreations < m_maxSize)
                    {
                        CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - CREATING: total=%zu, pending=%zu, max=%zu",
                                 m_allConnections.size(), m_pendingCreations, m_maxSize);
                        m_pendingCreations++;
                        lockPool.unlock();

                        // createPooledDBConnection() must be called outside locks (I/O).
                        // Lock and pendingCreations are always restored regardless of outcome.
                        {
                            auto newConnResult = createPooledDBConnection(std::nothrow);
                            lockPool.lock();
                            m_pendingCreations--;

                            if (!newConnResult.has_value())
                            {
                                CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - Failed to create connection: %s", newConnResult.error().what_s().data());
                            }
                            else
                            {
                                auto newConn = newConnResult.value();
                                // Recheck size under lock (another thread may have filled the pool
                                // while we were creating outside the lock)
                                if (m_allConnections.size() < m_maxSize)
                                {
                                    m_allConnections.push_back(newConn);
                                    result = newConn;
                                }
                                else
                                {
                                    // Pool became full — discard candidate
                                    CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - DISCARD candidate: total=%zu >= max=%zu", m_allConnections.size(), m_maxSize);
                                    newConn->m_closed.store(true, std::memory_order_release); // Prevent destructor returnToPool()
                                    lockPool.unlock();
                                    auto closeResult = newConn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                                    if (!closeResult.has_value())
                                    {
                                        CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - close() on discarded candidate failed: %s", closeResult.error().what_s().data());
                                    }
                                    lockPool.lock();
                                }
                            }
                        }
                        // Always restart outer loop: if result is set it exits,
                        // otherwise idle is re-checked before waiting (matches KV).
                        // This prevents missing connections returned during the unlock.
                        continue;
                    }

                    // Step 3: Wait for a connection to become available
                    ConnectionRequest request;
                    m_waitQueue.push_back(&request);
                    CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - WAITING on CV: active=%d, idle=%zu, total=%zu, pending=%zu, waiters=%zu",
                             m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size(),
                             m_pendingCreations, m_waitQueue.size());

                    // Multiple break statements required for mutually exclusive CV exit conditions
                    // (handoff, idle-after-timeout, idle-after-wakeup). Refactoring would break
                    // FIFO queue correctness under sanitizer-serialized execution.
                    while (true) // NOSONAR(cpp:S924) — Inner wait loop, request stays in queue at FIFO position
                    {
                        std::cv_status status;
                        try
                        {
                            status = request.cv.wait_until(lockPool, deadline);
                        }
                        catch (const std::exception &ex)
                        {
                            // Safety: remove from queue if wait_until throws (extremely rare),
                            // then return error instead of re-throwing from noexcept method.
                            CP_DEBUG("ColumnarDBConnectionPool::getConnection - wait_until threw: %s", ex.what());
                            std::erase(m_waitQueue, &request);
                            return cpp_dbc::unexpected(DBException("RBCMIUGPADBO",
                                                                   "wait_until threw in getColumnarDBConnection: " + std::string(ex.what()),
                                                                   system_utils::captureCallStack()));
                        }
                        catch (...) // NOSONAR(cpp:S2738) — non-std exceptions: remove from queue then return error
                        {
                            CP_DEBUG("ColumnarDBConnectionPool::getConnection - wait_until threw unknown exception");
                            std::erase(m_waitQueue, &request);
                            return cpp_dbc::unexpected(DBException("ZWT16OIBWJ3G",
                                                                   "Unknown exception from wait_until in getColumnarDBConnection",
                                                                   system_utils::captureCallStack()));
                        }

                        // FIRST: check fulfilled (takes priority over timeout — handoff may arrive
                        // simultaneously with deadline expiry)
                        if (request.fulfilled)
                        {
                            result = request.conn;
                            handedOff = true;
                            CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - HANDOFF received: active=%d, idle=%zu, total=%zu",
                                     m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                            break;
                        }

                        if (!m_running.load(std::memory_order_acquire))
                        {
                            std::erase(m_waitQueue, &request);
                            return cpp_dbc::unexpected(DBException("BM13U87YCH08", "Connection pool is closed", system_utils::captureCallStack()));
                        }

                        if (status == std::cv_status::timeout)
                        {
                            // Timed out: remove our request from the queue if still there
                            std::erase(m_waitQueue, &request);

                            // One last idle check: a connection may have arrived just as
                            // the deadline fired (matches KV pattern).
                            if (!m_idleConnections.empty())
                            {
                                result = m_idleConnections.front();
                                m_idleConnections.pop();
                                CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - GOT IDLE after timeout: active=%d, idle=%zu",
                                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size());
                                break;
                            }
                            CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - TIMEOUT: active=%d, idle=%zu, total=%zu, pending=%zu",
                                     m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size(), m_pendingCreations);
                            return cpp_dbc::unexpected(DBException("OAD202OF30N9", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
                        }

                        // Spurious wakeup: check idle again
                        if (!m_idleConnections.empty())
                        {
                            std::erase(m_waitQueue, &request);
                            result = m_idleConnections.front();
                            m_idleConnections.pop();
                            CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - GOT IDLE after wakeup: active=%d, idle=%zu",
                                     m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size());
                            break;
                        }

                        // Nothing available — stay in queue at same FIFO position, re-wait.
                        CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - CV WAKEUP (re-waiting): idle=%zu, total=%zu",
                                 m_idleConnections.size(), m_allConnections.size());
                    }
                }

                if (!handedOff)
                {
                    result->setActive(std::nothrow, true);
                    m_activeConnections.fetch_add(1, std::memory_order_acq_rel);
                }
                result->m_closed.store(false, std::memory_order_release);
            }

            // Phase 2: HikariCP validation skip (outside lock)
            if (m_testOnBorrow)
            {
                auto lastUsed = result->getLastUsedTime(std::nothrow);
                auto now = steady_clock::now();
                auto timeSinceLastUse = duration_cast<milliseconds>(now - lastUsed).count();

                if (timeSinceLastUse > m_validationTimeoutMillis)
                {
                    CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - VALIDATING (timeSinceLastUse=%lld ms > %ld ms)", (long long)timeSinceLastUse, m_validationTimeoutMillis);
                    auto valResult = validateConnection(std::nothrow, result->getUnderlyingConnection(std::nothrow));
                    bool validationPassed = valResult.has_value() && valResult.value();

                    if (!validationPassed)
                    {
                        CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - VALIDATION FAILED: removing connection, active=%d", m_activeConnections.load(std::memory_order_acquire));

                        // Validation failed — undo active marking, remove from pool
                        {
                            std::scoped_lock lockPool(m_mutexPool);
                            result->setActive(std::nothrow, false);
                            result->m_closed.store(true, std::memory_order_release);
                            m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);

                            auto it = std::ranges::find(m_allConnections, result);
                            if (it != m_allConnections.end())
                            {
                                m_allConnections.erase(it);
                            }
                            // Notify first waiter: freed slot allows re-check of deadline or idle
                            notifyFirstWaiter(std::nothrow);
                        }

                        // Close invalid connection outside lock
                        auto closeResult = result->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                        if (!closeResult.has_value())
                        {
                            CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - close() on invalid connection failed: %s", closeResult.error().what_s().data());
                        }

                        // Check deadline before retrying
                        if (steady_clock::now() >= deadline)
                        {
                            return cpp_dbc::unexpected(DBException("CF30ABE579BF", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
                        }

                        continue; // Retry
                    }
                }
            }

            // Phase 3: prepareForBorrow (outside lock)
            auto borrowResult = result->getUnderlyingConnection(std::nothrow)->prepareForBorrow(std::nothrow);
            if (!borrowResult.has_value())
            {
                CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - prepareForBorrow failed: %s", borrowResult.error().what_s().data());
            }

            return cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException>{
                std::static_pointer_cast<ColumnarDBConnection>(result)};
        }
    }

    cpp_dbc::expected<size_t, DBException> ColumnarDBConnectionPool::getActiveDBConnectionCount(std::nothrow_t) const noexcept
    {
        return static_cast<size_t>(m_activeConnections.load(std::memory_order_acquire));
    }

    cpp_dbc::expected<size_t, DBException> ColumnarDBConnectionPool::getIdleDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_idleConnections.size();
    }

    cpp_dbc::expected<size_t, DBException> ColumnarDBConnectionPool::getTotalDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_allConnections.size();
    }

    cpp_dbc::expected<void, DBException> ColumnarDBConnectionPool::close(std::nothrow_t) noexcept
    {
        CP_DEBUG("ColumnarDBConnectionPool::close - Starting close operation");

        if (!m_running.exchange(false, std::memory_order_acq_rel))
        {
            CP_DEBUG("ColumnarDBConnectionPool::close - Already closed, returning");
            return {}; // Already closed — idempotent
        }

        // Mark pool as no longer alive - this prevents pooled connections from trying to return
        if (m_poolAlive)
        {
            m_poolAlive->store(false, std::memory_order_release);
        }

        CP_DEBUG("ColumnarDBConnectionPool::close - Waiting for active operations to complete at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            CP_DEBUG("ColumnarDBConnectionPool::close - Initial active connections: %d", m_activeConnections.load(std::memory_order_acquire));

            while (m_activeConnections.load(std::memory_order_acquire) > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 2 == 0)
                {
                    CP_DEBUG("ColumnarDBConnectionPool::close - Waited %ld seconds for %d active connections",
                             elapsed_seconds, m_activeConnections.load(std::memory_order_acquire));
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("ColumnarDBConnectionPool::close - Timeout waiting for active connections, forcing close at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
                    m_activeConnections.store(0, std::memory_order_release);
                    break;
                }
            }
        }

        // Notify maintenance thread and all waiting borrowers
        {
            std::scoped_lock lock(m_mutexPool);
            m_maintenanceCondition.notify_all();
            // Wake every waiter individually so they see m_running == false and exit.
            for (auto *r : m_waitQueue)
            {
                r->cv.notify_one();
            }
        }

        if (m_maintenanceThread.joinable())
        {
            m_maintenanceThread.join();
        }

        // CRITICAL: Collect connections under pool lock, then close OUTSIDE pool lock.
        std::vector<std::shared_ptr<ColumnarPooledDBConnection>> connectionsToClose;
        {
            std::scoped_lock lock(m_mutexPool);
            connectionsToClose = m_allConnections;
            while (!m_idleConnections.empty())
            {
                m_idleConnections.pop();
            }
            m_allConnections.clear();
        }

        cpp_dbc::expected<void, DBException> lastError{};

        for (const auto &conn : connectionsToClose)
        {
            if (conn && conn->getUnderlyingConnection(std::nothrow))
            {
                conn->setActive(std::nothrow, false);
                auto r = conn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                if (!r.has_value())
                {
                    CP_DEBUG("ColumnarDBConnectionPool::close - connection close failed: %s", r.error().what_s().data());
                    lastError = r;
                }
            }
        }

        return lastError;
    }

    cpp_dbc::expected<bool, DBException> ColumnarDBConnectionPool::isRunning(std::nothrow_t) const noexcept
    {
        return m_running.load(std::memory_order_acquire);
    }

    // ColumnarPooledDBConnection implementation

    ColumnarPooledDBConnection::ColumnarPooledDBConnection(
        ColumnarPooledDBConnection::ConstructorTag,
        std::shared_ptr<ColumnarDBConnection> connection,
        std::weak_ptr<ColumnarDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive) noexcept
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive)
    {
        // m_creationTime and m_lastUsedTimeNs use in-class initializers
    }

    bool ColumnarPooledDBConnection::isPoolValid(std::nothrow_t) const noexcept
    {
        return m_poolAlive && m_poolAlive->load(std::memory_order_acquire) && !m_pool.expired();
    }

    ColumnarPooledDBConnection::~ColumnarPooledDBConnection()
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
                    pool->m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
                }
                m_active.store(false, std::memory_order_release);
            }

            auto closeResult = m_conn->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("ColumnarPooledDBConnection::~ColumnarPooledDBConnection - close failed: %s", closeResult.error().what_s().data());
            }
        }
    }

#ifdef __cpp_exceptions

    void ColumnarPooledDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool ColumnarPooledDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void ColumnarPooledDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool ColumnarPooledDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string ColumnarPooledDBConnection::getURL() const
    {
        auto result = getURL(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void ColumnarPooledDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool ColumnarPooledDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

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

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::close(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the close
        bool expected = false;
        if (m_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
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
                        auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<ColumnarPooledDBConnection>(shared_from_this()));
                        if (!returnResult.has_value())
                        {
                            CP_DEBUG("ColumnarPooledDBConnection::close - returnConnection failed: %s", returnResult.error().what_s().data());
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
                        CP_DEBUG("ColumnarPooledDBConnection::close - Failed to close underlying connection: %s", result.error().what_s().data());
                        return result;
                    }
                }

                return {};
            }
            catch (const std::exception &ex)
            {
                // Only shared_from_this() can throw here (std::bad_weak_ptr).
                // All other inner calls are nothrow. Close the connection and return error.
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("6J52BKYDQQOM",
                                                       "Exception in close: " + std::string(ex.what()),
                                                       system_utils::captureCallStack()));
            }
            catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions
            {
                CP_DEBUG("ColumnarPooledDBConnection::close - Unknown exception caught");
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("UQQSB03ZV8CR",
                                                       "Unknown exception in close",
                                                       system_utils::captureCallStack()));
            }
        }

        return {};
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::reset(std::nothrow_t) noexcept
    {
        if (!m_conn)
        {
            return cpp_dbc::unexpected(DBException("W9QZE5CW0T1C",
                                                   "Underlying connection is null",
                                                   system_utils::captureCallStack()));
        }

        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("GCMQQLPI0A3Y",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        updateLastUsedTime(std::nothrow);
        return m_conn->reset(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return true;
        }
        return m_conn->isClosed(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the return
        bool expected = false;
        if (!m_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return {};
        }

        try
        {
            // Return to pool instead of actually closing
            updateLastUsedTime(std::nothrow);

            // Check if pool is still alive using the shared atomic flag
            // Use qualified call to avoid virtual dispatch issues when called from destructor
            if (ColumnarPooledDBConnection::isPoolValid(std::nothrow))
            {
                // Try to obtain a shared_ptr from the weak_ptr
                if (auto poolShared = m_pool.lock())
                {
                    // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                    // (see close(nothrow) method for full explanation of the bug)
                    m_closed.store(false, std::memory_order_release);
                    auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<ColumnarPooledDBConnection>(this->shared_from_this()));
                    if (!returnResult.has_value())
                    {
                        CP_DEBUG("ColumnarPooledDBConnection::returnToPool - returnConnection failed: %s", returnResult.error().what_s().data());
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
            CP_DEBUG("ColumnarPooledDBConnection::returnToPool - shared_from_this failed: %s", ex.what());
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("6OOO8USCUNVM",
                                                   "Exception in returnToPool: " + std::string(ex.what()),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions
        {
            CP_DEBUG("ColumnarPooledDBConnection::returnToPool - Unknown exception caught");
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("C5TI8P7TLY82",
                                                   "Unknown exception in returnToPool",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return !m_active.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<std::string, DBException> ColumnarPooledDBConnection::getURL(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("VUMH1XH7ZJ05", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::ping(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("1K6OKWCZQIDP", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->ping(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> ColumnarPooledDBConnection::prepareStatement(std::nothrow_t, const std::string &query) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("A61A8B1BF33C", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->prepareStatement(std::nothrow, query);
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> ColumnarPooledDBConnection::executeQuery(std::nothrow_t, const std::string &query) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("TVW92Q0P8I7Y", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->executeQuery(std::nothrow, query);
    }

    cpp_dbc::expected<uint64_t, DBException> ColumnarPooledDBConnection::executeUpdate(std::nothrow_t, const std::string &query) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("IZTFCKPFH4TX", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->executeUpdate(std::nothrow, query);
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("EG65LGKZS291", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->beginTransaction(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::commit(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("F43ECCD8427F", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->commit(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::rollback(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("PAKKP3ZTKEH8", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->rollback(std::nothrow);
    }

    cpp_dbc::expected<void, DBException>
    ColumnarPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("I5Q63ZSP1VDU", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->setTransactionIsolation(std::nothrow, level);
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    ColumnarPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("VHE303KAZJ4S", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getTransactionIsolation(std::nothrow);
    }

    std::chrono::time_point<std::chrono::steady_clock> ColumnarPooledDBConnection::getCreationTime(std::nothrow_t) const noexcept
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> ColumnarPooledDBConnection::getLastUsedTime(std::nothrow_t) const noexcept
    {
        return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{m_lastUsedTimeNs.load(std::memory_order_acquire)}};
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::setActive(std::nothrow_t, bool isActive) noexcept
    {
        m_active.store(isActive, std::memory_order_release);
        return {};
    }

    bool ColumnarPooledDBConnection::isActive(std::nothrow_t) const noexcept
    {
        return m_active.load(std::memory_order_acquire);
    }

    std::shared_ptr<DBConnection> ColumnarPooledDBConnection::getUnderlyingConnection(std::nothrow_t) noexcept
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

    cpp_dbc::expected<void, DBException>
    ColumnarPooledDBConnection::prepareForPoolReturn(std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("ERB7A9KN4OZ9",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForPoolReturn(std::nothrow, isolationLevel);
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("FQYTSNOGDGZE",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForBorrow(std::nothrow);
    }

} // namespace cpp_dbc

// ScyllaDB connection pool implementation
namespace cpp_dbc::ScyllaDB
{
    ScyllaDBConnectionPool::ScyllaDBConnectionPool(DBConnectionPool::ConstructorTag,
                                                   const std::string &url,
                                                   const std::string &username,
                                                   const std::string &password) noexcept
        : ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag{}, url, username, password)
    {
        // Scylla-specific initialization if needed
    }

    ScyllaDBConnectionPool::ScyllaDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag{}, config)
    {
        // Scylla-specific initialization if needed
    }

#ifdef __cpp_exceptions

    std::shared_ptr<ScyllaDBConnectionPool> ScyllaDBConnectionPool::create(const std::string &url,
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
                                                                                                           const std::string &url,
                                                                                                           const std::string &username,
                                                                                                           const std::string &password) noexcept
    {
        auto pool = std::make_shared<ScyllaDBConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
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
