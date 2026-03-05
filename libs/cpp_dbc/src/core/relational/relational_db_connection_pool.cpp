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
 * @brief Implementation of connection pool for relational databases
 */

#include "cpp_dbc/core/relational/relational_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "../connection_pool_internal.hpp"
#include <algorithm>

namespace cpp_dbc
{

    // RelationalDBConnectionPool implementation
    RelationalDBConnectionPool::RelationalDBConnectionPool(DBConnectionPool::ConstructorTag,
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

    RelationalDBConnectionPool::RelationalDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
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

    #ifdef __cpp_exceptions
    cpp_dbc::expected<void, DBException> RelationalDBConnectionPool::initializePool(std::nothrow_t) noexcept
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

            // Start maintenance thread — std::jthread constructor can throw std::system_error
            m_maintenanceThread = std::jthread([this]
                                               { maintenanceTask(); });
        }
        catch (const std::exception &ex)
        {
            close(std::nothrow);
            return cpp_dbc::unexpected(DBException("L5BSOVG5PWW3", "Failed to initialize connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            close(std::nothrow);
            return cpp_dbc::unexpected(DBException("NAO8BHMRAANU", "Failed to initialize connection pool: unknown error", system_utils::captureCallStack()));
        }
        return {};
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnectionPool>, DBException> RelationalDBConnectionPool::create(std::nothrow_t,
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
            auto pool = std::make_shared<RelationalDBConnectionPool>(
                DBConnectionPool::ConstructorTag{}, url, username, password, options, initialSize, maxSize, minIdle,
                maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
                testOnBorrow, testOnReturn, validationQuery, transactionIsolation);

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
            return cpp_dbc::unexpected(DBException("IUB7PTQDNW1R", "Failed to create connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("UQQX17ADP7LE", "Failed to create connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnectionPool>, DBException> RelationalDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        try
        {
            auto pool = std::make_shared<RelationalDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

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
            return cpp_dbc::unexpected(DBException("IUB7PTQDNW1R", "Failed to create connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("UQQX17ADP7LE", "Failed to create connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

    RelationalDBConnectionPool::~RelationalDBConnectionPool()
    {
        CP_DEBUG("RelationalDBConnectionPool::~RelationalDBConnectionPool - Starting destructor at %lld",
                 (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        auto result = RelationalDBConnectionPool::close(std::nothrow);
        if (!result.has_value())
        {
            CP_DEBUG("RelationalDBConnectionPool::~RelationalDBConnectionPool - close failed: %s",
                     result.error().what_s().data());
        }

        CP_DEBUG("RelationalDBConnectionPool::~RelationalDBConnectionPool - Destructor completed at %lld",
                 (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>
    RelationalDBConnectionPool::createDBConnection(std::nothrow_t) noexcept
    {
        try
        {
            auto dbConn = DriverManager::getDBConnection(m_url, m_username, m_password, m_options);
            auto relationalConn = std::dynamic_pointer_cast<RelationalDBConnection>(dbConn);
            if (!relationalConn)
            {
                return cpp_dbc::unexpected(DBException("B3K9L4M1N2P5", "Connection pool only supports relational database connections", system_utils::captureCallStack()));
            }
            return relationalConn;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("AVFFVWG2H5DF", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("9AJ2I6MIR9ME", "Unknown error creating relational database connection", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<RelationalPooledDBConnection>, DBException>
    RelationalDBConnectionPool::createPooledDBConnection(std::nothrow_t) noexcept
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

        // Create pooled connection with weak_ptr.
        // shared_from_this() may throw std::bad_weak_ptr if the pool is stack-allocated;
        // in that case weakPool remains empty and the pooled connection holds no back-reference.
        std::weak_ptr<RelationalDBConnectionPool> weakPool;
        try
        {
            weakPool = shared_from_this();
        }
        catch (const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("RelationalDBConnectionPool::createPooledDBConnection - bad_weak_ptr: %s", ex.what());
        }

        try
        {
            auto pooledConn = std::make_shared<RelationalPooledDBConnection>(conn, weakPool, m_poolAlive);
            return pooledConn;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("4B3YXUQCEX9F", ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("UTP1C9G6PMH9", "Unknown error wrapping relational connection in pool", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException>
    RelationalDBConnectionPool::validateConnection(std::nothrow_t, std::shared_ptr<RelationalDBConnection> conn) const noexcept
    {
        auto resultSet = conn->executeQuery(std::nothrow, m_validationQuery);
        if (!resultSet.has_value())
        {
            CP_DEBUG("RelationalDBConnectionPool::validateConnection - executeQuery failed: %s", resultSet.error().what_s().data());
            return false;
        }
        return true;
    }

    cpp_dbc::expected<void, DBException>
    RelationalDBConnectionPool::returnConnection(std::nothrow_t, std::shared_ptr<RelationalPooledDBConnection> conn) noexcept
    {
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("2IJ2FJVCVZLG", "returnConnection called with null connection", system_utils::captureCallStack()));
        }

        if (!m_running.load(std::memory_order_acquire))
        {
            // Pool is closing — decrement counter so close() doesn't wait forever
            conn->setActive(std::nothrow, false);
            m_activeConnections--;
            auto closeResult = conn->getUnderlyingRelationalConnection()->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("RelationalDBConnectionPool::returnConnection - close failed during shutdown: %s", closeResult.error().what_s().data());
            }
            return {};
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive(std::nothrow))
        {
            CP_DEBUG("returnConnection - SKIPPED: connection already inactive");
            return cpp_dbc::unexpected(DBException("76MTOQW6NQVG", "returnConnection called on an already inactive connection (double-return bug)", system_utils::captureCallStack()));
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
                return cpp_dbc::unexpected(DBException("JXXLPNBLFVLI", "returnConnection called with a connection not belonging to this pool (orphan)", system_utils::captureCallStack()));
            }
        }

        // ========================================
        // Phase 1: I/O OUTSIDE any lock
        // Connection is marked active=true → maintenance skips it, borrowers can't see it.
        // ========================================
        bool valid = true;

        if (m_testOnReturn)
        {
            auto validResult = validateConnection(std::nothrow, conn->getUnderlyingRelationalConnection());
            valid = validResult.value_or(false);
        }

        if (valid && !conn->m_closed.load(std::memory_order_acquire))
        {
            auto resetResult = conn->getUnderlyingRelationalConnection()->reset(std::nothrow);
            if (!resetResult.has_value())
            {
                CP_DEBUG("RelationalDBConnectionPool::returnConnection - reset failed: %s", resetResult.error().what_s().data());
                valid = false;
            }
        }

        if (valid && !conn->m_closed.load(std::memory_order_acquire))
        {
            auto isoResult = conn->getTransactionIsolation(std::nothrow);
            if (!isoResult.has_value())
            {
                CP_DEBUG("RelationalDBConnectionPool::returnConnection - getTransactionIsolation failed: %s", isoResult.error().what_s().data());
                valid = false;
            }
            else if (isoResult.value() != m_transactionIsolation)
            {
                auto setIsoResult = conn->setTransactionIsolation(std::nothrow, m_transactionIsolation);
                if (!setIsoResult.has_value())
                {
                    CP_DEBUG("RelationalDBConnectionPool::returnConnection - setTransactionIsolation failed: %s", setIsoResult.error().what_s().data());
                    valid = false;
                }
            }
        }

        // ========================================
        // Phase 2: Pool state under m_mutexPool only (NO I/O)
        // ========================================
        if (valid)
        {
            conn->updateLastUsedTime(std::nothrow); // Mark return time for HikariCP validation skip
            std::scoped_lock lockPool(m_mutexPool);
            conn->setActive(std::nothrow, false);
            m_activeConnections--;

            // Direct handoff: if threads are waiting, give the connection directly
            // to the first waiter instead of pushing to idle. This eliminates the
            // "stolen wakeup" race where the returning thread immediately re-borrows
            // its own connection before the CV-notified thread can acquire the mutex.
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
                // Pitfall #2: Without this, destructor decrements AGAIN → underflow.
                // Pitfall #5: Without m_closed=true, destructor double-closes.
                conn->setActive(std::nothrow, false);
                conn->m_closed.store(true);
                m_activeConnections--;
                CP_DEBUG("returnConnection - INVALID: active=%d", m_activeConnections.load(std::memory_order_acquire));

                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    m_allConnections.erase(it);
                }
                std::queue<std::shared_ptr<RelationalPooledDBConnection>> tempQueue;
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
                auto closeResult = conn->getUnderlyingRelationalConnection()->close(std::nothrow);
                if (!closeResult.has_value())
                {
                    CP_DEBUG("RelationalDBConnectionPool::returnConnection - close failed on invalid connection: %s", closeResult.error().what_s().data());
                }
            }

            // Step 3: Create replacement OUTSIDE locks (I/O)
            if (!m_running.load(std::memory_order_acquire))
            {
                return {}; // Pool closing, skip replacement
            }

            std::shared_ptr<RelationalPooledDBConnection> replacement;
            {
                auto replacementResult = createPooledDBConnection(std::nothrow);
                if (replacementResult.has_value())
                {
                    replacement = replacementResult.value();
                }
                else
                {
                    CP_DEBUG("RelationalDBConnectionPool::returnConnection - failed creating replacement: %s", replacementResult.error().what_s().data());
                }
            }

            // Step 4: Insert replacement under lock with direct handoff
            if (replacement)
            {
                std::shared_ptr<RelationalPooledDBConnection> discardReplacement;
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
                    auto closeResult = discardReplacement->getUnderlyingRelationalConnection()->close(std::nothrow);
                    if (!closeResult.has_value())
                    {
                        CP_DEBUG("RelationalDBConnectionPool::returnConnection - close() on discarded replacement failed: %s", closeResult.error().what_s().data());
                    }
                }
            }
        }

        return {};
    }

    cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> RelationalDBConnectionPool::getDBConnection(std::nothrow_t) noexcept
    {
        auto result = getRelationalDBConnection(std::nothrow);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        return std::static_pointer_cast<DBConnection>(result.value());
    }

    std::shared_ptr<DBConnection> RelationalDBConnectionPool::getDBConnection()
    {
        auto result = getDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> RelationalDBConnectionPool::getRelationalDBConnection(std::nothrow_t) noexcept
    {
        try
        {
            auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(m_maxWaitMillis);

            while (true) // Retry loop for testOnBorrow validation failures
            {
                std::shared_ptr<RelationalPooledDBConnection> result;
                bool handedOff = false;

                // ========================================
                // Phase 1: Acquire candidate under m_mutexPool (NO I/O)
                // ========================================
                {
                    std::unique_lock lockPool(m_mutexPool);

                    if (!m_running.load(std::memory_order_acquire))
                    {
                        return cpp_dbc::unexpected(DBException("5Q6R7S8T9U0V", "Connection pool is closed", system_utils::captureCallStack()));
                    }

                    // Unified acquire loop: idle check → create → wait (with direct handoff)
                    // CRITICAL: Always check idle BEFORE wait_until to prevent lost notifications.
                    // When a thread is in an unlock/lock cycle (create I/O, discard I/O), a
                    // notify from returnConnection() has no waiting thread to wake.
                    // The returned connection sits in idle unclaimed. Without checking idle
                    // at the top of the loop, the thread goes straight to wait_until and
                    // misses the idle connection → timeout.
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
                            {
                                auto candidateResult = createPooledDBConnection(std::nothrow);
                                // Always restore lock and pendingCreations regardless of outcome
                                lockPool.lock();
                                m_pendingCreations--;

                                if (!candidateResult.has_value())
                                {
                                    CP_DEBUG("borrow - failed creating connection: %s", candidateResult.error().what_s().data());
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
                                        auto closeResult = candidate->getUnderlyingRelationalConnection()->close(std::nothrow);
                                        if (!closeResult.has_value())
                                        {
                                            CP_DEBUG("borrow - close() on discarded candidate failed: %s", closeResult.error().what_s().data());
                                        }
                                        lockPool.lock();
                                    }
                                }
                            }
                            continue; // ALWAYS re-check idle after any unlock/lock cycle
                        }

                        // Step 3: Nothing available — wait on CV with direct handoff request.
                        // The ConnectionRequest is stack-local. Its pointer is stored in
                        // m_waitQueue. When a returner has a connection and sees waiters, it
                        // pops the front request, writes the connection into it, and sets
                        // fulfilled=true — all under m_mutexPool. This eliminates the "stolen
                        // wakeup" race: the connection never enters the idle queue when waiters
                        // exist, so the returning thread can't immediately re-borrow it.
                        //
                        // CRITICAL: Inner loop keeps the request in its FIFO position.
                        // Non-fulfilled wakeups (from notify_all) must NOT dequeue and
                        // re-enqueue — that would push the thread to the back of the queue,
                        // causing deadline starvation under Helgrind's serialized execution.
                        ConnectionRequest request;
                        m_waitQueue.push_back(&request);
                        CP_DEBUG("borrow - WAITING on CV: active=%d, idle=%zu, total=%zu, pending=%zu, waiters=%zu",
                                 m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size(),
                                 m_pendingCreations, m_waitQueue.size());

                        // Multiple break statements required for mutually exclusive CV exit conditions
                        // (handoff, idle-after-timeout, idle-after-wakeup). Refactoring would require
                        // artificial state variables and would break FIFO queue correctness.
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
                                CP_DEBUG("RelationalDBConnectionPool::getConnection - wait_until threw: %s", ex.what());
                                std::erase(m_waitQueue, &request);
                                throw;
                            }
                            catch (...) // NOSONAR — non-std exceptions: remove from queue then propagate
                            {
                                CP_DEBUG("RelationalDBConnectionPool::getConnection - wait_until threw unknown exception");
                                std::erase(m_waitQueue, &request);
                                throw;
                            }

                            // Check if we received a direct handoff (connection already active + counted)
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
                                return cpp_dbc::unexpected(DBException("SVGG2874XLWL", "Connection pool is closed", system_utils::captureCallStack()));
                            }

                            if (status == std::cv_status::timeout)
                            {
                                std::erase(m_waitQueue, &request);
                                // One last idle check before returning error (connection may have arrived
                                // between the timeout and re-acquiring the lock)
                                if (!m_idleConnections.empty())
                                {
                                    result = m_idleConnections.front();
                                    m_idleConnections.pop();
                                    CP_DEBUG("borrow - GOT IDLE after timeout: active=%d, idle=%zu",
                                             m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size());
                                    break; // Exit inner loop with result set
                                }
                                CP_DEBUG("borrow - TIMEOUT: active=%d, idle=%zu, total=%zu, pending=%zu",
                                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size(), m_allConnections.size(), m_pendingCreations);
                                return cpp_dbc::unexpected(DBException("JV6LANDTF256", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
                            }

                            // Spurious/non-fulfilled wakeup — check if idle connection appeared
                            // (from maintenance replenish or non-handoff source)
                            if (!m_idleConnections.empty())
                            {
                                std::erase(m_waitQueue, &request);
                                result = m_idleConnections.front();
                                m_idleConnections.pop();
                                CP_DEBUG("borrow - GOT IDLE after wakeup: active=%d, idle=%zu",
                                         m_activeConnections.load(std::memory_order_acquire), m_idleConnections.size());
                                break; // Exit inner loop with result set
                            }

                            // Nothing available — stay in queue at same FIFO position, re-wait.
                            // DO NOT dequeue and re-enqueue (would cause FIFO starvation).
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
                // If the connection was active within m_validationTimeoutMillis, it's almost
                // certainly still valid. This eliminates SELECT 1 overhead for hot connections.
                // ========================================
                if (m_testOnBorrow)
                {
                    auto timeSinceLastUse = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::steady_clock::now() - result->getLastUsedTime(std::nothrow))
                                                .count();

                    if (timeSinceLastUse > m_validationTimeoutMillis)
                    {
                        CP_DEBUG("borrow - VALIDATING (timeSinceLastUse=%lld ms > %ld ms)", (long long)timeSinceLastUse, m_validationTimeoutMillis);
                    }

                    if (timeSinceLastUse > m_validationTimeoutMillis &&
                        !validateConnection(std::nothrow, result->getUnderlyingRelationalConnection()).value_or(false))
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
                        auto closeResult = result->getUnderlyingRelationalConnection()->close(std::nothrow);
                        if (!closeResult.has_value())
                        {
                            CP_DEBUG("borrow - close() on invalid connection failed: %s", closeResult.error().what_s().data());
                        }

                        // Check if we still have time to retry
                        if (std::chrono::steady_clock::now() >= deadline)
                        {
                            return cpp_dbc::unexpected(DBException("JV6LANDTF256", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
                        }

                        continue; // Retry with new candidate
                    }
                }

                // ========================================
                // Phase 3: prepareForBorrow (outside lock)
                // ========================================
                try
                {
                    result->getUnderlyingRelationalConnection()->prepareForBorrow();
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    CP_DEBUG("RelationalDBConnectionPool::getRelationalDBConnection - Exception in prepareForBorrow: %s", ex.what());
                }

                return cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>{
                    std::static_pointer_cast<RelationalDBConnection>(result)};
            }
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("S61XJXIV5HWP",
                                                   std::string("Exception in getRelationalDBConnection: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR
        {
            return cpp_dbc::unexpected(DBException("MNS4V7U4OVF0",
                                                   "Unknown exception in getRelationalDBConnection",
                                                   system_utils::captureCallStack()));
        }
    }

    std::shared_ptr<RelationalDBConnection> RelationalDBConnectionPool::getRelationalDBConnection()
    {
        auto result = getRelationalDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void RelationalDBConnectionPool::maintenanceTask()
    {
        do
        {
            // Wait for 30 seconds or until notified (e.g., when close() is called).
            // Uses m_mutexPool so both CVs (m_maintenanceCondition + m_connectionAvailable)
            // share the same mutex. The lock is released during wait_for().
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
                        std::queue<std::shared_ptr<RelationalPooledDBConnection>> tempQueue;
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

            // CRITICAL FIX (preventive): createPooledDBConnection() must NOT be called while
            // holding pool locks. Replenish minIdle connections OUTSIDE locks to avoid potential
            // lock ordering cycles with driver-internal mutexes.
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
                    CP_DEBUG("RelationalDBConnectionPool::maintenanceTask - failed creating minIdle connection: %s", pooledResult.error().what_s().data());
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

    cpp_dbc::expected<size_t, DBException> RelationalDBConnectionPool::getActiveDBConnectionCount(std::nothrow_t) const noexcept
    {
        return static_cast<size_t>(m_activeConnections.load(std::memory_order_acquire));
    }

    size_t RelationalDBConnectionPool::getActiveDBConnectionCount() const
    {
        auto result = getActiveDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<size_t, DBException> RelationalDBConnectionPool::getIdleDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_idleConnections.size();
    }

    size_t RelationalDBConnectionPool::getIdleDBConnectionCount() const
    {
        auto result = getIdleDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<size_t, DBException> RelationalDBConnectionPool::getTotalDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_allConnections.size();
    }

    size_t RelationalDBConnectionPool::getTotalDBConnectionCount() const
    {
        auto result = getTotalDBConnectionCount(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<void, DBException> RelationalDBConnectionPool::close(std::nothrow_t) noexcept
    {
        CP_DEBUG("RelationalDBConnectionPool::close - Starting close operation");

        if (!m_running.exchange(false))
        {
            CP_DEBUG("RelationalDBConnectionPool::close - Already closed, returning");
            return {}; // Already closed — idempotent
        }

        // Mark pool as no longer alive - this prevents pooled connections from trying to return
        if (m_poolAlive)
        {
            m_poolAlive->store(false);
        }

        CP_DEBUG("RelationalDBConnectionPool::close - Waiting for active operations to complete at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            CP_DEBUG("RelationalDBConnectionPool::close - Initial active connections: %d", m_activeConnections.load(std::memory_order_acquire));

            while (m_activeConnections.load(std::memory_order_acquire) > 0)
            {
                CP_DEBUG("RelationalDBConnectionPool::close - Waiting for %d active connections to finish at %lld",
                         m_activeConnections.load(std::memory_order_acquire), (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 1 == 0)
                {
                    CP_DEBUG("RelationalDBConnectionPool::close - Waited %ld seconds for active connections", elapsed_seconds);
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("RelationalDBConnectionPool::close - Timeout waiting for active connections, forcing close at %lld", (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                    // Force active connections to be marked as inactive
                    m_activeConnections.store(0);
                    break;
                }
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
        std::vector<std::shared_ptr<RelationalPooledDBConnection>> connectionsToClose;
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
        cpp_dbc::expected<void, DBException> lastError{};
        for (const auto &conn : connectionsToClose)
        {
            if (conn && conn->getUnderlyingConnection(std::nothrow))
            {
                conn->setActive(std::nothrow, false);
                auto r = conn->getUnderlyingConnection(std::nothrow)->close(std::nothrow);
                if (!r.has_value())
                {
                    CP_DEBUG("RelationalDBConnectionPool::close - connection close failed: %s", r.error().what_s().data());
                    lastError = r;
                }
            }
        }

        return lastError;
    }

    void RelationalDBConnectionPool::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<bool, DBException> RelationalDBConnectionPool::isRunning(std::nothrow_t) const noexcept
    {
        return m_running.load(std::memory_order_acquire);
    }

    bool RelationalDBConnectionPool::isRunning() const
    {
        auto result = isRunning(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    // RelationalPooledDBConnection implementation
    RelationalPooledDBConnection::RelationalPooledDBConnection(
        std::shared_ptr<RelationalDBConnection> connection,
        std::weak_ptr<RelationalDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive)
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive)
    {
        // m_creationTime and m_lastUsedTimeNs use in-class initializers
    }

    bool RelationalPooledDBConnection::isPoolValid(std::nothrow_t) const noexcept
    {
        return m_poolAlive && m_poolAlive->load(std::memory_order_acquire);
    }

    RelationalPooledDBConnection::~RelationalPooledDBConnection()
    {
        if (!m_closed.load(std::memory_order_acquire) && m_conn)
        {
            try
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
                        pool->m_activeConnections--;
                    }
                    m_active.store(false);
                }

                auto closeResult = m_conn->close(std::nothrow);
                if (!closeResult.has_value())
                {
                    CP_DEBUG("RelationalPooledDBConnection::~RelationalPooledDBConnection - close failed: %s", closeResult.error().what_s().data());
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("RelationalPooledDBConnection::~RelationalPooledDBConnection - Exception: %s", ex.what());
            }
        }
    }

    cpp_dbc::expected<void, cpp_dbc::DBException> RelationalPooledDBConnection::close(std::nothrow_t) noexcept
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
                        //   1. getRelationalDBConnection() acquires m_mutexPool
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
                        auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<RelationalPooledDBConnection>(shared_from_this()));
                        if (!returnResult.has_value())
                        {
                            CP_DEBUG("RelationalPooledDBConnection::close - returnConnection failed: %s", returnResult.error().what_s().data());
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
                        CP_DEBUG("RelationalPooledDBConnection::close - Failed to close underlying connection: %s", result.error().what_s().data());
                        return result;
                    }
                }

                return {};
            }
            catch (const std::bad_weak_ptr &ex)
            {
                // shared_from_this failed, just close the connection
                m_closed.store(true);
                if (m_conn)
                {
                    auto result = m_conn->close(std::nothrow);
                    if (!result.has_value())
                    {
                        return result;
                    }
                }
                return cpp_dbc::unexpected(cpp_dbc::DBException("54A9H0C3BX8E",
                                                                std::string("Failed to obtain shared_from_this: ") + ex.what(),
                                                                cpp_dbc::system_utils::captureCallStack()));
            }
            catch (const cpp_dbc::DBException &ex)
            {
                // DBException from underlying connection, just return it
                m_closed.store(true);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                // Any other exception, close the connection and return error
                m_closed.store(true);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(cpp_dbc::DBException("VYPDCT5DIRVF",
                                                                std::string("Exception in close: ") + ex.what(),
                                                                cpp_dbc::system_utils::captureCallStack()));
            }
            catch (...) // NOSONAR - Catch-all to ensure m_closed is always set correctly on any exception
            {
                CP_DEBUG("RelationalPooledDBConnection::close - Unknown exception caught");
                m_closed.store(true);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(cpp_dbc::DBException("85ZGPKW5MX8J",
                                                                "Unknown exception in close",
                                                                cpp_dbc::system_utils::captureCallStack()));
            }
        }

        return {};
    }

    void RelationalPooledDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool RelationalPooledDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> RelationalPooledDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("NGFY9OKTDK8C", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->prepareStatement(std::nothrow, sql);
    }

    std::shared_ptr<RelationalDBPreparedStatement> RelationalPooledDBConnection::prepareStatement(const std::string &sql)
    {
        auto result = prepareStatement(std::nothrow, sql);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> RelationalPooledDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("F9E32F56CFF4", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->executeQuery(std::nothrow, sql);
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

    cpp_dbc::expected<uint64_t, DBException> RelationalPooledDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("JURJ0DBHVVPS", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->executeUpdate(std::nothrow, sql);
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

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::setAutoCommit(std::nothrow_t, bool autoCommit) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("XNDX9UDCTDQF", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->setAutoCommit(std::nothrow, autoCommit);
    }

    void RelationalPooledDBConnection::setAutoCommit(bool autoCommit)
    {
        auto result = setAutoCommit(std::nothrow, autoCommit);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("E3DEAB8A5E6D", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getAutoCommit(std::nothrow);
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

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::commit(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("E32DBBC7316E", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->commit(std::nothrow);
    }

    void RelationalPooledDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::rollback(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("C3UAXP7DGDPI", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->rollback(std::nothrow);
    }

    void RelationalPooledDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<void, cpp_dbc::DBException> RelationalPooledDBConnection::reset(std::nothrow_t) noexcept
    {
        if (!m_conn)
        {
            return cpp_dbc::unexpected(cpp_dbc::DBException("YNH9PJK5FCZF",
                                                            "Underlying connection is null",
                                                            cpp_dbc::system_utils::captureCallStack()));
        }

        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(cpp_dbc::DBException("9U7LGHL2AK0P",
                                                            "Connection is closed",
                                                            cpp_dbc::system_utils::captureCallStack()));
        }

        updateLastUsedTime(std::nothrow);
        return m_conn->reset(std::nothrow);
    }

    void RelationalPooledDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("B7C8D9E0F1G2", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->beginTransaction(std::nothrow);
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

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("H3I4J5K6L7M8", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->transactionActive(std::nothrow);
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

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("F7A2B9C3D1E5", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->setTransactionIsolation(std::nothrow, level);
    }

    void RelationalPooledDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException> RelationalPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("A4B5C6D7E8F9", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getTransactionIsolation(std::nothrow);
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

    std::string RelationalPooledDBConnection::getURL() const
    {
        auto result = getURL(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::chrono::time_point<std::chrono::steady_clock> RelationalPooledDBConnection::getCreationTime(std::nothrow_t) const noexcept
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> RelationalPooledDBConnection::getLastUsedTime(std::nothrow_t) const noexcept
    {
        return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{m_lastUsedTimeNs.load(std::memory_order_relaxed)}};
    }

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::setActive(std::nothrow_t, bool isActive) noexcept
    {
        m_active.store(isActive, std::memory_order_release);
        return {};
    }

    bool RelationalPooledDBConnection::isActive(std::nothrow_t) const noexcept
    {
        return m_active.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the return
        bool expected = false;
        if (!m_closed.compare_exchange_strong(expected, true))
        {
            return {}; // Already being returned/closed by another thread
        }

        try
        {
            updateLastUsedTime(std::nothrow);

            // Check if pool is still alive using the shared atomic flag
            // Use qualified call to avoid virtual dispatch issues when called from destructor
            if (RelationalPooledDBConnection::isPoolValid(std::nothrow))
            {
                if (auto poolShared = m_pool.lock())
                {
                    // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                    // (see close() method for full explanation of the bug)
                    m_closed.store(false);
                    auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<RelationalPooledDBConnection>(this->shared_from_this()));
                    if (!returnResult.has_value())
                    {
                        CP_DEBUG("RelationalPooledDBConnection::returnToPool - returnConnection failed: %s", returnResult.error().what_s().data());
                        return returnResult;
                    }
                }
            }
            return {};
        }
        catch (const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("RelationalPooledDBConnection::returnToPool - bad_weak_ptr: %s", ex.what());
            m_closed.store(true);
            return cpp_dbc::unexpected(DBException("KDDVEFIZ5QUR",
                                                   std::string("Failed to obtain shared_from_this: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (const DBException &ex)
        {
            m_closed.store(true);
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            m_closed.store(true);
            return cpp_dbc::unexpected(DBException("8BN1W05EH7WK",
                                                   std::string("Exception in returnToPool: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR
        {
            m_closed.store(true);
            return cpp_dbc::unexpected(DBException("UF9WYX7K0BXP",
                                                   "Unknown exception in returnToPool",
                                                   system_utils::captureCallStack()));
        }
    }

    void RelationalPooledDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool RelationalPooledDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DBConnection> RelationalPooledDBConnection::getUnderlyingConnection(std::nothrow_t) noexcept
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

    std::shared_ptr<RelationalDBConnection> RelationalPooledDBConnection::getUnderlyingRelationalConnection()
    {
        return m_conn;
    }

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return true;
        }
        return m_conn->isClosed(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return {!m_active.load(std::memory_order_acquire)};
    }

    cpp_dbc::expected<std::string, DBException> RelationalPooledDBConnection::getURL(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("A4B7C9D2E5F1", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    void RelationalPooledDBConnection::prepareForPoolReturn()
    {
        auto result = prepareForPoolReturn(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::prepareForPoolReturn(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("73OMW2XSN96G",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForPoolReturn(std::nothrow);
    }

    void RelationalPooledDBConnection::prepareForBorrow()
    {
        auto result = prepareForBorrow(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("V4JST5V0D1K7",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForBorrow(std::nothrow);
    }

    bool RelationalPooledDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return *result;
    }
    #endif // __cpp_exceptions

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::ping(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("7B5ASUOLCER7", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->ping(std::nothrow);
    }

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

        cpp_dbc::expected<std::shared_ptr<MySQLConnectionPool>, DBException> MySQLConnectionPool::create(std::nothrow_t,
                                                                                                         const std::string &url,
                                                                                                         const std::string &username,
                                                                                                         const std::string &password) noexcept
        {
            try
            {
                auto pool = std::make_shared<MySQLConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
                auto initResult = pool->initializePool(std::nothrow);
                if (!initResult.has_value())
                {
                    return cpp_dbc::unexpected(initResult.error());
                }
                return pool;
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("0T5TQ1XDNBRB", "Failed to create MySQL connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("C5UGEAU0RKS4", "Failed to create MySQL connection pool: unknown error", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::shared_ptr<MySQLConnectionPool>, DBException> MySQLConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
        {
            try
            {
                auto pool = std::make_shared<MySQLConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
                auto initResult = pool->initializePool(std::nothrow);
                if (!initResult.has_value())
                {
                    return cpp_dbc::unexpected(initResult.error());
                }
                return pool;
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("0T5TQ1XDNBRB", "Failed to create MySQL connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("C5UGEAU0RKS4", "Failed to create MySQL connection pool: unknown error", system_utils::captureCallStack()));
            }
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

        cpp_dbc::expected<std::shared_ptr<PostgreSQLConnectionPool>, DBException> PostgreSQLConnectionPool::create(std::nothrow_t,
                                                                                                                   const std::string &url,
                                                                                                                   const std::string &username,
                                                                                                                   const std::string &password) noexcept
        {
            try
            {
                auto pool = std::make_shared<PostgreSQLConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
                auto initResult = pool->initializePool(std::nothrow);
                if (!initResult.has_value())
                {
                    return cpp_dbc::unexpected(initResult.error());
                }
                return pool;
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("0OBRVLS9RYQW", "Failed to create PostgreSQL connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("9DYJNX5U6MOH", "Failed to create PostgreSQL connection pool: unknown error", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::shared_ptr<PostgreSQLConnectionPool>, DBException> PostgreSQLConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
        {
            try
            {
                auto pool = std::make_shared<PostgreSQLConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
                auto initResult = pool->initializePool(std::nothrow);
                if (!initResult.has_value())
                {
                    return cpp_dbc::unexpected(initResult.error());
                }
                return pool;
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("0OBRVLS9RYQW", "Failed to create PostgreSQL connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("9DYJNX5U6MOH", "Failed to create PostgreSQL connection pool: unknown error", system_utils::captureCallStack()));
            }
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
            SQLiteConnectionPool::setPoolTransactionIsolation(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
        }

        SQLiteConnectionPool::SQLiteConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config)
            : RelationalDBConnectionPool(DBConnectionPool::ConstructorTag{}, config)
        {
            // SQLite only supports SERIALIZABLE isolation level
            // Override the isolation level from the config
            SQLiteConnectionPool::setPoolTransactionIsolation(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
        }

        cpp_dbc::expected<std::shared_ptr<SQLiteConnectionPool>, DBException> SQLiteConnectionPool::create(std::nothrow_t,
                                                                                                           const std::string &url,
                                                                                                           const std::string &username,
                                                                                                           const std::string &password) noexcept
        {
            try
            {
                auto pool = std::make_shared<SQLiteConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
                auto initResult = pool->initializePool(std::nothrow);
                if (!initResult.has_value())
                {
                    return cpp_dbc::unexpected(initResult.error());
                }
                return pool;
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("8TTDZKLT3XJI", "Failed to create SQLite connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("AJSTB8D4DFG5", "Failed to create SQLite connection pool: unknown error", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::shared_ptr<SQLiteConnectionPool>, DBException> SQLiteConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
        {
            try
            {
                auto pool = std::make_shared<SQLiteConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
                auto initResult = pool->initializePool(std::nothrow);
                if (!initResult.has_value())
                {
                    return cpp_dbc::unexpected(initResult.error());
                }
                return pool;
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("8TTDZKLT3XJI", "Failed to create SQLite connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("AJSTB8D4DFG5", "Failed to create SQLite connection pool: unknown error", system_utils::captureCallStack()));
            }
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

        cpp_dbc::expected<std::shared_ptr<FirebirdConnectionPool>, DBException> FirebirdConnectionPool::create(std::nothrow_t,
                                                                                                               const std::string &url,
                                                                                                               const std::string &username,
                                                                                                               const std::string &password) noexcept
        {
            try
            {
                auto pool = std::make_shared<FirebirdConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
                auto initResult = pool->initializePool(std::nothrow);
                if (!initResult.has_value())
                {
                    return cpp_dbc::unexpected(initResult.error());
                }
                return pool;
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("IAPR1BE2PCQL", "Failed to create Firebird connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("LP5EF7QAB5B4", "Failed to create Firebird connection pool: unknown error", system_utils::captureCallStack()));
            }
        }

        cpp_dbc::expected<std::shared_ptr<FirebirdConnectionPool>, DBException> FirebirdConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
        {
            try
            {
                auto pool = std::make_shared<FirebirdConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
                auto initResult = pool->initializePool(std::nothrow);
                if (!initResult.has_value())
                {
                    return cpp_dbc::unexpected(initResult.error());
                }
                return pool;
            }
            catch (const std::exception &ex)
            {
                return cpp_dbc::unexpected(DBException("IAPR1BE2PCQL", "Failed to create Firebird connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
            }
            catch (...)
            {
                return cpp_dbc::unexpected(DBException("LP5EF7QAB5B4", "Failed to create Firebird connection pool: unknown error", system_utils::captureCallStack()));
            }
        }
    }

} // namespace cpp_dbc
