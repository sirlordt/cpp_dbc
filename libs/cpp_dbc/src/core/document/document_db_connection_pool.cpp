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
#include <algorithm>
#include <iostream>
#include <ranges>

// Debug output is controlled by -DDEBUG_CONNECTION_POOL=1 CMake option
#if (defined(DEBUG_CONNECTION_POOL) && DEBUG_CONNECTION_POOL) || (defined(DEBUG_ALL) && DEBUG_ALL)
#define CP_DEBUG(x) std::cout << x << std::endl
#else
#define CP_DEBUG(x)
#endif

namespace cpp_dbc
{

    // DocumentDBConnectionPool implementation
    DocumentDBConnectionPool::DocumentDBConnectionPool(const std::string &url,
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

    DocumentDBConnectionPool::DocumentDBConnectionPool(const config::DBConnectionPoolConfig &config)
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
                    std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);
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
            throw DBException("DOC54157A1F4D8", "Failed to initialize connection pool: " + std::string(ex.what()), system_utils::captureCallStack());
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
        auto pool = std::shared_ptr<DocumentDBConnectionPool>(new DocumentDBConnectionPool(
            url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, validationQuery, transactionIsolation));

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    std::shared_ptr<DocumentDBConnectionPool> DocumentDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto pool = std::shared_ptr<DocumentDBConnectionPool>(new DocumentDBConnectionPool(config));

        // Initialize the pool after construction (creates connections and starts maintenance thread)
        pool->initializePool();

