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
#include <algorithm>
#include <iostream>

// Debug output is controlled by -DDEBUG_CONNECTION_POOL=1 CMake option
#if (defined(DEBUG_CONNECTION_POOL) && DEBUG_CONNECTION_POOL) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define CP_DEBUG(x) std::cout << x << std::endl
#else
#define CP_DEBUG(x)
#endif

namespace cpp_dbc
{

    /**
     * @brief Constructs a ColumnarDBConnectionPool configured with connection credentials and pool sizing/behavior options.
     *
     * Initializes the pool's configuration and internal state but does not allocate connections or start maintenance; pool initialization is performed by the factory create(...) methods which call initializePool().
     *
     * @param url Database connection URL.
     * @param username Username for authentication.
     * @param password Password for authentication.
     * @param options Additional driver-specific connection options.
     * @param initialSize Number of connections to create when the pool is initialized.
     * @param maxSize Maximum total number of connections allowed in the pool.
     * @param minIdle Minimum number of idle connections the pool will try to maintain.
     * @param maxWaitMillis Maximum time in milliseconds to wait for a connection when pool is at max capacity.
     * @param validationTimeoutMillis Timeout in milliseconds for validation operations.
     * @param idleTimeoutMillis Idle timeout in milliseconds after which idle connections may be eligible for removal.
     * @param maxLifetimeMillis Maximum lifetime in milliseconds for a connection before it is retired.
     * @param testOnBorrow If true, connections will be validated when borrowed.
     * @param testOnReturn If true, connections will be validated when returned.
     * @param validationQuery SQL query used to validate connections.
     * @param transactionIsolation Default transaction isolation level for created connections.
     */
    ColumnarDBConnectionPool::ColumnarDBConnectionPool(const std::string &url,
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
          m_transactionIsolation(transactionIsolation),
          m_running(true),
          m_activeConnections(0)
    {
        // Pool initialization will be done in the factory method
    }

    /**
     * @brief Constructs a ColumnarDBConnectionPool from a DBConnectionPoolConfig and initializes internal pool settings.
     *
     * Initializes pool state (URL, credentials, sizing, timeouts, validation settings, and transaction isolation) from the provided configuration.
     * Actual allocation of connections and start of the maintenance thread are deferred until initializePool() is invoked by the factory methods.
     *
     * @param config Configuration object providing URL, credentials, options, sizing, timeouts, validation query, and transaction isolation.
     */
    ColumnarDBConnectionPool::ColumnarDBConnectionPool(const config::DBConnectionPoolConfig &config)
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
          m_transactionIsolation(config.getTransactionIsolation()),
          m_running(true),
          m_activeConnections(0)
    {
        // Pool initialization will be done in the factory method
    }

    /**
     * @brief Initializes the connection pool by preallocating initial pooled connections and starting maintenance.
     *
     * Preallocates internal storage for up to the configured maximum size, creates the configured number of initial
     * pooled connections, enqueues them into the idle pool and records them in the all-connections list under the
     * appropriate locks, and launches the maintenance thread.
     *
     * @throws DBException if any step of initialization fails (for example, creating connections or starting the thread).
     */
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
                {
                    std::lock_guard<std::mutex> lockIdle(m_mutexIdleConnections);
                    std::lock_guard<std::mutex> lockAll(m_mutexAllConnections);
                    m_idleConnections.push(pooledConn);
                    m_allConnections.push_back(pooledConn);
                }
            }

