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

        KVDBConnectionPool::close();

        CP_DEBUG("KVDBConnectionPool::~KVDBConnectionPool - Destructor completed at %lld",
                 (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    std::shared_ptr<KVDBConnectionPool> KVDBConnectionPool::create(const std::string &url,
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
    {
        auto pool = std::make_shared<KVDBConnectionPool>(
            DBConnectionPool::ConstructorTag{}, url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis,
            maxLifetimeMillis, testOnBorrow, testOnReturn, validationQuery,
            transactionIsolation);

        pool->initializePool();
        return pool;
    }

    std::shared_ptr<KVDBConnectionPool> KVDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto pool = std::make_shared<KVDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
        pool->initializePool();
        return pool;
    }

    void KVDBConnectionPool::initializePool()
    {
        try
        {
            // Reserve space for connections
            m_allConnections.reserve(m_maxSize);

            // Create initial connections
            for (int i = 0; i < m_initialSize; i++)
            {
                auto pooledConn = createPooledDBConnection();

                // Add to idle connections and all connections lists under pool lock
                {
                    std::scoped_lock lock(m_mutexPool);
                    m_idleConnections.push(pooledConn);
                    m_allConnections.push_back(pooledConn);
                }
            }

            // Start maintenance thread (using std::jthread)
            m_maintenanceThread = std::jthread([this] { maintenanceTask(); });
        }
        catch (const std::exception &ex)
        {
            close();
            throw DBException("48FAB065D52D", "Failed to initialize connection pool: " + std::string(ex.what()), system_utils::captureCallStack());
        }
    }

    std::shared_ptr<KVDBConnection> KVDBConnectionPool::createDBConnection()
    {
        // Use the DriverManager to get a connection
        auto dbConn = DriverManager::getDBConnection(m_url, m_username, m_password, m_options);

        // Cast to KVDBConnection
        auto kvConn = std::dynamic_pointer_cast<KVDBConnection>(dbConn);
        if (!kvConn)
        {
            throw DBException("96CAD6FDCDD9", "Connection pool only supports key-value database connections", system_utils::captureCallStack());
        }

        return kvConn;
    }

    std::shared_ptr<KVPooledDBConnection> KVDBConnectionPool::createPooledDBConnection()
    {
        auto conn = createDBConnection();

        // Wrap shared_from_this() in try/catch: pool may not yet be managed by shared_ptr
        std::weak_ptr<KVDBConnectionPool> weakPool;
        try
        {
            weakPool = shared_from_this();
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("KVDBConnectionPool::createPooledDBConnection - bad_weak_ptr: %s", ex.what());
        }

        return std::make_shared<KVPooledDBConnection>(conn, weakPool, m_poolAlive);
    }

    bool KVDBConnectionPool::validateConnection(std::shared_ptr<KVDBConnection> conn) const
    {
        if (!conn)
        {
            return false;
        }

        if (conn->isClosed())
        {
            return false;
        }

        try
        {
            // Use ping for validation
            std::string response = conn->ping();
            return !response.empty();
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            CP_DEBUG("KVDBConnectionPool::validateConnection - Exception: %s", ex.what());
            return false;
        }
    }

    void KVDBConnectionPool::returnConnection(std::shared_ptr<KVPooledDBConnection> conn)
    {
        if (!conn)
        {
            return;
        }

        if (!m_running.load())
        {
            // Pool is closing — decrement counter so close() doesn't wait forever
            conn->setActive(false);
            m_activeConnections--;
            try
            {
                conn->getUnderlyingKVConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVDBConnectionPool::returnConnection - Exception during close: %s", ex.what());
            }
            return;
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive())
        {
            CP_DEBUG("returnConnection - SKIPPED: connection already inactive");
            return;
        }

        // Additional safety check: verify connection is in allConnections
        {
            std::scoped_lock lockPool(m_mutexPool);
            auto it = std::ranges::find(m_allConnections, conn);
            if (it == m_allConnections.end())
            {
                conn->setActive(false);
                m_activeConnections--;
                CP_DEBUG("returnConnection - ORPHAN: connection not in allConnections, active=%d", m_activeConnections.load());
                m_connectionAvailable.notify_one(); // Inside lock — freed slot
                return;
            }
        }

        // ========================================
        // Phase 1: I/O OUTSIDE any lock
        // Connection is marked active=true → maintenance skips it, borrowers can't see it.
        // ========================================
        bool valid = true;

        if (m_testOnReturn)
        {
            valid = validateConnection(conn->getUnderlyingKVConnection());
        }

        if (valid)
        {
            try
            {
                if (!conn->m_closed)
                {
                    auto resetResult = conn->getUnderlyingKVConnection()->prepareForPoolReturn(std::nothrow);
                    if (!resetResult)
                    {
                        CP_DEBUG("KVDBConnectionPool::returnConnection - prepareForPoolReturn failed: %s", resetResult.error().what_s().c_str());
                        valid = false;
                    }
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVDBConnectionPool::returnConnection - Exception in prepareForPoolReturn: %s", ex.what());
                valid = false;
            }
        }

        // ========================================
        // Phase 2: Pool state under m_mutexPool only (NO I/O)
        // ========================================
        if (valid)
        {
            conn->updateLastUsedTime(); // Mark return time for validation skip
            std::scoped_lock lockPool(m_mutexPool);
            conn->setActive(false);
            m_activeConnections--;

            // Direct handoff: if threads are waiting, give the connection directly
            // to the first waiter instead of pushing to idle.
            if (!m_waitQueue.empty())
            {
                auto *request = m_waitQueue.front();
                m_waitQueue.pop_front();
                request->conn = conn;
                request->fulfilled = true;
                conn->setActive(true);
                m_activeConnections++;
                CP_DEBUG("returnConnection - HANDOFF: direct to waiter, active=%d, waiters=%zu, total=%zu",
                         m_activeConnections.load(), m_waitQueue.size(), m_allConnections.size());
                m_connectionAvailable.notify_all(); // Wake all so fulfilled waiter checks its flag
            }
            else
            {
                m_idleConnections.push(conn);
                CP_DEBUG("returnConnection - VALID: pushed to idle, active=%d, idle=%zu, total=%zu",
                         m_activeConnections.load(), m_idleConnections.size(), m_allConnections.size());
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
                conn->setActive(false);
                conn->m_closed.store(true);
                m_activeConnections--;
                CP_DEBUG("returnConnection - INVALID: active=%d", m_activeConnections.load());

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
            try
            {
                conn->getUnderlyingKVConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVDBConnectionPool::returnConnection - Exception closing invalid connection: %s", ex.what());
            }

            // Step 3: Create replacement OUTSIDE locks (I/O)
            if (!m_running.load())
            {
                return; // Pool closing, skip replacement
            }

            std::shared_ptr<KVPooledDBConnection> replacement;
            try
            {
                replacement = createPooledDBConnection();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVDBConnectionPool::returnConnection - Exception creating replacement: %s", ex.what());
            }

            // Step 4: Insert replacement under lock with direct handoff
            if (replacement)
            {
                std::shared_ptr<KVPooledDBConnection> discardReplacement;
                {
                    std::scoped_lock lockPool(m_mutexPool);
                    if (m_running.load() && m_allConnections.size() < m_maxSize)
                    {
                        m_allConnections.push_back(replacement);

                        // Direct handoff for replacement if waiters exist
                        if (!m_waitQueue.empty())
                        {
                            auto *request = m_waitQueue.front();
                            m_waitQueue.pop_front();
                            request->conn = replacement;
                            request->fulfilled = true;
                            replacement->setActive(true);
                            m_activeConnections++;
                            CP_DEBUG("returnConnection - REPLACEMENT HANDOFF: active=%d, waiters=%zu, total=%zu",
                                     m_activeConnections.load(), m_waitQueue.size(), m_allConnections.size());
                            m_connectionAvailable.notify_all();
                        }
                        else
                        {
                            m_idleConnections.push(replacement);
                            CP_DEBUG("returnConnection - REPLACEMENT: pushed to idle, active=%d, idle=%zu, total=%zu",
                                     m_activeConnections.load(), m_idleConnections.size(), m_allConnections.size());
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
                    try { discardReplacement->getUnderlyingKVConnection()->close(); } catch (...) {}
                }
            }
        }
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
                                                { return !m_running; });
            }
            // m_mutexPool is now released. Maintenance runs without holding it.

            if (!m_running)
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
                    if (pooledConn->isActive())
                    {
                        ++it;
                        continue;
                    }

                    // Check if the connection has been idle for too long
                    auto idleTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - pooledConn->getLastUsedTime())
                                        .count();

                    // Check if the connection has lived for too long
                    auto lifeTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - pooledConn->getCreationTime())
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
            while (m_running && currentTotal < m_minIdle)
            {
                try
                {
                    auto pooledConn = createPooledDBConnection();
                    std::scoped_lock lock(m_mutexPool);
                    m_idleConnections.push(pooledConn);
                    m_allConnections.push_back(pooledConn);
                    m_connectionAvailable.notify_one(); // Wake waiting borrower if any
                    currentTotal = m_allConnections.size();
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    CP_DEBUG("KVDBConnectionPool::maintenanceTask - Exception creating minIdle connection: %s", ex.what());
                    break;
                }
            }
        } while (m_running);
    }

    std::shared_ptr<DBConnection> KVDBConnectionPool::getDBConnection()
    {
        return getKVDBConnection();
    }

    std::shared_ptr<KVDBConnection> KVDBConnectionPool::getKVDBConnection()
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

                if (!m_running.load())
                {
                    throw DBException("BAB0896A213B", "Connection pool is closed", system_utils::captureCallStack());
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
                                 m_activeConnections.load(), m_idleConnections.size(), m_allConnections.size());
                        break;
                    }

                    // Step 2: Try creating if pool has room
                    if (m_allConnections.size() + m_pendingCreations < m_maxSize)
                    {
                        CP_DEBUG("borrow - CREATING: total=%zu, pending=%zu, max=%zu",
                                 m_allConnections.size(), m_pendingCreations, m_maxSize);
                        m_pendingCreations++;
                        lockPool.unlock();
                        try
                        {
                            auto candidate = createPooledDBConnection();
                            lockPool.lock();
                            m_pendingCreations--;

                            // Recheck size under lock (another thread may have filled the pool)
                            if (m_allConnections.size() < m_maxSize)
                            {
                                m_allConnections.push_back(candidate);
                                result = candidate;
                            }
                            else if (candidate)
                            {
                                // Pool became full — discard candidate
                                CP_DEBUG("borrow - DISCARD candidate: total=%zu >= max=%zu", m_allConnections.size(), m_maxSize);
                                candidate->m_closed.store(true); // Prevent destructor returnToPool()
                                lockPool.unlock();
                                try { candidate->getUnderlyingKVConnection()->close(); } catch (...) {}
                                lockPool.lock();
                            }
                        }
                        catch (...)
                        {
                            lockPool.lock();
                            m_pendingCreations--;
                        }
                        continue; // ALWAYS re-check idle after any unlock/lock cycle
                    }

                    // Step 3: Nothing available — wait on CV with direct handoff request.
                    ConnectionRequest request;
                    m_waitQueue.push_back(&request);
                    CP_DEBUG("borrow - WAITING on CV: active=%d, idle=%zu, total=%zu, pending=%zu, waiters=%zu",
                             m_activeConnections.load(), m_idleConnections.size(), m_allConnections.size(),
                             m_pendingCreations, m_waitQueue.size());

                    while (true) // Inner wait loop — request stays in queue at FIFO position
                    {
                        std::cv_status status;
                        try
                        {
                            status = m_connectionAvailable.wait_until(lockPool, deadline);
                        }
                        catch (...)
                        {
                            std::erase(m_waitQueue, &request);
                            throw;
                        }

                        // Check if we received a direct handoff
                        if (request.fulfilled)
                        {
                            result = request.conn;
                            handedOff = true;
                            CP_DEBUG("borrow - HANDOFF received: active=%d, idle=%zu, total=%zu",
                                     m_activeConnections.load(), m_idleConnections.size(), m_allConnections.size());
                            break; // Exit inner loop; outer loop also exits (result is set)
                        }

                        // Not fulfilled — check termination conditions (stay in queue)
                        if (!m_running.load())
                        {
                            std::erase(m_waitQueue, &request);
                            throw DBException("9C4A03BF221C", "Connection pool is closed", system_utils::captureCallStack());
                        }

                        if (status == std::cv_status::timeout)
                        {
                            std::erase(m_waitQueue, &request);
                            // One last idle check before throwing
                            if (!m_idleConnections.empty())
                            {
                                result = m_idleConnections.front();
                                m_idleConnections.pop();
                                CP_DEBUG("borrow - GOT IDLE after timeout: active=%d, idle=%zu",
                                         m_activeConnections.load(), m_idleConnections.size());
                                break;
                            }
                            CP_DEBUG("borrow - TIMEOUT: active=%d, idle=%zu, total=%zu, pending=%zu",
                                     m_activeConnections.load(), m_idleConnections.size(), m_allConnections.size(), m_pendingCreations);
                            throw DBException("8B0E04F03A6D", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                        }

                        // Spurious/non-fulfilled wakeup — check if idle connection appeared
                        if (!m_idleConnections.empty())
                        {
                            std::erase(m_waitQueue, &request);
                            result = m_idleConnections.front();
                            m_idleConnections.pop();
                            CP_DEBUG("borrow - GOT IDLE after wakeup: active=%d, idle=%zu",
                                     m_activeConnections.load(), m_idleConnections.size());
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
                    result->setActive(true);
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
                    std::chrono::steady_clock::now() - result->getLastUsedTime()).count();

                if (timeSinceLastUse > m_validationTimeoutMillis &&
                    !validateConnection(result->getUnderlyingKVConnection()))
                {
                    CP_DEBUG("borrow - VALIDATION FAILED: removing connection, active=%d", m_activeConnections.load());
                    // Validation failed — undo active marking, remove from pool
                    {
                        std::scoped_lock lockPool(m_mutexPool);
                        result->setActive(false);
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
                    try { result->getUnderlyingKVConnection()->close(); } catch (...) {}

                    // Check if we still have time to retry
                    if (std::chrono::steady_clock::now() >= deadline)
                    {
                        throw DBException("8B0E04F03A6D", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                    }

                    continue; // Retry with new candidate
                }
            }

            return result;
        }
    }

    size_t KVDBConnectionPool::getActiveDBConnectionCount() const
    {
        return m_activeConnections;
    }

    size_t KVDBConnectionPool::getIdleDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexPool);
        return m_idleConnections.size();
    }

    size_t KVDBConnectionPool::getTotalDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexPool);
        return m_allConnections.size();
    }

    void KVDBConnectionPool::close()
    {
        CP_DEBUG("KVDBConnectionPool::close - Starting close operation");

        if (!m_running.exchange(false))
        {
            CP_DEBUG("KVDBConnectionPool::close - Already closed, returning");
            // Already closed
            return;
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
            CP_DEBUG("KVDBConnectionPool::close - Initial active connections: %d", m_activeConnections.load());

            while (m_activeConnections.load() > 0)
            {
                CP_DEBUG("KVDBConnectionPool::close - Waiting for %d active connections to finish at %lld",
                         m_activeConnections.load(), (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

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
                    // Force active connections to be marked as inactive
                    m_activeConnections.store(0);
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        // Notify all waiting threads to wake up and exit.
        // Both CVs use m_mutexPool.
        {
            std::scoped_lock lock(m_mutexPool);
            m_maintenanceCondition.notify_all();
            m_connectionAvailable.notify_all();
        }

        // Join maintenance thread (only if it was started)
        if (m_maintenanceThread.joinable())
        {
            m_maintenanceThread.join();
        }

        // Close all connections
        // CRITICAL: Collect connections under pool locks, then close OUTSIDE pool locks.
        // Closing a connection acquires per-connection mutex (m_connMutex).
        // initializePool() establishes order: m_connMutex FIRST, then pool locks.
        // Closing inside pool locks would invert that order → Helgrind LockOrder violation.
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

        // Close each connection outside pool locks (per-connection mutex acquired here)
        for (const auto &conn : connectionsToClose)
        {
            try
            {
                if (conn && conn->getUnderlyingConnection())
                {
                    conn->setActive(false);
                    conn->getUnderlyingConnection()->close();
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVDBConnectionPool::close - Exception closing connection: %s", ex.what());
            }
        }
    }

    bool KVDBConnectionPool::isRunning() const
    {
        return m_running;
    }

    // KVPooledDBConnection implementation

    KVPooledDBConnection::KVPooledDBConnection(std::shared_ptr<KVDBConnection> conn,
                                               std::weak_ptr<KVDBConnectionPool> pool,
                                               std::shared_ptr<std::atomic<bool>> poolAlive)
        : m_conn(conn),
          m_pool(pool),
          m_poolAlive(poolAlive)
    {
        // m_creationTime, m_lastUsedTime, m_active, m_closed use in-class initializers
    }

    KVPooledDBConnection::~KVPooledDBConnection()
    {
        if (!m_closed && m_conn)
        {
            try
            {
                // Check pool validity without virtual call (S1699)
                bool poolValid = m_poolAlive && *m_poolAlive && !m_pool.expired();

                // If the pool is no longer alive, close the physical connection
                if (!poolValid)
                {
                    m_conn->close();
                }
                else
                {
                    // Return to pool - inline the logic to avoid virtual call (S1699)
                    // See close() method for detailed documentation of the race condition fix
                    bool expected = false;
                    if (m_closed.compare_exchange_strong(expected, true))
                    {
                        updateLastUsedTime();

                        if (auto poolShared = m_pool.lock())
                        {
                            // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                            // (see close() method for full explanation)
                            m_closed.store(false);
                            poolShared->returnConnection(std::static_pointer_cast<KVPooledDBConnection>(shared_from_this()));
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

    bool KVPooledDBConnection::isPoolValid() const
    {
        return m_poolAlive && *m_poolAlive && !m_pool.expired();
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
                updateLastUsedTime();

                // Check if pool is still alive using the shared atomic flag
                if (isPoolValid())
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
                        poolShared->returnConnection(std::static_pointer_cast<KVPooledDBConnection>(shared_from_this()));
                    }
                }
                else if (m_conn)
                {
                    // If pool is invalid, actually close the connection
                    m_conn->close();
                }
            }
            catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
            {
                // shared_from_this failed, just close the connection
                CP_DEBUG("KVPooledDBConnection::close - shared_from_this failed: %s", ex.what());
                m_closed.store(true);
                if (m_conn)
                {
                    m_conn->close();
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                // Any other exception, just close the connection
                CP_DEBUG("KVPooledDBConnection::close - Exception: %s", ex.what());
                m_closed.store(true);
                if (m_conn)
                {
                    m_conn->close();
                }
            }
            catch (...) // NOSONAR - Intentional catch-all for non-std::exception types during cleanup
            {
                // Unknown exception, just close the connection
                CP_DEBUG("KVPooledDBConnection::close - Unknown exception");
                m_closed.store(true);
                if (m_conn)
                {
                    m_conn->close();
                }
            }
        }
        return {};
    }

    void KVPooledDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result)
            throw result.error();
    }

    bool KVPooledDBConnection::isClosed() const
    {
        return m_closed || (m_conn && m_conn->isClosed());
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load() || (m_conn && m_conn->isClosed());
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
            updateLastUsedTime();

            // Check if pool is still alive using the shared atomic flag
            if (isPoolValid())
            {
                // Try to obtain a shared_ptr from the weak_ptr
                if (auto poolShared = m_pool.lock())
                {
                    // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                    // (see close(nothrow) method for full explanation of the bug)
                    m_closed.store(false);
                    poolShared->returnConnection(std::static_pointer_cast<KVPooledDBConnection>(this->shared_from_this()));
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
        if (!result)
            throw result.error();
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
        if (m_closed)
        {
            return cpp_dbc::unexpected(DBException("WDBWCPX3AUDK", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    void KVPooledDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result)
            throw result.error();
    }

    cpp_dbc::expected<void, DBException> KVPooledDBConnection::reset(std::nothrow_t) noexcept
    {
        return prepareForPoolReturn(std::nothrow);
    }

    void KVPooledDBConnection::prepareForPoolReturn()
    {
        auto result = prepareForPoolReturn(std::nothrow);
        if (!result)
            throw result.error();
    }

    cpp_dbc::expected<void, DBException> KVPooledDBConnection::prepareForPoolReturn(std::nothrow_t) noexcept
    {
        return m_conn->prepareForPoolReturn(std::nothrow);
    }

    // Forward all KVDBConnection methods to underlying connection

    bool KVPooledDBConnection::setString(const std::string &key, const std::string &value,
                                         std::optional<int64_t> expirySeconds)
    {
        updateLastUsedTime();
        return m_conn->setString(key, value, expirySeconds);
    }

    std::string KVPooledDBConnection::getString(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->getString(key);
    }

    bool KVPooledDBConnection::exists(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->exists(key);
    }

    bool KVPooledDBConnection::deleteKey(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->deleteKey(key);
    }

    int64_t KVPooledDBConnection::deleteKeys(const std::vector<std::string> &keys)
    {
        updateLastUsedTime();
        return m_conn->deleteKeys(keys);
    }

    bool KVPooledDBConnection::expire(const std::string &key, int64_t seconds)
    {
        updateLastUsedTime();
        return m_conn->expire(key, seconds);
    }

    int64_t KVPooledDBConnection::getTTL(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->getTTL(key);
    }

    int64_t KVPooledDBConnection::increment(const std::string &key, int64_t by)
    {
        updateLastUsedTime();
        return m_conn->increment(key, by);
    }

    int64_t KVPooledDBConnection::decrement(const std::string &key, int64_t by)
    {
        updateLastUsedTime();
        return m_conn->decrement(key, by);
    }

    int64_t KVPooledDBConnection::listPushLeft(const std::string &key, const std::string &value)
    {
        updateLastUsedTime();
        return m_conn->listPushLeft(key, value);
    }

    int64_t KVPooledDBConnection::listPushRight(const std::string &key, const std::string &value)
    {
        updateLastUsedTime();
        return m_conn->listPushRight(key, value);
    }

    std::string KVPooledDBConnection::listPopLeft(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->listPopLeft(key);
    }

    std::string KVPooledDBConnection::listPopRight(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->listPopRight(key);
    }

    std::vector<std::string> KVPooledDBConnection::listRange(const std::string &key, int64_t start, int64_t stop)
    {
        updateLastUsedTime();
        return m_conn->listRange(key, start, stop);
    }

    int64_t KVPooledDBConnection::listLength(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->listLength(key);
    }

    bool KVPooledDBConnection::hashSet(const std::string &key, const std::string &field, const std::string &value)
    {
        updateLastUsedTime();
        return m_conn->hashSet(key, field, value);
    }

    std::string KVPooledDBConnection::hashGet(const std::string &key, const std::string &field)
    {
        updateLastUsedTime();
        return m_conn->hashGet(key, field);
    }

    bool KVPooledDBConnection::hashDelete(const std::string &key, const std::string &field)
    {
        updateLastUsedTime();
        return m_conn->hashDelete(key, field);
    }

    bool KVPooledDBConnection::hashExists(const std::string &key, const std::string &field)
    {
        updateLastUsedTime();
        return m_conn->hashExists(key, field);
    }

    std::map<std::string, std::string> KVPooledDBConnection::hashGetAll(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->hashGetAll(key);
    }

    int64_t KVPooledDBConnection::hashLength(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->hashLength(key);
    }

    bool KVPooledDBConnection::setAdd(const std::string &key, const std::string &member)
    {
        updateLastUsedTime();
        return m_conn->setAdd(key, member);
    }

    bool KVPooledDBConnection::setRemove(const std::string &key, const std::string &member)
    {
        updateLastUsedTime();
        return m_conn->setRemove(key, member);
    }

    bool KVPooledDBConnection::setIsMember(const std::string &key, const std::string &member)
    {
        updateLastUsedTime();
        return m_conn->setIsMember(key, member);
    }

    std::vector<std::string> KVPooledDBConnection::setMembers(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->setMembers(key);
    }

    int64_t KVPooledDBConnection::setSize(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->setSize(key);
    }

    bool KVPooledDBConnection::sortedSetAdd(const std::string &key, double score, const std::string &member)
    {
        updateLastUsedTime();
        return m_conn->sortedSetAdd(key, score, member);
    }

    bool KVPooledDBConnection::sortedSetRemove(const std::string &key, const std::string &member)
    {
        updateLastUsedTime();
        return m_conn->sortedSetRemove(key, member);
    }

    std::optional<double> KVPooledDBConnection::sortedSetScore(const std::string &key, const std::string &member)
    {
        updateLastUsedTime();
        return m_conn->sortedSetScore(key, member);
    }

    std::vector<std::string> KVPooledDBConnection::sortedSetRange(const std::string &key, int64_t start, int64_t stop)
    {
        updateLastUsedTime();
        return m_conn->sortedSetRange(key, start, stop);
    }

    int64_t KVPooledDBConnection::sortedSetSize(const std::string &key)
    {
        updateLastUsedTime();
        return m_conn->sortedSetSize(key);
    }

    std::vector<std::string> KVPooledDBConnection::scanKeys(const std::string &pattern, int64_t count)
    {
        updateLastUsedTime();
        return m_conn->scanKeys(pattern, count);
    }

    std::string KVPooledDBConnection::executeCommand(const std::string &command, const std::vector<std::string> &args)
    {
        updateLastUsedTime();
        return m_conn->executeCommand(command, args);
    }

    bool KVPooledDBConnection::flushDB(bool async)
    {
        updateLastUsedTime();
        return m_conn->flushDB(async);
    }

    std::string KVPooledDBConnection::ping()
    {
        updateLastUsedTime();
        return m_conn->ping();
    }

    std::map<std::string, std::string> KVPooledDBConnection::getServerInfo()
    {
        updateLastUsedTime();
        return m_conn->getServerInfo();
    }

    std::chrono::time_point<std::chrono::steady_clock> KVPooledDBConnection::getCreationTime() const
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> KVPooledDBConnection::getLastUsedTime() const
    {
        return m_lastUsedTime.load(std::memory_order_relaxed);
    }

    void KVPooledDBConnection::setActive(bool active)
    {
        m_active = active;
    }

    bool KVPooledDBConnection::isActive() const
    {
        return m_active;
    }

    std::shared_ptr<DBConnection> KVPooledDBConnection::getUnderlyingConnection()
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
        updateLastUsedTime();
        return m_conn->setString(std::nothrow, key, value, expirySeconds);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::getString(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->getString(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::exists(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->exists(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::deleteKey(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->deleteKey(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::deleteKeys(
        std::nothrow_t, const std::vector<std::string> &keys) noexcept
    {
        updateLastUsedTime();
        return m_conn->deleteKeys(std::nothrow, keys);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::expire(
        std::nothrow_t, const std::string &key, int64_t seconds) noexcept
    {
        updateLastUsedTime();
        return m_conn->expire(std::nothrow, key, seconds);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::getTTL(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->getTTL(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::increment(
        std::nothrow_t, const std::string &key, int64_t by) noexcept
    {
        updateLastUsedTime();
        return m_conn->increment(std::nothrow, key, by);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::decrement(
        std::nothrow_t, const std::string &key, int64_t by) noexcept
    {
        updateLastUsedTime();
        return m_conn->decrement(std::nothrow, key, by);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listPushLeft(
        std::nothrow_t, const std::string &key, const std::string &value) noexcept
    {
        updateLastUsedTime();
        return m_conn->listPushLeft(std::nothrow, key, value);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listPushRight(
        std::nothrow_t, const std::string &key, const std::string &value) noexcept
    {
        updateLastUsedTime();
        return m_conn->listPushRight(std::nothrow, key, value);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::listPopLeft(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->listPopLeft(std::nothrow, key);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::listPopRight(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->listPopRight(std::nothrow, key);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::listRange(
        std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
    {
        updateLastUsedTime();
        return m_conn->listRange(std::nothrow, key, start, stop);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->listLength(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashSet(
        std::nothrow_t,
        const std::string &key,
        const std::string &field,
        const std::string &value) noexcept
    {
        updateLastUsedTime();
        return m_conn->hashSet(std::nothrow, key, field, value);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::hashGet(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        updateLastUsedTime();
        return m_conn->hashGet(std::nothrow, key, field);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashDelete(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        updateLastUsedTime();
        return m_conn->hashDelete(std::nothrow, key, field);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashExists(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        updateLastUsedTime();
        return m_conn->hashExists(std::nothrow, key, field);
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> KVPooledDBConnection::hashGetAll(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->hashGetAll(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::hashLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->hashLength(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setAdd(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        updateLastUsedTime();
        return m_conn->setAdd(std::nothrow, key, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        updateLastUsedTime();
        return m_conn->setRemove(std::nothrow, key, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setIsMember(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        updateLastUsedTime();
        return m_conn->setIsMember(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::setMembers(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->setMembers(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::setSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->setSize(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::sortedSetAdd(
        std::nothrow_t, const std::string &key, double score, const std::string &member) noexcept
    {
        updateLastUsedTime();
        return m_conn->sortedSetAdd(std::nothrow, key, score, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::sortedSetRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        updateLastUsedTime();
        return m_conn->sortedSetRemove(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::optional<double>, DBException> KVPooledDBConnection::sortedSetScore(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        updateLastUsedTime();
        return m_conn->sortedSetScore(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::sortedSetRange(
        std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
    {
        updateLastUsedTime();
        return m_conn->sortedSetRange(std::nothrow, key, start, stop);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::sortedSetSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        updateLastUsedTime();
        return m_conn->sortedSetSize(std::nothrow, key);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::scanKeys(
        std::nothrow_t, const std::string &pattern, int64_t count) noexcept
    {
        updateLastUsedTime();
        return m_conn->scanKeys(std::nothrow, pattern, count);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::executeCommand(
        std::nothrow_t,
        const std::string &command,
        const std::vector<std::string> &args) noexcept
    {
        updateLastUsedTime();
        return m_conn->executeCommand(std::nothrow, command, args);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::flushDB(
        std::nothrow_t, bool async) noexcept
    {
        updateLastUsedTime();
        return m_conn->flushDB(std::nothrow, async);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::ping(std::nothrow_t) noexcept
    {
        updateLastUsedTime();
        return m_conn->ping(std::nothrow);
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> KVPooledDBConnection::getServerInfo(
        std::nothrow_t) noexcept
    {
        updateLastUsedTime();
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

        std::shared_ptr<RedisConnectionPool> RedisConnectionPool::create(const std::string &url,
                                                                         const std::string &username,
                                                                         const std::string &password)
        {
            auto pool = std::make_shared<RedisConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<RedisConnectionPool> RedisConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::make_shared<RedisConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
            pool->initializePool();
            return pool;
        }
    }

} // namespace cpp_dbc
