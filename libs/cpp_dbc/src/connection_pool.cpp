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

#include "cpp_dbc/connection_pool.hpp"
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
        : m_url(url),
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

    RelationalDBConnectionPool::RelationalDBConnectionPool(const config::DBConnectionPoolConfig &config)
        : m_url(config.getUrl()),
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

        // Create initial connections
        for (int i = 0; i < m_initialSize; i++)
        {
            auto pooledConn = createPooledDBConnection();
            m_idleConnections.push(pooledConn);
            m_allConnections.push_back(pooledConn);
        }

        m_maintenanceThread = std::thread(&RelationalDBConnectionPool::maintenanceTask, this);
    }

    std::shared_ptr<RelationalDBConnectionPool> RelationalDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        return std::make_shared<RelationalDBConnectionPool>(config);
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

    std::shared_ptr<RelationalDBPooledConnection> RelationalDBConnectionPool::createPooledDBConnection()
    {
        auto conn = createDBConnection();

        // Set transaction isolation level on the new connection
        conn->setTransactionIsolation(m_transactionIsolation);

        auto pooledConn = std::make_shared<RelationalDBPooledConnection>(conn, this);
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

    void RelationalDBConnectionPool::returnConnection(std::shared_ptr<RelationalDBPooledConnection> conn)
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
            if (!validateConnection(conn->getUnderlyingConnection()))
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

    std::shared_ptr<RelationalDBPooledConnection> RelationalDBConnectionPool::getIdleDBConnection()
    {
        if (!m_idleConnections.empty())
        {
            auto conn = m_idleConnections.front();
            m_idleConnections.pop();

            // Test connection before use if configured
            if (m_testOnBorrow)
            {
                if (!validateConnection(conn->getUnderlyingConnection()))
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

    std::shared_ptr<RelationalDBConnection> RelationalDBConnectionPool::getDBConnection()
    {
        std::unique_lock<std::mutex> lock(m_mutexGetConnection);

        if (!m_running.load())
        {
            throw DBException("5Q6R7S8T9U0V", "Connection pool is closed", system_utils::captureCallStack());
        }

        std::shared_ptr<RelationalDBPooledConnection> result = this->getIdleDBConnection();

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

        // Mark as active
        result->setActive(true);
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
                    std::queue<std::shared_ptr<RelationalDBPooledConnection>> tempQueue;
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

    int RelationalDBConnectionPool::getActiveDBConnectionCount() const
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

    // RelationalDBPooledConnection implementation
    RelationalDBPooledConnection::RelationalDBPooledConnection(std::shared_ptr<RelationalDBConnection> connection, RelationalDBConnectionPool *connectionPool)
        : m_conn(connection), m_pool(connectionPool), m_active(false), m_closed(false)
    {
        m_creationTime = std::chrono::steady_clock::now();
        m_lastUsedTime = m_creationTime;
    }

    RelationalDBPooledConnection::~RelationalDBPooledConnection()
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

    void RelationalDBPooledConnection::close()
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

            if (m_pool)
            {
                m_pool->returnConnection(std::static_pointer_cast<RelationalDBPooledConnection>(shared_from_this()));
                m_closed.store(false);
            }
        }
        catch (const std::bad_weak_ptr &e)
        {
            // shared_from_this failed, mark as closed
            m_closed = true;
        }
        catch (const std::exception &e)
        {
            // Any other exception, mark as closed
            m_closed = true;
        }
        catch (...)
        {
            // Any other exception, mark as closed
            m_closed = true;
        }
    }

    bool RelationalDBPooledConnection::isClosed()
    {
        return m_closed || m_conn->isClosed();
    }

    std::shared_ptr<RelationalDBPreparedStatement> RelationalDBPooledConnection::prepareStatement(const std::string &sql)
    {
        if (m_closed)
        {
            throw DBException("95097A0AE22B", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->prepareStatement(sql);
    }

    std::shared_ptr<RelationalDBResultSet> RelationalDBPooledConnection::executeQuery(const std::string &sql)
    {
        if (m_closed)
        {
            throw DBException("F9E32F56CFF4", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->executeQuery(sql);
    }

    uint64_t RelationalDBPooledConnection::executeUpdate(const std::string &sql)
    {
        if (m_closed)
        {
            throw DBException("3D5C6173E4D1", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->executeUpdate(sql);
    }

    void RelationalDBPooledConnection::setAutoCommit(bool autoCommit)
    {
        if (m_closed)
        {
            throw DBException("5742170C69B4", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->setAutoCommit(autoCommit);
    }

    bool RelationalDBPooledConnection::getAutoCommit()
    {
        if (m_closed)
        {
            throw DBException("E3DEAB8A5E6D", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getAutoCommit();
    }

    void RelationalDBPooledConnection::commit()
    {
        if (m_closed)
        {
            throw DBException("E32DBBC7316E", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->commit();
    }

    void RelationalDBPooledConnection::rollback()
    {
        if (m_closed)
        {
            throw DBException("094612AE91B6", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->rollback();
    }

    bool RelationalDBPooledConnection::beginTransaction()
    {
        if (m_closed)
        {
            throw DBException("B7C8D9E0F1G2", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->beginTransaction();
    }

    bool RelationalDBPooledConnection::transactionActive()
    {
        if (m_closed)
        {
            throw DBException("H3I4J5K6L7M8", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->transactionActive();
    }

    void RelationalDBPooledConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        if (m_closed)
        {
            throw DBException("F7A2B9C3D1E5", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->setTransactionIsolation(level);
    }

    TransactionIsolationLevel RelationalDBPooledConnection::getTransactionIsolation()
    {
        if (m_closed)
        {
            throw DBException("A4B5C6D7E8F9", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getTransactionIsolation();
    }

    std::string RelationalDBPooledConnection::getURL() const
    {
        if (m_closed)
        {
            throw DBException("A4B7C9D2E5F1", "Connection is closed", system_utils::captureCallStack());
        }
        // Can't modify m_lastUsedTime in a const method
        return m_conn->getURL();
    }

    std::chrono::time_point<std::chrono::steady_clock> RelationalDBPooledConnection::getCreationTime() const
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> RelationalDBPooledConnection::getLastUsedTime() const
    {
        return m_lastUsedTime;
    }

    void RelationalDBPooledConnection::setActive(bool isActive)
    {
        m_active = isActive;
    }

    bool RelationalDBPooledConnection::isActive() const
    {
        return m_active;
    }

    void RelationalDBPooledConnection::returnToPool()
    {
        this->close();
    }

    bool RelationalDBPooledConnection::isPooled()
    {
        return this->m_active == false;
    }

    std::shared_ptr<RelationalDBConnection> RelationalDBPooledConnection::getUnderlyingConnection()
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
    }

} // namespace cpp_dbc
