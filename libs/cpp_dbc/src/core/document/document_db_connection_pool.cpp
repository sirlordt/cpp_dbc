/*
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
 * @file document_db_connection_pool.cpp
 * @brief Connection pool implementation for document databases
 */

#include "cpp_dbc/core/document/document_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "../connection_pool_internal.hpp"
#include <algorithm>
#include <ranges>

namespace cpp_dbc
{

    // DocumentDBConnectionPool implementation
    DocumentDBConnectionPool::DocumentDBConnectionPool(DBConnectionPool::ConstructorTag,
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
        m_allConnections.reserve(m_maxSize);
        // Note: Initial connections are created in the factory method after construction
        // to ensure shared_from_this() works correctly
    }

    DocumentDBConnectionPool::DocumentDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
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
        m_allConnections.reserve(m_maxSize);
        // Note: Initial connections are created in the factory method after construction
        // to ensure shared_from_this() works correctly
    }

    void DocumentDBConnectionPool::initializePool()
    {
        try
        {
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
            m_maintenanceThread = std::jthread(&DocumentDBConnectionPool::maintenanceTask, this);
        }
        catch (const std::exception &ex)
        {
            close();
            throw DBException("E5B7034FDB2F", "Failed to initialize connection pool: " + std::string(ex.what()), system_utils::captureCallStack());
        }
    }

    std::shared_ptr<DocumentDBConnectionPool> DocumentDBConnectionPool::create(const std::string &url,
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
        auto pool = std::make_shared<DocumentDBConnectionPool>(
            DBConnectionPool::ConstructorTag{}, url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, validationQuery, transactionIsolation);

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    std::shared_ptr<DocumentDBConnectionPool> DocumentDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto pool = std::make_shared<DocumentDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    DocumentDBConnectionPool::~DocumentDBConnectionPool()
    {
        CP_DEBUG("DocumentDBConnectionPool::~DocumentDBConnectionPool - Starting destructor at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Use qualified call to avoid virtual dispatch in destructor
        DocumentDBConnectionPool::close();

        CP_DEBUG("DocumentDBConnectionPool::~DocumentDBConnectionPool - Destructor completed at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    std::shared_ptr<DocumentDBConnection> DocumentDBConnectionPool::createDBConnection()
    {
        auto dbConn = DriverManager::getDBConnection(m_url, m_username, m_password, m_options);
        auto documentConn = std::dynamic_pointer_cast<DocumentDBConnection>(dbConn);
        if (!documentConn)
        {
            throw DBException("1EA1E853ED8F", "Connection pool only supports document database connections", system_utils::captureCallStack());
        }
        return documentConn;
    }

    std::shared_ptr<DocumentPooledDBConnection> DocumentDBConnectionPool::createPooledDBConnection()
    {
        auto conn = createDBConnection();

        // Create pooled connection with weak_ptr
        std::weak_ptr<DocumentDBConnectionPool> weakPool;
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
            CP_DEBUG("DocumentDBConnectionPool::createPooledDBConnection - Pool not managed by shared_ptr: %s", ex.what());
        }

        // Create the pooled connection
        auto pooledConn = std::make_shared<DocumentPooledDBConnection>(conn, weakPool, m_poolAlive);
        return pooledConn;
    }

    bool DocumentDBConnectionPool::validateConnection(std::shared_ptr<DocumentDBConnection> conn) const
    {
        try
        {
            // Use ping to validate document database connection
            return conn->ping();
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            CP_DEBUG("DocumentDBConnectionPool::validateConnection - Exception: %s", ex.what());
            return false;
        }
    }

    void DocumentDBConnectionPool::returnConnection(std::shared_ptr<DocumentPooledDBConnection> conn)
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
                conn->getUnderlyingDocumentConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - Exception closing connection during shutdown: %s", ex.what());
            }
            return;
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive())
        {
            CP_DEBUG("DocumentDBConnectionPool::returnConnection - Connection is not active, SKIPPED");
            return;
        }

        // Additional safety check: verify connection is in allConnections (detect orphan)
        {
            std::scoped_lock lockPool(m_mutexPool);
            auto it = std::ranges::find(m_allConnections, conn);
            if (it == m_allConnections.end())
            {
                conn->setActive(false);
                m_activeConnections--;
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - Connection not found in pool, SKIPPED");
                m_connectionAvailable.notify_one();
                return;
            }
        }

        bool valid = true;

        // Phase 1: I/O outside lock (validation and pool-return prep)
        if (m_testOnReturn)
        {
            valid = validateConnection(conn->getUnderlyingDocumentConnection());
        }

        if (valid)
        {
            auto result = conn->getUnderlyingDocumentConnection()->prepareForPoolReturn(std::nothrow);
            if (!result.has_value())
            {
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - prepareForPoolReturn failed: %s", result.error().what_s().c_str());
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
                std::queue<std::shared_ptr<DocumentPooledDBConnection>> tempQueue;
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

                // Notify inside lock: freed slot allows waiting threads to create
                m_connectionAvailable.notify_one();
            }

            // Close outside lock
            try
            {
                conn->getUnderlyingDocumentConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - Exception closing invalid connection: %s", ex.what());
            }

            // Replenish: create outside lock
            if (m_running.load())
            {
                std::shared_ptr<DocumentPooledDBConnection> replacement;
                try
                {
                    replacement = createPooledDBConnection();
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    CP_DEBUG("DocumentDBConnectionPool::returnConnection - Exception creating replacement: %s", ex.what());
                }

                if (replacement)
                {
                    std::shared_ptr<DocumentPooledDBConnection> discardReplacement;
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
                            // Pool full or closing — discard replacement
                            replacement->m_closed.store(true);
                            discardReplacement = replacement;
                        }
                    }

                    // Close discarded replacement outside lock
                    if (discardReplacement)
                    {
                        auto closeResult = discardReplacement->getUnderlyingDocumentConnection()->close(std::nothrow);
                        if (!closeResult.has_value())
                        {
                            CP_DEBUG("DocumentDBConnectionPool::returnConnection - close() on discarded replacement failed: %s", closeResult.error().what_s().c_str());
                        }
                    }
                }
            }
        }
    }

    std::shared_ptr<DBConnection> DocumentDBConnectionPool::getDBConnection()
    {
        return getDocumentDBConnection();
    }

    std::shared_ptr<DocumentDBConnection> DocumentDBConnectionPool::getDocumentDBConnection()
    {
        using namespace std::chrono;
        auto deadline = steady_clock::now() + milliseconds(m_maxWaitMillis);

        while (true)
        {
            std::shared_ptr<DocumentPooledDBConnection> result;
            bool handedOff = false;

            {
                std::unique_lock lockPool(m_mutexPool);

                if (!m_running.load())
                {
                    throw DBException("26405F5CF154", "Connection pool is closed", system_utils::captureCallStack());
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

                        std::shared_ptr<DocumentPooledDBConnection> newConn;
                        try
                        {
                            newConn = createPooledDBConnection();
                        }
                        catch ([[maybe_unused]] const std::exception &ex)
                        {
                            CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - Exception creating connection: %s", ex.what());
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
                                auto closeResult = newConn->getUnderlyingDocumentConnection()->close(std::nothrow);
                                if (!closeResult.has_value())
                                {
                                    CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - close() on discarded candidate failed: %s", closeResult.error().what_s().c_str());
                                }
                                lockPool.lock();
                            }
                        }
                        // Always restart outer loop: if result is set it exits,
                        // otherwise idle is re-checked before waiting (matches KV).
                        // This prevents missing connections returned during the unlock.
                        continue;
                    }

                    // Step 3: Wait for a connection to become available.
                    // CRITICAL: Inner loop keeps the request at its FIFO position.
                    // Non-fulfilled wakeups must NOT dequeue and re-enqueue — that
                    // would push the thread to the back, causing deadline starvation.
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
                            // Safety: remove from queue if wait_until throws (extremely rare)
                            std::erase(m_waitQueue, &req);
                            throw;
                        }

                        // FIRST: check fulfilled (takes priority over timeout)
                        if (req.fulfilled)
                        {
                            result = req.conn;
                            handedOff = true;
                            break;
                        }

                        if (!m_running.load())
                        {
                            std::erase(m_waitQueue, &req);
                            throw DBException("04E13757D18B", "Connection pool is closed", system_utils::captureCallStack());
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

                            throw DBException("D2472D7067A8", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
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
                    !validateConnection(result->getUnderlyingDocumentConnection()))
                {
                    CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - Validation failed, removing connection");

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

                    auto closeResult = result->getUnderlyingDocumentConnection()->close(std::nothrow);
                    if (!closeResult.has_value())
                    {
                        CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - close() on invalid connection failed: %s", closeResult.error().what_s().c_str());
                    }

                    // Check deadline before retrying
                    if (steady_clock::now() >= deadline)
                    {
                        throw DBException("FFA42D3264B6", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                    }

                    continue; // Retry
                }
            }

            return result;
        }
    }

    void DocumentDBConnectionPool::maintenanceTask()
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
                        std::queue<std::shared_ptr<DocumentPooledDBConnection>> tempQueue;
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
                std::shared_ptr<DocumentPooledDBConnection> pooledConn;
                try
                {
                    pooledConn = createPooledDBConnection();
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    CP_DEBUG("DocumentDBConnectionPool::maintenanceTask - Exception creating minIdle connection: %s", ex.what());
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

    size_t DocumentDBConnectionPool::getActiveDBConnectionCount() const
    {
        return m_activeConnections;
    }

    size_t DocumentDBConnectionPool::getIdleDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexPool);
        return m_idleConnections.size();
    }

    size_t DocumentDBConnectionPool::getTotalDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexPool);
        return m_allConnections.size();
    }

    void DocumentDBConnectionPool::close()
    {
        CP_DEBUG("DocumentDBConnectionPool::close - Starting close operation");

        if (!m_running.exchange(false))
        {
            CP_DEBUG("DocumentDBConnectionPool::close - Already closed, returning");
            return; // Already closed
        }

        // Mark pool as no longer alive - this prevents pooled connections from trying to return
        if (m_poolAlive)
        {
            m_poolAlive->store(false);
        }

        CP_DEBUG("DocumentDBConnectionPool::close - Waiting for active operations to complete at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            CP_DEBUG("DocumentDBConnectionPool::close - Initial active connections: %d", m_activeConnections.load());

            while (m_activeConnections.load() > 0)
            {
                CP_DEBUG("DocumentDBConnectionPool::close - Waiting for %d active connections to finish at %lld", m_activeConnections.load(), (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 1 == 0)
                {
                    CP_DEBUG("DocumentDBConnectionPool::close - Waited %ld seconds for active connections", elapsed_seconds);
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("DocumentDBConnectionPool::close - Timeout waiting for active connections, forcing close at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

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
        std::vector<std::shared_ptr<DocumentPooledDBConnection>> connectionsToClose;
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
                CP_DEBUG("DocumentDBConnectionPool::close - Exception during connection close: %s", ex.what());
            }
        }
    }

    bool DocumentDBConnectionPool::isRunning() const
    {
        return m_running.load();
    }

    // DocumentPooledDBConnection implementation
    DocumentPooledDBConnection::DocumentPooledDBConnection(
        std::shared_ptr<DocumentDBConnection> connection,
        std::weak_ptr<DocumentDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive)
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive)
    {
        m_creationTime = std::chrono::steady_clock::now();
        m_lastUsedTime = m_creationTime;
    }

    bool DocumentPooledDBConnection::isPoolValid() const
    {
        return m_poolAlive && m_poolAlive->load();
    }

    DocumentPooledDBConnection::~DocumentPooledDBConnection()
    {
        if (!m_closed && m_conn)
        {
            try
            {
                // If the pool is no longer alive, close the physical connection.
                // Use qualified call to avoid virtual dispatch in destructor.
                // NOSONAR(cpp:S1699) - intentional qualified call in destructor to avoid virtual dispatch
                if (!DocumentPooledDBConnection::isPoolValid())
                {
                    m_conn->close();
                }
                else
                {
                    DocumentPooledDBConnection::returnToPool(); // NOSONAR(cpp:S1699) — qualified call in destructor intentional to avoid virtual dispatch
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("DocumentPooledDBConnection::~DocumentPooledDBConnection - Exception: %s", ex.what());
            }
        }
    }

    expected<void, DBException> DocumentPooledDBConnection::close(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the close
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
                    // will set m_closed back to true, maintaining correct error handling semantics.
                    // ============================================================================
                    m_closed.store(false);
                    poolShared->returnConnection(std::static_pointer_cast<DocumentPooledDBConnection>(this->shared_from_this()));
                }
            }
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("DocumentPooledDBConnection::close - shared_from_this failed: %s", ex.what());
            m_closed = true;
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            CP_DEBUG("DocumentPooledDBConnection::close - Exception: %s", ex.what());
            m_closed = true;
        }
        catch (...) // NOSONAR - Catch-all to ensure m_closed is always set correctly on any exception
        {
            CP_DEBUG("DocumentPooledDBConnection::close - Unknown exception caught");
            m_closed = true;
        }
        return {};
    }

    void DocumentPooledDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool DocumentPooledDBConnection::isClosed() const
    {
        return m_closed || m_conn->isClosed();
    }

    expected<bool, DBException> DocumentPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load() || m_conn->isClosed();
    }

    // Implementation of DBConnectionPooled interface methods
    std::chrono::time_point<std::chrono::steady_clock> DocumentPooledDBConnection::getCreationTime() const
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> DocumentPooledDBConnection::getLastUsedTime() const
    {
        return m_lastUsedTime.load(std::memory_order_relaxed);
    }

    void DocumentPooledDBConnection::setActive(bool isActive)
    {
        m_active = isActive;
    }

    bool DocumentPooledDBConnection::isActive() const
    {
        return m_active;
    }

    expected<void, DBException> DocumentPooledDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        return close(std::nothrow);
    }

    void DocumentPooledDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool DocumentPooledDBConnection::isPooled() const
    {
        return this->m_active == false;
    }

    expected<bool, DBException> DocumentPooledDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return !m_active.load();
    }

    std::string DocumentPooledDBConnection::getURL() const
    {
        if (m_closed)
        {
            throw DBException("3FB54DDBA7BA", "Connection is closed", system_utils::captureCallStack());
        }
        // Can't modify m_lastUsedTime in a const method
        return m_conn->getURL();
    }

    expected<std::string, DBException> DocumentPooledDBConnection::getURL(std::nothrow_t) const noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("KV03UAMVXQX7", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    void DocumentPooledDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    expected<void, DBException> DocumentPooledDBConnection::reset(std::nothrow_t) noexcept
    {
        return prepareForPoolReturn(std::nothrow);
    }

    void DocumentPooledDBConnection::prepareForPoolReturn()
    {
        auto result = prepareForPoolReturn(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    expected<void, DBException> DocumentPooledDBConnection::prepareForPoolReturn(std::nothrow_t) noexcept
    {
        return m_conn->prepareForPoolReturn(std::nothrow);
    }

    std::shared_ptr<DBConnection> DocumentPooledDBConnection::getUnderlyingConnection()
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

    std::shared_ptr<DocumentDBConnection> DocumentPooledDBConnection::getUnderlyingDocumentConnection()
    {
        return m_conn;
    }

    // Implementation of DocumentDBConnection interface methods
    std::string DocumentPooledDBConnection::getDatabaseName() const
    {
        if (m_closed)
        {
            throw DBException("D16CD1398113", "Connection is closed", system_utils::captureCallStack());
        }
        return m_conn->getDatabaseName();
    }

    std::vector<std::string> DocumentPooledDBConnection::listDatabases()
    {
        if (m_closed)
        {
            throw DBException("E214F2D3D2AC", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->listDatabases();
    }

    bool DocumentPooledDBConnection::databaseExists(const std::string &databaseName)
    {
        if (m_closed)
        {
            throw DBException("AEF208932857", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->databaseExists(databaseName);
    }

    void DocumentPooledDBConnection::useDatabase(const std::string &databaseName)
    {
        if (m_closed)
        {
            throw DBException("A188FEBF0840", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        m_conn->useDatabase(databaseName);
    }

    void DocumentPooledDBConnection::dropDatabase(const std::string &databaseName)
    {
        if (m_closed)
        {
            throw DBException("33ABD0122921", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        m_conn->dropDatabase(databaseName);
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::getCollection(const std::string &collectionName)
    {
        if (m_closed)
        {
            throw DBException("8781A8112718", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->getCollection(collectionName);
    }

    std::vector<std::string> DocumentPooledDBConnection::listCollections()
    {
        if (m_closed)
        {
            throw DBException("4B1920591BBF", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->listCollections();
    }

    bool DocumentPooledDBConnection::collectionExists(const std::string &collectionName)
    {
        if (m_closed)
        {
            throw DBException("8074225964F3", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->collectionExists(collectionName);
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::createCollection(
        const std::string &collectionName, const std::string &options)
    {
        if (m_closed)
        {
            throw DBException("E7BD2C9371A2", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->createCollection(collectionName, options);
    }

    void DocumentPooledDBConnection::dropCollection(const std::string &collectionName)
    {
        if (m_closed)
        {
            throw DBException("B948C3233D2E", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        m_conn->dropCollection(collectionName);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument()
    {
        if (m_closed)
        {
            throw DBException("A551087D0C0F", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->createDocument();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument(const std::string &json)
    {
        if (m_closed)
        {
            throw DBException("57A932F74BC9", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->createDocument(json);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::runCommand(const std::string &command)
    {
        if (m_closed)
        {
            throw DBException("2D95D9EF03AA", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->runCommand(command);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerInfo()
    {
        if (m_closed)
        {
            throw DBException("8ED95C914BAD", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->getServerInfo();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerStatus()
    {
        if (m_closed)
        {
            throw DBException("162B1BDB50A9", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->getServerStatus();
    }

    bool DocumentPooledDBConnection::ping()
    {
        if (m_closed)
        {
            throw DBException("9BCD37627AAD", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->ping();
    }

    std::string DocumentPooledDBConnection::startSession()
    {
        if (m_closed)
        {
            throw DBException("12B829E08231", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->startSession();
    }

    void DocumentPooledDBConnection::endSession(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("7CB8D9E25BF3", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        m_conn->endSession(sessionId);
    }

    void DocumentPooledDBConnection::startTransaction(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("A6BB94FF27F4", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        m_conn->startTransaction(sessionId);
    }

    void DocumentPooledDBConnection::commitTransaction(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("1AF497A444D9", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        m_conn->commitTransaction(sessionId);
    }

    void DocumentPooledDBConnection::abortTransaction(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("4A713C729CE4", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        m_conn->abortTransaction(sessionId);
    }

    bool DocumentPooledDBConnection::supportsTransactions()
    {
        if (m_closed)
        {
            throw DBException("2FAE3A027B77", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime();
        return m_conn->supportsTransactions();
    }

    // ====================================================================
    // NOTHROW VERSIONS - delegate to underlying connection
    // ====================================================================

    expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listDatabases(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("7A8B9C0D1E2F", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->listDatabases(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::getCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("8B9C0D1E2F3A", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->getCollection(std::nothrow, collectionName);
    }

    expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listCollections(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("9C0D1E2F3A4B", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->listCollections(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::createCollection(
        std::nothrow_t,
        const std::string &collectionName,
        const std::string &options) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("0D1E2F3A4B5C", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->createCollection(std::nothrow, collectionName, options);
    }

    expected<void, DBException> DocumentPooledDBConnection::dropCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("1E2F3A4B5C6D", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->dropCollection(std::nothrow, collectionName);
    }

    expected<void, DBException> DocumentPooledDBConnection::dropDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("2F3A4B5C6D7E", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->dropDatabase(std::nothrow, databaseName);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("3A4B5C6D7E8F", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->createDocument(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(
        std::nothrow_t, const std::string &json) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("4B5C6D7E8F9A", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->createDocument(std::nothrow, json);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::runCommand(
        std::nothrow_t, const std::string &command) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("5C6D7E8F9A0B", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->runCommand(std::nothrow, command);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("6D7E8F9A0B1C", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->getServerInfo(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerStatus(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("7E8F9A0B1C2D", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->getServerStatus(std::nothrow);
    }

    expected<std::string, DBException> DocumentPooledDBConnection::getDatabaseName(std::nothrow_t) const noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("QI6DG2WF85Q2", "Connection is closed"));
        }
        return m_conn->getDatabaseName(std::nothrow);
    }

    expected<bool, DBException> DocumentPooledDBConnection::databaseExists(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("FABBX2QR62GT", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->databaseExists(std::nothrow, databaseName);
    }

    expected<void, DBException> DocumentPooledDBConnection::useDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("CFZMARADNIPZ", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->useDatabase(std::nothrow, databaseName);
    }

    expected<bool, DBException> DocumentPooledDBConnection::collectionExists(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("AEDVQNDH5OBR", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->collectionExists(std::nothrow, collectionName);
    }

    expected<bool, DBException> DocumentPooledDBConnection::ping(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("218DK7FXM1U4", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->ping(std::nothrow);
    }

    expected<std::string, DBException> DocumentPooledDBConnection::startSession(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("WTWINKFWO8WZ", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->startSession(std::nothrow);
    }

    expected<void, DBException> DocumentPooledDBConnection::endSession(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("X7VNI3HTM5NN", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->endSession(std::nothrow, sessionId);
    }

    expected<void, DBException> DocumentPooledDBConnection::startTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("YYJXBAMEI07O", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->startTransaction(std::nothrow, sessionId);
    }

    expected<void, DBException> DocumentPooledDBConnection::commitTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("VQ2C4Y9YU1LX", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->commitTransaction(std::nothrow, sessionId);
    }

    expected<void, DBException> DocumentPooledDBConnection::abortTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("A7VGP008305O", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->abortTransaction(std::nothrow, sessionId);
    }

    expected<bool, DBException> DocumentPooledDBConnection::supportsTransactions(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("PLV8XFKGJ0PO", "Connection is closed"));
        }
        updateLastUsedTime();
        return m_conn->supportsTransactions(std::nothrow);
    }

    // MongoDB connection pool implementation
    namespace MongoDB
    {
        MongoDBConnectionPool::MongoDBConnectionPool(DBConnectionPool::ConstructorTag tag,
                                                     const std::string &url,
                                                     const std::string &username,
                                                     const std::string &password)
            : DocumentDBConnectionPool(tag, url, username, password)
        {
            // MongoDB-specific initialization if needed
        }

        MongoDBConnectionPool::MongoDBConnectionPool(DBConnectionPool::ConstructorTag tag, const config::DBConnectionPoolConfig &config)
            : DocumentDBConnectionPool(tag, config)
        {
            // MongoDB-specific initialization if needed
        }

        std::shared_ptr<MongoDBConnectionPool> MongoDBConnectionPool::create(const std::string &url,
                                                                             const std::string &username,
                                                                             const std::string &password)
        {
            auto pool = std::make_shared<MongoDBConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<MongoDBConnectionPool> MongoDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::make_shared<MongoDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
            pool->initializePool();
            return pool;
        }
    }

} // namespace cpp_dbc
