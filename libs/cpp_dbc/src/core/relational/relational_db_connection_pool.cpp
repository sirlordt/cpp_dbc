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
          m_transactionIsolation(transactionIsolation)
    {
        // Pool initialization will be done in the factory method
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
          m_transactionIsolation(config.getTransactionIsolation())
    {
        // Pool initialization will be done in the factory method
    }

    void RelationalDBConnectionPool::initializePool()
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
            m_maintenanceThread = std::jthread([this]
                                               { maintenanceTask(); });
        }
        catch (const std::exception &ex)
        {
            close();
            throw DBException("D54157A1F4C7", "Failed to initialize connection pool: " + std::string(ex.what()), system_utils::captureCallStack());
        }
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
        auto pool = std::shared_ptr<RelationalDBConnectionPool>(new RelationalDBConnectionPool( // NOSONAR - make_shared cannot be used with private constructor
            url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, validationQuery, transactionIsolation));

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    std::shared_ptr<RelationalDBConnectionPool> RelationalDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto pool = std::shared_ptr<RelationalDBConnectionPool>(new RelationalDBConnectionPool(config)); // NOSONAR - make_shared cannot be used with private constructor

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    RelationalDBConnectionPool::~RelationalDBConnectionPool()
    {
        CP_DEBUG("RelationalDBConnectionPool::~RelationalDBConnectionPool - Starting destructor at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        RelationalDBConnectionPool::close();

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
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            // Pool is not managed by shared_ptr, weakPool remains empty
            // This can happen if the pool is stack-allocated
            CP_DEBUG("RelationalDBConnectionPool::createPooledDBConnection - bad_weak_ptr: " << ex.what());
        }

        // Create the pooled connection
        auto pooledConn = std::make_shared<RelationalPooledDBConnection>(conn, weakPool, m_poolAlive);
        return pooledConn;
    }

    bool RelationalDBConnectionPool::validateConnection(std::shared_ptr<RelationalDBConnection> conn) const
    {
        try
        {
            // Use validation query to check connection
            auto resultSet = conn->executeQuery(m_validationQuery);
            return true;
        }
        catch ([[maybe_unused]] const DBException &ex)
        {
            CP_DEBUG("RelationalDBConnectionPool::validateConnection - DBException: " << ex.what());
            return false;
        }
    }

    void RelationalDBConnectionPool::returnConnection(std::shared_ptr<RelationalPooledDBConnection> conn)
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
                conn->getUnderlyingRelationalConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("RelationalDBConnectionPool::returnConnection - Exception during close: " << ex.what());
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
            valid = validateConnection(conn->getUnderlyingRelationalConnection());
        }

        if (valid)
        {
            // Reset transaction isolation level if needed
            try
            {
                if (!conn->m_closed)
                {
                    TransactionIsolationLevel connIsolation = conn->getTransactionIsolation();
                    if (connIsolation != m_transactionIsolation)
                    {
                        conn->setTransactionIsolation(m_transactionIsolation);
                    }
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("RelationalDBConnectionPool::returnConnection - Exception resetting transaction isolation: " << ex.what());
            }

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
                m_activeConnections--;
                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    *it = createPooledDBConnection();
                    m_idleConnections.push(*it);
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    m_allConnections.erase(it);
                }
                CP_DEBUG("RelationalDBConnectionPool::returnConnection - Exception replacing invalid connection: " << ex.what());
            }

            // Close the old invalid connection (conn still points to the original invalid connection)
            try
            {
                conn->getUnderlyingRelationalConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("RelationalDBConnectionPool::returnConnection - Exception closing invalid connection: " << ex.what());
            }
        }

        // Notify maintenance thread that a connection was returned
        m_maintenanceCondition.notify_one();
    }

    std::shared_ptr<RelationalPooledDBConnection> RelationalDBConnectionPool::getIdleDBConnection()
    {
        // Lock both mutexes in consistent order to prevent deadlock
        std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);

        if (!m_idleConnections.empty())
        {
            auto conn = m_idleConnections.front();
            m_idleConnections.pop();

            // Test connection before use if configured
            if (m_testOnBorrow && !validateConnection(conn->getUnderlyingRelationalConnection()))
            {
                // Close the invalid underlying connection to prevent resource leak
                try
                {
                    conn->getUnderlyingRelationalConnection()->close();
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    CP_DEBUG("RelationalDBConnectionPool::getIdleDBConnection - Exception closing invalid connection: " << ex.what());
                }

                // Remove from allConnections
                auto it = std::ranges::find(m_allConnections, conn);
                if (it != m_allConnections.end())
                {
                    m_allConnections.erase(it);
                }

                // Create new connection and register it if we're still running
                if (m_running.load())
                {
                    try
                    {
                        auto newConn = createPooledDBConnection();
                        m_allConnections.push_back(newConn);
                        return newConn;
                    }
                    catch ([[maybe_unused]] const std::exception &ex)
                    {
                        CP_DEBUG("RelationalDBConnectionPool::getIdleDBConnection - Exception creating new connection: " << ex.what());
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

    std::shared_ptr<DBConnection> RelationalDBConnectionPool::getDBConnection()
    {
        return getRelationalDBConnection();
    }

    std::shared_ptr<RelationalDBConnection> RelationalDBConnectionPool::getRelationalDBConnection()
    {
        std::unique_lock lock(m_mutexGetConnection);

        if (!m_running.load())
        {
            throw DBException("5Q6R7S8T9U0V", "Connection pool is closed", system_utils::captureCallStack());
        }

        std::shared_ptr<RelationalPooledDBConnection> result = this->getIdleDBConnection();

        // If no connection available, check if we can create a new one
        size_t currentSize;
        {
            std::scoped_lock lock_all(m_mutexAllConnections);
            currentSize = m_allConnections.size();
        }
        if (result == nullptr && currentSize < m_maxSize)
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
            if (result == nullptr && candidate && candidate->getUnderlyingRelationalConnection())
            {
                candidate->getUnderlyingRelationalConnection()->close();
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
            std::unique_lock lock(m_mutexMaintenance);
            m_maintenanceCondition.wait_for(lock, std::chrono::seconds(30), [this]
                                            { return !m_running; });

            if (!m_running)
            {
                break;
            }

            auto now = std::chrono::steady_clock::now();

            // Ensure no body touch the allConnections and idleConnections variables when is used by this thread
            // Use scoped_lock for consistent lock ordering to prevent deadlock
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
        std::scoped_lock lock(m_mutexIdleConnections);
        return m_idleConnections.size();
    }

    size_t RelationalDBConnectionPool::getTotalDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexAllConnections);
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
                CP_DEBUG("RelationalDBConnectionPool::close - Exception during connection close: " << ex.what());
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
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive)
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
        if (!m_closed && m_conn)
        {
            try
            {
                // If the pool is no longer alive, close the physical connection
                // Use qualified call to avoid virtual dispatch in destructor
                if (!RelationalPooledDBConnection::isPoolValid())
                {
                    m_conn->close();
                }
                else
                {
                    // Return to pool - use qualified call to avoid virtual dispatch in destructor
                    RelationalPooledDBConnection::returnToPool();
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("RelationalPooledDBConnection::~RelationalPooledDBConnection - Exception: " << ex.what());
            }
        }
    }

    void RelationalPooledDBConnection::close()
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
                        poolShared->returnConnection(std::static_pointer_cast<RelationalPooledDBConnection>(shared_from_this()));
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

    cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> RelationalPooledDBConnection::prepareStatement(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("95097A0AE22B", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
            return m_conn->prepareStatement(std::nothrow, sql);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("95097A0AE22C",
                                                   std::string("prepareStatement failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("95097A0AE22D",
                                                   "prepareStatement failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

    cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> RelationalPooledDBConnection::executeQuery(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("F9E32F56CFF4", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
            return m_conn->executeQuery(std::nothrow, sql);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F9E32F56CFF5",
                                                   std::string("executeQuery failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F9E32F56CFF6",
                                                   "executeQuery failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

    cpp_dbc::expected<uint64_t, DBException> RelationalPooledDBConnection::executeUpdate(std::nothrow_t, const std::string &sql) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("3D5C6173E4D1", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
            return m_conn->executeUpdate(std::nothrow, sql);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("3D5C6173E4D2",
                                                   std::string("executeUpdate failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("3D5C6173E4D3",
                                                   "executeUpdate failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::setAutoCommit(std::nothrow_t, bool autoCommit) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("5742170C69B4", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
            return m_conn->setAutoCommit(std::nothrow, autoCommit);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("5742170C69B5",
                                                   std::string("setAutoCommit failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("5742170C69B6",
                                                   "setAutoCommit failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::getAutoCommit(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("E3DEAB8A5E6D", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
            return m_conn->getAutoCommit(std::nothrow);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("E3DEAB8A5E6E",
                                                   std::string("getAutoCommit failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E3DEAB8A5E6F",
                                                   "getAutoCommit failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::commit(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("E32DBBC7316E", "Connection is closed", system_utils::captureCallStack()));
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
            return cpp_dbc::unexpected(DBException("E32DBBC7316F",
                                                   std::string("commit failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("E32DBBC73170",
                                                   "commit failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::rollback(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("094612AE91B6", "Connection is closed", system_utils::captureCallStack()));
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
            return cpp_dbc::unexpected(DBException("094612AE91B7",
                                                   std::string("rollback failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("094612AE91B8",
                                                   "rollback failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("B7C8D9E0F1G2", "Connection is closed", system_utils::captureCallStack()));
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
            return cpp_dbc::unexpected(DBException("B7C8D9E0F1G3",
                                                   std::string("beginTransaction failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("B7C8D9E0F1G4",
                                                   "beginTransaction failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

    cpp_dbc::expected<bool, DBException> RelationalPooledDBConnection::transactionActive(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("H3I4J5K6L7M8", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
            return m_conn->transactionActive(std::nothrow);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("H3I4J5K6L7M9",
                                                   std::string("transactionActive failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("H3I4J5K6L7MA",
                                                   "transactionActive failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

    cpp_dbc::expected<void, DBException> RelationalPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("F7A2B9C3D1E5", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
            return m_conn->setTransactionIsolation(std::nothrow, level);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("F7A2B9C3D1E6",
                                                   std::string("setTransactionIsolation failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("F7A2B9C3D1E7",
                                                   "setTransactionIsolation failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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

    cpp_dbc::expected<TransactionIsolationLevel, DBException> RelationalPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        try
        {
            if (m_closed)
            {
                return cpp_dbc::unexpected(DBException("A4B5C6D7E8F9", "Connection is closed", system_utils::captureCallStack()));
            }
            m_lastUsedTime = std::chrono::steady_clock::now();
            return m_conn->getTransactionIsolation(std::nothrow);
        }
        catch (const DBException &ex)
        {
            return cpp_dbc::unexpected(ex);
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("A4B5C6D7E8FA",
                                                   std::string("getTransactionIsolation failed: ") + ex.what(),
                                                   system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("A4B5C6D7E8FB",
                                                   "getTransactionIsolation failed: unknown error",
                                                   system_utils::captureCallStack()));
        }
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
                    poolShared->returnConnection(std::static_pointer_cast<RelationalPooledDBConnection>(this->shared_from_this()));
                    m_closed.store(false);
                }
            }
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            // shared_from_this failed, keep as closed
            CP_DEBUG("RelationalPooledDBConnection::returnToPool - bad_weak_ptr: " << ex.what());
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            // Any other exception, keep as closed
            CP_DEBUG("RelationalPooledDBConnection::returnToPool - Exception: " << ex.what());
        }
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
            auto pool = std::shared_ptr<MySQLConnectionPool>(new MySQLConnectionPool(url, username, password)); // NOSONAR - make_shared cannot be used with private constructor
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<MySQLConnectionPool> MySQLConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<MySQLConnectionPool>(new MySQLConnectionPool(config)); // NOSONAR - make_shared cannot be used with private constructor
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
            auto pool = std::shared_ptr<PostgreSQLConnectionPool>(new PostgreSQLConnectionPool(url, username, password)); // NOSONAR - make_shared cannot be used with private constructor
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<PostgreSQLConnectionPool> PostgreSQLConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<PostgreSQLConnectionPool>(new PostgreSQLConnectionPool(config)); // NOSONAR - make_shared cannot be used with private constructor
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
            SQLiteConnectionPool::setPoolTransactionIsolation(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
        }

        SQLiteConnectionPool::SQLiteConnectionPool(const config::DBConnectionPoolConfig &config)
            : RelationalDBConnectionPool(config)
        {
            // SQLite only supports SERIALIZABLE isolation level
            // Override the isolation level from the config
            SQLiteConnectionPool::setPoolTransactionIsolation(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
        }

        std::shared_ptr<SQLiteConnectionPool> SQLiteConnectionPool::create(const std::string &url,
                                                                           const std::string &username,
                                                                           const std::string &password)
        {
            auto pool = std::shared_ptr<SQLiteConnectionPool>(new SQLiteConnectionPool(url, username, password)); // NOSONAR - make_shared cannot be used with private constructor
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<SQLiteConnectionPool> SQLiteConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<SQLiteConnectionPool>(new SQLiteConnectionPool(config)); // NOSONAR - make_shared cannot be used with private constructor
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
            auto pool = std::shared_ptr<FirebirdConnectionPool>(new FirebirdConnectionPool(url, username, password)); // NOSONAR - make_shared cannot be used with private constructor
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<FirebirdConnectionPool> FirebirdConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<FirebirdConnectionPool>(new FirebirdConnectionPool(config)); // NOSONAR - make_shared cannot be used with private constructor
            pool->initializePool();
            return pool;
        }
    }

} // namespace cpp_dbc
