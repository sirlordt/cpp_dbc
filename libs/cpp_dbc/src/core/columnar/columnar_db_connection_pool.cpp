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

    ColumnarDBConnectionPool::ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
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
          m_validationQuery(config.getValidationQuery()),
          m_transactionIsolation(config.getTransactionIsolation())
    {
        // Pool initialization will be done in the factory method
    }

    void ColumnarDBConnectionPool::initializePool()
    {
        try
        {
            // Reserve space for connections
            m_allConnections.reserve(m_maxSize);

            // Create initial connections
            for (int i = 0; i < m_initialSize; i++)
            {
                auto pooledConn = createPooledDBConnection();

                // Add to idle connections and all connections lists under proper locks
                // Use scoped_lock for consistent lock ordering to prevent deadlock
                {
                    std::scoped_lock lock(m_mutexPool);
                    m_idleConnections.push(pooledConn);
                    m_allConnections.push_back(pooledConn);
                }
            }

            // Start maintenance thread
            m_maintenanceThread = std::jthread(&ColumnarDBConnectionPool::maintenanceTask, this);
        }
        catch (const std::exception &e)
        {
            close();
            throw DBException("C54157A1F4D8", "Failed to initialize connection pool: " + std::string(e.what()), system_utils::captureCallStack());
        }
    }

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
                                                                               const std::string &validationQuery,
                                                                               TransactionIsolationLevel transactionIsolation)
    {
        auto pool = std::make_shared<ColumnarDBConnectionPool>(
            DBConnectionPool::ConstructorTag{}, url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, validationQuery, transactionIsolation);

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    std::shared_ptr<ColumnarDBConnectionPool> ColumnarDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto pool = std::make_shared<ColumnarDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    ColumnarDBConnectionPool::~ColumnarDBConnectionPool()
    {
        CP_DEBUG("ColumnarDBConnectionPool::~ColumnarDBConnectionPool - Starting destructor at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        ColumnarDBConnectionPool::close();

        CP_DEBUG("ColumnarDBConnectionPool::~ColumnarDBConnectionPool - Destructor completed at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    std::shared_ptr<ColumnarDBConnection> ColumnarDBConnectionPool::createDBConnection()
    {
        auto dbConn = DriverManager::getDBConnection(m_url, m_username, m_password, m_options);
        auto columnarConn = std::dynamic_pointer_cast<ColumnarDBConnection>(dbConn);
        if (!columnarConn)
        {
            throw DBException("625AA68B72B8", "Connection pool only supports columnar database connections", system_utils::captureCallStack());
        }
        return columnarConn;
    }

    std::shared_ptr<ColumnarPooledDBConnection> ColumnarDBConnectionPool::createPooledDBConnection()
    {
        auto conn = createDBConnection();

        // Transaction isolation is stored but not set here because ColumnarDBConnection doesn't support setting it directly
        // If the underlying driver supports it via options or query, it should be handled there or via executeQuery

        // Create pooled connection with weak_ptr
        std::weak_ptr<ColumnarDBConnectionPool> weakPool;
        try
        {
            // Try to create a weak_ptr from this pool using shared_from_this
            // This works if the pool is managed by a shared_ptr
            weakPool = shared_from_this();
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            // Pool is not managed by shared_ptr, weakPool remains empty
            // This can happen if the pool is stack-allocated
            CP_DEBUG("ColumnarDBConnectionPool::createPooledDBConnection - bad_weak_ptr: %s", ex.what());
        }

        // Create the pooled connection
        auto pooledConn = std::make_shared<ColumnarPooledDBConnection>(conn, weakPool, m_poolAlive);
        return pooledConn;
    }

    bool ColumnarDBConnectionPool::validateConnection(std::shared_ptr<ColumnarDBConnection> conn) const
    {
        try
        {
            // Use validation query to check connection
            auto resultSet = conn->executeQuery(m_validationQuery);
            return true;
        }
        catch ([[maybe_unused]] const DBException &ex)
        {
            CP_DEBUG("ColumnarDBConnectionPool::validateConnection - Exception: %s", ex.what_s().c_str());
            return false;
        }
    }

    void ColumnarDBConnectionPool::returnConnection(std::shared_ptr<ColumnarPooledDBConnection> conn)
    {
        if (!conn)
        {
            return;
        }

        if (!m_running.load())
        {
            conn->setActive(false);
            m_activeConnections--;
            try
            {
                conn->getUnderlyingColumnarConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - Exception closing connection during shutdown: %s", ex.what());
            }
            return;
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive())
        {
            CP_DEBUG("ColumnarDBConnectionPool::returnConnection - Connection is not active, SKIPPED");
            return;
        }

        // Additional safety check: verify connection is in allConnections (detect orphan)
        {
            std::scoped_lock lockPool(m_mutexPool);
            auto it = std::ranges::find(m_allConnections, conn);
            if (it == m_allConnections.end())
            {
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - Connection not found in pool, SKIPPED");
                conn->setActive(false);
                m_activeConnections--;
                m_connectionAvailable.notify_one();
                return;
            }
        }

        bool valid = true;

        // Phase 1: I/O outside lock (validation and pool-return prep)
        if (m_testOnReturn)
        {
            valid = validateConnection(conn->getUnderlyingColumnarConnection());
        }

        if (valid)
        {
            auto result = conn->getUnderlyingColumnarConnection()->prepareForPoolReturn(std::nothrow);
            if (!result.has_value())
            {
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - prepareForPoolReturn failed: %s", result.error().what_s().c_str());
                valid = false;
            }
        }

        // Phase 2: State changes under m_mutexPool
        if (valid)
        {
            conn->updateLastUsedTime();
            std::scoped_lock lockPool(m_mutexPool);
            conn->setActive(false);
            m_activeConnections--;

            if (!m_waitQueue.empty())
            {
                // Direct handoff to first waiter.
                // Restore active state: we decremented above unconditionally,
                // but the borrower skips setActive(true)/m_activeConnections++
                // when handedOff=true, so we must restore here.
                ConnectionRequest *req = m_waitQueue.front();
                m_waitQueue.pop_front();
                conn->setActive(true);
                m_activeConnections++;
                req->conn = conn;
                req->fulfilled = true;
                m_connectionAvailable.notify_all();
            }
            else
            {
                m_idleConnections.push(conn);
                m_connectionAvailable.notify_one();
            }
        }
        else
        {
            // Invalid connection: remove from pool, close outside lock, then replenish
            {
                std::scoped_lock lockPool(m_mutexPool);
                conn->setActive(false);
                conn->m_closed.store(true); // prevent destructor double-decrement
                m_activeConnections--;

                // Remove from allConnections
                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    m_allConnections.erase(it);
                }

                // Remove from idle queue if present (defensive)
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
                m_idleConnections = std::move(tempQueue);

                // Notify inside lock: the freed slot allows waiting threads to create
                m_connectionAvailable.notify_one();
            }

            // Close outside lock
            try
            {
                conn->getUnderlyingColumnarConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - Exception closing invalid connection: %s", ex.what());
            }

            // Replenish: create outside lock
            if (m_running.load())
            {
                std::shared_ptr<ColumnarPooledDBConnection> replacement;
                try
                {
                    replacement = createPooledDBConnection();
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    CP_DEBUG("ColumnarDBConnectionPool::returnConnection - Exception creating replacement: %s", ex.what());
                }

                if (replacement)
                {
                    std::shared_ptr<ColumnarPooledDBConnection> discardReplacement;
                    {
                        std::scoped_lock lockPool(m_mutexPool);
                        if (m_running.load() && m_allConnections.size() < m_maxSize)
                        {
                            m_allConnections.push_back(replacement);

                            if (!m_waitQueue.empty())
                            {
                                // Direct handoff to first waiter
                                ConnectionRequest *req = m_waitQueue.front();
                                m_waitQueue.pop_front();
                                replacement->setActive(true);
                                m_activeConnections++;
                                replacement->m_closed.store(false);
                                req->conn = replacement;
                                req->fulfilled = true;
                                m_connectionAvailable.notify_all();
                            }
                            else
                            {
                                m_idleConnections.push(replacement);
                                m_connectionAvailable.notify_one();
                            }
                        }
                        else
                        {
                            replacement->m_closed.store(true);
                            discardReplacement = replacement;
                        }
                    }
                    if (discardReplacement)
                    {
                        auto closeResult = discardReplacement->getUnderlyingColumnarConnection()->close(std::nothrow);
                        if (!closeResult.has_value())
                        {
                            CP_DEBUG("ColumnarDBConnectionPool::returnConnection - close() on discarded replacement failed: %s", closeResult.error().what_s().c_str());
                        }
                    }
                }
            }
        }
    }

    std::shared_ptr<DBConnection> ColumnarDBConnectionPool::getDBConnection()
    {
        return getColumnarDBConnection();
    }

    std::shared_ptr<ColumnarDBConnection> ColumnarDBConnectionPool::getColumnarDBConnection()
    {
        using namespace std::chrono;
        auto deadline = steady_clock::now() + milliseconds(m_maxWaitMillis);

        while (true)
        {
            std::shared_ptr<ColumnarPooledDBConnection> result;
            bool handedOff = false;

            {
                std::unique_lock lockPool(m_mutexPool);

                if (!m_running.load())
                {
                    throw DBException("6R7S8T9U0V1W", "Connection pool is closed", system_utils::captureCallStack());
                }

                // NOSONAR(cpp:S924) — multiple break statements required to express mutually exclusive
                // exit conditions (idle found, result created). Refactoring would add artificial state.
                // Acquire loop: idle → create → wait
                while (!result)
                {
                    // Step 1: Check idle connections
                    if (!m_idleConnections.empty())
                    {
                        result = m_idleConnections.front();
                        m_idleConnections.pop();
                        break;
                    }

                    // Step 2: Create new connection if pool not full
                    size_t totalPlusCreating = m_allConnections.size() + m_pendingCreations;
                    if (totalPlusCreating < m_maxSize)
                    {
                        m_pendingCreations++;
                        lockPool.unlock();

                        std::shared_ptr<ColumnarPooledDBConnection> newConn;
                        try
                        {
                            newConn = createPooledDBConnection();
                        }
                        catch ([[maybe_unused]] const std::exception &ex)
                        {
                            CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - Exception creating connection: %s", ex.what());
                        }

                        lockPool.lock();
                        m_pendingCreations--;

                        if (newConn)
                        {
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
                                newConn->m_closed.store(true); // Prevent destructor returnToPool()
                                lockPool.unlock();
                                auto closeResult = newConn->getUnderlyingColumnarConnection()->close(std::nothrow);
                                if (!closeResult.has_value())
                                {
                                    CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - close() on discarded candidate failed: %s", closeResult.error().what_s().c_str());
                                }
                                lockPool.lock();
                            }
                        }
                        // Always restart outer loop: if result is set it exits,
                        // otherwise idle is re-checked before waiting (matches KV).
                        // This prevents missing connections returned during the unlock.
                        continue;
                    }

                    // Step 3: Wait for a connection to become available
                    ConnectionRequest req;
                    m_waitQueue.push_back(&req);

                    // NOSONAR(cpp:S924) — multiple break statements required for mutually exclusive CV exit
                    // conditions (handoff, idle-after-timeout, idle-after-wakeup). Refactoring would break
                    // FIFO queue correctness under sanitizer-serialized execution.
                    while (true) // Inner wait loop — request stays in queue at FIFO position
                    {
                        std::cv_status status;
                        try
                        {
                            status = m_connectionAvailable.wait_until(lockPool, deadline);
                        }
                        catch (...)
                        {
                            // If wait throws, remove our request from the queue to avoid dangling pointer
                            std::erase(m_waitQueue, &req);
                            throw;
                        }

                        // FIRST: check fulfilled (takes priority over timeout — handoff may arrive
                        // simultaneously with deadline expiry)
                        if (req.fulfilled)
                        {
                            result = req.conn;
                            handedOff = true;
                            break;
                        }

                        if (!m_running.load())
                        {
                            std::erase(m_waitQueue, &req);
                            throw DBException("69677B95E2B2", "Connection pool is closed", system_utils::captureCallStack());
                        }

                        if (status == std::cv_status::timeout)
                        {
                            // Timed out: remove our request from the queue if still there
                            std::erase(m_waitQueue, &req);

                            // One last idle check: a connection may have arrived just as
                            // the deadline fired (matches KV pattern).
                            if (!m_idleConnections.empty())
                            {
                                result = m_idleConnections.front();
                                m_idleConnections.pop();
                                break;
                            }

                            throw DBException("99E01137B87B", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                        }

                        // Spurious wakeup: check idle again
                        if (!m_idleConnections.empty())
                        {
                            std::erase(m_waitQueue, &req);
                            result = m_idleConnections.front();
                            m_idleConnections.pop();
                            break;
                        }
                        // Nothing available — stay in queue at same FIFO position, re-wait.
                    }

                    if (result)
                        break;
                }

                if (!handedOff)
                {
                    result->setActive(true);
                    m_activeConnections++;
                }
                result->m_closed.store(false);
            }

            // Phase 2: HikariCP validation skip (outside lock)
            if (m_testOnBorrow)
            {
                auto lastUsed = result->getLastUsedTime();
                auto now = steady_clock::now();
                auto timeSinceLastUse = duration_cast<milliseconds>(now - lastUsed).count();

                if (timeSinceLastUse > m_validationTimeoutMillis &&
                    !validateConnection(result->getUnderlyingColumnarConnection()))
                {
                    CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - Validation failed, removing connection");

                    {
                        std::scoped_lock lockPool(m_mutexPool);
                        result->setActive(false);
                        m_activeConnections--;
                        auto it = std::ranges::find(m_allConnections, result);
                        if (it != m_allConnections.end())
                        {
                            m_allConnections.erase(it);
                        }
                    }

                    auto closeResult = result->getUnderlyingColumnarConnection()->close(std::nothrow);
                    if (!closeResult.has_value())
                    {
                        CP_DEBUG("ColumnarDBConnectionPool::getColumnarDBConnection - close() on invalid connection failed: %s", closeResult.error().what_s().c_str());
                    }

                    // Check deadline before retrying
                    if (steady_clock::now() >= deadline)
                    {
                        throw DBException("99E01137B87B", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                    }

                    continue; // Retry
                }
            }

            return result;
        }
    }

    void ColumnarDBConnectionPool::maintenanceTask()
    {
        do
        {
            {
                // Wait 30 seconds or until notified by close()
                std::unique_lock lock(m_mutexPool);
                m_maintenanceCondition.wait_for(lock, std::chrono::seconds(30), [this]
                                                { return !m_running; });
            }

            if (!m_running)
            {
                break;
            }

            auto now = std::chrono::steady_clock::now();

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

                    auto idleTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - pooledConn->getLastUsedTime())
                                        .count();

                    auto lifeTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - pooledConn->getCreationTime())
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
                        m_idleConnections = std::move(tempQueue);

                        it = m_allConnections.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            // Replenish minIdle connections OUTSIDE lock
            // (createPooledDBConnection may acquire driver-internal locks)
            size_t currentTotal;
            {
                std::scoped_lock lock(m_mutexPool);
                currentTotal = m_allConnections.size() + m_pendingCreations;
            }
            while (m_running && currentTotal < m_minIdle)
            {
                std::shared_ptr<ColumnarPooledDBConnection> pooledConn;
                try
                {
                    pooledConn = createPooledDBConnection();
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    CP_DEBUG("ColumnarDBConnectionPool::maintenanceTask - Exception creating minIdle connection: %s", ex.what());
                    break;
                }

                {
                    std::scoped_lock lock(m_mutexPool);
                    m_idleConnections.push(pooledConn);
                    m_allConnections.push_back(pooledConn);
                    currentTotal = m_allConnections.size() + m_pendingCreations;
                    m_connectionAvailable.notify_one();
                }
            }
        } while (m_running);
    }

    size_t ColumnarDBConnectionPool::getActiveDBConnectionCount() const
    {
        return m_activeConnections;
    }

    size_t ColumnarDBConnectionPool::getIdleDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexPool);
        return m_idleConnections.size();
    }

    size_t ColumnarDBConnectionPool::getTotalDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexPool);
        return m_allConnections.size();
    }

    void ColumnarDBConnectionPool::close()
    {
        CP_DEBUG("ColumnarDBConnectionPool::close - Starting close operation");

        if (!m_running.exchange(false))
        {
            CP_DEBUG("ColumnarDBConnectionPool::close - Already closed, returning");
            return; // Already closed
        }

        // Mark pool as no longer alive - this prevents pooled connections from trying to return
        if (m_poolAlive)
        {
            m_poolAlive->store(false);
        }

        CP_DEBUG("ColumnarDBConnectionPool::close - Waiting for active operations to complete at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            CP_DEBUG("ColumnarDBConnectionPool::close - Initial active connections: %d", m_activeConnections.load());

            while (m_activeConnections.load() > 0)
            {
                CP_DEBUG("ColumnarDBConnectionPool::close - Waiting for %d active connections to finish at %lld", m_activeConnections.load(), (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 1 == 0)
                {
                    CP_DEBUG("ColumnarDBConnectionPool::close - Waited %lld seconds for active connections", (long long)elapsed_seconds);
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("ColumnarDBConnectionPool::close - Timeout waiting for active connections, forcing close at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                    // Force active connections to be marked as inactive
                    m_activeConnections.store(0);
                    break;
                }
            }
        }

        // Notify maintenance thread and all waiting borrowers
        {
            std::scoped_lock lock(m_mutexPool);
            m_maintenanceCondition.notify_all();
            m_connectionAvailable.notify_all();
        }

        // Join maintenance thread
        if (m_maintenanceThread.joinable())
        {
            m_maintenanceThread.join();
        }

        // Close all connections
        // CRITICAL: Collect connections under pool lock, then close OUTSIDE pool lock.
        // Closing a connection acquires per-connection mutex (m_connMutex).
        // Closing inside pool lock would invert that order → Helgrind LockOrder violation.
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
                CP_DEBUG("ColumnarDBConnectionPool::close - Exception during connection close: %s", ex.what());
            }
        }
    }

    bool ColumnarDBConnectionPool::isRunning() const
    {
        return m_running.load();
    }

    // ColumnarPooledDBConnection implementation
    ColumnarPooledDBConnection::ColumnarPooledDBConnection(
        std::shared_ptr<ColumnarDBConnection> connection,
        std::weak_ptr<ColumnarDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive)
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive)
    {
        m_creationTime = std::chrono::steady_clock::now();
        m_lastUsedTime = m_creationTime;
    }

    bool ColumnarPooledDBConnection::isPoolValid() const
    {
        return m_poolAlive && m_poolAlive->load();
    }

    ColumnarPooledDBConnection::~ColumnarPooledDBConnection()
    {
        if (!m_closed && m_conn)
        {
            try
            {
                // If the pool is no longer alive, close the physical connection
                if (!isPoolValid())
                {
                    m_conn->close();
                }
                else
                {
                    ColumnarPooledDBConnection::returnToPool(); // NOSONAR(cpp:S1699) — qualified call in destructor intentional to avoid virtual dispatch
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("ColumnarPooledDBConnection::~ColumnarPooledDBConnection - Exception: %s", ex.what());
            }
        }
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::close(std::nothrow_t) noexcept
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
                        poolShared->returnConnection(std::static_pointer_cast<ColumnarPooledDBConnection>(shared_from_this()));
                    }
                }
                else if (m_conn)
                {
                    // If pool is invalid, actually close the connection
                    m_conn->close();
                }
            }
            catch (const std::exception &ex)
            {
                CP_DEBUG("ColumnarPooledDBConnection::close - Exception during return to pool: %s", ex.what());
                m_closed.store(true);
                if (m_conn)
                {
                    m_conn->close();
                }
            }
            catch (...) // NOSONAR - Catch-all to ensure m_closed is always set correctly on any exception
            {
                CP_DEBUG("ColumnarPooledDBConnection::close - Unknown exception caught");
                m_closed.store(true);
                if (m_conn)
                {
                    m_conn->close();
                }
            }
        }
        return {};
    }

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
        return m_closed || m_conn->isClosed();
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load() || m_conn->isClosed();
    }

    std::shared_ptr<ColumnarDBPreparedStatement> ColumnarPooledDBConnection::prepareStatement(const std::string &query)
    {
        if (m_closed)
        {
            throw DBException("A61A8B1BF33C", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->prepareStatement(query);
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> ColumnarPooledDBConnection::prepareStatement(std::nothrow_t, const std::string &query) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("A61A8B1BF33C", "Connection is closed", system_utils::captureCallStack()));
            }
            updateLastUsedTime();
            return m_conn->prepareStatement(std::nothrow, query);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("A61A8B1BF33D",
                                                   std::string("prepareStatement failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("A61A8B1BF33E",
                                                   "prepareStatement failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    std::shared_ptr<ColumnarDBResultSet> ColumnarPooledDBConnection::executeQuery(const std::string &query)
    {
        if (m_closed)
        {
            throw DBException("0AF43G67D005", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->executeQuery(query);
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> ColumnarPooledDBConnection::executeQuery(std::nothrow_t, const std::string &query) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("0AF43G67D005", "Connection is closed", system_utils::captureCallStack()));
            }
            updateLastUsedTime();
            return m_conn->executeQuery(std::nothrow, query);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("0AF43G67D006",
                                                   std::string("executeQuery failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("0AF43G67D007",
                                                   "executeQuery failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    uint64_t ColumnarPooledDBConnection::executeUpdate(const std::string &query)
    {
        if (m_closed)
        {
            throw DBException("4E6D7284F5E2", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->executeUpdate(query);
    }

    cpp_dbc::expected<uint64_t, DBException> ColumnarPooledDBConnection::executeUpdate(std::nothrow_t, const std::string &query) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("4E6D7284F5E2", "Connection is closed", system_utils::captureCallStack()));
            }
            updateLastUsedTime();
            return m_conn->executeUpdate(std::nothrow, query);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("4E6D7284F5E3",
                                                   std::string("executeUpdate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("4E6D7284F5E4",
                                                   "executeUpdate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    bool ColumnarPooledDBConnection::beginTransaction()
    {
        if (m_closed)
        {
            throw DBException("C8D9E0F1G2H3", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->beginTransaction();
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("C8D9E0F1G2H3", "Connection is closed", system_utils::captureCallStack()));
            }
            updateLastUsedTime();
            return m_conn->beginTransaction(std::nothrow);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("C8D9E0F1G2H4",
                                                   std::string("beginTransaction failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("C8D9E0F1G2H5",
                                                   "beginTransaction failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    void ColumnarPooledDBConnection::commit()
    {
        if (m_closed)
        {
            throw DBException("F43ECCD8427F", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        m_conn->commit();
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::commit(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("F43ECCD8427F", "Connection is closed", system_utils::captureCallStack()));
            }
            updateLastUsedTime();
            return m_conn->commit(std::nothrow);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F43ECCD84280",
                                                   std::string("commit failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F43ECCD84281",
                                                   "commit failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    void ColumnarPooledDBConnection::rollback()
    {
        if (m_closed)
        {
            throw DBException("1A5723BF02C7", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        m_conn->rollback();
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::rollback(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("1A5723BF02C7", "Connection is closed", system_utils::captureCallStack()));
            }
            updateLastUsedTime();
            return m_conn->rollback(std::nothrow);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("1A5723BF02C8",
                                                   std::string("rollback failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("1A5723BF02C9",
                                                   "rollback failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
    }

    std::string ColumnarPooledDBConnection::getURL() const
    {
        if (m_closed)
        {
            throw DBException("B5C8D0E3F6G2", "Connection is closed", system_utils::captureCallStack());
        }
        // Can't modify m_lastUsedTime in a const method
        return m_conn->getURL();
    }

    cpp_dbc::expected<std::string, DBException> ColumnarPooledDBConnection::getURL(std::nothrow_t) const noexcept
    {
        if (m_closed)
        {
            return cpp_dbc::unexpected(DBException("VUMH1XH7ZJ05", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    std::chrono::time_point<std::chrono::steady_clock> ColumnarPooledDBConnection::getCreationTime() const
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> ColumnarPooledDBConnection::getLastUsedTime() const
    {
        return m_lastUsedTime.load(std::memory_order_relaxed);
    }

    void ColumnarPooledDBConnection::setActive(bool isActive)
    {
        m_active = isActive;
    }

    bool ColumnarPooledDBConnection::isActive() const
    {
        return m_active;
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::returnToPool(std::nothrow_t) noexcept
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
                    poolShared->returnConnection(std::static_pointer_cast<ColumnarPooledDBConnection>(this->shared_from_this()));
                }
            }
        }
        catch (const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("ColumnarPooledDBConnection::returnToPool - shared_from_this failed: %s", ex.what());
            m_closed.store(true);
        }
        catch (const std::exception &ex)
        {
            CP_DEBUG("ColumnarPooledDBConnection::returnToPool - Exception: %s", ex.what());
            m_closed.store(true);
        }
        catch (...) // NOSONAR - Catch-all to ensure m_closed is always set correctly on any exception
        {
            CP_DEBUG("ColumnarPooledDBConnection::returnToPool - Unknown exception caught");
            m_closed.store(true);
        }
        return {};
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
        return this->m_active == false;
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return !m_active.load();
    }

    void ColumnarPooledDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::reset(std::nothrow_t) noexcept
    {
        return prepareForPoolReturn(std::nothrow);
    }

    void ColumnarPooledDBConnection::prepareForPoolReturn()
    {
        auto result = prepareForPoolReturn(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::prepareForPoolReturn(std::nothrow_t) noexcept
    {
        return m_conn->prepareForPoolReturn(std::nothrow);
    }

    std::shared_ptr<DBConnection> ColumnarPooledDBConnection::getUnderlyingConnection()
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

    std::shared_ptr<ColumnarDBConnection> ColumnarPooledDBConnection::getUnderlyingColumnarConnection()
    {
        return m_conn;
    }

    // ScyllaDB connection pool implementation
    namespace ScyllaDB
    {
        ScyllaConnectionPool::ScyllaConnectionPool(DBConnectionPool::ConstructorTag,
                                                   const std::string &url,
                                                   const std::string &username,
                                                   const std::string &password)
            : ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag{}, url, username, password)
        {
            // Scylla-specific initialization if needed
        }

        ScyllaConnectionPool::ScyllaConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
            : ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag{}, config)
        {
            // Scylla-specific initialization if needed
        }

        std::shared_ptr<ScyllaConnectionPool> ScyllaConnectionPool::create(const std::string &url,
                                                                           const std::string &username,
                                                                           const std::string &password)
        {
            auto pool = std::make_shared<ScyllaConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<ScyllaConnectionPool> ScyllaConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::make_shared<ScyllaConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
            pool->initializePool();
            return pool;
        }
    }

} // namespace cpp_dbc