        return pool;
    }

    DocumentDBConnectionPool::~DocumentDBConnectionPool()
    {
        CP_DEBUG("DocumentDBConnectionPool::~DocumentDBConnectionPool - Starting destructor at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Use qualified call to avoid virtual dispatch in destructor
        DocumentDBConnectionPool::close();

        CP_DEBUG("DocumentDBConnectionPool::~DocumentDBConnectionPool - Destructor completed at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
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
            CP_DEBUG("DocumentDBConnectionPool::createPooledDBConnection - Pool not managed by shared_ptr: " << ex.what());
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
            CP_DEBUG("DocumentDBConnectionPool::validateConnection - Exception: " << ex.what());
            return false;
        }
    }

    void DocumentDBConnectionPool::returnConnection(std::shared_ptr<DocumentPooledDBConnection> conn)
    {
        std::scoped_lock lock(m_mutexReturnConnection);

        // Null check to prevent crash
        if (!conn)
        {
            return;
        }

        if (!m_running.load())
        {
            // If pool is shutting down, close the connection to free resources
            try
            {
                conn->getUnderlyingDocumentConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - Exception during close (pool shutting down): " << ex.what());
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
            valid = validateConnection(conn->getUnderlyingDocumentConnection());
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
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - Exception replacing invalid connection: " << ex.what());
            }

            // Close the old invalid connection (conn still points to the original invalid connection)
            try
            {
                conn->getUnderlyingDocumentConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("DocumentDBConnectionPool::returnConnection - Exception closing invalid connection: " << ex.what());
            }
        }

        // Notify maintenance thread that a connection was returned
        m_maintenanceCondition.notify_one();
    }

    std::shared_ptr<DocumentPooledDBConnection> DocumentDBConnectionPool::getIdleDBConnection()
    {
        // Lock both mutexes in consistent order to prevent deadlock
        // Always lock m_mutexAllConnections first, then m_mutexIdleConnections
        std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);

        if (!m_idleConnections.empty())
        {
            auto conn = m_idleConnections.front();
            m_idleConnections.pop();

            // Test connection before use if configured
            if (m_testOnBorrow && !validateConnection(conn->getUnderlyingDocumentConnection()))
            {
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
                    catch ([[maybe_unused]] const std::exception &ex)
                    {
                        CP_DEBUG("DocumentDBConnectionPool::getIdleDBConnection - Exception creating replacement connection: " << ex.what());
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

    std::shared_ptr<DBConnection> DocumentDBConnectionPool::getDBConnection()
    {
        return getDocumentDBConnection();
    }

    std::shared_ptr<DocumentDBConnection> DocumentDBConnectionPool::getDocumentDBConnection()
    {
        std::unique_lock lock(m_mutexGetConnection);

        if (!m_running.load())
        {
            throw DBException("DOC5Q6R7S8T9U0V", "Connection pool is closed", system_utils::captureCallStack());
        }

        std::shared_ptr<DocumentPooledDBConnection> result = this->getIdleDBConnection();

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
            if (result == nullptr && candidate && candidate->getUnderlyingDocumentConnection())
            {
                candidate->getUnderlyingDocumentConnection()->close();
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
                    throw DBException("DOC88D90026A76A", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                }

                if (!m_running.load())
                {
                    throw DBException("DOC58566A84D1A1", "Connection pool is closed", system_utils::captureCallStack());
                }

                result = this->getIdleDBConnection();

            } while (result == nullptr);
        }

        // Mark as active
        result->setActive(true);
        m_activeConnections++;

        return result;
    }

    void DocumentDBConnectionPool::maintenanceTask()
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

    size_t DocumentDBConnectionPool::getActiveDBConnectionCount() const
    {
        return m_activeConnections;
    }

    size_t DocumentDBConnectionPool::getIdleDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexIdleConnections);
        return m_idleConnections.size();
    }

    size_t DocumentDBConnectionPool::getTotalDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexAllConnections);
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

        CP_DEBUG("DocumentDBConnectionPool::close - Waiting for active operations to complete at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            CP_DEBUG("DocumentDBConnectionPool::close - Initial active connections: " << m_activeConnections.load());

            while (m_activeConnections.load() > 0)
            {
                CP_DEBUG("DocumentDBConnectionPool::close - Waiting for " << m_activeConnections.load()
                                                                          << " active connections to finish at "
                                                                          << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 1 == 0)
                {
                    CP_DEBUG("DocumentDBConnectionPool::close - Waited " << elapsed_seconds
                                                                         << " seconds for active connections");
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("DocumentDBConnectionPool::close - Timeout waiting for active connections, forcing close at "
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
                CP_DEBUG("DocumentDBConnectionPool::close - Exception during connection close: " << ex.what());
            }
        }

        // Clear collections
        while (!m_idleConnections.empty())
        {
            m_idleConnections.pop();
        }
        m_allConnections.clear();
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
        // Mark as closed to prevent any further operations
        m_closed = true;

        // Simply close the underlying connection without returning to pool
        if (m_conn)
        {
            try
            {
                m_conn->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("DocumentPooledDBConnection::~DocumentPooledDBConnection - Exception during close: " << ex.what());
            }
        }
    }

    void DocumentPooledDBConnection::close()
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
                    poolShared->returnConnection(std::static_pointer_cast<DocumentPooledDBConnection>(this->shared_from_this()));
                    m_closed.store(false);
                }
            }
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("DocumentPooledDBConnection::close - shared_from_this failed: " << ex.what());
            m_closed = true;
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            CP_DEBUG("DocumentPooledDBConnection::close - Exception: " << ex.what());
            m_closed = true;
        }
    }

    bool DocumentPooledDBConnection::isClosed()
    {
        return m_closed || m_conn->isClosed();
    }

    // Implementation of DBConnectionPooled interface methods
    std::chrono::time_point<std::chrono::steady_clock> DocumentPooledDBConnection::getCreationTime() const
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> DocumentPooledDBConnection::getLastUsedTime() const
    {
        return m_lastUsedTime;
    }

    void DocumentPooledDBConnection::setActive(bool isActive)
    {
        m_active = isActive;
    }

    bool DocumentPooledDBConnection::isActive() const
    {
        return m_active;
    }

    void DocumentPooledDBConnection::returnToPool()
    {
        this->close();
    }

    bool DocumentPooledDBConnection::isPooled()
    {
        return this->m_active == false;
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
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listDatabases();
    }

    bool DocumentPooledDBConnection::databaseExists(const std::string &databaseName)
    {
        if (m_closed)
        {
            throw DBException("AEF208932857", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->databaseExists(databaseName);
    }

    void DocumentPooledDBConnection::useDatabase(const std::string &databaseName)
    {
        if (m_closed)
        {
            throw DBException("A188FEBF0840", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->useDatabase(databaseName);
    }

    void DocumentPooledDBConnection::dropDatabase(const std::string &databaseName)
    {
        if (m_closed)
        {
            throw DBException("33ABD0122921", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->dropDatabase(databaseName);
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::getCollection(const std::string &collectionName)
    {
        if (m_closed)
        {
            throw DBException("8781A8112718", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getCollection(collectionName);
    }

    std::vector<std::string> DocumentPooledDBConnection::listCollections()
    {
        if (m_closed)
        {
            throw DBException("4B1920591BBF", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listCollections();
    }

    bool DocumentPooledDBConnection::collectionExists(const std::string &collectionName)
    {
        if (m_closed)
        {
            throw DBException("8074225964F3", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->collectionExists(collectionName);
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::createCollection(
        const std::string &collectionName, const std::string &options)
    {
        if (m_closed)
        {
            throw DBException("E7BD2C9371A2", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->createCollection(collectionName, options);
    }

    void DocumentPooledDBConnection::dropCollection(const std::string &collectionName)
    {
        if (m_closed)
        {
            throw DBException("B948C3233D2E", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->dropCollection(collectionName);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument()
    {
        if (m_closed)
        {
            throw DBException("A551087D0C0F", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->createDocument();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument(const std::string &json)
    {
        if (m_closed)
        {
            throw DBException("57A932F74BC9", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->createDocument(json);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::runCommand(const std::string &command)
    {
        if (m_closed)
        {
            throw DBException("2D95D9EF03AA", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->runCommand(command);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerInfo()
    {
        if (m_closed)
        {
            throw DBException("8ED95C914BAD", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getServerInfo();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerStatus()
    {
        if (m_closed)
        {
            throw DBException("162B1BDB50A9", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getServerStatus();
    }

    bool DocumentPooledDBConnection::ping()
    {
        if (m_closed)
        {
            throw DBException("9BCD37627AAD", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->ping();
    }

    std::string DocumentPooledDBConnection::startSession()
    {
        if (m_closed)
        {
            throw DBException("12B829E08231", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->startSession();
    }

    void DocumentPooledDBConnection::endSession(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("7CB8D9E25BF3", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->endSession(sessionId);
    }

    void DocumentPooledDBConnection::startTransaction(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("A6BB94FF27F4", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->startTransaction(sessionId);
    }

    void DocumentPooledDBConnection::commitTransaction(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("1AF497A444D9", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->commitTransaction(sessionId);
    }

    void DocumentPooledDBConnection::abortTransaction(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("4A713C729CE4", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->abortTransaction(sessionId);
    }

    bool DocumentPooledDBConnection::supportsTransactions()
    {
        if (m_closed)
        {
            throw DBException("2FAE3A027B77", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
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
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listDatabases(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::getCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("8B9C0D1E2F3A", "Connection is closed"));
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getCollection(std::nothrow, collectionName);
    }

    expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listCollections(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("9C0D1E2F3A4B", "Connection is closed"));
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
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
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->createCollection(std::nothrow, collectionName, options);
    }

    expected<void, DBException> DocumentPooledDBConnection::dropCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("1E2F3A4B5C6D", "Connection is closed"));
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->dropCollection(std::nothrow, collectionName);
    }

    expected<void, DBException> DocumentPooledDBConnection::dropDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("2F3A4B5C6D7E", "Connection is closed"));
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->dropDatabase(std::nothrow, databaseName);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("3A4B5C6D7E8F", "Connection is closed"));
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->createDocument(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(
        std::nothrow_t, const std::string &json) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("4B5C6D7E8F9A", "Connection is closed"));
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->createDocument(std::nothrow, json);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::runCommand(
        std::nothrow_t, const std::string &command) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("5C6D7E8F9A0B", "Connection is closed"));
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->runCommand(std::nothrow, command);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("6D7E8F9A0B1C", "Connection is closed"));
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getServerInfo(std::nothrow);
    }

    expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerStatus(std::nothrow_t) noexcept
    {
        if (m_closed)
        {
            return unexpected<DBException>(DBException("7E8F9A0B1C2D", "Connection is closed"));
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getServerStatus(std::nothrow);
    }

    // MongoDB connection pool implementation
    namespace MongoDB
    {
        MongoDBConnectionPool::MongoDBConnectionPool(const std::string &url,
                                                     const std::string &username,
                                                     const std::string &password)
            : DocumentDBConnectionPool(url, username, password)
        {
            // MongoDB-specific initialization if needed
        }

        MongoDBConnectionPool::MongoDBConnectionPool(const config::DBConnectionPoolConfig &config)
            : DocumentDBConnectionPool(config)
        {
            // MongoDB-specific initialization if needed
        }

        std::shared_ptr<MongoDBConnectionPool> MongoDBConnectionPool::create(const std::string &url,
                                                                             const std::string &username,
                                                                             const std::string &password)
        {
            auto pool = std::shared_ptr<MongoDBConnectionPool>(new MongoDBConnectionPool(url, username, password));
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<MongoDBConnectionPool> MongoDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<MongoDBConnectionPool>(new MongoDBConnectionPool(config));
            pool->initializePool();
            return pool;
        }
    }

} // namespace cpp_dbc