            // Start maintenance thread
            m_maintenanceThread = std::thread(&ColumnarDBConnectionPool::maintenanceTask, this);
        }
        catch (const std::exception &e)
        {
            close();
            throw DBException("C54157A1F4D8", "Failed to initialize connection pool: " + std::string(e.what()), system_utils::captureCallStack());
        }
    }

    /**
     * @brief Create and initialize a ColumnarDBConnectionPool with the given configuration.
     *
     * Constructs a pool configured by the provided parameters, initializes it (pre-allocating
     * initial connections and starting the maintenance thread), and returns a shared pointer
     * to the ready-to-use pool.
     *
     * @param url Connection URL for the target columnar database.
     * @param username Authentication username.
     * @param password Authentication password.
     * @param options Additional driver/connection options as key/value pairs.
     * @param initialSize Number of connections to create during pool initialization.
     * @param maxSize Maximum total number of connections the pool may create.
     * @param minIdle Minimum number of idle connections the pool will try to maintain.
     * @param maxWaitMillis Maximum time in milliseconds to wait for a connection when the pool is exhausted.
     * @param validationTimeoutMillis Timeout in milliseconds for connection validation operations.
     * @param idleTimeoutMillis Duration in milliseconds after which an idle connection may be considered expired.
     * @param maxLifetimeMillis Maximum lifetime in milliseconds for a connection before it is retired.
     * @param testOnBorrow If true, connections are validated when borrowed from the pool.
     * @param testOnReturn If true, connections are validated when returned to the pool.
     * @param validationQuery SQL query used to validate connections; ignored if empty.
     * @param transactionIsolation Transaction isolation level to apply to created connections.
     *
     * @return std::shared_ptr<ColumnarDBConnectionPool> Shared pointer to the initialized connection pool.
     */
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
        auto pool = std::shared_ptr<ColumnarDBConnectionPool>(new ColumnarDBConnectionPool(
            url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, validationQuery, transactionIsolation));

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    /**
     * @brief Create and initialize a ColumnarDBConnectionPool from the given configuration.
     *
     * Constructs a new pool using values from `config`, then initializes it (pre-allocates initial
     * connections and starts the maintenance thread) before returning the shared pointer.
     *
     * @param config Configuration values used to build the pool (sizing, timeouts, credentials, etc.).
     * @return std::shared_ptr<ColumnarDBConnectionPool> Shared pointer to the initialized connection pool.
     *
     * @throws DBException If pool initialization fails (e.g., connection creation or maintenance thread startup).
     */
    std::shared_ptr<ColumnarDBConnectionPool> ColumnarDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto pool = std::shared_ptr<ColumnarDBConnectionPool>(new ColumnarDBConnectionPool(config));

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    /**
     * @brief Destroy the connection pool and release its resources.
     *
     * Ensures the pool is closed, stops the maintenance thread, and releases all pooled connections and related resources before destruction.
     */
    ColumnarDBConnectionPool::~ColumnarDBConnectionPool()
    {
        CP_DEBUG("ColumnarDBConnectionPool::~ColumnarDBConnectionPool - Starting destructor at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        close();

        CP_DEBUG("ColumnarDBConnectionPool::~ColumnarDBConnectionPool - Destructor completed at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    /**
     * @brief Obtains a new columnar database connection from the driver manager.
     *
     * Acquires a DBConnection using the pool's configured URL, credentials, and options,
     * and returns it as a ColumnarDBConnection.
     *
     * @return std::shared_ptr<ColumnarDBConnection> Shared pointer to the newly obtained ColumnarDBConnection.
     *
     * @throws DBException If the underlying driver returns a connection that is not a ColumnarDBConnection.
     */
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

    /**
     * @brief Creates a pooled wrapper around a new ColumnarDBConnection tied to this pool.
     *
     * The returned object wraps a freshly created underlying ColumnarDBConnection and holds a weak
     * reference to this pool so it can be returned to the pool when closed. If this pool instance is
     * not owned by a std::shared_ptr, the weak pool reference will be empty and the wrapper will act
     * without a pool target.
     *
     * Note: the pool's configured transaction isolation level is recorded by the pool but is not
     * applied to the underlying connection here.
     *
     * @return std::shared_ptr<ColumnarPooledDBConnection> A shared pointer to the new pooled connection;
     *         the pooled connection contains a weak reference to the pool which may be empty if the
     *         pool is not managed by a shared_ptr.
     */
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
        catch (const std::bad_weak_ptr &)
        {
            // Pool is not managed by shared_ptr, weakPool remains empty
            // This can happen if the pool is stack-allocated
        }

        // Create the pooled connection
        auto pooledConn = std::make_shared<ColumnarPooledDBConnection>(conn, weakPool, m_poolAlive);
        return pooledConn;
    }

    /**
     * @brief Checks whether a pooled ColumnarDBConnection is valid by executing the pool's validation query.
     *
     * @param conn Connection to validate; the validation query will be executed on this connection.
     * @return true if the validation query succeeded, false otherwise.
     */
    bool ColumnarDBConnectionPool::validateConnection(std::shared_ptr<ColumnarDBConnection> conn)
    {
        try
        {
            // Use validation query to check connection
            auto resultSet = conn->executeQuery(m_validationQuery);
            return true;
        }
        catch (const DBException &e)
        {
            return false;
        }
    }

    /**
     * @brief Returns a pooled connection to the pool and updates pool state.
     *
     * If the provided pooled connection is null, already inactive, or not tracked by the pool,
     * the call does nothing. If the pool is stopped, the underlying connection is closed.
     * When the pool is running, the connection is optionally validated; a valid connection is
     * marked inactive and enqueued as idle, while an invalid connection is replaced with a
     * newly created pooled connection and the invalid one is discarded. The method updates
     * active/idle counters and notifies the maintenance thread.
     *
     * @param conn The pooled connection being returned to the pool.
     */
    void ColumnarDBConnectionPool::returnConnection(std::shared_ptr<ColumnarPooledDBConnection> conn)
    {
        std::lock_guard<std::mutex> lock(m_mutexReturnConnection);

        if (!conn)
        {
            return;
        }

        if (!m_running.load())
        {
            try
            {
                conn->getUnderlyingColumnarConnection()->close();
            }
            catch (const std::exception &)
            {
                // Ignore exceptions during close
            }
            return;
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive())
        {
            return;
        }

        // Additional safety check: verify connection is in allConnections
        {
            std::lock_guard<std::mutex> lockAll(m_mutexAllConnections);
            auto it = std::find(m_allConnections.begin(), m_allConnections.end(), conn);
            if (it == m_allConnections.end())
            {
                return;
            }
        }

        bool valid = true;

        if (m_testOnReturn)
        {
            valid = validateConnection(conn->getUnderlyingColumnarConnection());
        }

        if (valid)
        {
            // Mark as inactive and update last used time
            conn->setActive(false);

            // Add to idle connections queue
            {
                std::lock_guard<std::mutex> lockIdle(m_mutexIdleConnections);
                m_idleConnections.push(conn);
                m_activeConnections--;
            }
        }
        else
        {
            // Replace invalid connection with a new one
            try
            {
                {
                    std::lock_guard<std::mutex> lockAll(m_mutexAllConnections);
                    auto it = std::find(m_allConnections.begin(), m_allConnections.end(), conn);
                    if (it != m_allConnections.end())
                    {
                        *it = createPooledDBConnection();

                        {
                            std::lock_guard<std::mutex> lockIdle(m_mutexIdleConnections);
                            m_idleConnections.push(*it);
                        }
                    }
                }
                m_activeConnections--;
            }
            catch (const std::exception &)
            {
                m_activeConnections--;
                // Log error here if needed
            }
        }

        // Notify maintenance thread that a connection was returned
        m_maintenanceCondition.notify_one();
    }

    /**
     * @brief Acquire an idle pooled columnar DB connection if one is immediately available.
     *
     * Attempts to pop an existing idle connection and, if configured, validates it before returning.
     * If validation fails the connection is removed from the pool and, if the pool is still running,
     * the function will try to create and return a replacement; if creation fails or the pool is not
     * running, the function returns `nullptr`. If no idle connection is available and the pool has
     * capacity to grow, the function returns `nullptr` to indicate a new connection may be created.
     *
     * @return std::shared_ptr<ColumnarPooledDBConnection> An idle, validated pooled connection, or
     * `nullptr` if no suitable idle connection is available (either none exist, validation failed and
     * replacement could not be created, or the caller should create a new connection).
     */
    std::shared_ptr<ColumnarPooledDBConnection> ColumnarDBConnectionPool::getIdleDBConnection()
    {
        std::lock_guard<std::mutex> lock(m_mutexIdleConnections);

        if (!m_idleConnections.empty())
        {
            auto conn = m_idleConnections.front();
            m_idleConnections.pop();

            // Test connection before use if configured
            if (m_testOnBorrow)
            {
                if (!validateConnection(conn->getUnderlyingColumnarConnection()))
                {
                    // Remove from allConnections
                    std::lock_guard<std::mutex> lockAll(m_mutexAllConnections);
                    auto it = std::find(m_allConnections.begin(), m_allConnections.end(), conn);
                    if (it != m_allConnections.end())
                    {
                        m_allConnections.erase(it);
                    }

                    // Create new connection if we're still running
                    if (m_running.load())
                    {
                        try
                        {
                            return createPooledDBConnection();
                        }
                        catch (const std::exception &)
                        {
                            // Return nullptr if we can't create a new connection
                            return nullptr;
                        }
                    }
                    return nullptr;
                }
            }

            return conn;
        }
        else if (m_allConnections.size() < m_maxSize)
        {
            // Signal that we need to create a new connection
            return nullptr;
        }

        return nullptr;
    }

    /**
     * @brief Obtain a pooled database connection.
     *
     * Retrieves a pooled Columnar DB connection suitable for use as a DBConnection.
     *
     * @return std::shared_ptr<DBConnection> A shared pointer to an active pooled DBConnection.
     */
    std::shared_ptr<DBConnection> ColumnarDBConnectionPool::getDBConnection()
    {
        return getColumnarDBConnection();
    }

    /**
     * @brief Acquire a pooled columnar database connection, creating one if allowed or waiting until one is available.
     *
     * Attempts to obtain an idle pooled connection; if none are available and the pool has capacity, a new pooled
     * connection is created and tracked. If the pool is at max capacity, waits up to the configured timeout for a
     * connection to become available.
     *
     * @return std::shared_ptr<ColumnarDBConnection> A pooled ColumnarDBConnection wrapped in a shared_ptr. The returned
     * connection is marked active and its closed flag is cleared.
     *
     * @throws DBException If the pool is closed when called or becomes closed while waiting.
     * @throws DBException If no connection becomes available before the configured wait timeout.
     */
    std::shared_ptr<ColumnarDBConnection> ColumnarDBConnectionPool::getColumnarDBConnection()
    {
        std::unique_lock<std::mutex> lock(m_mutexGetConnection);

        if (!m_running.load())
        {
            throw DBException("6R7S8T9U0V1W", "Connection pool is closed", system_utils::captureCallStack());
        }

        std::shared_ptr<ColumnarPooledDBConnection> result = this->getIdleDBConnection();

        // If no connection available, check if we can create a new one
        if (result == nullptr && m_allConnections.size() < m_maxSize)
        {
            result = createPooledDBConnection();

            {
                std::lock_guard<std::mutex> lock_all(m_mutexAllConnections);
                m_allConnections.push_back(result);
            }
        }
        // If no connection available, wait until one becomes available
        else if (result == nullptr)
        {
            auto waitStart = std::chrono::steady_clock::now();

            // Wait until a connection is returned to the pool or timeout
            do
            {
                auto now = std::chrono::steady_clock::now();

                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                auto waitedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - waitStart).count();

                if (waitedMs >= m_maxWaitMillis)
                {
                    throw DBException("99E01137B87B", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                }

                if (!m_running.load())
                {
                    throw DBException("69677B95E2B2", "Connection pool is closed", system_utils::captureCallStack());
                }

                result = this->getIdleDBConnection();

            } while (result == nullptr);
        }

        // Mark as active and reset closed flag (connection is being reused)
        result->setActive(true);
        result->m_closed.store(false);
        m_activeConnections++;

        return result;
    }

    /**
     * @brief Background maintenance routine for the connection pool.
     *
     * Runs in a dedicated thread while the pool is running to reclaim expired idle
     * connections and to ensure the pool maintains at least the configured minimum
     * number of idle connections.
     *
     * The task waits up to 30 seconds or until notified (for example when the pool
     * is being closed), inspects idle and pooled connections under mutex
     * protection, removes expired idle connections when there are more than
     * `m_minIdle` available (expiration is based on idle time and total lifetime),
     * and creates new pooled connections as needed to reach `m_minIdle`.
     *
     * This function acquires locks protecting the pool's connection lists while
     * inspecting and modifying them and exits promptly when the pool is stopped.
     */
    void ColumnarDBConnectionPool::maintenanceTask()
    {
        do
        {
            // Wait for 30 seconds or until notified (e.g., when close() is called)
            std::unique_lock<std::mutex> lock(m_mutexMaintenance);
            m_maintenanceCondition.wait_for(lock, std::chrono::seconds(30), [this]
                                            { return !m_running; });

            if (!m_running)
            {
                break;
            }

            auto now = std::chrono::steady_clock::now();

            // Ensure no body touch the allConnections and idleConnections variables when is used by this thread
            std::lock_guard<std::mutex> lockAllConnections(m_mutexAllConnections);
            std::lock_guard<std::mutex> lockIdleConnectons(m_mutexIdleConnections);

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

                    // Remove from allConnections
                    it = m_allConnections.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            // Ensure we have at least minIdle connections
            while (m_running && m_allConnections.size() < m_minIdle)
            {
                auto pooledConn = createPooledDBConnection();
                m_idleConnections.push(pooledConn);
                m_allConnections.push_back(pooledConn);
            }
        } while (m_running);
    }

    /**
     * @brief Reports how many pooled connections are currently in use by clients.
     *
     * @return size_t Number of active (borrowed and not yet returned) connections.
     */
    size_t ColumnarDBConnectionPool::getActiveDBConnectionCount() const
    {
        return m_activeConnections;
    }

    /**
     * @brief Returns the number of currently idle pooled connections.
     *
     * @return size_t Number of connections currently sitting idle and available for borrowing.
     */
    size_t ColumnarDBConnectionPool::getIdleDBConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutexIdleConnections);
        return m_idleConnections.size();
    }

    /**
     * @brief Get the total number of connections currently tracked by the pool.
     *
     * This count includes both idle and active connections and is obtained under the pool's internal lock.
     *
     * @return size_t Total number of connections tracked by the pool (idle + active).
     */
    size_t ColumnarDBConnectionPool::getTotalDBConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutexAllConnections);
        return m_allConnections.size();
    }

    /**
     * @brief Gracefully shuts down the connection pool and closes all managed connections.
     *
     * Stops pool operation so no new borrows will succeed, marks the pool as not alive so pooled
     * connections will not attempt to return, waits briefly for active operations to finish (with
     * a bounded timeout), stops the maintenance thread, and closes and clears all idle and tracked
     * connections.
     *
     * After this call the pool is no longer usable; subsequent operations that require a running
     * pool will fail.
     */
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

        CP_DEBUG("ColumnarDBConnectionPool::close - Waiting for active operations to complete at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            CP_DEBUG("ColumnarDBConnectionPool::close - Initial active connections: " << m_activeConnections.load());

            while (m_activeConnections.load() > 0)
            {
                CP_DEBUG("ColumnarDBConnectionPool::close - Waiting for " << m_activeConnections.load()
                                                                          << " active connections to finish at "
                                                                          << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 1 == 0)
                {
                    CP_DEBUG("ColumnarDBConnectionPool::close - Waited " << elapsed_seconds
                                                                         << " seconds for active connections");
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("ColumnarDBConnectionPool::close - Timeout waiting for active connections, forcing close at "
                             << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                    // Force active connections to be marked as inactive
                    m_activeConnections.store(0);
                    break;
                }
            }
        }

        // Notify all waiting threads
        m_maintenanceCondition.notify_all();

        // Join maintenance thread (only if it was started)
        if (m_maintenanceThread.joinable())
        {
            m_maintenanceThread.join();
        }

        // Close all connections
        std::lock_guard<std::mutex> lockAllConnections(m_mutexAllConnections);
        std::lock_guard<std::mutex> lockIdleConnections(m_mutexIdleConnections);

        for (size_t i = 0; i < m_allConnections.size(); ++i)
        {
            try
            {
                auto &conn = m_allConnections[i];
                if (conn && conn->getUnderlyingConnection())
                {
                    // Mark connection as inactive before closing
                    conn->setActive(false);

                    // Close the underlying connection
                    conn->getUnderlyingConnection()->close();

                    // Ensure the connection is properly returned to the pool
                    if (conn->getUnderlyingConnection()->isPooled())
                    {
                        conn->getUnderlyingConnection()->returnToPool();
                    }
                }
            }
            catch (...)
            {
                // Ignore errors during close
            }
        }

        // Clear collections
        while (!m_idleConnections.empty())
        {
            m_idleConnections.pop();
        }
        m_allConnections.clear();
    }

    /**
     * @brief Reports whether the connection pool is currently running.
     *
     * @return `true` if the pool is running and accepting/maintaining connections, `false` otherwise.
     */
    bool ColumnarDBConnectionPool::isRunning() const
    {
        return m_running.load();
    }

    /**
     * @brief Wraps a real ColumnarDBConnection with pool metadata and lifecycle tracking.
     *
     * Constructs a pooled connection that holds the underlying ColumnarDBConnection, a weak
     * reference to its owning pool, and an atomic flag that indicates whether the pool is alive.
     * Records creation and last-used timestamps and initializes active/closed state.
     *
     * @param connection Shared pointer to the underlying ColumnarDBConnection to be managed.
     * @param connectionPool Weak pointer to the owning ColumnarDBConnectionPool used to return the connection.
     * @param poolAlive Shared atomic flag that is true while the pool is alive and false after pool shutdown.
     */
    ColumnarPooledDBConnection::ColumnarPooledDBConnection(
        std::shared_ptr<ColumnarDBConnection> connection,
        std::weak_ptr<ColumnarDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive)
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive), m_active(false), m_closed(false)
    {
        m_creationTime = std::chrono::steady_clock::now();
        m_lastUsedTime = m_creationTime;
    }

    /**
     * @brief Checks whether the originating connection pool is still alive.
     *
     * @return `true` if the pool's alive flag exists and is set to true, `false` otherwise.
     */
    bool ColumnarPooledDBConnection::isPoolValid() const
    {
        return m_poolAlive && m_poolAlive->load();
    }

    /**
     * @brief Cleans up a pooled connection when the wrapper is destroyed.
     *
     * If the pooled wrapper is not already closed and holds an underlying connection,
     * this destructor either returns the connection to its pool (if the pool is still valid)
     * or closes the underlying physical connection. Any exceptions thrown during cleanup
     * are caught and ignored.
     */
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
                    // Return to pool
                    returnToPool();
                }
            }
            catch (const std::exception &)
            {
                // Ignore exceptions in destructor
            }
        }
    }

    /**
     * @brief Close the pooled connection or return it to its pool.
     *
     * Atomically marks this pooled connection as closed and either returns it to the owning
     * pool (if the pool is still valid) or closes the underlying connection. If the connection
     * is successfully returned to the pool, the closed flag is cleared so the connection remains usable.
     *
     * Exceptions from obtaining a shared pointer to the pool or from pool operations are caught;
     * on such failures the underlying connection is closed if present.
     */
    void ColumnarPooledDBConnection::close()
    {
        // Use atomic exchange to ensure only one thread processes the close
        bool expected = false;
        if (m_closed.compare_exchange_strong(expected, true))
        {
            try
            {
                // Return to pool instead of actually closing
                m_lastUsedTime = std::chrono::steady_clock::now();

                // Check if pool is still alive using the shared atomic flag
                if (isPoolValid())
                {
                    // Try to obtain a shared_ptr from the weak_ptr
                    if (auto poolShared = m_pool.lock())
                    {
                        poolShared->returnConnection(std::static_pointer_cast<ColumnarPooledDBConnection>(shared_from_this()));
                        m_closed.store(false); // Connection has been returned to pool, mark as not closed
                    }
                }
                else if (m_conn)
                {
                    // If pool is invalid, actually close the connection
                    m_conn->close();
                }
            }
            catch (const std::bad_weak_ptr &)
            {
                // shared_from_this failed, just close the connection
                if (m_conn)
                {
                    m_conn->close();
                }
            }
            catch (const std::exception &)
            {
                // Any other exception, just close the connection
                if (m_conn)
                {
                    m_conn->close();
                }
            }
        }
    }

    /**
     * @brief Indicates whether this pooled connection is closed or its underlying connection is closed.
     *
     * @return `true` if this pooled connection has been marked closed or the underlying connection reports closed, `false` otherwise.
     */
    bool ColumnarPooledDBConnection::isClosed()
    {
        return m_closed || m_conn->isClosed();
    }

    /**
     * @brief Create a prepared statement for the given SQL query on this pooled connection.
     *
     * Updates the connection's last-used timestamp and delegates statement preparation to the underlying connection.
     *
     * @param query SQL query to compile into a prepared statement.
     * @return std::shared_ptr<ColumnarDBPreparedStatement> Prepared statement for the provided query.
     * @throws DBException if the pooled connection is closed.
     */
    std::shared_ptr<ColumnarDBPreparedStatement> ColumnarPooledDBConnection::prepareStatement(const std::string &query)
    {
        if (m_closed)
        {
            throw DBException("A61A8B1BF33C", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->prepareStatement(query);
    }

    /**
     * @brief Attempts to prepare a statement from the underlying connection without throwing.
     *
     * Prepares the given SQL query on the underlying ColumnarDBConnection and updates the connection's last-used timestamp.
     *
     * @param query SQL query text to prepare.
     * @return cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> Prepared statement on success; `unexpected(DBException)` if the pooled connection is closed or if preparation fails.
     */
    cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> ColumnarPooledDBConnection::prepareStatement(std::nothrow_t, const std::string &query) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("A61A8B1BF33C", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
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

    /**
     * @brief Execute a SQL query using the underlying columnar connection.
     *
     * Updates the connection's last-used timestamp and delegates the query to the underlying
     * ColumnarDBConnection, returning its result set.
     *
     * @param query SQL query to execute.
     * @return std::shared_ptr<ColumnarDBResultSet> Result set produced by the query.
     * @throws DBException If this pooled connection is closed or if the underlying connection fails.
     */
    std::shared_ptr<ColumnarDBResultSet> ColumnarPooledDBConnection::executeQuery(const std::string &query)
    {
        if (m_closed)
        {
            throw DBException("0AF43G67D005", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->executeQuery(query);
    }

    /**
     * Execute the given SQL query and provide its result set or an error.
     *
     * @param query SQL query string to execute.
     * @return cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> On success contains a shared pointer to the resulting ColumnarDBResultSet. On failure contains a `DBException` (for example: closed connection error `0AF43G67D005`, mapped runtime exception `0AF43G67D006`, or unknown error `0AF43G67D007`).
     */
    cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> ColumnarPooledDBConnection::executeQuery(std::nothrow_t, const std::string &query) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("0AF43G67D005", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
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

    /**
     * @brief Execute an SQL update statement on the underlying connection.
     *
     * Executes the provided update query and updates the connection's last-used timestamp.
     *
     * @param query The SQL update statement to execute.
     * @return uint64_t Number of rows affected by the update.
     * @throws DBException If the pooled connection is closed or if underlying execution fails.
     */
    uint64_t ColumnarPooledDBConnection::executeUpdate(const std::string &query)
    {
        if (m_closed)
        {
            throw DBException("4E6D7284F5E2", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->executeUpdate(query);
    }

    /**
     * @brief Execute an update statement without throwing and report the affected row count.
     *
     * @param query SQL update/insert/delete statement to execute.
     * @return cpp_dbc::expected<uint64_t, DBException> Number of rows affected on success; `unexpected(DBException)` on error (for example if the connection is closed or execution fails).
     */
    cpp_dbc::expected<uint64_t, DBException> ColumnarPooledDBConnection::executeUpdate(std::nothrow_t, const std::string &query) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("4E6D7284F5E2", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
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

    /**
     * Begin a transaction on the underlying columnar database connection.
     *
     * Updates the connection's last-used timestamp.
     *
     * @return `true` if the transaction was started successfully, `false` otherwise.
     * @throws DBException If the pooled connection is closed.
     */
    bool ColumnarPooledDBConnection::beginTransaction()
    {
        if (m_closed)
        {
            throw DBException("C8D9E0F1G2H3", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->beginTransaction();
    }

    /**
     * @brief Begins a transaction using the non-throwing variant and reports errors as a DBException result.
     *
     * If the pooled connection is closed, returns an unexpected `DBException`. On success, the returned
     * expected contains `true` when the transaction was started and `false` when it was not.
     *
     * @returns cpp_dbc::expected<bool, DBException> An expected containing `true` if the transaction was started,
     * `false` if it was not, or an unexpected `DBException` on error (including when the connection is closed).
     */
    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("C8D9E0F1G2H3", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
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

    /**
     * @brief Commit the current transaction on the underlying connection.
     *
     * Updates the connection's last-used timestamp and forwards the commit to the underlying ColumnarDBConnection.
     *
     * @throws DBException If this pooled connection is closed.
     * @throws DBException Propagates any DBException thrown by the underlying connection's commit.
     */
    void ColumnarPooledDBConnection::commit()
    {
        if (m_closed)
        {
            throw DBException("F43ECCD8427F", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->commit();
    }

    /**
     * @brief Attempts to commit the current transaction on the underlying connection without throwing.
     *
     * Performs the commit if the pooled connection is open and updates the connection's last-used timestamp.
     *
     * @return cpp_dbc::expected<void, DBException> `void` on success; `unexpected(DBException)` containing an error code, message, and call stack on failure (including when the connection is closed).
     */
    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::commit(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("F43ECCD8427F", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
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

    /**
     * @brief Rolls back the current transaction on the underlying connection.
     *
     * Updates the connection's last-used timestamp and delegates the rollback to the underlying ColumnarDBConnection.
     *
     * @throws DBException if the pooled connection is closed or if the underlying connection fails to roll back.
     */
    void ColumnarPooledDBConnection::rollback()
    {
        if (m_closed)
        {
            throw DBException("1A5723BF02C7", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->rollback();
    }

    /**
     * @brief Attempts to roll back the current transaction on the underlying connection.
     *
     * Updates the connection's last-used timestamp and invokes a non-throwing rollback on the wrapped
     * ColumnarDBConnection. On success returns a engaged expected; on failure returns an unexpected
     * containing a DBException describing the error.
     *
     * @returns cpp_dbc::expected<void, DBException>
     * - engaged on success.
     * - unexpected with a DBException if the pooled connection is closed, if the underlying rollback
     *   operation reported an error, if a std::exception was thrown during rollback, or for an
     *   unknown error.
     */
    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::rollback(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("1A5723BF02C7", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
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

    /**
     * Retrieve the URL of the underlying connection.
     *
     * @return std::string The connection URL.
     * @throws DBException If the pooled connection is closed.
     */
    std::string ColumnarPooledDBConnection::getURL() const
    {
        if (m_closed)
        {
            throw DBException("B5C8D0E3F6G2", "Connection is closed", system_utils::captureCallStack());
        }
        // Can't modify m_lastUsedTime in a const method
        return m_conn->getURL();
    }

    /**
     * @brief Timestamp of when this pooled connection was created.
     *
     * @return std::chrono::time_point<std::chrono::steady_clock> Time point representing the connection's creation time.
     */
    std::chrono::time_point<std::chrono::steady_clock> ColumnarPooledDBConnection::getCreationTime() const
    {
        return m_creationTime;
    }

    /**
     * @brief Returns the timestamp when this pooled connection was last used.
     *
     * @return std::chrono::time_point<std::chrono::steady_clock> Time point of the last use. 
     */
    std::chrono::time_point<std::chrono::steady_clock> ColumnarPooledDBConnection::getLastUsedTime() const
    {
        return m_lastUsedTime;
    }

    /**
     * @brief Set the pooled connection's active state.
     *
     * Marks the connection as active when checked out from the pool or inactive when returned.
     *
     * @param isActive `true` to mark the connection as active (checked out), `false` to mark it as inactive/available.
     */
    void ColumnarPooledDBConnection::setActive(bool isActive)
    {
        m_active = isActive;
    }

    /**
     * @brief Indicates whether the pooled connection is currently marked active (in use).
     *
     * @return `true` if the connection is marked active (in use), `false` otherwise.
     */
    bool ColumnarPooledDBConnection::isActive() const
    {
        return m_active;
    }

    /**
     * @brief Return this pooled connection to its owning pool if possible.
     *
     * Attempts to hand the connection back to its pool: the method marks the
     * connection as closed for the duration of the handoff, updates its last-used
     * timestamp, and—if the pool is still valid and a shared pointer to it can be
     * obtained—invokes the pool's returnConnection to re-enqueue the pooled
     * connection and clears the closed flag so the connection remains reusable.
     *
     * If the pool is no longer available or the handoff fails, the connection
     * remains closed. The function swallows exceptions and does not throw.
     */
    void ColumnarPooledDBConnection::returnToPool()
    {
        // Use atomic exchange to ensure only one thread processes the close
        bool expected = false;
        if (!m_closed.compare_exchange_strong(expected, true))
        {
            return;
        }

        try
        {
            // Return to pool instead of actually closing
            m_lastUsedTime = std::chrono::steady_clock::now();

            // Check if pool is still alive using the shared atomic flag
            if (isPoolValid())
            {
                // Try to obtain a shared_ptr from the weak_ptr
                if (auto poolShared = m_pool.lock())
                {
                    poolShared->returnConnection(std::static_pointer_cast<ColumnarPooledDBConnection>(this->shared_from_this()));
                    m_closed.store(false);
                }
            }
        }
        catch (const std::bad_weak_ptr &e)
        {
            // shared_from_this failed, keep as closed
        }
        catch (const std::exception &e)
        {
            // Any other exception, keep as closed
        }
    }

    /**
     * @brief Indicates whether this connection is currently returned to the pool.
     *
     * @return `true` if the connection is pooled (not marked active), `false` otherwise.
     */
    bool ColumnarPooledDBConnection::isPooled()
    {
        return this->m_active == false;
    }

    /**
     * @brief Returns the underlying DBConnection wrapped by this pooled connection.
     *
     * @return std::shared_ptr<DBConnection> Shared pointer to the underlying DBConnection, or nullptr if no underlying connection exists.
     */
    std::shared_ptr<DBConnection> ColumnarPooledDBConnection::getUnderlyingConnection()
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

    /**
     * @brief Accesses the underlying ColumnarDBConnection wrapped by this pooled connection.
     *
     * @return std::shared_ptr<ColumnarDBConnection> Shared pointer to the underlying ColumnarDBConnection, or `nullptr` if no underlying connection exists.
     */
    std::shared_ptr<ColumnarDBConnection> ColumnarPooledDBConnection::getUnderlyingColumnarConnection()
    {
        return m_conn;
    }

    // Scylla connection pool implementation
    namespace Scylla
    {
        /**
         * @brief Construct a ScyllaConnectionPool configured with the provided connection parameters.
         *
         * Initializes a Scylla-specific connection pool instance using the given URL and credentials.
         *
         * @param url Database connection URL.
         * @param username Username for authentication.
         * @param password Password for authentication.
         */
        ScyllaConnectionPool::ScyllaConnectionPool(const std::string &url,
                                                   const std::string &username,
                                                   const std::string &password)
            : ColumnarDBConnectionPool(url, username, password)
        {
            // Scylla-specific initialization if needed
        }

        /**
         * @brief Constructs a ScyllaConnectionPool configured from the given pool configuration.
         *
         * Initializes a Scylla-specific connection pool using values from `config`.
         *
         * @param config Configuration values used to initialize pool sizing, timeouts, credentials, and validation behavior.
         */
        ScyllaConnectionPool::ScyllaConnectionPool(const config::DBConnectionPoolConfig &config)
            : ColumnarDBConnectionPool(config)
        {
            // Scylla-specific initialization if needed
        }

        /**
         * @brief Creates and initializes a ScyllaConnectionPool for the provided connection credentials.
         *
         * @param url Connection URL for the Scylla cluster.
         * @param username Username used for authentication.
         * @param password Password used for authentication.
         * @return std::shared_ptr<ScyllaConnectionPool> Shared pointer to the initialized connection pool.
         *
         * @throws DBException If pool initialization fails.
         */
        std::shared_ptr<ScyllaConnectionPool> ScyllaConnectionPool::create(const std::string &url,
                                                                           const std::string &username,
                                                                           const std::string &password)
        {
            auto pool = std::shared_ptr<ScyllaConnectionPool>(new ScyllaConnectionPool(url, username, password));
            pool->initializePool();
            return pool;
        }

        /**
         * @brief Create and initialize a ScyllaConnectionPool from the provided configuration.
         *
         * Constructs a ScyllaConnectionPool configured according to `config`, runs pool initialization (pre-allocating initial connections and starting maintenance), and returns a shared pointer to the ready-to-use pool.
         *
         * @param config Configuration parameters for the connection pool.
         * @return std::shared_ptr<ScyllaConnectionPool> Shared pointer to the initialized connection pool.
         * @throws DBException If pool initialization fails.
         */
        std::shared_ptr<ScyllaConnectionPool> ScyllaConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<ScyllaConnectionPool>(new ScyllaConnectionPool(config));
            pool->initializePool();
            return pool;
        }
    }

} // namespace cpp_dbc