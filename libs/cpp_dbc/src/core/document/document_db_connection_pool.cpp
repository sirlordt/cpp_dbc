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
        m_maintenanceThread = std::thread(&DocumentDBConnectionPool::maintenanceTask, this);
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

        m_maintenanceThread = std::thread(&DocumentDBConnectionPool::maintenanceTask, this);
    }

    std::shared_ptr<DocumentDBConnectionPool> DocumentDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        return std::make_shared<DocumentDBConnectionPool>(config);
    }

    DocumentDBConnectionPool::~DocumentDBConnectionPool()
    {
        CP_DEBUG("DocumentDBConnectionPool::~DocumentDBConnectionPool - Starting destructor at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        close();

        CP_DEBUG("DocumentDBConnectionPool::~DocumentDBConnectionPool - Destructor completed at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    std::shared_ptr<DocumentDBConnection> DocumentDBConnectionPool::createDBConnection()
    {
        auto dbConn = DriverManager::getDBConnection(m_url, m_username, m_password, m_options);
        auto documentConn = std::dynamic_pointer_cast<DocumentDBConnection>(dbConn);
        if (!documentConn)
        {
            throw DBException("DOC_POOL_CONN_TYPE", "Connection pool only supports document database connections", system_utils::captureCallStack());
        }
        return documentConn;
    }

    std::shared_ptr<DocumentPooledDBConnection> DocumentDBConnectionPool::createPooledDBConnection()
    {
        auto conn = createDBConnection();

        // Create pooled connection with weak_ptr (may be empty if pool is stack-allocated),
        // shared poolAlive flag, and raw pointer for pool access
        std::weak_ptr<DocumentDBConnectionPool> weakPool;
        // Note: weak_from_this() would throw if not managed by shared_ptr, so we don't use it

        auto pooledConn = std::make_shared<DocumentPooledDBConnection>(conn, weakPool, m_poolAlive, this);
        return pooledConn;
    }

    bool DocumentDBConnectionPool::validateConnection(std::shared_ptr<DocumentDBConnection> conn)
    {
        try
        {
            // Use ping to validate document database connection
            return conn->ping();
        }
        catch (const DBException &e)
        {
            return false;
        }
    }

    void DocumentDBConnectionPool::returnConnection(std::shared_ptr<DocumentPooledDBConnection> conn)
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
            if (!validateConnection(conn->getUnderlyingDocumentConnection()))
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

        // Mark as inactive and update last used time
        conn->setActive(false);

        // Add to idle connections queue and tracking set
        {
            std::lock_guard<std::mutex> lock_idle(m_mutexIdleConnections);
            m_idleConnections.push(conn);
            m_activeConnections--;
        }
    }

    std::shared_ptr<DocumentPooledDBConnection> DocumentDBConnectionPool::getIdleDBConnection()
    {
        if (!m_idleConnections.empty())
        {
            auto conn = m_idleConnections.front();
            m_idleConnections.pop();

            // Test connection before use if configured
            if (m_testOnBorrow)
            {
                if (!validateConnection(conn->getUnderlyingDocumentConnection()))
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
            // Signal that we need to create a new connection
            return nullptr;
        }

        return nullptr;
    };

    std::shared_ptr<DBConnection> DocumentDBConnectionPool::getDBConnection()
    {
        return getDocumentDBConnection();
    }

    std::shared_ptr<DocumentDBConnection> DocumentDBConnectionPool::getDocumentDBConnection()
    {
        std::unique_lock<std::mutex> lock(m_mutexGetConnection);

        if (!m_running.load())
        {
            throw DBException("DOC5Q6R7S8T9U0V", "Connection pool is closed", system_utils::captureCallStack());
        }

        std::shared_ptr<DocumentPooledDBConnection> result = this->getIdleDBConnection();

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
        std::lock_guard<std::mutex> lock(m_mutexIdleConnections);
        return m_idleConnections.size();
    }

    size_t DocumentDBConnectionPool::getTotalDBConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutexAllConnections);
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

    bool DocumentDBConnectionPool::isRunning() const
    {
        return m_running.load();
    }

    // DocumentPooledDBConnection implementation
    DocumentPooledDBConnection::DocumentPooledDBConnection(
        std::shared_ptr<DocumentDBConnection> connection,
        std::weak_ptr<DocumentDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive,
        DocumentDBConnectionPool *poolPtr)
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive), m_poolPtr(poolPtr), m_active(false), m_closed(false)
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
            catch (...)
            {
                // Ignore exceptions during destruction
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
            if (isPoolValid() && m_poolPtr)
            {
                m_poolPtr->returnConnection(std::static_pointer_cast<DocumentPooledDBConnection>(this->shared_from_this()));
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
            throw DBException("DOC_A4B7C9D2E5F1", "Connection is closed", system_utils::captureCallStack());
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
            throw DBException("DOC_DB_NAME_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        return m_conn->getDatabaseName();
    }

    std::vector<std::string> DocumentPooledDBConnection::listDatabases()
    {
        if (m_closed)
        {
            throw DBException("DOC_LIST_DBS_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listDatabases();
    }

    bool DocumentPooledDBConnection::databaseExists(const std::string &databaseName)
    {
        if (m_closed)
        {
            throw DBException("DOC_DB_EXISTS_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->databaseExists(databaseName);
    }

    void DocumentPooledDBConnection::useDatabase(const std::string &databaseName)
    {
        if (m_closed)
        {
            throw DBException("DOC_USE_DB_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->useDatabase(databaseName);
    }

    void DocumentPooledDBConnection::dropDatabase(const std::string &databaseName)
    {
        if (m_closed)
        {
            throw DBException("DOC_DROP_DB_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->dropDatabase(databaseName);
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::getCollection(const std::string &collectionName)
    {
        if (m_closed)
        {
            throw DBException("DOC_GET_COLL_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getCollection(collectionName);
    }

    std::vector<std::string> DocumentPooledDBConnection::listCollections()
    {
        if (m_closed)
        {
            throw DBException("DOC_LIST_COLL_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listCollections();
    }

    bool DocumentPooledDBConnection::collectionExists(const std::string &collectionName)
    {
        if (m_closed)
        {
            throw DBException("DOC_COLL_EXISTS_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->collectionExists(collectionName);
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::createCollection(
        const std::string &collectionName, const std::string &options)
    {
        if (m_closed)
        {
            throw DBException("DOC_CREATE_COLL_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->createCollection(collectionName, options);
    }

    void DocumentPooledDBConnection::dropCollection(const std::string &collectionName)
    {
        if (m_closed)
        {
            throw DBException("DOC_DROP_COLL_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->dropCollection(collectionName);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument()
    {
        if (m_closed)
        {
            throw DBException("DOC_CREATE_DOC_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->createDocument();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument(const std::string &json)
    {
        if (m_closed)
        {
            throw DBException("DOC_CREATE_DOC_JSON_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->createDocument(json);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::runCommand(const std::string &command)
    {
        if (m_closed)
        {
            throw DBException("DOC_RUN_CMD_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->runCommand(command);
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerInfo()
    {
        if (m_closed)
        {
            throw DBException("DOC_SERVER_INFO_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getServerInfo();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerStatus()
    {
        if (m_closed)
        {
            throw DBException("DOC_SERVER_STATUS_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getServerStatus();
    }

    bool DocumentPooledDBConnection::ping()
    {
        if (m_closed)
        {
            throw DBException("DOC_PING_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->ping();
    }

    std::string DocumentPooledDBConnection::startSession()
    {
        if (m_closed)
        {
            throw DBException("DOC_START_SESSION_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->startSession();
    }

    void DocumentPooledDBConnection::endSession(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("DOC_END_SESSION_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->endSession(sessionId);
    }

    void DocumentPooledDBConnection::startTransaction(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("DOC_START_TX_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->startTransaction(sessionId);
    }

    void DocumentPooledDBConnection::commitTransaction(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("DOC_COMMIT_TX_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->commitTransaction(sessionId);
    }

    void DocumentPooledDBConnection::abortTransaction(const std::string &sessionId)
    {
        if (m_closed)
        {
            throw DBException("DOC_ABORT_TX_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->abortTransaction(sessionId);
    }

    bool DocumentPooledDBConnection::supportsTransactions()
    {
        if (m_closed)
        {
            throw DBException("DOC_SUPPORT_TX_ERR", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->supportsTransactions();
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
    }

} // namespace cpp_dbc