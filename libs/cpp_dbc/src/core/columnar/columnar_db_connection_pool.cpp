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

    // ColumnarDBConnectionPool implementation
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
          m_transactionIsolation(transactionIsolation)
    {
        // Pool initialization will be done in the factory method
    }

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
                    std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);
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
        auto pool = std::shared_ptr<ColumnarDBConnectionPool>(new ColumnarDBConnectionPool( // NOSONAR - make_shared cannot be used with private constructor
            url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, validationQuery, transactionIsolation));

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    std::shared_ptr<ColumnarDBConnectionPool> ColumnarDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto pool = std::shared_ptr<ColumnarDBConnectionPool>(new ColumnarDBConnectionPool(config)); // NOSONAR - make_shared cannot be used with private constructor

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    ColumnarDBConnectionPool::~ColumnarDBConnectionPool()
    {
        CP_DEBUG("ColumnarDBConnectionPool::~ColumnarDBConnectionPool - Starting destructor at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        close();

        CP_DEBUG("ColumnarDBConnectionPool::~ColumnarDBConnectionPool - Destructor completed at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
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
            CP_DEBUG("ColumnarDBConnectionPool::createPooledDBConnection - bad_weak_ptr: " << ex.what());
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
        catch (const DBException &)
        {
            return false;
        }
    }

    void ColumnarDBConnectionPool::returnConnection(std::shared_ptr<ColumnarPooledDBConnection> conn)
    {
        std::scoped_lock lock(m_mutexReturnConnection);

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
            catch (const DBException &ex)
            {
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - Exception during close (pool shutting down): " << ex.what());
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
            std::scoped_lock lockAll(m_mutexAllConnections);
            auto it = std::ranges::find(m_allConnections, conn);
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
                std::scoped_lock lockIdle(m_mutexIdleConnections);
                m_idleConnections.push(conn);
                m_activeConnections--;
            }
        }
        else
        {
            // Use scoped_lock for consistent lock ordering to prevent deadlock
            std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);

            // Replace invalid connection with a new one
            try
            {
                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    *it = createPooledDBConnection();
                    m_idleConnections.push(*it);
                }
                m_activeConnections--;
            }
            catch (const std::exception &ex)
            {
                m_activeConnections--;
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - Exception replacing invalid connection: " << ex.what());
            }

            // Close the old invalid connection (conn still points to the original invalid connection)
            try
            {
                conn->getUnderlyingColumnarConnection()->close();
            }
            catch (const std::exception &ex)
            {
                CP_DEBUG("ColumnarDBConnectionPool::returnConnection - Exception closing invalid connection: " << ex.what());
            }
        }

        // Notify maintenance thread that a connection was returned
        m_maintenanceCondition.notify_one();
    }

    std::shared_ptr<ColumnarPooledDBConnection> ColumnarDBConnectionPool::getIdleDBConnection()
    {
        // Lock both mutexes in consistent order to prevent deadlocks
        // Always lock m_mutexAllConnections first, then m_mutexIdleConnections
        std::scoped_lock lock(m_mutexAllConnections, m_mutexIdleConnections);

        if (!m_idleConnections.empty())
        {
            auto conn = m_idleConnections.front();
            m_idleConnections.pop();

            // Test connection before use if configured
            if (m_testOnBorrow && !validateConnection(conn->getUnderlyingColumnarConnection()))
            {
                // Close the invalid connection to avoid leaking handles
                try
                {
                    conn->getUnderlyingColumnarConnection()->close();
                }
                catch (const DBException &ex)
                {
                    CP_DEBUG("ColumnarDBConnectionPool::getIdleDBConnection - Exception closing invalid connection: " << ex.what());
                }

                // Remove from allConnections
                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    m_allConnections.erase(it);
                }

                // Create new connection if we're still running
                if (m_running.load())
                {
                    try
                    {
                        auto newConn = createPooledDBConnection();
                        // Register the replacement connection in m_allConnections
                        m_allConnections.push_back(newConn);
                        return newConn;
                    }
                    catch ([[maybe_unused]] const DBException &ex)
                    {
                        // Return nullptr if we can't create a new connection
                        return nullptr;
                    }
                }
                return nullptr;
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

    std::shared_ptr<DBConnection> ColumnarDBConnectionPool::getDBConnection()
    {
        return getColumnarDBConnection();
    }

    std::shared_ptr<ColumnarDBConnection> ColumnarDBConnectionPool::getColumnarDBConnection()
    {
        std::unique_lock lock(m_mutexGetConnection);

        if (!m_running.load())
        {
            throw DBException("6R7S8T9U0V1W", "Connection pool is closed", system_utils::captureCallStack());
        }

        std::shared_ptr<ColumnarPooledDBConnection> result = this->getIdleDBConnection();

        // If no connection available, check if we can create a new one
        if (result == nullptr)
        {
            bool canCreateNew = false;
            {
                std::scoped_lock lock_all(m_mutexAllConnections);
                canCreateNew = m_allConnections.size() < m_maxSize;
            }

            if (canCreateNew)
            {
                auto candidate = createPooledDBConnection();

                {
                    std::scoped_lock lock_all(m_mutexAllConnections);
                    // Recheck size under lock to prevent exceeding m_maxSize under concurrent creation
                    if (m_allConnections.size() < m_maxSize)
                    {
                        m_allConnections.push_back(candidate);
                        result = candidate;
                    }
                }

                // If we couldn't register the connection (pool became full), close it
                if (result == nullptr && candidate && candidate->getUnderlyingColumnarConnection())
                {
                    candidate->getUnderlyingColumnarConnection()->close();
                }
            }
        }
        // If no connection available, wait until one becomes available
        if (result == nullptr)
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

    void ColumnarDBConnectionPool::maintenanceTask()
    {
        do
        {
            // Wait for 30 seconds or until notified (e.g., when close() is called)
            std::unique_lock lock(m_mutexMaintenance);
            m_maintenanceCondition.wait_for(lock, std::chrono::seconds(30), [this]
                                            { return !m_running; });

            if (!m_running)
            {
                break;
            }

            auto now = std::chrono::steady_clock::now();

            // Lock both mutexes in consistent order to prevent deadlocks
            // Always lock m_mutexAllConnections first, then m_mutexIdleConnections
            std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);

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

    size_t ColumnarDBConnectionPool::getActiveDBConnectionCount() const
    {
        return m_activeConnections;
    }

    size_t ColumnarDBConnectionPool::getIdleDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexIdleConnections);
        return m_idleConnections.size();
    }

    size_t ColumnarDBConnectionPool::getTotalDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexAllConnections);
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
        // Use scoped_lock for consistent lock ordering to prevent deadlock
        std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);

        for (const auto &conn : m_allConnections)
        {
            try
            {
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
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("ColumnarDBConnectionPool::close - Exception during connection close: " << ex.what());
            }
        }

        // Clear collections
        while (!m_idleConnections.empty())
        {
            m_idleConnections.pop();
        }
        m_allConnections.clear();
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
                    // Return to pool
                    returnToPool();
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("ColumnarPooledDBConnection::~ColumnarPooledDBConnection - Exception: " << ex.what());
            }
        }
    }

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
            catch (const std::exception &ex)
            {
                CP_DEBUG("ColumnarPooledDBConnection::close - Exception during return to pool: " << ex.what());
                if (m_conn)
                {
                    m_conn->close();
                }
            }
        }
    }

    bool ColumnarPooledDBConnection::isClosed()
    {
        return m_closed || m_conn->isClosed();
    }

    std::shared_ptr<ColumnarDBPreparedStatement> ColumnarPooledDBConnection::prepareStatement(const std::string &query)
    {
        if (m_closed)
        {
            throw DBException("A61A8B1BF33C", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
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

    std::shared_ptr<ColumnarDBResultSet> ColumnarPooledDBConnection::executeQuery(const std::string &query)
    {
        if (m_closed)
        {
            throw DBException("0AF43G67D005", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
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

    uint64_t ColumnarPooledDBConnection::executeUpdate(const std::string &query)
    {
        if (m_closed)
        {
            throw DBException("4E6D7284F5E2", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
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

    bool ColumnarPooledDBConnection::beginTransaction()
    {
        if (m_closed)
        {
            throw DBException("C8D9E0F1G2H3", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
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

    void ColumnarPooledDBConnection::commit()
    {
        if (m_closed)
        {
            throw DBException("F43ECCD8427F", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
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

    void ColumnarPooledDBConnection::rollback()
    {
        if (m_closed)
        {
            throw DBException("1A5723BF02C7", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
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

    std::string ColumnarPooledDBConnection::getURL() const
    {
        if (m_closed)
        {
            throw DBException("B5C8D0E3F6G2", "Connection is closed", system_utils::captureCallStack());
        }
        // Can't modify m_lastUsedTime in a const method
        return m_conn->getURL();
    }

    std::chrono::time_point<std::chrono::steady_clock> ColumnarPooledDBConnection::getCreationTime() const
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> ColumnarPooledDBConnection::getLastUsedTime() const
    {
        return m_lastUsedTime;
    }

    void ColumnarPooledDBConnection::setActive(bool isActive)
    {
        m_active = isActive;
    }

    bool ColumnarPooledDBConnection::isActive() const
    {
        return m_active;
    }

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
        catch (const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("ColumnarPooledDBConnection::returnToPool - shared_from_this failed: " << ex.what());
        }
        catch (const std::exception &ex)
        {
            CP_DEBUG("ColumnarPooledDBConnection::returnToPool - Exception: " << ex.what());
        }
    }

    bool ColumnarPooledDBConnection::isPooled()
    {
        return this->m_active == false;
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
        ScyllaConnectionPool::ScyllaConnectionPool(const std::string &url,
                                                   const std::string &username,
                                                   const std::string &password)
            : ColumnarDBConnectionPool(url, username, password)
        {
            // Scylla-specific initialization if needed
        }

        ScyllaConnectionPool::ScyllaConnectionPool(const config::DBConnectionPoolConfig &config)
            : ColumnarDBConnectionPool(config)
        {
            // Scylla-specific initialization if needed
        }

        std::shared_ptr<ScyllaConnectionPool> ScyllaConnectionPool::create(const std::string &url,
                                                                           const std::string &username,
                                                                           const std::string &password)
        {
            auto pool = std::shared_ptr<ScyllaConnectionPool>(new ScyllaConnectionPool(url, username, password)); // NOSONAR - make_shared cannot be used with private constructor
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<ScyllaConnectionPool> ScyllaConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<ScyllaConnectionPool>(new ScyllaConnectionPool(config)); // NOSONAR - make_shared cannot be used with private constructor
            pool->initializePool();
            return pool;
        }
    }

} // namespace cpp_dbc
