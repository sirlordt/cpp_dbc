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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file connection_pool.cpp
 @brief Connection pool implementation for relational databases

*/

#include "cpp_dbc/core/relational/relational_db_connection_pool.hpp"
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

    // RelationalDBConnectionPool implementation
    RelationalDBConnectionPool::RelationalDBConnectionPool(const std::string &url,
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
        m_allConnections.reserve(m_maxSize);
        // Note: Initial connections are created in the factory method after construction
        // to ensure shared_from_this() works correctly
    }

    RelationalDBConnectionPool::RelationalDBConnectionPool(const config::DBConnectionPoolConfig &config)
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
        m_allConnections.reserve(m_maxSize);
        // Note: Initial connections are created in the factory method after construction
        // to ensure shared_from_this() works correctly
    }

    void RelationalDBConnectionPool::initializePool()
    {
        // Create initial connections
        for (int i = 0; i < m_initialSize; i++)
        {
            auto pooledConn = createPooledDBConnection();
            m_idleConnections.push(pooledConn);
            m_allConnections.push_back(pooledConn);
        }

        // Start maintenance thread
        m_maintenanceThread = std::thread(&RelationalDBConnectionPool::maintenanceTask, this);
    }

    std::shared_ptr<RelationalDBConnectionPool> RelationalDBConnectionPool::create(const std::string &url,
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
        auto pool = std::shared_ptr<RelationalDBConnectionPool>(new RelationalDBConnectionPool(
            url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, validationQuery, transactionIsolation));

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    std::shared_ptr<RelationalDBConnectionPool> RelationalDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto pool = std::shared_ptr<RelationalDBConnectionPool>(new RelationalDBConnectionPool(config));

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    RelationalDBConnectionPool::~RelationalDBConnectionPool()
    {
        CP_DEBUG("RelationalDBConnectionPool::~RelationalDBConnectionPool - Starting destructor at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        close();

        CP_DEBUG("RelationalDBConnectionPool::~RelationalDBConnectionPool - Destructor completed at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    std::shared_ptr<RelationalDBConnection> RelationalDBConnectionPool::createDBConnection()
    {
        auto dbConn = DriverManager::getDBConnection(m_url, m_username, m_password, m_options);
        auto relationalConn = std::dynamic_pointer_cast<RelationalDBConnection>(dbConn);
        if (!relationalConn)
        {
            throw DBException("POOL_CONN_TYPE", "Connection pool only supports relational database connections", system_utils::captureCallStack());
        }
        return relationalConn;
    }

    std::shared_ptr<RelationalPooledDBConnection> RelationalDBConnectionPool::createPooledDBConnection()
    {
        auto conn = createDBConnection();

        // Set transaction isolation level on the new connection
        conn->setTransactionIsolation(m_transactionIsolation);

        // Create pooled connection with weak_ptr
        std::weak_ptr<RelationalDBConnectionPool> weakPool;
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
        auto pooledConn = std::make_shared<RelationalPooledDBConnection>(conn, weakPool, m_poolAlive);
        return pooledConn;
    }

    bool RelationalDBConnectionPool::validateConnection(std::shared_ptr<RelationalDBConnection> conn)
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

    void RelationalDBConnectionPool::returnConnection(std::shared_ptr<RelationalPooledDBConnection> conn)
    {
        std::lock_guard<std::mutex> lock(m_mutexReturnConnection);

        if (!m_running.load())
        {
            // If pool is shutting down, just discard the connection
            return;
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive())
        {
            return;
        }

        // Additional safety check: verify connection is in allConnections
        auto it = std::find(m_allConnections.begin(), m_allConnections.end(), conn);
        if (it == m_allConnections.end())
        {
            return;
        }

        // Test connection before returning to pool if configured
        if (m_testOnReturn)
        {
            if (!validateConnection(conn->getUnderlyingRelationalConnection()))
            {
                // Remove from allConnections
                auto it_inner = std::find(m_allConnections.begin(), m_allConnections.end(), conn);
                if (it_inner != m_allConnections.end())
                {
                    m_allConnections.erase(it_inner);
                }

                // Replace with new connection if pool is running
                if (m_running && m_allConnections.size() < m_minIdle)
                {
                    auto newConn = createPooledDBConnection();
                    m_idleConnections.push(newConn);
                    m_allConnections.push_back(newConn);
                }

                return;
            }
        }

        try
        {
            if (!conn->m_closed)
            {
                // Check if the connection's transaction isolation level is different from the pool's
                // If so, reset it to match the pool's level
                TransactionIsolationLevel connIsolation = conn->getTransactionIsolation();
                if (connIsolation != m_transactionIsolation)
                {
                    conn->setTransactionIsolation(m_transactionIsolation);
                }
            }
        }
        catch (...)
        {
        }

        // Mark as inactive and update last used time
        conn->setActive(false);

        // Add to idle connections queue and tracking set
        {
            std::lock_guard<std::mutex> lock_idle(m_mutexIdleConnections);
            m_idleConnections.push(conn);
            m_activeConnections--;
        }
    }

    std::shared_ptr<RelationalPooledDBConnection> RelationalDBConnectionPool::getIdleDBConnection()
    {
        if (!m_idleConnections.empty())
        {
            auto conn = m_idleConnections.front();
            m_idleConnections.pop();

            // Test connection before use if configured
            if (m_testOnBorrow)
            {
                if (!validateConnection(conn->getUnderlyingRelationalConnection()))
                {
                    // Remove from allConnections
                    auto it = std::find(m_allConnections.begin(), m_allConnections.end(), conn);
                    if (it != m_allConnections.end())
                    {
                        m_allConnections.erase(it);
                    }

                    // Create new connection
                    return createPooledDBConnection();
                }
            }

            return conn;
        }
        else if (m_allConnections.size() < m_maxSize)
        {
            // We need to create a new connection, but we'll do it outside the lambda
            // to avoid holding the lock too long
            return nullptr; // Signal that we need to create a new connection
        }

        return nullptr;
    };

    std::shared_ptr<DBConnection> RelationalDBConnectionPool::getDBConnection()
    {
        return getRelationalDBConnection();
    }

    std::shared_ptr<RelationalDBConnection> RelationalDBConnectionPool::getRelationalDBConnection()
    {
        std::unique_lock<std::mutex> lock(m_mutexGetConnection);

        if (!m_running.load())
        {
            throw DBException("5Q6R7S8T9U0V", "Connection pool is closed", system_utils::captureCallStack());
        }

        std::shared_ptr<RelationalPooledDBConnection> result = this->getIdleDBConnection();

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
                    throw DBException("88D90026A76A", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                }

                if (!m_running.load())
                {
                    throw DBException("58566A84D1A1", "Connection pool is closed", system_utils::captureCallStack());
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

    void RelationalDBConnectionPool::maintenanceTask()
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

            // Ensure we have at least minIdle connections
            while (m_running && m_allConnections.size() < m_minIdle)
            {
                auto pooledConn = createPooledDBConnection();
                m_idleConnections.push(pooledConn);
                m_allConnections.push_back(pooledConn);
            }
        } while (m_running);
    }

    size_t RelationalDBConnectionPool::getActiveDBConnectionCount() const
    {
        return m_activeConnections;
    }

    size_t RelationalDBConnectionPool::getIdleDBConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutexIdleConnections);
        return m_idleConnections.size();
    }

    size_t RelationalDBConnectionPool::getTotalDBConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutexAllConnections);
        return m_allConnections.size();
    }

    void RelationalDBConnectionPool::close()
    {
        CP_DEBUG("RelationalDBConnectionPool::close - Starting close operation");

        if (!m_running.exchange(false))
        {
            CP_DEBUG("RelationalDBConnectionPool::close - Already closed, returning");
            return; // Already closed
        }

        // Mark pool as no longer alive - this prevents pooled connections from trying to return
        if (m_poolAlive)
        {
            m_poolAlive->store(false);
        }

        CP_DEBUG("RelationalDBConnectionPool::close - Waiting for active operations to complete at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            CP_DEBUG("RelationalDBConnectionPool::close - Initial active connections: " << m_activeConnections.load());

            while (m_activeConnections.load() > 0)
            {
                CP_DEBUG("RelationalDBConnectionPool::close - Waiting for " << m_activeConnections.load()
                                                                            << " active connections to finish at "
                                                                            << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 1 == 0)
                {
                    CP_DEBUG("RelationalDBConnectionPool::close - Waited " << elapsed_seconds
                                                                           << " seconds for active connections");
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("RelationalDBConnectionPool::close - Timeout waiting for active connections, forcing close at "
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

    bool RelationalDBConnectionPool::isRunning() const
    {
        return m_running.load();
    }

    // RelationalPooledDBConnection implementation
    RelationalPooledDBConnection::RelationalPooledDBConnection(
        std::shared_ptr<RelationalDBConnection> connection,
        std::weak_ptr<RelationalDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive)
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive), m_active(false), m_closed(false)
    {
        m_creationTime = std::chrono::steady_clock::now();
        m_lastUsedTime = m_creationTime;
    }

    bool RelationalPooledDBConnection::isPoolValid() const
    {
        return m_poolAlive && m_poolAlive->load();
    }

    RelationalPooledDBConnection::~RelationalPooledDBConnection()
    {
        // Mark as closed to prevent any further operations
        m_closed = true;

        // Simply close the underlying connection without returning to pool
        if (m_conn)
        {
            try
            {
                m_conn->close();
            }
            catch (...)
            {
                // Ignore exceptions during destruction
            }
        }
    }

    void RelationalPooledDBConnection::close()
    {
        // Use atomic exchange to ensure only one thread processes the close
        bool expected = false;
        if (!m_closed.compare_exchange_strong(expected, true))
        {
            return; // Already closed, nothing to do
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
                    poolShared->returnConnection(std::static_pointer_cast<RelationalPooledDBConnection>(this->shared_from_this()));
                    // Note: We keep m_closed = true. From the user's perspective, this connection
                    // is closed. The pool will reset m_closed when it hands out this connection again.
                }
            }
        }
        catch (const std::bad_weak_ptr &)
        {
            // shared_from_this failed, connection remains closed
        }
        catch (const std::exception &)
        {
            // Any other exception, connection remains closed
        }
        catch (...)
        {
            // Any other exception, connection remains closed
        }
    }

    bool RelationalPooledDBConnection::isClosed()
    {
        return m_closed || m_conn->isClosed();
    }

    std::shared_ptr<RelationalDBPreparedStatement> RelationalPooledDBConnection::prepareStatement(const std::string &sql)
    {
        if (m_closed)
        {
            throw DBException("95097A0AE22B", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->prepareStatement(sql);
    }

    std::shared_ptr<RelationalDBResultSet> RelationalPooledDBConnection::executeQuery(const std::string &sql)
    {
        if (m_closed)
        {
            throw DBException("F9E32F56CFF4", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->executeQuery(sql);
    }

    uint64_t RelationalPooledDBConnection::executeUpdate(const std::string &sql)
    {
        if (m_closed)
        {
            throw DBException("3D5C6173E4D1", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->executeUpdate(sql);
    }

    void RelationalPooledDBConnection::setAutoCommit(bool autoCommit)
    {
        if (m_closed)
        {
            throw DBException("5742170C69B4", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->setAutoCommit(autoCommit);
    }

    bool RelationalPooledDBConnection::getAutoCommit()
    {
        if (m_closed)
        {
            throw DBException("E3DEAB8A5E6D", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getAutoCommit();
    }

    void RelationalPooledDBConnection::commit()
    {
        if (m_closed)
        {
            throw DBException("E32DBBC7316E", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->commit();
    }

    void RelationalPooledDBConnection::rollback()
    {
        if (m_closed)
        {
            throw DBException("094612AE91B6", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->rollback();
    }

    bool RelationalPooledDBConnection::beginTransaction()
    {
        if (m_closed)
        {
            throw DBException("B7C8D9E0F1G2", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->beginTransaction();
    }

    bool RelationalPooledDBConnection::transactionActive()
    {
        if (m_closed)
        {
            throw DBException("H3I4J5K6L7M8", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->transactionActive();
    }

    void RelationalPooledDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        if (m_closed)
        {
            throw DBException("F7A2B9C3D1E5", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->setTransactionIsolation(level);
    }

    TransactionIsolationLevel RelationalPooledDBConnection::getTransactionIsolation()
    {
        if (m_closed)
        {
            throw DBException("A4B5C6D7E8F9", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getTransactionIsolation();
    }

    std::string RelationalPooledDBConnection::getURL() const
    {
        if (m_closed)
        {
            throw DBException("A4B7C9D2E5F1", "Connection is closed", system_utils::captureCallStack());
        }
        // Can't modify m_lastUsedTime in a const method
        return m_conn->getURL();
    }

    std::chrono::time_point<std::chrono::steady_clock> RelationalPooledDBConnection::getCreationTime() const
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> RelationalPooledDBConnection::getLastUsedTime() const
    {
        return m_lastUsedTime;
    }

    void RelationalPooledDBConnection::setActive(bool isActive)
    {
        m_active = isActive;
    }

    bool RelationalPooledDBConnection::isActive() const
    {
        return m_active;
    }

    void RelationalPooledDBConnection::returnToPool()
    {
        this->close();
    }

    bool RelationalPooledDBConnection::isPooled()
    {
        return this->m_active == false;
    }

    std::shared_ptr<DBConnection> RelationalPooledDBConnection::getUnderlyingConnection()
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

    std::shared_ptr<RelationalDBConnection> RelationalPooledDBConnection::getUnderlyingRelationalConnection()
    {
        return m_conn;
    }

    // MySQL connection pool implementation
    namespace MySQL
    {
        MySQLConnectionPool::MySQLConnectionPool(const std::string &url,
                                                 const std::string &username,
                                                 const std::string &password)
            : RelationalDBConnectionPool(url, username, password)
        {
            // MySQL-specific initialization if needed
        }

        MySQLConnectionPool::MySQLConnectionPool(const config::DBConnectionPoolConfig &config)
            : RelationalDBConnectionPool(config)
        {
            // MySQL-specific initialization if needed
        }

        std::shared_ptr<MySQLConnectionPool> MySQLConnectionPool::create(const std::string &url,
                                                                         const std::string &username,
                                                                         const std::string &password)
        {
            auto pool = std::shared_ptr<MySQLConnectionPool>(new MySQLConnectionPool(url, username, password));
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<MySQLConnectionPool> MySQLConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<MySQLConnectionPool>(new MySQLConnectionPool(config));
            pool->initializePool();
            return pool;
        }
    }

    // PostgreSQL connection pool implementation
    namespace PostgreSQL
    {
        PostgreSQLConnectionPool::PostgreSQLConnectionPool(const std::string &url,
                                                           const std::string &username,
                                                           const std::string &password)
            : RelationalDBConnectionPool(url, username, password)
        {
            // PostgreSQL-specific initialization if needed
        }

        PostgreSQLConnectionPool::PostgreSQLConnectionPool(const config::DBConnectionPoolConfig &config)
            : RelationalDBConnectionPool(config)
        {
            // PostgreSQL-specific initialization if needed
        }

        std::shared_ptr<PostgreSQLConnectionPool> PostgreSQLConnectionPool::create(const std::string &url,
                                                                                   const std::string &username,
                                                                                   const std::string &password)
        {
            auto pool = std::shared_ptr<PostgreSQLConnectionPool>(new PostgreSQLConnectionPool(url, username, password));
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<PostgreSQLConnectionPool> PostgreSQLConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<PostgreSQLConnectionPool>(new PostgreSQLConnectionPool(config));
            pool->initializePool();
            return pool;
        }
    }

    // SQLite connection pool implementation
    namespace SQLite
    {
        SQLiteConnectionPool::SQLiteConnectionPool(const std::string &url,
                                                   const std::string &username,
                                                   const std::string &password)
            : RelationalDBConnectionPool(url, username, password)
        {
            // SQLite only supports SERIALIZABLE isolation level
            // Override the isolation level from the constructor
            this->setPoolTransactionIsolation(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
        }

        SQLiteConnectionPool::SQLiteConnectionPool(const config::DBConnectionPoolConfig &config)
            : RelationalDBConnectionPool(config)
        {
            // SQLite only supports SERIALIZABLE isolation level
            // Override the isolation level from the config
            this->setPoolTransactionIsolation(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
        }

        std::shared_ptr<SQLiteConnectionPool> SQLiteConnectionPool::create(const std::string &url,
                                                                           const std::string &username,
                                                                           const std::string &password)
        {
            auto pool = std::shared_ptr<SQLiteConnectionPool>(new SQLiteConnectionPool(url, username, password));
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<SQLiteConnectionPool> SQLiteConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<SQLiteConnectionPool>(new SQLiteConnectionPool(config));
            pool->initializePool();
            return pool;
        }
    }

    // Firebird connection pool implementation
    namespace Firebird
    {
        FirebirdConnectionPool::FirebirdConnectionPool(const std::string &url,
                                                       const std::string &username,
                                                       const std::string &password)
            : RelationalDBConnectionPool(url, username, password)
        {
            // Firebird-specific initialization if needed
        }

        FirebirdConnectionPool::FirebirdConnectionPool(const config::DBConnectionPoolConfig &config)
            : RelationalDBConnectionPool(config)
        {
            // Firebird-specific initialization if needed
        }

        std::shared_ptr<FirebirdConnectionPool> FirebirdConnectionPool::create(const std::string &url,
                                                                               const std::string &username,
                                                                               const std::string &password)
        {
            auto pool = std::shared_ptr<FirebirdConnectionPool>(new FirebirdConnectionPool(url, username, password));
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<FirebirdConnectionPool> FirebirdConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<FirebirdConnectionPool>(new FirebirdConnectionPool(config));
            pool->initializePool();
            return pool;
        }
    }

} // namespace cpp_dbc
