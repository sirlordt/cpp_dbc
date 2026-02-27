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

    cpp_dbc::expected<void, DBException> DocumentDBConnectionPool::initializePool(std::nothrow_t) noexcept
    {
        try
        {
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
            m_maintenanceThread = std::jthread(&DocumentDBConnectionPool::maintenanceTask, this);
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
                                                                               const std::string &validationQuery,
                                                                               TransactionIsolationLevel transactionIsolation) noexcept
    {
        try
        {
            auto pool = std::make_shared<DocumentDBConnectionPool>(
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

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException>
    DocumentDBConnectionPool::createDBConnection(std::nothrow_t) noexcept
    {
        try
        {
            auto dbConn = DriverManager::getDBConnection(m_url, m_username, m_password, m_options);
            auto documentConn = std::dynamic_pointer_cast<DocumentDBConnection>(dbConn);
            if (!documentConn)
            {
                return cpp_dbc::unexpected(DBException("1EA1E853ED8F", "Connection pool only supports document database connections", system_utils::captureCallStack()));
            }
            return documentConn;
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("2FH78ZLDJRC5", std::string("Failed to create document database connection: ") + ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("MTPQJ3JQBXLU", "Unknown error creating document database connection", system_utils::captureCallStack()));
        }
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

        // std::make_shared can throw std::bad_alloc, so wrap in try/catch
        try
        {
            return std::make_shared<DocumentPooledDBConnection>(conn, weakPool, m_poolAlive);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("9W7G1U1XVSQA", std::string("Failed to allocate pooled document connection: ") + ex.what(), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("82ZVGZU2Q353", "Unknown error allocating pooled document connection", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException>
    DocumentDBConnectionPool::validateConnection(std::nothrow_t, std::shared_ptr<DocumentDBConnection> conn) const noexcept
    {
        auto result = conn->ping(std::nothrow);
        if (!result.has_value())
        {
            CP_DEBUG("DocumentDBConnectionPool::validateConnection - ping failed: %s", result.error().what_s().data());
            return false;
        }
        return result.value();
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
            m_activeConnections--;
            auto closeResult = conn->getUnderlyingDocumentConnection()->close(std::nothrow);
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
                m_activeConnections--;
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - Connection not found in pool, SKIPPED");
                m_connectionAvailable.notify_one();
                return cpp_dbc::unexpected(DBException("AX95P6TXEL5I", "returnConnection called with a connection not belonging to this pool (orphan)", system_utils::captureCallStack()));
            }
        }

        bool valid = true;

        // Phase 1: I/O outside lock (validation and pool-return prep)
        if (m_testOnReturn)
        {
            valid = validateConnection(std::nothrow, conn->getUnderlyingDocumentConnection()).value_or(false);
        }

        if (valid)
        {
            auto result = conn->getUnderlyingDocumentConnection()->prepareForPoolReturn(std::nothrow);
            if (!result.has_value())
            {
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - prepareForPoolReturn failed: %s", result.error().what_s().data());
                valid = false;
            }
        }

        // Phase 2: State changes under m_mutexPool
        if (valid)
        {
            conn->updateLastUsedTime(std::nothrow);
            std::scoped_lock lockPool(m_mutexPool);
            conn->setActive(std::nothrow, false);
            m_activeConnections--;

            if (!m_waitQueue.empty())
            {
                // Direct handoff to first waiter.
                // Restore active state: we decremented above unconditionally,
                // but the borrower skips setActive(true)/m_activeConnections++
                // when handedOff=true, so we must restore here.
                ConnectionRequest *req = m_waitQueue.front();
                m_waitQueue.pop_front();
                // Restore active state before handoff: setActive(false) + m_activeConnections--
                // were called unconditionally above, so they must be undone here when a waiter
                // takes the connection directly.
                conn->setActive(std::nothrow, true);
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
                conn->setActive(std::nothrow, false);
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
            auto closeResult = conn->getUnderlyingDocumentConnection()->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - close() on invalid connection failed: %s", closeResult.error().what_s().data());
            }

            // Replenish: create outside lock
            if (m_running.load(std::memory_order_acquire))
            {
                auto replacementResult = createPooledDBConnection(std::nothrow);
                if (!replacementResult.has_value())
                {
                    CP_DEBUG("DocumentDBConnectionPool::returnConnection - Failed to create replacement: %s", replacementResult.error().what_s().data());
                }

                if (replacementResult.has_value())
                {
                    auto replacement = replacementResult.value();
                    std::shared_ptr<DocumentPooledDBConnection> discardReplacement;
                    {
                        std::scoped_lock lockPool(m_mutexPool);
                        if (m_running.load(std::memory_order_acquire) && m_allConnections.size() < m_maxSize)
                        {
                            m_allConnections.push_back(replacement);

                            if (!m_waitQueue.empty())
                            {
                                // Direct handoff to first waiter
                                ConnectionRequest *req = m_waitQueue.front();
                                m_waitQueue.pop_front();
                                replacement->setActive(std::nothrow, true);
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
                        auto discardCloseResult = discardReplacement->getUnderlyingDocumentConnection()->close(std::nothrow);
                        if (!discardCloseResult.has_value())
                        {
                            CP_DEBUG("DocumentDBConnectionPool::returnConnection - close() on discarded replacement failed: %s", discardCloseResult.error().what_s().data());
                        }
                    }
                }
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
                            break;
                        }

                        // Step 2: Create new connection if pool not full
                        size_t totalPlusCreating = m_allConnections.size() + m_pendingCreations;
                        if (totalPlusCreating < m_maxSize)
                        {
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
                                        newConn->m_closed.store(true); // Prevent destructor returnToPool()
                                        lockPool.unlock();
                                        auto closeResult = newConn->getUnderlyingDocumentConnection()->close(std::nothrow);
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
                        ConnectionRequest req;
                        m_waitQueue.push_back(&req);

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
                                CP_DEBUG("DocumentDBConnectionPool::getConnection - wait_until threw: %s", ex.what());
                                std::erase(m_waitQueue, &req);
                                throw;
                            }
                            catch (...) // NOSONAR — non-std exceptions: remove from queue then propagate
                            {
                                CP_DEBUG("DocumentDBConnectionPool::getConnection - wait_until threw unknown exception");
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

                            if (!m_running.load(std::memory_order_acquire))
                            {
                                std::erase(m_waitQueue, &req);
                                return cpp_dbc::unexpected(DBException("U9ZUMJ9SDFTN", "Connection pool is closed", system_utils::captureCallStack()));
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

                                return cpp_dbc::unexpected(DBException("HF3V83PS83LA", "Timeout waiting for connection from the pool", system_utils::captureCallStack()));
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
                        {
                            break;
                        }
                    }

                    if (!handedOff)
                    {
                        result->setActive(std::nothrow, true);
                        m_activeConnections++;
                    }
                    result->m_closed.store(false);
                }

                // Phase 2: HikariCP validation skip (outside lock)
                if (m_testOnBorrow)
                {
                    auto lastUsed = result->getLastUsedTime(std::nothrow);
                    auto now = steady_clock::now();
                    auto timeSinceLastUse = duration_cast<milliseconds>(now - lastUsed).count();

                    if (timeSinceLastUse > m_validationTimeoutMillis &&
                        !validateConnection(std::nothrow, result->getUnderlyingDocumentConnection()).value_or(false))
                    {
                        CP_DEBUG("DocumentDBConnectionPool::getDocumentDBConnection - Validation failed, removing connection");

                        {
                            std::scoped_lock lockPool(m_mutexPool);
                            result->setActive(std::nothrow, false);
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
            return cpp_dbc::unexpected(DBException("DWG5T3W0JL6V", ex.what(), system_utils::captureCallStack()));
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

    void DocumentDBConnectionPool::maintenanceTask()
    {
        do
        {
            {
                // Wait 30 seconds or until notified by close()
                std::unique_lock lock(m_mutexPool);
                m_maintenanceCondition.wait_for(lock, std::chrono::seconds(30), [this]
                                                { return !m_running.load(std::memory_order_acquire); });
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
            while (m_running.load(std::memory_order_acquire) && currentTotal < m_minIdle)
            {
                auto pooledResult = createPooledDBConnection(std::nothrow);
                if (!pooledResult.has_value())
                {
                    CP_DEBUG("DocumentDBConnectionPool::maintenanceTask - Failed to create minIdle connection: %s", pooledResult.error().what_s().data());
                    break;
                }

                {
                    std::scoped_lock lock(m_mutexPool);
                    m_idleConnections.push(pooledResult.value());
                    m_allConnections.push_back(pooledResult.value());
                    currentTotal = m_allConnections.size() + m_pendingCreations;
                    m_connectionAvailable.notify_one();
                }
            }
        } while (m_running.load(std::memory_order_acquire));
    }

    cpp_dbc::expected<size_t, DBException> DocumentDBConnectionPool::getActiveDBConnectionCount(std::nothrow_t) const noexcept
    {
        return static_cast<size_t>(m_activeConnections.load(std::memory_order_acquire));
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


    cpp_dbc::expected<size_t, DBException> DocumentDBConnectionPool::getIdleDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_idleConnections.size();
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

    cpp_dbc::expected<size_t, DBException> DocumentDBConnectionPool::getTotalDBConnectionCount(std::nothrow_t) const noexcept
    {
        std::scoped_lock lock(m_mutexPool);
        return m_allConnections.size();
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

    cpp_dbc::expected<void, DBException> DocumentDBConnectionPool::close(std::nothrow_t) noexcept
    {
        CP_DEBUG("DocumentDBConnectionPool::close - Starting close operation");

        if (!m_running.exchange(false))
        {
            CP_DEBUG("DocumentDBConnectionPool::close - Already closed, returning");
            return {}; // Already closed — idempotent
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
            CP_DEBUG("DocumentDBConnectionPool::close - Initial active connections: %d", m_activeConnections.load(std::memory_order_acquire));

            while (m_activeConnections.load(std::memory_order_acquire) > 0)
            {
                CP_DEBUG("DocumentDBConnectionPool::close - Waiting for %d active connections to finish at %lld", m_activeConnections.load(std::memory_order_acquire), (long long)std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

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

    void DocumentDBConnectionPool::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    cpp_dbc::expected<bool, DBException> DocumentDBConnectionPool::isRunning(std::nothrow_t) const noexcept
    {
        return m_running.load(std::memory_order_acquire);
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

    // DocumentPooledDBConnection implementation
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
        return m_poolAlive && m_poolAlive->load(std::memory_order_acquire);
    }

    DocumentPooledDBConnection::~DocumentPooledDBConnection()
    {
        if (!m_closed.load(std::memory_order_acquire) && m_conn)
        {
            try
            {
                // If the pool is no longer alive, close the physical connection.
                // Use qualified call to avoid virtual dispatch in destructor.
                // NOSONAR(cpp:S1699) - intentional qualified call in destructor to avoid virtual dispatch
                if (!DocumentPooledDBConnection::isPoolValid(std::nothrow))
                {
                    auto closeResult = m_conn->close(std::nothrow);
                    if (!closeResult.has_value())
                    {
                        CP_DEBUG("DocumentPooledDBConnection::~DocumentPooledDBConnection - close failed: %s", closeResult.error().what_s().data());
                    }
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
                    // will set m_closed back to true, maintaining correct error handling semantics.
                    // ============================================================================
                    m_closed.store(false);
                    auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<DocumentPooledDBConnection>(this->shared_from_this()));
                    if (!returnResult.has_value())
                    {
                        CP_DEBUG("DocumentPooledDBConnection::close - returnConnection failed: %s", returnResult.error().what_s().data());
                    }
                }
            }
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("DocumentPooledDBConnection::close - shared_from_this failed: %s", ex.what());
            m_closed.store(true);
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            CP_DEBUG("DocumentPooledDBConnection::close - Exception: %s", ex.what());
            m_closed.store(true);
        }
        catch (...) // NOSONAR - Catch-all to ensure m_closed is always set correctly on any exception
        {
            CP_DEBUG("DocumentPooledDBConnection::close - Unknown exception caught");
            m_closed.store(true);
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

    expected<bool, DBException> DocumentPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return true;
        }
        auto result = m_conn->isClosed(std::nothrow);
        if (!result.has_value())
        {
            return result;
        }
        return result.value();
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

    expected<void, DBException> DocumentPooledDBConnection::setActive(std::nothrow_t, bool isActive) noexcept
    {
        m_active.store(isActive, std::memory_order_release);
        return {};
    }

    bool DocumentPooledDBConnection::isActive(std::nothrow_t) const noexcept
    {
        return m_active.load(std::memory_order_acquire);
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
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    expected<bool, DBException> DocumentPooledDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return !m_active.load(std::memory_order_acquire);
    }

    std::string DocumentPooledDBConnection::getURL() const
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("3FB54DDBA7BA", "Connection is closed", system_utils::captureCallStack());
        }
        // Can't modify m_lastUsedTimeNs in a const method
        return m_conn->getURL();
    }

    expected<std::string, DBException> DocumentPooledDBConnection::getURL(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
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

    std::shared_ptr<DBConnection> DocumentPooledDBConnection::getUnderlyingConnection(std::nothrow_t) noexcept
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
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("L8G1QT50UXIA", "Connection is closed", system_utils::captureCallStack());
        }
        return m_conn->getDatabaseName();
    }

    std::vector<std::string> DocumentPooledDBConnection::listDatabases()
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("E214F2D3D2AC", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->listDatabases();
    }

    bool DocumentPooledDBConnection::databaseExists(const std::string &databaseName)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("OQ3AKT1GWEFX", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->databaseExists(databaseName);
    }

    void DocumentPooledDBConnection::useDatabase(const std::string &databaseName)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("A188FEBF0840", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        m_conn->useDatabase(databaseName);
    }

    void DocumentPooledDBConnection::dropDatabase(const std::string &databaseName)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("4WJIPC98HTAS", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        m_conn->dropDatabase(databaseName);
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::getCollection(const std::string &collectionName)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("W77JNUYM7UNB", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getCollection(collectionName);
    }

    std::vector<std::string> DocumentPooledDBConnection::listCollections()
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("1C1ZHI36YMBN", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->listCollections();
    }

    bool DocumentPooledDBConnection::collectionExists(const std::string &collectionName)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("LB8AQEYQ8W79", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->collectionExists(collectionName);
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::createCollection(
        const std::string &collectionName, const std::string &options)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("E7BD2C9371A2", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createCollection(collectionName, options);
    }

    void DocumentPooledDBConnection::dropCollection(const std::string &collectionName)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("BBTF7NNNRUGA", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        m_conn->dropCollection(collectionName);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument()
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("FGUY2RG1U7TL", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createDocument();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument(const std::string &json)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("QP1HG80G6YXD", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createDocument(json);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::runCommand(const std::string &command)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("2D95D9EF03AA", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->runCommand(command);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerInfo()
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("8ED95C914BAD", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerInfo();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerStatus()
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("162B1BDB50A9", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerStatus();
    }

    bool DocumentPooledDBConnection::ping()
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("9BCD37627AAD", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->ping();
    }

    std::string DocumentPooledDBConnection::startSession()
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("NQGFYKY2EI6L", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->startSession();
    }

    void DocumentPooledDBConnection::endSession(const std::string &sessionId)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("7CB8D9E25BF3", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        m_conn->endSession(sessionId);
    }

    void DocumentPooledDBConnection::startTransaction(const std::string &sessionId)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("A6BB94FF27F4", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        m_conn->startTransaction(sessionId);
    }

    void DocumentPooledDBConnection::commitTransaction(const std::string &sessionId)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("NL6LRD2YUH7O", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        m_conn->commitTransaction(sessionId);
    }

    void DocumentPooledDBConnection::abortTransaction(const std::string &sessionId)
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("T8VY0CZRCSD1", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        m_conn->abortTransaction(sessionId);
    }

    bool DocumentPooledDBConnection::supportsTransactions()
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            throw DBException("2FAE3A027B77", "Connection is closed", system_utils::captureCallStack());
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->supportsTransactions();
    }

    // ====================================================================
    // NOTHROW VERSIONS - delegate to underlying connection
    // ====================================================================

    expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listDatabases(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("7A8B9C0D1E2F", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->listDatabases(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::getCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("8B9C0D1E2F3A", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getCollection(std::nothrow, collectionName);
    }

    expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listCollections(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("9C0D1E2F3A4B", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->listCollections(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::createCollection(
        std::nothrow_t,
        const std::string &collectionName,
        const std::string &options) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("0D1E2F3A4B5C", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createCollection(std::nothrow, collectionName, options);
    }

    expected<void, DBException> DocumentPooledDBConnection::dropCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("1E2F3A4B5C6D", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->dropCollection(std::nothrow, collectionName);
    }

    expected<void, DBException> DocumentPooledDBConnection::dropDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("2F3A4B5C6D7E", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->dropDatabase(std::nothrow, databaseName);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("3A4B5C6D7E8F", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createDocument(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(
        std::nothrow_t, const std::string &json) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("4B5C6D7E8F9A", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createDocument(std::nothrow, json);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::runCommand(
        std::nothrow_t, const std::string &command) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("5C6D7E8F9A0B", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->runCommand(std::nothrow, command);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("6D7E8F9A0B1C", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerInfo(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerStatus(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("7E8F9A0B1C2D", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerStatus(std::nothrow);
    }

    expected<std::string, DBException> DocumentPooledDBConnection::getDatabaseName(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("QI6DG2WF85Q2", "Connection is closed"));
        }
        return m_conn->getDatabaseName(std::nothrow);
    }

    expected<bool, DBException> DocumentPooledDBConnection::databaseExists(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("FABBX2QR62GT", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->databaseExists(std::nothrow, databaseName);
    }

    expected<void, DBException> DocumentPooledDBConnection::useDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("CFZMARADNIPZ", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->useDatabase(std::nothrow, databaseName);
    }

    expected<bool, DBException> DocumentPooledDBConnection::collectionExists(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("AEDVQNDH5OBR", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->collectionExists(std::nothrow, collectionName);
    }

    expected<bool, DBException> DocumentPooledDBConnection::ping(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("218DK7FXM1U4", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->ping(std::nothrow);
    }

    expected<std::string, DBException> DocumentPooledDBConnection::startSession(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("WTWINKFWO8WZ", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->startSession(std::nothrow);
    }

    expected<void, DBException> DocumentPooledDBConnection::endSession(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("X7VNI3HTM5NN", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->endSession(std::nothrow, sessionId);
    }

    expected<void, DBException> DocumentPooledDBConnection::startTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("YYJXBAMEI07O", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->startTransaction(std::nothrow, sessionId);
    }

    expected<void, DBException> DocumentPooledDBConnection::commitTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("VQ2C4Y9YU1LX", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->commitTransaction(std::nothrow, sessionId);
    }

    expected<void, DBException> DocumentPooledDBConnection::abortTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("A7VGP008305O", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->abortTransaction(std::nothrow, sessionId);
    }

    expected<bool, DBException> DocumentPooledDBConnection::supportsTransactions(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return unexpected<DBException>(DBException("PLV8XFKGJ0PO", "Connection is closed"));
        }
        updateLastUsedTime(std::nothrow);
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
    }

} // namespace cpp_dbc
