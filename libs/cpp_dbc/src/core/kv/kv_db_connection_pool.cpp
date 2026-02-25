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
 * @brief Implementation of connection pool for key-value databases
 */

#include "cpp_dbc/core/kv/kv_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/core/kv/kv_db_driver.hpp"
#include "cpp_dbc/drivers/kv/driver_redis.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <random>
#include <ranges>

#include "../connection_pool_internal.hpp"

namespace cpp_dbc
{
    // KVDBConnectionPool implementation

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
                                           const std::string &validationQuery,
                                           TransactionIsolationLevel transactionIsolation)
        : m_poolAlive(std::make_shared<std::atomic<bool>>(true)),
          m_url(url),
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
          m_validationQuery(validationQuery),
          m_transactionIsolation(transactionIsolation)
    {
        // Pool initialization will be done in the factory method
    }

    KVDBConnectionPool::KVDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
        : m_poolAlive(std::make_shared<std::atomic<bool>>(true)),
          m_url(config.getUrl()),
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
          m_validationQuery(config.getValidationQuery().empty() ? "PING" : config.getValidationQuery()),
          m_transactionIsolation(config.getTransactionIsolation())
    {
        // Pool initialization will be done in the factory method
    }

    KVDBConnectionPool::~KVDBConnectionPool()
    {
        CP_DEBUG("KVDBConnectionPool::~KVDBConnectionPool - Starting destructor at %lld",
                 (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        auto result = KVDBConnectionPool::close(std::nothrow);
        if (!result.has_value())
        {
            CP_DEBUG("KVDBConnectionPool::~KVDBConnectionPool - close failed: %s",
                     result.error().what_s().c_str());
        }

        CP_DEBUG("KVDBConnectionPool::~KVDBConnectionPool - Destructor completed at %lld",
                 (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

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
                                                                                                   const std::string &validationQuery,
                                                                                                   TransactionIsolationLevel transactionIsolation) noexcept
    {
        try
        {
            auto pool = std::make_shared<KVDBConnectionPool>(
                DBConnectionPool::ConstructorTag{}, url, username, password, options, initialSize, maxSize, minIdle,
                maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis,
                maxLifetimeMillis, testOnBorrow, testOnReturn, validationQuery,
                transactionIsolation);

            // Initialize the pool after construction (creates connections and starts maintenance thread)
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

            // Initialize the pool after construction (creates connections and starts maintenance thread)
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

    cpp_dbc::expected<void, DBException> KVDBConnectionPool::initializePool(std::nothrow_t) noexcept
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

                // Add to idle connections and all connections lists under pool lock
                {
                    std::scoped_lock lock(m_mutexPool);
                    m_idleConnections.push(pooledResult.value());
                    m_allConnections.push_back(pooledResult.value());
                }
            }

            // Start maintenance thread (using std::jthread) — can throw std::system_error
            m_maintenanceThread = std::jthread([this]
                                               { maintenanceTask(); });
        }
        catch (const std::exception &ex)
        {
            close(std::nothrow);
            return cpp_dbc::unexpected(DBException("N3B86CRLRP96", "Failed to initialize connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            close(std::nothrow);
            return cpp_dbc::unexpected(DBException("70WV9CHZOW5A", "Failed to initialize connection pool: unknown error", system_utils::captureCallStack()));
        }
        return {};
    }

    cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException>
    KVDBConnectionPool::createDBConnection(std::nothrow_t) noexcept
    {
        try
        {
            auto dbConn = DriverManager::getDBConnection(m_url, m_username, m_password, m_options);
            auto kvConn = std::dynamic_pointer_cast<KVDBConnection>(dbConn);
            if (!kvConn)
            {
                return cpp_dbc::unexpected(DBException("96CAD6FDCDD9", "Connection pool only supports key-value database connections", system_utils::captureCallStack()));
            }
            return kvConn;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("RH8LS3L4UIUR", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("0JELIYLCHS3R", "Unknown error creating KV database connection", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<KVPooledDBConnection>, DBException>
    KVDBConnectionPool::createPooledDBConnection(std::nothrow_t) noexcept
    {
        auto connResult = createDBConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }

        // shared_from_this() may throw std::bad_weak_ptr if the pool is stack-allocated;
        // in that case weakPool remains empty and the pooled connection holds no back-reference.
        std::weak_ptr<KVDBConnectionPool> weakPool;
        try
        {
            weakPool = shared_from_this();
        }
        catch (const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("KVDBConnectionPool::createPooledDBConnection - bad_weak_ptr: %s", ex.what());
        }

        try
        {
            return std::make_shared<KVPooledDBConnection>(connResult.value(), weakPool, m_poolAlive);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("UP2CDP5TQULK", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("JHW4W2BEIYOI", "Unknown error wrapping KV connection in pool", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException>
    KVDBConnectionPool::validateConnection(std::nothrow_t, std::shared_ptr<KVDBConnection> conn) const noexcept
    {
        if (!conn)
        {
            return false;
        }

        auto closedResult = conn->isClosed(std::nothrow);
        if (!closedResult.has_value())
        {
            CP_DEBUG("KVDBConnectionPool::validateConnection - isClosed failed: %s", closedResult.error().what_s().c_str());
            return false;
        }
        if (closedResult.value())
        {
            return false;
        }

        auto pingResult = conn->ping(std::nothrow);
        if (!pingResult.has_value())
        {
            CP_DEBUG("KVDBConnectionPool::validateConnection - ping failed: %s", pingResult.error().what_s().c_str());
            return false;
        }
        return !pingResult.value().empty();
    }

    cpp_dbc::expected<void, DBException>
    KVDBConnectionPool::returnConnection(std::nothrow_t, std::shared_ptr<KVPooledDBConnection> conn) noexcept
    {
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("MFK2O3KD108T", "returnConnection called with null connection", system_utils::captureCallStack()));
        }

        if (!m_running.load(std::memory_order_acquire))
        {
            // Pool is closing — decrement counter so close() doesn't wait forever
            conn->setActive(std::nothrow, false);
            m_activeConnections--;
            auto closeResult = conn->getUnderlyingKVConnection()->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("KVDBConnectionPool::returnConnection - close failed during shutdown: %s", closeResult.error().what_s().c_str());
            }
            return {};
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive(std::nothrow))
        {
            CP_DEBUG("returnConnection - SKIPPED: connection already inactive");
            return cpp_dbc::unexpected(DBException("0MCZUBO3OVHG", "returnConnection called on an already inactive connection (double-return bug)", system_utils::captureCallStack()));
        }

        // Additional safety check: verify connection is in allConnections
        {
            std::scoped_lock lockPool(m_mutexPool);
            auto it = std::ranges::find(m_allConnections, conn);
            if (it == m_allConnections.end())
            {
                conn->setActive(std::nothrow, false);
                m_activeConnections--;
                CP_DEBUG("returnConnection - ORPHAN: connection not in allConnections, active=%d", m_activeConnections.load(std::memory_order_acquire));
                m_connectionAvailable.notify_one(); // Inside lock — freed slot
                return cpp_dbc::unexpected(DBException("FRETYZRRNK23", "returnConnection called with a connection not belonging to this pool (orphan)", system_utils::captureCallStack()));
            }
        }

        // ========================================
        // Phase 1: I/O OUTSIDE any lock
        // Connection is marked active=true → maintenance skips it, borrowers can't see it.
        // ========================================
        bool valid = true;

        if (m_testOnReturn)
        {
            auto validResult = validateConnection(std::nothrow, conn->getUnderlyingKVConnection());
            valid = validResult.value_or(false);
        }

        if (valid && !conn->m_closed.load(std::memory_order_acquire))
        {
            auto resetResult = conn->getUnderlyingKVConnection()->prepareForPoolReturn(std::nothrow);
            if (!resetResult.has_value())
            {
                CP_DEBUG("KVDBConnectionPool::returnConnection - prepareForPoolReturn failed: %s", resetResult.error().what_s().c_str());
                valid = false;
            }
        }

        // ========================================
        // Phase 2: Pool state under m_mutexPool only (NO I/O)
        // ========================================
        if (valid)
        {
            conn->updateLastUsedTime(std::nothrow); // Mark return time for validation skip
            std::scoped_lock lockPool(m_mutexPool);
            conn->setActive(std::nothrow, false);
            m_activeConnections--;

            // Direct handoff: if threads are waiting, give the connection directly
            // to the first waiter instead of pushing to idle.
            if (!m_waitQueue.empty())
            {
                auto *request = m_waitQueue.front();
                m_waitQueue.pop_front();
                request->conn = conn;
                request->fulfilled = true;
                conn->setActive(std::nothrow, true);
                m_activeConnections++;
                CP_DEBUG("returnConnection - HANDOFF: direct to waiter, active=%d, waiters=%zu, total=%zu",
                         m_activeConnections.load(std::memory_order_acquire), m_waitQueue.size(), m_allConnections.size());
                m_connectionAvailable.notify_all(); // Wake all so fulfilled waiter checks its flag
            }
            else
            {
                m_idleConnections.push(conn);
                CP_DEBUG("returnConnection - VALID: pushed to idle, active=%d, idle=%zu, total=%zu",
                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                m_connectionAvailable.notify_one();
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
                conn->m_closed.store(true);
                m_activeConnections--;
                CP_DEBUG("returnConnection - INVALID: active=%d", m_activeConnections.load(std::memory_order_acquire));

                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    m_allConnections.erase(it);
                }
                std::queue<std::shared_ptr<KVPooledDBConnection>> tempQueue;
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

                // Notify inside lock: freed slot allows waiting borrower to create
                m_connectionAvailable.notify_one();
            }

            // Step 2: Close invalid connection OUTSIDE locks (I/O)
            {
                auto closeResult = conn->getUnderlyingKVConnection()->close(std::nothrow);
                if (!closeResult.has_value())
                {
                    CP_DEBUG("KVDBConnectionPool::returnConnection - close failed on invalid connection: %s", closeResult.error().what_s().c_str());
                }
            }

            // Step 3: Create replacement OUTSIDE locks (I/O)
            if (!m_running.load(std::memory_order_acquire))
            {
                return {}; // Pool closing, skip replacement
            }

            std::shared_ptr<KVPooledDBConnection> replacement;
            {
                auto replacementResult = createPooledDBConnection(std::nothrow);
                if (replacementResult.has_value())
                {
                    replacement = replacementResult.value();
                }
                else
                {
                    CP_DEBUG("KVDBConnectionPool::returnConnection - failed creating replacement: %s", replacementResult.error().what_s().c_str());
                }
            }

            // Step 4: Insert replacement under lock with direct handoff
            if (replacement)
            {
                std::shared_ptr<KVPooledDBConnection> discardReplacement;
                {
                    std::scoped_lock lockPool(m_mutexPool);
                    if (m_running.load(std::memory_order_acquire) && m_allConnections.size() < m_maxSize)
                    {
                        m_allConnections.push_back(replacement);

                        // Direct handoff for replacement if waiters exist
                        if (!m_waitQueue.empty())
                        {
                            auto *request = m_waitQueue.front();
                            m_waitQueue.pop_front();
                            request->conn = replacement;
                            request->fulfilled = true;
                            replacement->setActive(std::nothrow, true);
                            m_activeConnections++;
                            CP_DEBUG("returnConnection - REPLACEMENT HANDOFF: active=%d, waiters=%zu, total=%zu",
                                     m_activeConnections.load(std::memory_order_acquire), m_waitQueue.size(), m_allConnections.size());
                            m_connectionAvailable.notify_all();
                        }
                        else
                        {
                            m_idleConnections.push(replacement);
                            CP_DEBUG("returnConnection - REPLACEMENT: pushed to idle, active=%d, idle=%zu, total=%zu",
                                     m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                            m_connectionAvailable.notify_one();
                        }
                    }
                    else
                    {
                        // Pool full or closing — discard replacement
                        replacement->m_closed.store(true);
                        discardReplacement = replacement;
                    }
                }

                // Close discarded replacement outside lock
                if (discardReplacement)
                {
                    auto closeResult = discardReplacement->getUnderlyingKVConnection()->close(std::nothrow);
                    if (!closeResult.has_value())
                    {
                        CP_DEBUG("KVDBConnectionPool::returnConnection - close() on discarded replacement failed: %s", closeResult.error().what_s().c_str());
                    }
                }
            }
        }

        return {};
    }

    void KVDBConnectionPool::maintenanceTask()
    {
        do
        {
            // Wait for 30 seconds or until notified (e.g., when close() is called).
            // Uses m_mutexPool so both CVs share the same mutex. The lock is released during wait_for().
            {
                std::unique_lock lock(m_mutexPool);
                m_maintenanceCondition.wait_for(lock, std::chrono::seconds(30), [this]
                                                { return !m_running.load(std::memory_order_acquire); });
            }
            // m_mutexPool is now released. Maintenance runs without holding it.

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

                    // Check if the connection has been idle for too long
                    auto idleTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - pooledConn->getLastUsedTime(std::nothrow))
                                        .count();

                    // Check if the connection has lived for too long
                    auto lifeTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - pooledConn->getCreationTime(std::nothrow))
                                        .count();

                    bool expired = (idleTime > m_idleTimeoutMillis) ||
                                   (lifeTime > m_maxLifetimeMillis);

                    // Close and remove expired connections if we have more than minIdle
                    if (expired && m_allConnections.size() > m_minIdle)
                    {
                        // Remove from idle queue if present
                        std::queue<std::shared_ptr<KVPooledDBConnection>> tempQueue;
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

                        // Remove from allConnections
                        it = m_allConnections.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            // Locks released here — replenishment happens outside locks

            // CRITICAL: createPooledDBConnection() must NOT be called while holding pool locks.
            // Replenish minIdle connections OUTSIDE locks to avoid potential lock ordering cycles.
            size_t currentTotal;
            {
                std::scoped_lock lock(m_mutexPool);
                currentTotal = m_allConnections.size();
            }
            while (m_running.load(std::memory_order_acquire) && currentTotal < m_minIdle)
            {
                auto pooledResult = createPooledDBConnection(std::nothrow);
                if (!pooledResult.has_value())
                {
                    CP_DEBUG("KVDBConnectionPool::maintenanceTask - Failed to create minIdle connection: %s", pooledResult.error().what_s().c_str());
                    break;
                }
                std::scoped_lock lock(m_mutexPool);
                m_idleConnections.push(pooledResult.value());
                m_allConnections.push_back(pooledResult.value());
                m_connectionAvailable.notify_one(); // Wake waiting borrower if any
                currentTotal = m_allConnections.size();
            }
        } while (m_running.load(std::memory_order_acquire));
    }

    cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException> KVDBConnectionPool::getKVDBConnection(std::nothrow_t) noexcept
    {
        try
        {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(m_maxWaitMillis);

            while (true) // Retry loop for testOnBorrow validation failures
            {
                std::shared_ptr<KVPooledDBConnection> result;
                bool handedOff = false;

                // ========================================
                // Phase 1: Acquire candidate under m_mutexPool (NO I/O)
                // ========================================
                {
                    std::unique_lock lockPool(m_mutexPool);

                    if (!m_running.load(std::memory_order_acquire))
                    {
                        return cpp_dbc::unexpected(DBException("BAB0896A213B", "Connection pool is closed", system_utils::captureCallStack()));
                    }

                    // Unified acquire loop: idle check → create → wait (with direct handoff)
                    while (!result)
                    {
                        // Step 1: Check idle (catches connections from lost notifications)
                        if (!m_idleConnections.empty())
                        {
                            result = m_idleConnections.front();
                            m_idleConnections.pop();
                            CP_DEBUG("borrow - GOT IDLE: active=%d, idle=%zu, total=%zu",
                                     m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                            break;
                        }

                        // Step 2: Try creating if pool has room
                        if (m_allConnections.size() + m_pendingCreations < m_maxSize)
                        {
                            CP_DEBUG("borrow - CREATING: total=%zu, pending=%zu, max=%zu",
                                     m_allConnections.size(), m_pendingCreations, m_maxSize);
                            m_pendingCreations++;
                            lockPool.unlock();

                            // createPooledDBConnection() must be called outside locks (I/O).
                            // Lock and pendingCreations are always restored regardless of outcome.
                            {
                                auto candidateResult = createPooledDBConnection(std::nothrow);
                                lockPool.lock();
                                m_pendingCreations--;

                                if (!candidateResult.has_value())
                                {
                                    CP_DEBUG("borrow - Failed to create connection: %s", candidateResult.error().what_s().c_str());
                                }
                                else
                                {
                                    auto candidate = candidateResult.value();
                                    // Recheck size under lock (another thread may have filled the pool)
                                    if (m_allConnections.size() < m_maxSize)
                                    {
                                        m_allConnections.push_back(candidate);
                                        result = candidate;
                                    }
                                    else
                                    {
                                        // Pool became full — discard candidate
                                        CP_DEBUG("borrow - DISCARD candidate: total=%zu >= max=%zu", m_allConnections.size(), m_maxSize);
                                        candidate->m_closed.store(true); // Prevent destructor returnToPool()
                                        lockPool.unlock();
                                        auto closeResult = candidate->getUnderlyingKVConnection()->close(std::nothrow);
                                        if (!closeResult.has_value())
                                        {
                                            CP_DEBUG("borrow - close() on discarded candidate failed: %s", closeResult.error().what_s().c_str());
                                        }
                                        lockPool.lock();
                                    }
                                }
                            }
                            continue; // ALWAYS re-check idle after any unlock/lock cycle
                        }

                        // Step 3: Nothing available — wait on CV with direct handoff request.
                        ConnectionRequest request;
                        m_waitQueue.push_back(&request);
                        CP_DEBUG("borrow - WAITING on CV: active=%d, idle=%zu, total=%zu, pending=%zu, waiters=%zu",
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
                                status = m_connectionAvailable.wait_until(lockPool, deadline);
                            }
                            catch (const std::exception &ex)
                            {
                                // Safety: remove from queue if wait_until throws (extremely rare), then propagate
                                CP_DEBUG("KVDBConnectionPool::getConnection - wait_until threw: %s", ex.what());
                                std::erase(m_waitQueue, &request);
                                throw;
                            }
                            catch (...) // NOSONAR — non-std exceptions: remove from queue then propagate
                            {
                                CP_DEBUG("KVDBConnectionPool::getConnection - wait_until threw unknown exception");
                                std::erase(m_waitQueue, &request);
                                throw;
                            }

                            // Check if we received a direct handoff
                            if (request.fulfilled)
                            {
                                result = request.conn;
                                handedOff = true;
                                CP_DEBUG("borrow - HANDOFF received: active=%d, idle=%zu, total=%zu",
                                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                                break; // Exit inner loop; outer loop also exits (result is set)
                            }

                            // Not fulfilled — check termination conditions (stay in queue)
                            if (!m_running.load(std::memory_order_acquire))
                            {
                                std::erase(m_waitQueue, &request);
                                return cpp_dbc::unexpected(DBException("9C4A03BF221C", "Connection pool is closed", system_utils::captureCallStack()));
                            }

                            if (status == std::cv_status::timeout)
                            {
                                std::erase(m_waitQueue, &request);
                                // One last idle check before returning error
                                if (!m_idleConnections.empty())
                                {
                                    result = m_idleConnections.front();
                                    m_idleConnections.pop();
                                    CP_DEBUG("borrow - GOT IDLE after timeout: active=%d, idle=%zu",
                                             m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size());
                                    break;
                                }
                                CP_DEBUG("borrow - TIMEOUT: active=%d, idle=%zu, total=%zu, pending=%zu",
                                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size(), m_pendingCreations);
                                return cpp_dbc::unexpected(DBException("8B0E04F03A6D", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
                            }

                            // Spurious/non-fulfilled wakeup — check if idle connection appeared
                            if (!m_idleConnections.empty())
                            {
                                std::erase(m_waitQueue, &request);
                                result = m_idleConnections.front();
                                m_idleConnections.pop();
                                CP_DEBUG("borrow - GOT IDLE after wakeup: active=%d, idle=%zu",
                                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size());
                                break;
                            }

                            // Nothing available — stay in queue at same FIFO position, re-wait.
                            CP_DEBUG("borrow - CV WAKEUP (re-waiting): idle=%zu, total=%zu",
                                     m_idleConnections.size(), m_allConnections.size());
                        } // End inner wait loop
                    }

                    // Mark active UNDER lock (prevents maintenance from stealing)
                    // Skip for handoff — returner already set active + incremented counter
                    if (!handedOff)
                    {
                        result->setActive(std::nothrow, true);
                        m_activeConnections++;
                    }
                    result->m_closed.store(false); // Defensive: always ensure not-closed
                } // lockPool released

                // ========================================
                // Phase 2: Validate OUTSIDE lock (I/O)
                // HikariCP pattern: skip validation for recently-used connections.
                // ========================================
                if (m_testOnBorrow)
                {
                    auto timeSinceLastUse = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::steady_clock::now() - result->getLastUsedTime(std::nothrow))
                                                .count();

                    if (timeSinceLastUse > m_validationTimeoutMillis &&
                        !validateConnection(std::nothrow, result->getUnderlyingKVConnection()).value_or(false))
                    {
                        CP_DEBUG("borrow - VALIDATION FAILED: removing connection, active=%d", m_activeConnections.load(std::memory_order_acquire));
                        // Validation failed — undo active marking, remove from pool
                        {
                            std::scoped_lock lockPool(m_mutexPool);
                            result->setActive(std::nothrow, false);
                            result->m_closed.store(true);
                            m_activeConnections--;

                            auto it = std::ranges::find(m_allConnections, result);
                            if (it != m_allConnections.end())
                            {
                                m_allConnections.erase(it);
                            }
                            // Notify inside lock — freed slot allows waiting threads to create
                            m_connectionAvailable.notify_one();
                        }

                        // Close invalid connection outside lock
                        auto closeResult = result->getUnderlyingKVConnection()->close(std::nothrow);
                        if (!closeResult.has_value())
                        {
                            CP_DEBUG("borrow - close() on invalid connection failed: %s", closeResult.error().what_s().c_str());
                        }

                        // Check if we still have time to retry
                        if (std::chrono::steady_clock::now() >= deadline)
                        {
                            return cpp_dbc::unexpected(DBException("8B0E04F03A6D", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
                        }

                        continue; // Retry with new candidate
                    }
                }

                return cpp_dbc::expected<std::shared_ptr<KVDBConnection>, DBException>{
                    std::static_pointer_cast<KVDBConnection>(result)};
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("IR1KBH287UXF",
                                                   std::string("Exception in getKVDBConnection: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR
        {
            return cpp_dbc::unexpected(DBException("0H1V112W40SO",
                                                   "Unknown exception in getKVDBConnection",
                                                   system_utils::captureCallStack()));
        }
    }

    std::shared_ptr<KVDBConnection> KVDBConnectionPool::getKVDBConnection()
    {
        auto result = getKVDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> KVDBConnectionPool::getDBConnection(std::nothrow_t) noexcept
    {
        auto result = getKVDBConnection(std::nothrow);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        return std::static_pointer_cast<DBConnection>(result.value());
    }

    std::shared_ptr<DBConnection> KVDBConnectionPool::getDBConnection()
    {
        auto result = getDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<size_t, DBException> KVDBConnectionPool::getActiveDBConnectionCount(std::nothrow_t) const noexcept
    {
        return static_cast<size_t>(m_activeConnections.load(std::memory_order_acquire));
    }

    size_t KVDBConnectionPool::getActiveDBConnectionCount() const
    {
        auto result = getActiveDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<size_t, DBException> KVDBConnectionPool::getIdleDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_idleConnections.size();
    }

    size_t KVDBConnectionPool::getIdleDBConnectionCount() const
    {
        auto result = getIdleDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<size_t, DBException> KVDBConnectionPool::getTotalDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_allConnections.size();
    }

    size_t KVDBConnectionPool::getTotalDBConnectionCount() const
    {
        auto result = getTotalDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<void, DBException> KVDBConnectionPool::close(std::nothrow_t) noexcept
    {
        CP_DEBUG("KVDBConnectionPool::close - Starting close operation");

        if (!m_running.exchange(false))
        {
            CP_DEBUG("KVDBConnectionPool::close - Already closed, returning");
            return {}; // Already closed — idempotent
        }

        // Mark pool as no longer alive - this prevents pooled connections from trying to return
        if (m_poolAlive)
        {
            m_poolAlive->store(false);
        }

        CP_DEBUG("KVDBConnectionPool::close - Waiting for active operations to complete at %lld",
                 (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete (with timeout)
        {
            auto waitStart = std::chrono::steady_clock::now();
            CP_DEBUG("KVDBConnectionPool::close - Initial active connections: %d", m_activeConnections.load(std::memory_order_acquire));

            while (m_activeConnections.load(std::memory_order_acquire) > 0)
            {
                CP_DEBUG("KVDBConnectionPool::close - Waiting for %d active connections to finish at %lld",
                         m_activeConnections.load(std::memory_order_acquire), (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 1 == 0)
                {
                    CP_DEBUG("KVDBConnectionPool::close - Waited %ld seconds for active connections", elapsed_seconds);
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("KVDBConnectionPool::close - Timeout waiting for active connections, forcing close at %lld",
                             (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
                    m_activeConnections.store(0);
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        // Notify all waiting threads to wake up and exit.
        {
            std::scoped_lock lock(m_mutexPool);
            m_maintenanceCondition.notify_all();
            m_connectionAvailable.notify_all();
        }

        if (m_maintenanceThread.joinable())
        {
            m_maintenanceThread.join();
        }

        // CRITICAL: Collect connections under pool locks, then close OUTSIDE pool locks.
        std::vector<std::shared_ptr<KVPooledDBConnection>> connectionsToClose;
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
                    CP_DEBUG("KVDBConnectionPool::close - connection close failed: %s", r.error().what_s().c_str());
                    lastError = r;
                }
            }
        }

        return lastError;
    }

    void KVDBConnectionPool::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<bool, DBException> KVDBConnectionPool::isRunning(std::nothrow_t) const noexcept
    {
        return m_running.load(std::memory_order_acquire);
    }

    bool KVDBConnectionPool::isRunning() const
    {
        auto result = isRunning(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    // KVPooledDBConnection implementation

    KVPooledDBConnection::KVPooledDBConnection(std::shared_ptr<KVDBConnection> conn,
                                               std::weak_ptr<KVDBConnectionPool> pool,
                                               std::shared_ptr<std::atomic<bool>> poolAlive)
        : m_conn(conn),
          m_pool(pool),
          m_poolAlive(poolAlive)
    {
        // m_creationTime, m_lastUsedTimeNs, m_active, m_closed use in-class initializers
    }

    KVPooledDBConnection::~KVPooledDBConnection()
    {
        if (!m_closed.load(std::memory_order_acquire) && m_conn)
        {
            try
            {
                // Check pool validity without virtual call (S1699)
                bool poolValid = m_poolAlive && m_poolAlive->load(std::memory_order_acquire) && !m_pool.expired();

                // If the pool is no longer alive, close the physical connection
                if (!poolValid)
                {
                    auto closeResult = m_conn->close(std::nothrow);
                    if (!closeResult.has_value())
                    {
                        CP_DEBUG("KVPooledDBConnection::~KVPooledDBConnection - close failed: %s", closeResult.error().what_s().c_str());
                    }
                }
                else
                {
                    // Return to pool - inline the logic to avoid virtual call (S1699)
                    // See close() method for detailed documentation of the race condition fix
                    bool expected = false;
                    if (m_closed.compare_exchange_strong(expected, true))
                    {
                        updateLastUsedTime(std::nothrow);

                        if (auto poolShared = m_pool.lock())
                        {
                            // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                            // (see close() method for full explanation)
                            m_closed.store(false);
                            auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<KVPooledDBConnection>(shared_from_this()));
                            if (!returnResult.has_value())
                            {
                                CP_DEBUG("KVPooledDBConnection::~KVPooledDBConnection - returnConnection failed: %s", returnResult.error().what_s().c_str());
                            }
                        }
                    }
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVPooledDBConnection::~KVPooledDBConnection - Exception: %s", ex.what());
                m_closed.store(true);
            }
            catch (...) // NOSONAR - Catch-all to ensure no exception escapes destructor
            {
                CP_DEBUG("KVPooledDBConnection::~KVPooledDBConnection - Unknown exception caught");
                m_closed.store(true);
            }
        }
    }

    bool KVPooledDBConnection::isPoolValid(std::nothrow_t) const noexcept
    {
        return m_poolAlive && m_poolAlive->load(std::memory_order_acquire) && !m_pool.expired();
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
                        m_closed.store(false);
                        auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<KVPooledDBConnection>(shared_from_this()));
                        if (!returnResult.has_value())
                        {
                            CP_DEBUG("KVPooledDBConnection::close - returnConnection failed: %s", returnResult.error().what_s().c_str());
                        }
                    }
                }
                else if (m_conn)
                {
                    // If pool is invalid, actually close the connection
                    auto closeResult = m_conn->close(std::nothrow);
                    if (!closeResult.has_value())
                    {
                        CP_DEBUG("KVPooledDBConnection::close - underlying close failed: %s", closeResult.error().what_s().c_str());
                    }
                }
            }
            catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
            {
                // shared_from_this failed, just close the connection
                CP_DEBUG("KVPooledDBConnection::close - shared_from_this failed: %s", ex.what());
                m_closed.store(true);
                if (m_conn)
                {
                    auto closeResult = m_conn->close(std::nothrow);
                    if (!closeResult.has_value())
                    {
                        CP_DEBUG("KVPooledDBConnection::close - underlying close failed: %s", closeResult.error().what_s().c_str());
                    }
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                // Any other exception, just close the connection
                CP_DEBUG("KVPooledDBConnection::close - Exception: %s", ex.what());
                m_closed.store(true);
                if (m_conn)
                {
                    auto closeResult = m_conn->close(std::nothrow);
                    if (!closeResult.has_value())
                    {
                        CP_DEBUG("KVPooledDBConnection::close - underlying close failed: %s", closeResult.error().what_s().c_str());
                    }
                }
            }
            catch (...) // NOSONAR - Intentional catch-all for non-std::exception types during cleanup
            {
                // Unknown exception, just close the connection
                CP_DEBUG("KVPooledDBConnection::close - Unknown exception");
                m_closed.store(true);
                if (m_conn)
                {
                    auto closeResult = m_conn->close(std::nothrow);
                    if (!closeResult.has_value())
                    {
                        CP_DEBUG("KVPooledDBConnection::close - underlying close failed: %s", closeResult.error().what_s().c_str());
                    }
                }
            }
        }
        return {};
    }

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

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return true;
        }
        if (!m_conn)
        {
            return false;
        }
        auto result = m_conn->isClosed(std::nothrow);
        if (!result.has_value())
        {
            return result;
        }
        return result.value();
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
            if (isPoolValid(std::nothrow))
            {
                // Try to obtain a shared_ptr from the weak_ptr
                if (auto poolShared = m_pool.lock())
                {
                    // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                    // (see close(nothrow) method for full explanation of the bug)
                    m_closed.store(false);
                    auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<KVPooledDBConnection>(this->shared_from_this()));
                    if (!returnResult.has_value())
                    {
                        CP_DEBUG("KVPooledDBConnection::returnToPool - returnConnection failed: %s", returnResult.error().what_s().c_str());
                    }
                }
            }
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("KVPooledDBConnection::returnToPool - shared_from_this failed: %s", ex.what());
            m_closed.store(true);
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            CP_DEBUG("KVPooledDBConnection::returnToPool - Exception: %s", ex.what());
            m_closed.store(true);
        }
        catch (...) // NOSONAR - Catch-all to ensure m_closed is always set correctly on any exception
        {
            CP_DEBUG("KVPooledDBConnection::returnToPool - Unknown exception caught");
            m_closed.store(true);
        }
        return {};
    }

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
        return true;
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return true;
    }

    std::string KVPooledDBConnection::getURL() const
    {
        if (m_conn)
        {
            return m_conn->getURL();
        }
        return "";
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::getURL(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("WDBWCPX3AUDK", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    void KVPooledDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<void, DBException> KVPooledDBConnection::reset(std::nothrow_t) noexcept
    {
        return prepareForPoolReturn(std::nothrow);
    }

    void KVPooledDBConnection::prepareForPoolReturn()
    {
        auto result = prepareForPoolReturn(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<void, DBException> KVPooledDBConnection::prepareForPoolReturn(std::nothrow_t) noexcept
    {
        return m_conn->prepareForPoolReturn(std::nothrow);
    }

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

    std::string KVPooledDBConnection::ping()
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->ping();
    }

    std::map<std::string, std::string> KVPooledDBConnection::getServerInfo()
    {
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerInfo();
    }

    std::chrono::time_point<std::chrono::steady_clock> KVPooledDBConnection::getCreationTime(std::nothrow_t) const noexcept
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> KVPooledDBConnection::getLastUsedTime(std::nothrow_t) const noexcept
    {
        return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{m_lastUsedTimeNs.load(std::memory_order_relaxed)}};
    }

    cpp_dbc::expected<void, DBException> KVPooledDBConnection::setActive(std::nothrow_t, bool active) noexcept
    {
        m_active.store(active, std::memory_order_release);
        return {};
    }

    bool KVPooledDBConnection::isActive(std::nothrow_t) const noexcept
    {
        return m_active.load(std::memory_order_acquire);
    }

    std::shared_ptr<DBConnection> KVPooledDBConnection::getUnderlyingConnection(std::nothrow_t) noexcept
    {
        return m_conn;
    }

    std::shared_ptr<KVDBConnection> KVPooledDBConnection::getUnderlyingKVConnection()
    {
        return m_conn;
    }

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

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::ping(std::nothrow_t) noexcept
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

    // RedisConnectionPool implementation

    namespace Redis
    {
        RedisConnectionPool::RedisConnectionPool(DBConnectionPool::ConstructorTag,
                                                 const std::string &url,
                                                 const std::string &username,
                                                 const std::string &password)
            : KVDBConnectionPool(DBConnectionPool::ConstructorTag{}, url, username, password, {}, 5, 20, 3, 5000, 5000, 300000, 1800000, true, false, "PING")
        {
        }

        RedisConnectionPool::RedisConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
            : KVDBConnectionPool(DBConnectionPool::ConstructorTag{},
                                 config.getUrl(),
                                 config.getUsername(),
                                 config.getPassword(),
                                 config.getOptions(),
                                 config.getInitialSize(),
                                 config.getMaxSize(),
                                 config.getMinIdle(),
                                 config.getConnectionTimeout(),
                                 config.getValidationInterval(),
                                 config.getIdleTimeout(),
                                 config.getMaxLifetimeMillis(),
                                 config.getTestOnBorrow(),
                                 config.getTestOnReturn(),
                                 config.getValidationQuery().empty() || config.getValidationQuery() == "SELECT 1" ? "PING" : config.getValidationQuery(),
                                 config.getTransactionIsolation())
        {
            // Redis-specific initialization if needed
        }

        cpp_dbc::expected<std::shared_ptr<RedisConnectionPool>, DBException> RedisConnectionPool::create(std::nothrow_t,
                                                                                                         const std::string &url,
                                                                                                         const std::string &username,
                                                                                                         const std::string &password) noexcept
        {
            try
            {
                auto pool = std::make_shared<RedisConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
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

        cpp_dbc::expected<std::shared_ptr<RedisConnectionPool>, DBException> RedisConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
        {
            try
            {
                auto pool = std::make_shared<RedisConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
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
    }

} // namespace cpp_dbc
