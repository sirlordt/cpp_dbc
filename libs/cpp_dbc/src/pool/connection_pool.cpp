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
 * @file connection_pool.cpp
 * @brief Implementation of DBConnectionPoolBase — unified pool logic for all database families
 *
 * Ported from the ColumnarDBConnectionPool reference implementation.
 * See: research/connection_pool_unified.md
 */

#include "cpp_dbc/pool/connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "connection_pool_internal.hpp"
#include <algorithm>
#include <ranges>

namespace cpp_dbc
{

    // ── Constructors ──────────────────────────────────────────────────────────

    DBConnectionPoolBase::DBConnectionPoolBase(DBConnectionPool::ConstructorTag,
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
        : m_uri(uri),
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
        // Pool initialization will be done in the factory method via initializePool()
    }

    DBConnectionPoolBase::DBConnectionPoolBase(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : m_uri(config.getUri()),
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
        // Pool initialization will be done in the factory method via initializePool()
    }

    // ── Private helpers ───────────────────────────────────────────────────────

    cpp_dbc::expected<bool, DBException>
    DBConnectionPoolBase::validateConnection(std::nothrow_t, std::shared_ptr<DBConnection> conn) const noexcept
    {
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("1DWRX4UAQUXZ", "validateConnection called with null connection", system_utils::captureCallStack()));
        }

        auto result = conn->ping(std::nothrow);
        if (!result.has_value())
        {
            CP_DEBUG("DBConnectionPoolBase::validateConnection - ping failed: %s", result.error().what_s().data());
            return cpp_dbc::unexpected(result.error());
        }
        return result.value();
    }

    void DBConnectionPoolBase::notifyFirstWaiter(std::nothrow_t) noexcept
    {
        if (!m_waitQueue.empty())
        {
            m_waitQueue.front()->cv.notify_one();
        }
    }

    void DBConnectionPoolBase::decrementActiveCount(std::nothrow_t) noexcept
    {
        m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
    }

    std::weak_ptr<DBConnectionPoolBase> DBConnectionPoolBase::getPoolWeakPtr(std::nothrow_t) noexcept
    {
        try
        {
            return shared_from_this();
        }
        catch (const std::bad_weak_ptr &ex)
        {
            // Pool is not managed by shared_ptr, return empty weak_ptr (stack-allocated pool)
            CP_DEBUG("DBConnectionPoolBase::getPoolWeakPtr - Pool not managed by shared_ptr: %s", ex.what());
            return {};
        }
    }

    cpp_dbc::expected<void, DBException>
    DBConnectionPoolBase::returnConnection(std::nothrow_t, std::shared_ptr<DBConnectionPooled> conn) noexcept
    {
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("6GWX5NZIVL66", "returnConnection called with null connection", system_utils::captureCallStack()));
        }

        if (!m_running.load(std::memory_order_acquire))
        {
            conn->setActive(std::nothrow, false);
            m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
            auto closeResult = conn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("DBConnectionPoolBase::returnConnection - close() during shutdown failed: %s", closeResult.error().what_s().data());
            }
            return {};
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive(std::nothrow))
        {
            CP_DEBUG("DBConnectionPoolBase::returnConnection - Connection is not active, SKIPPED");
            return cpp_dbc::unexpected(DBException("7LGEVPEUY813", "returnConnection called on an already inactive connection (double-return bug)", system_utils::captureCallStack()));
        }

        // Additional safety check: verify connection is in allConnections (detect orphan)
        {
            std::scoped_lock lockPool(m_mutexPool);
            auto it = std::ranges::find(m_allConnections, conn);
            if (it == m_allConnections.end())
            {
                conn->setActive(std::nothrow, false);
                m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
                CP_DEBUG("DBConnectionPoolBase::returnConnection - ORPHAN: connection not in allConnections, active=%d", m_activeConnections.load(std::memory_order_acquire));
                notifyFirstWaiter(std::nothrow);
                return cpp_dbc::unexpected(DBException("G9MQC6Q7LYI2", "returnConnection called with a connection not belonging to this pool (orphan)", system_utils::captureCallStack()));
            }
        }

        bool valid = true;

        // Phase 1: I/O outside lock (validation and pool-return prep)
        if (m_testOnReturn.load(std::memory_order_acquire))
        {
            auto valResult = validateConnection(std::nothrow, conn->getUnderlyingConnection(std::nothrow));
            valid = valResult.has_value() && valResult.value();
        }

        if (valid && !conn->isPoolClosed(std::nothrow))
        {
            auto prepReturnPoolResult = conn->getUnderlyingConnection(std::nothrow)->prepareForPoolReturn(std::nothrow, m_transactionIsolation);
            if (!prepReturnPoolResult.has_value())
            {
                CP_DEBUG("DBConnectionPoolBase::returnConnection - prepareForPoolReturn failed: %s", prepReturnPoolResult.error().what_s().data());
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
                CP_DEBUG("DBConnectionPoolBase::returnConnection - HANDOFF: direct to waiter, active=%d, waiters=%zu, total=%zu",
                         m_activeConnections.load(std::memory_order_acquire), m_waitQueue.size(), m_allConnections.size());
                request->cv.notify_one(); // Per-waiter: only the fulfilled waiter is woken
            }
            else
            {
                m_idleConnections.push(conn);
                CP_DEBUG("DBConnectionPoolBase::returnConnection - VALID: pushed to idle, active=%d, idle=%zu, total=%zu",
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
                conn->markPoolClosed(std::nothrow, true);
                m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
                CP_DEBUG("DBConnectionPoolBase::returnConnection - INVALID: active=%d", m_activeConnections.load(std::memory_order_acquire));

                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    m_allConnections.erase(it);
                }
                std::queue<std::shared_ptr<DBConnectionPooled>> tempQueue;
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
                    CP_DEBUG("DBConnectionPoolBase::returnConnection - close failed on invalid connection: %s", closeResult.error().what_s().data());
                }
            }

            // Signal maintenance thread to create replacement asynchronously.
            // The returning thread is NOT blocked by connection creation I/O
            // (which takes 1.3-1.5s+ under Valgrind). Borrowers self-heal via
            // Step 2 in acquireConnection() if the pool is temporarily short.
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

    void DBConnectionPoolBase::maintenanceTask(std::nothrow_t) noexcept
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

                    auto idleTime = static_cast<size_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - pooledConn->getLastUsedTime(std::nothrow))
                            .count());

                    auto lifeTime = static_cast<size_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - pooledConn->getCreationTime(std::nothrow))
                            .count());

                    bool expired = (idleTime > m_idleTimeoutMillis.load(std::memory_order_acquire)) ||
                                   (lifeTime > m_maxLifetimeMillis.load(std::memory_order_acquire));

                    if (expired && m_allConnections.size() > m_minIdle.load(std::memory_order_acquire))
                    {
                        // Remove from idle queue if present
                        std::queue<std::shared_ptr<DBConnectionPooled>> tempQueue;
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
            // Each iteration: lock -> check -> unlock -> create (I/O) -> lock -> register.
            // This prevents m_mutexPool from being held during the slow createPooledDBConnection()
            // which would block borrowers. The m_pendingCreations counter prevents overallocation
            // by connection-creation I/O (1.3s+ under Valgrind).
            while (m_running.load(std::memory_order_acquire)) // NOSONAR(cpp:S924) — two breaks needed: one for "nothing to replenish", one for creation failure
            {
                {
                    std::scoped_lock lock(m_mutexPool);
                    if (m_replenishNeeded == 0 || m_allConnections.size() + m_pendingCreations >= m_maxSize.load(std::memory_order_acquire))
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
                        CP_DEBUG("DBConnectionPoolBase::maintenanceTask - Failed to create replacement: %s",
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
                        CP_DEBUG("DBConnectionPoolBase::maintenanceTask - REPLACEMENT HANDOFF: active=%d, waiters=%zu, total=%zu",
                                 m_activeConnections.load(std::memory_order_acquire), m_waitQueue.size(), m_allConnections.size());
                        req->cv.notify_one();
                    }
                    else
                    {
                        m_idleConnections.push(pooledResult.value());
                        CP_DEBUG("DBConnectionPoolBase::maintenanceTask - REPLACEMENT: pushed to idle, active=%d, idle=%zu, total=%zu",
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
                    if (m_allConnections.size() + m_pendingCreations >= m_minIdle.load(std::memory_order_acquire))
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
                        CP_DEBUG("DBConnectionPoolBase::maintenanceTask - Failed to create minIdle connection: %s",
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

    cpp_dbc::expected<void, DBException> DBConnectionPoolBase::initializePool(std::nothrow_t) noexcept
    {
        // Validate pool configuration before allocating any resources
        auto maxSize = m_maxSize.load(std::memory_order_acquire);
        auto minIdle = m_minIdle.load(std::memory_order_acquire);
        if (maxSize == 0)
        {
            return cpp_dbc::unexpected(DBException("NHGHX12485KU", "Invalid pool configuration: maxSize must be greater than 0", system_utils::captureCallStack()));
        }
        if (m_initialSize > maxSize)
        {
            return cpp_dbc::unexpected(DBException("XH0BG16TH5ZA", "Invalid pool configuration: initialSize (" + std::to_string(m_initialSize) + ") exceeds maxSize (" + std::to_string(maxSize) + ")", system_utils::captureCallStack()));
        }
        if (minIdle > maxSize)
        {
            return cpp_dbc::unexpected(DBException("0VESO2SABTOP", "Invalid pool configuration: minIdle (" + std::to_string(minIdle) + ") exceeds maxSize (" + std::to_string(maxSize) + ")", system_utils::captureCallStack()));
        }

        try
        {
            // Reserve space for connections — can throw std::bad_alloc (no nothrow overload)
            m_allConnections.reserve(maxSize);

            // Create initial connections
            for (size_t i = 0; i < m_initialSize; i++)
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
            return cpp_dbc::unexpected(DBException("NOJ9TUYDXKJY", "Failed to initialize connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            close(std::nothrow);
            return cpp_dbc::unexpected(DBException("UY1FDRVWO9KP", "Failed to initialize connection pool: unknown error", system_utils::captureCallStack()));
        }
        return {};
    }

    // ── Destructor ────────────────────────────────────────────────────────────

    DBConnectionPoolBase::~DBConnectionPoolBase()
    {
        CP_DEBUG("DBConnectionPoolBase::~DBConnectionPoolBase - Starting destructor at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        auto result = DBConnectionPoolBase::close(std::nothrow);
        if (!result.has_value())
        {
            CP_DEBUG("DBConnectionPoolBase::~DBConnectionPoolBase - close failed: %s",
                     result.error().what_s().data());
        }

        CP_DEBUG("DBConnectionPoolBase::~DBConnectionPoolBase - Destructor completed at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    // ── Throwing API ──────────────────────────────────────────────────────────

#ifdef __cpp_exceptions

    std::shared_ptr<DBConnection> DBConnectionPoolBase::getDBConnection()
    {
        auto result = getDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DBConnection> DBConnectionPoolBase::getDBConnection(size_t timeoutMs)
    {
        auto result = getDBConnection(std::nothrow, timeoutMs);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t DBConnectionPoolBase::getActiveDBConnectionCount() const
    {
        auto result = getActiveDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t DBConnectionPoolBase::getIdleDBConnectionCount() const
    {
        auto result = getIdleDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t DBConnectionPoolBase::getTotalDBConnectionCount() const
    {
        auto result = getTotalDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void DBConnectionPoolBase::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool DBConnectionPoolBase::isRunning() const
    {
        auto result = isRunning(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    // ── Nothrow API ───────────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> DBConnectionPoolBase::getDBConnection(std::nothrow_t) noexcept
    {
        auto result = acquireConnection(std::nothrow);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        return std::static_pointer_cast<DBConnection>(result.value());
    }

    cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> DBConnectionPoolBase::getDBConnection(std::nothrow_t, size_t timeoutMs) noexcept
    {
        auto result = acquireConnection(std::nothrow, timeoutMs);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        return std::static_pointer_cast<DBConnection>(result.value());
    }

    cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException> DBConnectionPoolBase::acquireConnection(std::nothrow_t, size_t timeoutMs) noexcept
    {
        using namespace std::chrono;
        auto deadline = steady_clock::now() + milliseconds(timeoutMs > 0 ? timeoutMs : m_maxWaitMillis.load(std::memory_order_acquire));

        while (true)
        {
            std::shared_ptr<DBConnectionPooled> result;
            bool handedOff = false;

            {
                std::unique_lock lockPool(m_mutexPool);

                if (!m_running.load(std::memory_order_acquire))
                {
                    return cpp_dbc::unexpected(DBException("7YSAF5WE3U0P", "Connection pool is closed", system_utils::captureCallStack()));
                }

                // Multiple break statements required to express mutually exclusive exit conditions
                // (idle found, result created). Refactoring would add artificial state variables.
                // Acquire loop: idle -> create -> wait
                while (!result) // NOSONAR(cpp:S924)
                {
                    // Step 1: Check idle connections
                    if (!m_idleConnections.empty())
                    {
                        result = m_idleConnections.front();
                        m_idleConnections.pop();
                        CP_DEBUG("DBConnectionPoolBase::acquireConnection - GOT IDLE: active=%d, idle=%zu, total=%zu",
                                 m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                        break;
                    }

                    // Step 2: Create new connection if pool not full
                    if (m_allConnections.size() + m_pendingCreations < m_maxSize.load(std::memory_order_acquire))
                    {
                        CP_DEBUG("DBConnectionPoolBase::acquireConnection - CREATING: total=%zu, pending=%zu, max=%zu",
                                 m_allConnections.size(), m_pendingCreations, m_maxSize.load(std::memory_order_acquire));
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
                                CP_DEBUG("DBConnectionPoolBase::acquireConnection - Failed to create connection: %s", newConnResult.error().what_s().data());
                            }
                            else
                            {
                                auto newConn = newConnResult.value();
                                // Recheck size under lock (another thread may have filled the pool
                                // while we were creating outside the lock)
                                if (m_allConnections.size() < m_maxSize.load(std::memory_order_acquire))
                                {
                                    m_allConnections.push_back(newConn);
                                    result = newConn;
                                }
                                else
                                {
                                    // Pool became full — discard candidate
                                    CP_DEBUG("DBConnectionPoolBase::acquireConnection - DISCARD candidate: total=%zu >= max=%zu", m_allConnections.size(), m_maxSize.load(std::memory_order_acquire));
                                    newConn->markPoolClosed(std::nothrow, true); // Prevent destructor returnToPool()
                                    lockPool.unlock();
                                    auto closeResult = newConn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                                    if (!closeResult.has_value())
                                    {
                                        CP_DEBUG("DBConnectionPoolBase::acquireConnection - close() on discarded candidate failed: %s", closeResult.error().what_s().data());
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
                    CP_DEBUG("DBConnectionPoolBase::acquireConnection - WAITING on CV: active=%d, idle=%zu, total=%zu, pending=%zu, waiters=%zu",
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
                            CP_DEBUG("DBConnectionPoolBase::acquireConnection - wait_until threw: %s", ex.what());
                            std::erase(m_waitQueue, &request);
                            return cpp_dbc::unexpected(DBException("67MA2A9J7QCG",
                                                                   "wait_until threw in acquireConnection: " + std::string(ex.what()),
                                                                   system_utils::captureCallStack()));
                        }
                        catch (...) // NOSONAR(cpp:S2738) — non-std exceptions: remove from queue then return error
                        {
                            CP_DEBUG("DBConnectionPoolBase::acquireConnection - wait_until threw unknown exception");
                            std::erase(m_waitQueue, &request);
                            return cpp_dbc::unexpected(DBException("WCHTMFU6SKNM",
                                                                   "Unknown exception from wait_until in acquireConnection",
                                                                   system_utils::captureCallStack()));
                        }

                        // FIRST: check fulfilled (takes priority over timeout — handoff may arrive
                        // simultaneously with deadline expiry)
                        if (request.fulfilled)
                        {
                            result = request.conn;
                            handedOff = true;
                            CP_DEBUG("DBConnectionPoolBase::acquireConnection - HANDOFF received: active=%d, idle=%zu, total=%zu",
                                     m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                            break;
                        }

                        if (!m_running.load(std::memory_order_acquire))
                        {
                            std::erase(m_waitQueue, &request);
                            return cpp_dbc::unexpected(DBException("K4YZRXTZ2JCJ", "Connection pool is closed", system_utils::captureCallStack()));
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
                                CP_DEBUG("DBConnectionPoolBase::acquireConnection - GOT IDLE after timeout: active=%d, idle=%zu",
                                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size());
                                break;
                            }
                            CP_DEBUG("DBConnectionPoolBase::acquireConnection - TIMEOUT: active=%d, idle=%zu, total=%zu, pending=%zu",
                                     m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size(), m_pendingCreations);
                            return cpp_dbc::unexpected(DBException("32V0M0TXVE18", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
                        }

                        // Spurious wakeup: check idle again
                        if (!m_idleConnections.empty())
                        {
                            std::erase(m_waitQueue, &request);
                            result = m_idleConnections.front();
                            m_idleConnections.pop();
                            CP_DEBUG("DBConnectionPoolBase::acquireConnection - GOT IDLE after wakeup: active=%d, idle=%zu",
                                     m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size());
                            break;
                        }

                        // Nothing available — stay in queue at same FIFO position, re-wait.
                        CP_DEBUG("DBConnectionPoolBase::acquireConnection - CV WAKEUP (re-waiting): idle=%zu, total=%zu",
                                 m_idleConnections.size(), m_allConnections.size());
                    }
                }

                if (!handedOff)
                {
                    result->setActive(std::nothrow, true);
                    m_activeConnections.fetch_add(1, std::memory_order_acq_rel);
                }
                result->markPoolClosed(std::nothrow, false);
            }

            // Phase 2: HikariCP validation skip (outside lock)
            if (m_testOnBorrow.load(std::memory_order_acquire))
            {
                auto lastUsed = result->getLastUsedTime(std::nothrow);
                auto now = steady_clock::now();
                auto timeSinceLastUse = static_cast<size_t>(duration_cast<milliseconds>(now - lastUsed).count());
                auto validationTimeout = m_validationTimeoutMillis.load(std::memory_order_acquire);

                if (timeSinceLastUse > validationTimeout)
                {
                    CP_DEBUG("DBConnectionPoolBase::acquireConnection - VALIDATING (timeSinceLastUse=%zu ms > %zu ms)", timeSinceLastUse, validationTimeout);
                    auto valResult = validateConnection(std::nothrow, result->getUnderlyingConnection(std::nothrow));
                    bool validationPassed = valResult.has_value() && valResult.value();

                    if (!validationPassed)
                    {
                        CP_DEBUG("DBConnectionPoolBase::acquireConnection - VALIDATION FAILED: removing connection, active=%d", m_activeConnections.load(std::memory_order_acquire));

                        // Validation failed — undo active marking, remove from pool
                        {
                            std::scoped_lock lockPool(m_mutexPool);
                            result->setActive(std::nothrow, false);
                            result->markPoolClosed(std::nothrow, true);
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
                            CP_DEBUG("DBConnectionPoolBase::acquireConnection - close() on invalid connection failed: %s", closeResult.error().what_s().data());
                        }

                        // Check deadline before retrying
                        if (steady_clock::now() >= deadline)
                        {
                            return cpp_dbc::unexpected(DBException("Y3D7BN77BUMM", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
                        }

                        continue; // Retry
                    }
                }
            }

            // Phase 3: prepareForBorrow (outside lock)
            auto borrowResult = result->getUnderlyingConnection(std::nothrow)->prepareForBorrow(std::nothrow);
            if (!borrowResult.has_value())
            {
                CP_DEBUG("DBConnectionPoolBase::acquireConnection - prepareForBorrow failed: %s", borrowResult.error().what_s().data());

                // prepareForBorrow failed — connection state is unreliable.
                // Undo active marking, remove from pool, close underlying connection, and retry.
                {
                    std::scoped_lock lockPool(m_mutexPool);
                    result->setActive(std::nothrow, false);
                    result->markPoolClosed(std::nothrow, true);
                    m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);

                    auto it = std::ranges::find(m_allConnections, result);
                    if (it != m_allConnections.end())
                    {
                        m_allConnections.erase(it);
                    }
                    notifyFirstWaiter(std::nothrow);
                }

                auto closeResult = result->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                if (!closeResult.has_value())
                {
                    CP_DEBUG("DBConnectionPoolBase::acquireConnection - close() after prepareForBorrow failure: %s", closeResult.error().what_s().data());
                }

                if (steady_clock::now() >= deadline)
                {
                    return cpp_dbc::unexpected(DBException("QMP82J46OQOO", "Timeout waiting for connection from the pool (prepareForBorrow failed)", system_utils::captureCallStack()));
                }

                continue; // Retry
            }

            return result;
        }
    }

    cpp_dbc::expected<size_t, DBException> DBConnectionPoolBase::getActiveDBConnectionCount(std::nothrow_t) const noexcept
    {
        return static_cast<size_t>(m_activeConnections.load(std::memory_order_acquire));
    }

    cpp_dbc::expected<size_t, DBException> DBConnectionPoolBase::getIdleDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_idleConnections.size();
    }

    cpp_dbc::expected<size_t, DBException> DBConnectionPoolBase::getTotalDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_allConnections.size();
    }

    cpp_dbc::expected<void, DBException> DBConnectionPoolBase::close(std::nothrow_t) noexcept
    {
        CP_DEBUG("DBConnectionPoolBase::close - Starting close operation");

        if (!m_running.exchange(false, std::memory_order_acq_rel))
        {
            CP_DEBUG("DBConnectionPoolBase::close - Already closed, returning");
            return {}; // Already closed — idempotent
        }

        // Mark pool as no longer alive - this prevents pooled connections from trying to return
        if (m_poolAlive)
        {
            m_poolAlive->store(false, std::memory_order_release);
        }

        CP_DEBUG("DBConnectionPoolBase::close - Waiting for active operations to complete at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            CP_DEBUG("DBConnectionPoolBase::close - Initial active connections: %d", m_activeConnections.load(std::memory_order_acquire));

            while (m_activeConnections.load(std::memory_order_acquire) > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 2 == 0)
                {
                    CP_DEBUG("DBConnectionPoolBase::close - Waited %ld seconds for %d active connections",
                             elapsed_seconds, m_activeConnections.load(std::memory_order_acquire));
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("DBConnectionPoolBase::close - Timeout waiting for active connections, forcing close at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
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
        std::vector<std::shared_ptr<DBConnectionPooled>> connectionsToClose;
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
                    CP_DEBUG("DBConnectionPoolBase::close - connection close failed: %s", r.error().what_s().data());
                    lastError = r;
                }
            }
        }

        return lastError;
    }

    cpp_dbc::expected<bool, DBException> DBConnectionPoolBase::isRunning(std::nothrow_t) const noexcept
    {
        return m_running.load(std::memory_order_acquire);
    }

    void DBConnectionPoolBase::setConnectionTimeout(std::nothrow_t, size_t millis) noexcept
    {
        m_maxWaitMillis.store(millis, std::memory_order_release);
    }

    size_t DBConnectionPoolBase::getConnectionTimeout(std::nothrow_t) const noexcept
    {
        return m_maxWaitMillis.load(std::memory_order_acquire);
    }

    void DBConnectionPoolBase::setIdleTimeout(std::nothrow_t, size_t millis) noexcept
    {
        m_idleTimeoutMillis.store(millis, std::memory_order_release);
    }

    size_t DBConnectionPoolBase::getIdleTimeout(std::nothrow_t) const noexcept
    {
        return m_idleTimeoutMillis.load(std::memory_order_acquire);
    }

    void DBConnectionPoolBase::setMaxLifetimeMillis(std::nothrow_t, size_t millis) noexcept
    {
        m_maxLifetimeMillis.store(millis, std::memory_order_release);
    }

    size_t DBConnectionPoolBase::getMaxLifetimeMillis(std::nothrow_t) const noexcept
    {
        return m_maxLifetimeMillis.load(std::memory_order_acquire);
    }

    void DBConnectionPoolBase::setTestOnBorrow(std::nothrow_t, bool enabled) noexcept
    {
        m_testOnBorrow.store(enabled, std::memory_order_release);
    }

    bool DBConnectionPoolBase::getTestOnBorrow(std::nothrow_t) const noexcept
    {
        return m_testOnBorrow.load(std::memory_order_acquire);
    }

    void DBConnectionPoolBase::setTestOnReturn(std::nothrow_t, bool enabled) noexcept
    {
        m_testOnReturn.store(enabled, std::memory_order_release);
    }

    bool DBConnectionPoolBase::getTestOnReturn(std::nothrow_t) const noexcept
    {
        return m_testOnReturn.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<void, DBException> DBConnectionPoolBase::setMaxSize(std::nothrow_t, size_t size) noexcept
    {
        if (size == 0)
        {
            return cpp_dbc::unexpected(DBException("YN8DO1FJUKH8",
                "maxSize must be greater than 0",
                system_utils::captureCallStack()));
        }

        std::vector<std::shared_ptr<DBConnectionPooled>> toEvict;

        {
            std::scoped_lock lock(m_mutexPool);

            if (size < m_minIdle.load(std::memory_order_acquire))
            {
                m_minIdle.store(size, std::memory_order_release);
            }

            m_maxSize.store(size, std::memory_order_release);

            // Evict excess idle connections
            while (m_allConnections.size() > m_maxSize.load(std::memory_order_acquire) && !m_idleConnections.empty())
            {
                auto conn = m_idleConnections.front();
                m_idleConnections.pop();
                conn->markPoolClosed(std::nothrow, true);

                // Remove from m_allConnections
                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    m_allConnections.erase(it);
                }

                toEvict.push_back(conn);
            }
        }

        // Close evicted connections outside lock (I/O)
        for (const auto &conn : toEvict)
        {
            auto closeResult = conn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("DBConnectionPoolBase::setMaxSize - close() on evicted connection failed: %s",
                         closeResult.error().what_s().data());
            }
        }

        return {};
    }

    size_t DBConnectionPoolBase::getMaxSize(std::nothrow_t) const noexcept
    {
        return m_maxSize.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<void, DBException> DBConnectionPoolBase::setMinIdle(std::nothrow_t, size_t size) noexcept
    {
        auto currentMaxSize = m_maxSize.load(std::memory_order_acquire);
        if (size > currentMaxSize)
        {
            return cpp_dbc::unexpected(DBException("XLIRPO4B4K8S",
                "minIdle (" + std::to_string(size) + ") cannot exceed maxSize (" + std::to_string(currentMaxSize) + ")",
                system_utils::captureCallStack()));
        }

        m_minIdle.store(size, std::memory_order_release);

        // Wake maintenance thread so it can replenish if needed
        m_maintenanceCondition.notify_one();

        return {};
    }

    size_t DBConnectionPoolBase::getMinIdle(std::nothrow_t) const noexcept
    {
        return m_minIdle.load(std::memory_order_acquire);
    }

} // namespace cpp_dbc
