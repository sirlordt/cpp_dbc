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
 * @file document_db_connection_pool.cpp
 * @brief Connection pool implementation for document databases
 */

#include "cpp_dbc/core/document/document_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "../connection_pool_internal.hpp"
#include <algorithm>

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
          m_transactionIsolation(transactionIsolation)
    {
        // Pool initialization will be done in the factory method
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
          m_transactionIsolation(config.getTransactionIsolation())
    {
        // Pool initialization will be done in the factory method
    }

#ifdef __cpp_exceptions
    cpp_dbc::expected<void, DBException> DocumentDBConnectionPool::initializePool(std::nothrow_t) noexcept
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

            // Start maintenance thread — std::jthread constructor can throw std::system_error
            m_maintenanceThread = std::jthread([this]
                                               { maintenanceTask(); });
        }
        catch (const std::exception &ex)
        {
            close(std::nothrow);
            return cpp_dbc::unexpected(DBException("6I4U7QKHYH5P", "Failed to initialize connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            close(std::nothrow);
            return cpp_dbc::unexpected(DBException("HGQGTR9E2DR7", "Failed to initialize connection pool: unknown error", system_utils::captureCallStack()));
        }
        return {};
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnectionPool>, DBException> DocumentDBConnectionPool::create(std::nothrow_t,
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
            auto pool = std::make_shared<DocumentDBConnectionPool>(
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
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("TSH2S7H56QKI", "Failed to create connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("CQ827ZOIDL0H", "Failed to create connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnectionPool>, DBException> DocumentDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        try
        {
            auto pool = std::make_shared<DocumentDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

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
            return cpp_dbc::unexpected(DBException("TSH2S7H56QKI", "Failed to create connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("CQ827ZOIDL0H", "Failed to create connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException>
    DocumentDBConnectionPool::createDBConnection(std::nothrow_t) noexcept
    {
        auto dbConnResult = DriverManager::getDBConnection(std::nothrow, m_url, m_username, m_password, m_options);
        if (!dbConnResult.has_value())
        {
            return cpp_dbc::unexpected(dbConnResult.error());
        }

        auto documentConn = std::dynamic_pointer_cast<DocumentDBConnection>(dbConnResult.value());
        if (!documentConn)
        {
            return cpp_dbc::unexpected(DBException("1EA1E853ED8F", "Connection pool only supports document database connections", system_utils::captureCallStack()));
        }
        return documentConn;
    }

    cpp_dbc::expected<std::shared_ptr<DocumentPooledDBConnection>, DBException>
    DocumentDBConnectionPool::createPooledDBConnection(std::nothrow_t) noexcept
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
        std::weak_ptr<DocumentDBConnectionPool> weakPool;
        try
        {
            weakPool = shared_from_this();
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            // Pool is not managed by shared_ptr, weakPool remains empty (stack-allocated pool)
            CP_DEBUG("DocumentDBConnectionPool::createPooledDBConnection - Pool not managed by shared_ptr: %s", ex.what());
        }

        return std::make_shared<DocumentPooledDBConnection>(conn, weakPool, m_poolAlive);
    }
#endif // __cpp_exceptions

    // ====================================================================
    // DocumentDBConnectionPool — Pure nothrow methods (always compiled)
    // ====================================================================

    DocumentDBConnectionPool::~DocumentDBConnectionPool()
    {
        CP_DEBUG("DocumentDBConnectionPool::~DocumentDBConnectionPool - Starting destructor at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Use qualified call to avoid virtual dispatch in destructor
        auto result = DocumentDBConnectionPool::close(std::nothrow);
        if (!result.has_value())
        {
            CP_DEBUG("DocumentDBConnectionPool::~DocumentDBConnectionPool - close failed: %s", result.error().what_s().data());
        }

        CP_DEBUG("DocumentDBConnectionPool::~DocumentDBConnectionPool - Destructor completed at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    cpp_dbc::expected<bool, DBException>
    DocumentDBConnectionPool::validateConnection(std::nothrow_t, std::shared_ptr<DBConnection> conn) const noexcept
    {
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("GCEHF37I0W51", "validateConnection called with null connection", system_utils::captureCallStack()));
        }

        auto result = conn->ping(std::nothrow);
        if (!result.has_value())
        {
            CP_DEBUG("DocumentDBConnectionPool::validateConnection - ping failed: %s", result.error().what_s().data());
            return cpp_dbc::unexpected(result.error());
        }
        return result.value();
    }

    void DocumentDBConnectionPool::notifyFirstWaiter(std::nothrow_t) noexcept
    {
        if (!m_waitQueue.empty())
        {
            m_waitQueue.front()->cv.notify_one();
        }
    }

    cpp_dbc::expected<void, DBException>
    DocumentDBConnectionPool::returnConnection(std::nothrow_t, std::shared_ptr<DocumentPooledDBConnection> conn) noexcept
    {
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("7U7UT1V3IBET", "returnConnection called with null connection", system_utils::captureCallStack()));
        }

        if (!m_running.load(std::memory_order_acquire))
        {
            conn->setActive(std::nothrow, false);
            m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
            auto closeResult = conn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - close() during shutdown failed: %s", closeResult.error().what_s().data());
            }
            return {};
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive(std::nothrow))
        {
            CP_DEBUG("DocumentDBConnectionPool::returnConnection - Connection is not active, SKIPPED");
            return cpp_dbc::unexpected(DBException("ASAQU4YNA02C", "returnConnection called on an already inactive connection (double-return bug)", system_utils::captureCallStack()));
        }

        // Additional safety check: verify connection is in allConnections (detect orphan)
        {
            std::scoped_lock lockPool(m_mutexPool);
            auto it = std::ranges::find(m_allConnections, conn);
            if (it == m_allConnections.end())
            {
                conn->setActive(std::nothrow, false);
                m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - ORPHAN: connection not in allConnections, active=%d", m_activeConnections.load(std::memory_order_acquire));
                notifyFirstWaiter(std::nothrow);
                return cpp_dbc::unexpected(DBException("AX95P6TXEL5I", "returnConnection called with a connection not belonging to this pool (orphan)", system_utils::captureCallStack()));
            }
        }

        bool valid = true;

        // Phase 1: I/O outside lock (validation and pool-return prep)
        if (m_testOnReturn)
        {
            valid = validateConnection(std::nothrow, conn->getUnderlyingConnection(std::nothrow)).value_or(false);
        }

        if (valid && !conn->m_closed.load(std::memory_order_acquire))
        {
            auto prepReturnPoolResult = conn->getUnderlyingConnection(std::nothrow)->prepareForPoolReturn(
                std::nothrow, m_transactionIsolation);
            if (!prepReturnPoolResult.has_value())
            {
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - prepareForPoolReturn failed: %s", prepReturnPoolResult.error().what_s().data());
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
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - HANDOFF: direct to waiter, active=%d, waiters=%zu, total=%zu",
                         m_activeConnections.load(std::memory_order_acquire), m_waitQueue.size(), m_allConnections.size());
                request->cv.notify_one(); // Per-waiter: only the fulfilled waiter is woken
            }
            else
            {
                m_idleConnections.push(conn);
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - VALID: pushed to idle, active=%d, idle=%zu, total=%zu",
                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                // No notification needed: waitQueue is guaranteed empty here (checked at line 366).
                // The next borrower will find this idle connection directly in Step 1.
            }
        }
        else
        {
            // Invalid connection: remove from pool, close outside lock, then replenish
            {
                std::scoped_lock lockPool(m_mutexPool);
                conn->setActive(std::nothrow, false);
                conn->m_closed.store(true, std::memory_order_release); // prevent destructor double-decrement
                m_activeConnections.fetch_sub(1, std::memory_order_acq_rel);
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - INVALID: active=%d", m_activeConnections.load(std::memory_order_acquire));

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
                m_idleConnections = tempQueue;

                // Notify first waiter: freed slot allows re-check of deadline or idle
                notifyFirstWaiter(std::nothrow);
            }

            // Step 2: Close invalid connection OUTSIDE locks (I/O)
            {
                auto closeResult = conn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                if (!closeResult.has_value())
                {
                    CP_DEBUG("DocumentDBConnectionPool::returnConnection - close() on invalid connection failed: %s", closeResult.error().what_s().data());
                }
            }

            // Signal maintenance thread to create replacement asynchronously.
            // The returning thread is NOT blocked by connection creation I/O
            // (which takes 1.3-1.5s+ under Valgrind). Borrowers self-heal via
            // Step 2 in getDocumentDBConnection() if the pool is temporarily short.
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

    cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> DocumentDBConnectionPool::getDBConnection(std::nothrow_t) noexcept
    {
        auto result = getDocumentDBConnection(std::nothrow);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        return std::static_pointer_cast<DBConnection>(result.value());
    }

#ifdef __cpp_exceptions
    std::shared_ptr<DBConnection> DocumentDBConnectionPool::getDBConnection()
    {
        auto result = getDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException> DocumentDBConnectionPool::getDocumentDBConnection(std::nothrow_t) noexcept
    {
        try
        {
            using namespace std::chrono;
            auto deadline = steady_clock::now() + milliseconds(m_maxWaitMillis);

            while (true)
            {
                std::shared_ptr<DocumentPooledDBConnection> result;
                bool handedOff = false;

                {
                    std::unique_lock lockPool(m_mutexPool);

                    if (!m_running.load(std::memory_order_acquire))
                    {
                        return cpp_dbc::unexpected(DBException("LJVQ3CEDEYL7", "Connection pool is closed", system_utils::captureCallStack()));
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
                            CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - GOT IDLE: active=%d, idle=%zu, total=%zu",
                                     m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                            break;
                        }

                        // Step 2: Create new connection if pool not full
                        if (m_allConnections.size() + m_pendingCreations < m_maxSize)
                        {
                            CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - CREATING: total=%zu, pending=%zu, max=%zu",
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
                                    CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - Failed to create connection: %s", newConnResult.error().what_s().data());
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
                                        CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - DISCARD candidate: total=%zu >= max=%zu", m_allConnections.size(), m_maxSize);
                                        newConn->m_closed.store(true, std::memory_order_release); // Prevent destructor returnToPool()
                                        lockPool.unlock();
                                        auto closeResult = newConn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                                        if (!closeResult.has_value())
                                        {
                                            CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - close() on discarded candidate failed: %s", closeResult.error().what_s().data());
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

                        // Step 3: Wait for a connection to become available.
                        // CRITICAL: Inner loop keeps the request at its FIFO position.
                        // Non-fulfilled wakeups must NOT dequeue and re-enqueue — that
                        // would push the thread to the back, causing deadline starvation.
                        ConnectionRequest request;
                        m_waitQueue.push_back(&request);
                        CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - WAITING on CV: active=%d, idle=%zu, total=%zu, pending=%zu, waiters=%zu",
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
                                // Safety: remove from queue if wait_until throws (extremely rare), then propagate
                                CP_DEBUG("DocumentDBConnectionPool::getConnection - wait_until threw: %s", ex.what());
                                std::erase(m_waitQueue, &request);
                                throw;
                            }
                            catch (...) // NOSONAR — non-std exceptions: remove from queue then propagate
                            {
                                CP_DEBUG("DocumentDBConnectionPool::getConnection - wait_until threw unknown exception");
                                std::erase(m_waitQueue, &request);
                                throw;
                            }

                            // FIRST: check fulfilled (takes priority over timeout)
                            if (request.fulfilled)
                            {
                                result = request.conn;
                                handedOff = true;
                                CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - HANDOFF received: active=%d, idle=%zu, total=%zu",
                                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size());
                                break;
                            }

                            if (!m_running.load(std::memory_order_acquire))
                            {
                                std::erase(m_waitQueue, &request);
                                return cpp_dbc::unexpected(DBException("U9ZUMJ9SDFTN", "Connection pool is closed", system_utils::captureCallStack()));
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
                                    CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - GOT IDLE after timeout: active=%d, idle=%zu",
                                             m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size());
                                    break;
                                }
                                CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - TIMEOUT: active=%d, idle=%zu, total=%zu, pending=%zu",
                                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size(), m_pendingCreations);
                                return cpp_dbc::unexpected(DBException("HF3V83PS83LA", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
                            }

                            // Spurious wakeup: check idle again
                            if (!m_idleConnections.empty())
                            {
                                std::erase(m_waitQueue, &request);
                                result = m_idleConnections.front();
                                m_idleConnections.pop();
                                CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - GOT IDLE after wakeup: active=%d, idle=%zu",
                                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size());
                                break;
                            }

                            // Nothing available — stay in queue at same FIFO position, re-wait.
                            CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - CV WAKEUP (re-waiting): idle=%zu, total=%zu",
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
                        CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - VALIDATING (timeSinceLastUse=%lld ms > %ld ms)", (long long)timeSinceLastUse, m_validationTimeoutMillis);
                    }

                    if (timeSinceLastUse > m_validationTimeoutMillis &&
                        !validateConnection(std::nothrow, result->getUnderlyingConnection(std::nothrow)).value_or(false))
                    {
                        CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - VALIDATION FAILED: removing connection, active=%d", m_activeConnections.load(std::memory_order_acquire));

                        {
                            std::scoped_lock lockPool(m_mutexPool);
                            result->setActive(std::nothrow, false);
                            // Prevent destructor from attempting to return this already-removed
                            // connection to the pool (would fire shared_from_this in destructor context).
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

                        auto closeResult = result->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                        if (!closeResult.has_value())
                        {
                            CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - close() on invalid connection failed: %s", closeResult.error().what_s().data());
                        }

                        // Check deadline before retrying
                        if (steady_clock::now() >= deadline)
                        {
                            return cpp_dbc::unexpected(DBException("FFA42D3264B6", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
                        }

                        continue; // Retry
                    }
                }

                // ========================================
                // Phase 3: prepareForBorrow (outside lock)
                // ========================================
                auto borrowResult = result->getUnderlyingConnection(std::nothrow)->prepareForBorrow(std::nothrow);
                if (!borrowResult.has_value())
                {
                    CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - prepareForBorrow failed: %s", borrowResult.error().what_s().data());
                }

                return cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException>{
                    std::static_pointer_cast<DocumentDBConnection>(result)};
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("DWG5T3W0JL6V", "Exception in getDocumentDBConnection: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR
        {
            return cpp_dbc::unexpected(DBException("I5BAEIO8QXTC", "Unknown exception in getDocumentDBConnection", system_utils::captureCallStack()));
        }
    }

    std::shared_ptr<DocumentDBConnection> DocumentDBConnectionPool::getDocumentDBConnection()
    {
        auto result = getDocumentDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }
#endif // __cpp_exceptions

    void DocumentDBConnectionPool::maintenanceTask()
    {
        do
        {
            {
                // Wait 30 seconds, or wake early on close() or replenish request
                std::unique_lock lock(m_mutexPool);
                m_maintenanceCondition.wait_for(lock, std::chrono::seconds(30), [this]
                                                { return !m_running.load(std::memory_order_acquire) || m_replenishNeeded > 0; });
            }

            if (!m_running.load(std::memory_order_acquire))
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
                        m_idleConnections = tempQueue;

                        it = m_allConnections.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            // Replenish connections that were invalidated in returnConnection().
            // Created here asynchronously so the returning thread is not blocked
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
                        CP_DEBUG("DocumentDBConnectionPool::maintenanceTask - Failed to create replacement: %s",
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
                        CP_DEBUG("DocumentDBConnectionPool::maintenanceTask - REPLACEMENT HANDOFF: active=%d, waiters=%zu, total=%zu",
                                 m_activeConnections.load(std::memory_order_acquire), m_waitQueue.size(), m_allConnections.size());
                        req->cv.notify_one();
                    }
                    else
                    {
                        m_idleConnections.push(pooledResult.value());
                        CP_DEBUG("DocumentDBConnectionPool::maintenanceTask - REPLACEMENT: pushed to idle, active=%d, idle=%zu, total=%zu",
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
                        CP_DEBUG("DocumentDBConnectionPool::maintenanceTask - Failed to create minIdle connection: %s",
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

    cpp_dbc::expected<size_t, DBException> DocumentDBConnectionPool::getActiveDBConnectionCount(std::nothrow_t) const noexcept
    {
        return static_cast<size_t>(m_activeConnections.load(std::memory_order_acquire));
    }

    cpp_dbc::expected<size_t, DBException> DocumentDBConnectionPool::getIdleDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_idleConnections.size();
    }

    cpp_dbc::expected<size_t, DBException> DocumentDBConnectionPool::getTotalDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_allConnections.size();
    }

    cpp_dbc::expected<void, DBException> DocumentDBConnectionPool::close(std::nothrow_t) noexcept
    {
        CP_DEBUG("DocumentDBConnectionPool::close - Starting close operation");

        if (!m_running.exchange(false, std::memory_order_acq_rel))
        {
            CP_DEBUG("DocumentDBConnectionPool::close - Already closed, returning");
            return {}; // Already closed — idempotent
        }

        // Mark pool as no longer alive - this prevents pooled connections from trying to return
        if (m_poolAlive)
        {
            m_poolAlive->store(false, std::memory_order_release);
        }

        CP_DEBUG("DocumentDBConnectionPool::close - Waiting for active operations to complete at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            CP_DEBUG("DocumentDBConnectionPool::close - Initial active connections: %d", m_activeConnections.load(std::memory_order_acquire));

            while (m_activeConnections.load(std::memory_order_acquire) > 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 2 == 0)
                {
                    CP_DEBUG("DocumentDBConnectionPool::close - Waited %ld seconds for %d active connections",
                             elapsed_seconds, m_activeConnections.load(std::memory_order_acquire));
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("DocumentDBConnectionPool::close - Timeout waiting for active connections, forcing close at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                    // Force active connections to be marked as inactive
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

        cpp_dbc::expected<void, DBException> lastError{};

        // Close each connection outside pool locks (per-connection mutex acquired here)
        for (const auto &conn : connectionsToClose)
        {
            if (conn && conn->getUnderlyingConnection(std::nothrow))
            {
                conn->setActive(std::nothrow, false);
                auto r = conn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                if (!r.has_value())
                {
                    CP_DEBUG("DocumentDBConnectionPool::close - connection close failed: %s", r.error().what_s().data());
                    lastError = r;
                }
            }
        }

        return lastError;
    }

    cpp_dbc::expected<bool, DBException> DocumentDBConnectionPool::isRunning(std::nothrow_t) const noexcept
    {
        return m_running.load(std::memory_order_acquire);
    }

#ifdef __cpp_exceptions
    void DocumentDBConnectionPool::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool DocumentDBConnectionPool::isRunning() const
    {
        auto result = isRunning(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t DocumentDBConnectionPool::getActiveDBConnectionCount() const
    {
        auto result = getActiveDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t DocumentDBConnectionPool::getIdleDBConnectionCount() const
    {
        auto result = getIdleDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    size_t DocumentDBConnectionPool::getTotalDBConnectionCount() const
    {
        auto result = getTotalDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }
#endif // __cpp_exceptions

    // ====================================================================
    // DocumentPooledDBConnection — Pure nothrow methods (always compiled)
    // ====================================================================

    DocumentPooledDBConnection::DocumentPooledDBConnection(
        std::shared_ptr<DocumentDBConnection> connection,
        std::weak_ptr<DocumentDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive)
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive)
    {
        // m_creationTime and m_lastUsedTimeNs use in-class initializers
    }

    bool DocumentPooledDBConnection::isPoolValid(std::nothrow_t) const noexcept
    {
        return m_poolAlive && m_poolAlive->load(std::memory_order_acquire) && !m_pool.expired();
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return true;
        }
        return m_conn->isClosed(std::nothrow);
    }

    // Implementation of DBConnectionPooled interface methods
    std::chrono::time_point<std::chrono::steady_clock> DocumentPooledDBConnection::getCreationTime(std::nothrow_t) const noexcept
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> DocumentPooledDBConnection::getLastUsedTime(std::nothrow_t) const noexcept
    {
        return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{m_lastUsedTimeNs.load(std::memory_order_relaxed)}};
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::setActive(std::nothrow_t, bool isActive) noexcept
    {
        m_active.store(isActive, std::memory_order_release);
        return {};
    }

    bool DocumentPooledDBConnection::isActive(std::nothrow_t) const noexcept
    {
        return m_active.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return !m_active.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<std::string, DBException> DocumentPooledDBConnection::getURL(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("KV03UAMVXQX7", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::reset(std::nothrow_t) noexcept
    {
        if (!m_conn)
        {
            return cpp_dbc::unexpected(DBException("0CUYVUE519T6",
                                                    "Underlying connection is null",
                                                    system_utils::captureCallStack()));
        }

        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("DVVC2P4912ZE",
                                                    "Connection is closed",
                                                    system_utils::captureCallStack()));
        }

        updateLastUsedTime(std::nothrow);
        return m_conn->reset(std::nothrow);
    }

    cpp_dbc::expected<void, DBException>
    DocumentPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("YXMF43OOV7WP", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->setTransactionIsolation(std::nothrow, level);
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    DocumentPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("3ZNBTS4N04WD", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getTransactionIsolation(std::nothrow);
    }

    cpp_dbc::expected<void, DBException>
    DocumentPooledDBConnection::prepareForPoolReturn(std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("IHKK4HM07XXA",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForPoolReturn(std::nothrow, isolationLevel);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("LRG1PLMORCRX",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForBorrow(std::nothrow);
    }

    std::shared_ptr<DBConnection> DocumentPooledDBConnection::getUnderlyingConnection(std::nothrow_t) noexcept
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

    // ====================================================================
    // NOTHROW VERSIONS - delegate to underlying connection
    // ====================================================================

    cpp_dbc::expected<std::string, DBException> DocumentPooledDBConnection::getDatabaseName(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("QI6DG2WF85Q2", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getDatabaseName(std::nothrow);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listDatabases(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("U6THWPYMYN4L", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->listDatabases(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::databaseExists(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("FABBX2QR62GT", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->databaseExists(std::nothrow, databaseName);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::useDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("CFZMARADNIPZ", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->useDatabase(std::nothrow, databaseName);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::dropDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("6C55O75AJHS5", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->dropDatabase(std::nothrow, databaseName);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::getCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("G5BB749801OM", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getCollection(std::nothrow, collectionName);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listCollections(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("5C1WAY0V1SOL", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->listCollections(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::collectionExists(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("AEDVQNDH5OBR", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->collectionExists(std::nothrow, collectionName);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::createCollection(
        std::nothrow_t,
        const std::string &collectionName,
        const std::string &options) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("ZU34XKPX5NXO", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createCollection(std::nothrow, collectionName, options);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::dropCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("7239S0NFTH6X", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->dropCollection(std::nothrow, collectionName);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("92MQ8YF1833Z", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createDocument(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(
        std::nothrow_t, const std::string &json) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("X02O1QY16VJ2", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createDocument(std::nothrow, json);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::runCommand(
        std::nothrow_t, const std::string &command) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("GYIGUFYPTNB0", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->runCommand(std::nothrow, command);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("VC47QN01W5WH", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerInfo(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerStatus(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("737UOJCBRKJ0", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerStatus(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::ping(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("218DK7FXM1U4", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->ping(std::nothrow);
    }

    cpp_dbc::expected<std::string, DBException> DocumentPooledDBConnection::startSession(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("WTWINKFWO8WZ", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->startSession(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::endSession(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("X7VNI3HTM5NN", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->endSession(std::nothrow, sessionId);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::startTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("YYJXBAMEI07O", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->startTransaction(std::nothrow, sessionId);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::commitTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("VQ2C4Y9YU1LX", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->commitTransaction(std::nothrow, sessionId);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::abortTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("A7VGP008305O", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->abortTransaction(std::nothrow, sessionId);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::supportsTransactions(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("PLV8XFKGJ0PO", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->supportsTransactions(std::nothrow);
    }

    // ====================================================================
    // DocumentPooledDBConnection — Throwing + try/catch methods
    // ====================================================================

#ifdef __cpp_exceptions
    DocumentPooledDBConnection::~DocumentPooledDBConnection()
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
                CP_DEBUG("DocumentPooledDBConnection::~DocumentPooledDBConnection - close failed: %s", closeResult.error().what_s().data());
            }
        }
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::close(std::nothrow_t) noexcept
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
                        //      - LOCK(m_mutexPool)
                        //      - m_idleConnections.push(conn)  <-- Connection available with m_closed=TRUE
                        //      - UNLOCK(m_mutexPool) <-- Race window opens here
                        //   4. m_closed.store(false)  <-- Too late if Thread B already got the connection
                        //
                        // Thread B (acquiring connection) - executes between steps 3 and 4:
                        //   1. getDocumentDBConnection() acquires m_mutexPool
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
                        auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<DocumentPooledDBConnection>(shared_from_this()));
                        if (!returnResult.has_value())
                        {
                            CP_DEBUG("DocumentPooledDBConnection::close - returnConnection failed: %s", returnResult.error().what_s().data());
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
                        CP_DEBUG("DocumentPooledDBConnection::close - Failed to close underlying connection: %s", result.error().what_s().data());
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
                return cpp_dbc::unexpected(DBException("B6ZEXDIEMWV7",
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
                return cpp_dbc::unexpected(DBException("8XN84Y8GGSQU",
                                                        "Exception in close: " + std::string(ex.what()),
                                                        system_utils::captureCallStack()));
            }
            catch (...) // NOSONAR - Catch-all to ensure m_closed is always set correctly on any exception
            {
                CP_DEBUG("DocumentPooledDBConnection::close - Unknown exception caught");
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("KK3USJBXJPR1",
                                                        "Unknown exception in close",
                                                        system_utils::captureCallStack()));
            }
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
        auto result = isClosed(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::returnToPool(std::nothrow_t) noexcept
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
            if (DocumentPooledDBConnection::isPoolValid(std::nothrow))
            {
                // Try to obtain a shared_ptr from the weak_ptr
                if (auto poolShared = m_pool.lock())
                {
                    // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                    // (see close(nothrow) method for full explanation of the bug)
                    m_closed.store(false, std::memory_order_release);
                    auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<DocumentPooledDBConnection>(this->shared_from_this()));
                    if (!returnResult.has_value())
                    {
                        CP_DEBUG("DocumentPooledDBConnection::returnToPool - returnConnection failed: %s", returnResult.error().what_s().data());
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
            CP_DEBUG("DocumentPooledDBConnection::returnToPool - shared_from_this failed: %s", ex.what());
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("JS9EDG2CRO9T",
                                                   "Exception in returnToPool: " + std::string(ex.what()),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions
        {
            CP_DEBUG("DocumentPooledDBConnection::returnToPool - Unknown exception caught");
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("O7RKPD89YPAW",
                                                   "Unknown exception in returnToPool",
                                                   system_utils::captureCallStack()));
        }
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
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string DocumentPooledDBConnection::getURL() const
    {
        auto result = getURL(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void DocumentPooledDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void DocumentPooledDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel DocumentPooledDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    // Implementation of DocumentDBConnection interface methods
    std::string DocumentPooledDBConnection::getDatabaseName() const
    {
        auto result = getDatabaseName(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> DocumentPooledDBConnection::listDatabases()
    {
        auto result = listDatabases(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool DocumentPooledDBConnection::databaseExists(const std::string &databaseName)
    {
        auto result = databaseExists(std::nothrow, databaseName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void DocumentPooledDBConnection::useDatabase(const std::string &databaseName)
    {
        auto result = useDatabase(std::nothrow, databaseName);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void DocumentPooledDBConnection::dropDatabase(const std::string &databaseName)
    {
        auto result = dropDatabase(std::nothrow, databaseName);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::getCollection(const std::string &collectionName)
    {
        auto result = getCollection(std::nothrow, collectionName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> DocumentPooledDBConnection::listCollections()
    {
        auto result = listCollections(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool DocumentPooledDBConnection::collectionExists(const std::string &collectionName)
    {
        auto result = collectionExists(std::nothrow, collectionName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::createCollection(
        const std::string &collectionName, const std::string &options)
    {
        auto result = createCollection(std::nothrow, collectionName, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void DocumentPooledDBConnection::dropCollection(const std::string &collectionName)
    {
        auto result = dropCollection(std::nothrow, collectionName);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument()
    {
        auto result = createDocument(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument(const std::string &json)
    {
        auto result = createDocument(std::nothrow, json);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::runCommand(const std::string &command)
    {
        auto result = runCommand(std::nothrow, command);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerInfo()
    {
        auto result = getServerInfo(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerStatus()
    {
        auto result = getServerStatus(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool DocumentPooledDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string DocumentPooledDBConnection::startSession()
    {
        auto result = startSession(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void DocumentPooledDBConnection::endSession(const std::string &sessionId)
    {
        auto result = endSession(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void DocumentPooledDBConnection::startTransaction(const std::string &sessionId)
    {
        auto result = startTransaction(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void DocumentPooledDBConnection::commitTransaction(const std::string &sessionId)
    {
        auto result = commitTransaction(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void DocumentPooledDBConnection::abortTransaction(const std::string &sessionId)
    {
        auto result = abortTransaction(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool DocumentPooledDBConnection::supportsTransactions()
    {
        auto result = supportsTransactions(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }
#endif // __cpp_exceptions

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

#ifdef __cpp_exceptions
        cpp_dbc::expected<std::shared_ptr<MongoDBConnectionPool>, DBException> MongoDBConnectionPool::create(std::nothrow_t,
                                                                             const std::string &url,
                                                                             const std::string &username,
                                                                             const std::string &password) noexcept
        {
            try
            {
                auto pool = std::make_shared<MongoDBConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
                auto initResult = pool->initializePool(std::nothrow);
                if (!initResult.has_value())
                {
                    return cpp_dbc::unexpected(initResult.error());
                }
                return pool;
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("WBORKFD9SLKV", "Failed to create MongoDB connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("1YU3JTHO2WBL", "Failed to create MongoDB connection pool: unknown error", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::shared_ptr<MongoDBConnectionPool>, DBException> MongoDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
        {
            try
            {
                auto pool = std::make_shared<MongoDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
                auto initResult = pool->initializePool(std::nothrow);
                if (!initResult.has_value())
                {
                    return cpp_dbc::unexpected(initResult.error());
                }
                return pool;
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("WBORKFD9SLKV", "Failed to create MongoDB connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("1YU3JTHO2WBL", "Failed to create MongoDB connection pool: unknown error", system_utils::captureCallStack()));
            }
        }
#endif // __cpp_exceptions
    }

} // namespace cpp_dbc
