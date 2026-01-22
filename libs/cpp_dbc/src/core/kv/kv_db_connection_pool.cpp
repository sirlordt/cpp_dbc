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
 * @file kv_db_connection_pool.cpp
 * @brief Implementation of connection pool for key-value databases
 */

#include "cpp_dbc/core/kv/kv_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "cpp_dbc/core/kv/kv_db_driver.hpp"
#include "cpp_dbc/drivers/kv/driver_redis.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <iostream>
#include <random>
#include <ranges>

#ifdef CPP_DBC_DEBUG
#define CP_DEBUG(x) std::cout << x << std::endl
#else
#define CP_DEBUG(x)
#endif

namespace cpp_dbc
{
    // KVDBConnectionPool implementation

    KVDBConnectionPool::KVDBConnectionPool(const std::string &url,
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

    KVDBConnectionPool::KVDBConnectionPool(const config::DBConnectionPoolConfig &config)
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
          m_validationQuery(config.getValidationQuery().empty() ? "PING" : config.getValidationQuery()),
          m_transactionIsolation(config.getTransactionIsolation())
    {
        // Pool initialization will be done in the factory method
    }

    KVDBConnectionPool::~KVDBConnectionPool()
    {
        // Directly inline close() logic to avoid virtual call in destructor (S1699)
        if (!m_running.exchange(false))
        {
            // Already closed
            return;
        }

        // Mark pool as no longer alive
        if (m_poolAlive)
        {
            m_poolAlive->store(false);
        }

        // Wait for all active operations to complete (with timeout)
        {
            auto waitStart = std::chrono::steady_clock::now();

            while (m_activeConnections.load() > 0)
            {
                auto elapsed = std::chrono::steady_clock::now() - waitStart;

                if (elapsed > std::chrono::seconds(10))
                {
                    m_activeConnections.store(0);
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        // Notify all waiting threads
        m_maintenanceCondition.notify_all();

        // std::jthread auto-joins, but we request stop for clean shutdown
        if (m_maintenanceThread.joinable())
        {
            m_maintenanceThread.request_stop();
        }

        // Close all connections
        std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);

        for (const auto &conn : m_allConnections)
        {
            try
            {
                if (conn && conn->getUnderlyingConnection())
                {
                    conn->setActive(false);
                    conn->getUnderlyingConnection()->close();
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVDBConnectionPool::~KVDBConnectionPool - Exception closing connection: " << ex.what());
            }
        }

        // Clear collections
        while (!m_idleConnections.empty())
        {
            m_idleConnections.pop();
        }
        m_allConnections.clear();
    }

    std::shared_ptr<KVDBConnectionPool> KVDBConnectionPool::create(const std::string &url,
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
        auto pool = std::shared_ptr<KVDBConnectionPool>(  // NOSONAR - Cannot use make_shared with protected constructor (factory pattern)
            new KVDBConnectionPool(url, username, password, options, initialSize, maxSize, minIdle,
                                   maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis,
                                   maxLifetimeMillis, testOnBorrow, testOnReturn, validationQuery,
                                   transactionIsolation));

        pool->initializePool();
        return pool;
    }

    std::shared_ptr<KVDBConnectionPool> KVDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto pool = std::shared_ptr<KVDBConnectionPool>(new KVDBConnectionPool(config));  // NOSONAR - Cannot use make_shared with protected constructor (factory pattern)
        pool->initializePool();
        return pool;
    }

    void KVDBConnectionPool::initializePool()
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

            // Start maintenance thread (using std::jthread)
            m_maintenanceThread = std::jthread([this] { maintenanceTask(); });
        }
        catch (const std::exception &ex)
        {
            close();
            throw DBException("48FAB065D52D", "Failed to initialize connection pool: " + std::string(ex.what()), system_utils::captureCallStack());
        }
    }

    std::shared_ptr<KVDBConnection> KVDBConnectionPool::createDBConnection()
    {
        // Use the DriverManager to get a connection
        auto dbConn = DriverManager::getDBConnection(m_url, m_username, m_password, m_options);

        // Cast to KVDBConnection
        auto kvConn = std::dynamic_pointer_cast<KVDBConnection>(dbConn);
        if (!kvConn)
        {
            throw DBException("96CAD6FDCDD9", "Connection pool only supports key-value database connections", system_utils::captureCallStack());
        }

        return kvConn;
    }

    std::shared_ptr<KVPooledDBConnection> KVDBConnectionPool::createPooledDBConnection()
    {
        auto conn = createDBConnection();
        return std::make_shared<KVPooledDBConnection>(conn, shared_from_this(), m_poolAlive);
    }

    bool KVDBConnectionPool::validateConnection(std::shared_ptr<KVDBConnection> conn) const
    {
        if (!conn)
        {
            return false;
        }

        if (conn->isClosed())
        {
            return false;
        }

        try
        {
            // Use ping for validation
            std::string response = conn->ping();
            return !response.empty();
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            CP_DEBUG("KVDBConnectionPool::validateConnection - Exception: " << ex.what());
            return false;
        }
    }

    std::shared_ptr<KVPooledDBConnection> KVDBConnectionPool::getIdleDBConnection()
    {
        // Lock both mutexes in consistent order to prevent deadlock
        std::scoped_lock lockBoth(m_mutexAllConnections, m_mutexIdleConnections);

        if (!m_idleConnections.empty())
        {
            auto conn = m_idleConnections.front();
            m_idleConnections.pop();

            // Test connection before use if configured (S1066: merged nested if)
            if (m_testOnBorrow && !validateConnection(conn->getUnderlyingKVConnection()))
            {
                // Close the invalid underlying connection to prevent resource leak
                try
                {
                    conn->getUnderlyingKVConnection()->close();
                }
                catch ([[maybe_unused]] const std::exception &ex)
                {
                    CP_DEBUG("KVDBConnectionPool::getIdleDBConnection - Exception closing invalid connection: " << ex.what());
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
                        CP_DEBUG("KVDBConnectionPool::getIdleDBConnection - Exception creating new connection: " << ex.what());
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

    void KVDBConnectionPool::returnConnection(std::shared_ptr<KVPooledDBConnection> conn)
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
                conn->getUnderlyingKVConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVDBConnectionPool::returnConnection - Exception closing connection: " << ex.what());
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
            valid = validateConnection(conn->getUnderlyingKVConnection());
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
                CP_DEBUG("KVDBConnectionPool::returnConnection - Exception replacing invalid connection: " << ex.what());
            }

            // Close the old invalid connection
            try
            {
                conn->getUnderlyingKVConnection()->close();
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVDBConnectionPool::returnConnection - Exception closing invalid connection: " << ex.what());
            }
        }

        // Notify maintenance thread that a connection was returned
        m_maintenanceCondition.notify_one();
    }

    void KVDBConnectionPool::maintenanceTask()
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

            // Ensure no body touch the allConnections and idleConnections variables when used by this thread
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
                    std::queue<std::shared_ptr<KVPooledDBConnection>> tempQueue;
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

    std::shared_ptr<DBConnection> KVDBConnectionPool::getDBConnection()
    {
        return getKVDBConnection();
    }

    std::shared_ptr<KVDBConnection> KVDBConnectionPool::getKVDBConnection()
    {
        std::unique_lock lock(m_mutexGetConnection);

        if (!m_running.load())
        {
            throw DBException("BAB0896A213B", "Connection pool is closed", system_utils::captureCallStack());
        }

        std::shared_ptr<KVPooledDBConnection> result = this->getIdleDBConnection();

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
            if (result == nullptr && candidate && candidate->getUnderlyingKVConnection())
            {
                candidate->getUnderlyingKVConnection()->close();
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
                    throw DBException("8B0E04F03A6D", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                }

                if (!m_running.load())
                {
                    throw DBException("9C4A03BF221C", "Connection pool is closed", system_utils::captureCallStack());
                }

                result = this->getIdleDBConnection();

            } while (result == nullptr);
        }

        // Mark as active
        result->setActive(true);
        m_activeConnections++;

        return result;
    }

    size_t KVDBConnectionPool::getActiveDBConnectionCount() const
    {
        return m_activeConnections;
    }

    size_t KVDBConnectionPool::getIdleDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexIdleConnections);
        return m_idleConnections.size();
    }

    size_t KVDBConnectionPool::getTotalDBConnectionCount() const
    {
        std::scoped_lock lock(m_mutexAllConnections);
        return m_allConnections.size();
    }

    void KVDBConnectionPool::close()
    {
        if (!m_running.exchange(false))
        {
            // Already closed
            return;
        }

        // Mark pool as no longer alive - this prevents pooled connections from trying to return
        if (m_poolAlive)
        {
            m_poolAlive->store(false);
        }

        // Wait for all active operations to complete (with timeout)
        {
            auto waitStart = std::chrono::steady_clock::now();

            while (m_activeConnections.load() > 0)
            {
                auto elapsed = std::chrono::steady_clock::now() - waitStart;

                if (elapsed > std::chrono::seconds(10))
                {
                    // Force active connections to be marked as inactive
                    m_activeConnections.store(0);
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVDBConnectionPool::close - Exception closing connection: " << ex.what());
            }
        }

        // Clear collections
        while (!m_idleConnections.empty())
        {
            m_idleConnections.pop();
        }
        m_allConnections.clear();
    }

    bool KVDBConnectionPool::isRunning() const
    {
        return m_running;
    }

    // KVPooledDBConnection implementation

    KVPooledDBConnection::KVPooledDBConnection(std::shared_ptr<KVDBConnection> conn,
                                               std::weak_ptr<KVDBConnectionPool> pool,
                                               std::shared_ptr<std::atomic<bool>> poolAlive)
        : m_conn(conn),
          m_pool(pool),
          m_poolAlive(poolAlive)
    {
        // m_creationTime, m_lastUsedTime, m_active, m_closed use in-class initializers
    }

    KVPooledDBConnection::~KVPooledDBConnection()
    {
        if (!m_closed && m_conn)
        {
            try
            {
                // Check pool validity without virtual call (S1699)
                bool poolValid = m_poolAlive && *m_poolAlive && !m_pool.expired();

                // If the pool is no longer alive, close the physical connection
                if (!poolValid)
                {
                    m_conn->close();
                }
                else
                {
                    // Return to pool - inline the logic to avoid virtual call (S1699)
                    bool expected = false;
                    if (m_closed.compare_exchange_strong(expected, true))
                    {
                        m_lastUsedTime = std::chrono::steady_clock::now();

                        if (auto poolShared = m_pool.lock())
                        {
                            poolShared->returnConnection(std::static_pointer_cast<KVPooledDBConnection>(shared_from_this()));
                            m_closed.store(false);
                        }
                    }
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                CP_DEBUG("KVPooledDBConnection::~KVPooledDBConnection - Exception: " << ex.what());
            }
        }
    }

    bool KVPooledDBConnection::isPoolValid() const
    {
        return m_poolAlive && *m_poolAlive && !m_pool.expired();
    }

    void KVPooledDBConnection::close()
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
                        poolShared->returnConnection(std::static_pointer_cast<KVPooledDBConnection>(shared_from_this()));
                        m_closed.store(false); // Connection has been returned to pool, mark as not closed
                    }
                }
                else if (m_conn)
                {
                    // If pool is invalid, actually close the connection
                    m_conn->close();
                }
            }
            catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
            {
                // shared_from_this failed, just close the connection
                CP_DEBUG("KVPooledDBConnection::close - shared_from_this failed: " << ex.what());
                if (m_conn)
                {
                    m_conn->close();
                }
            }
            catch ([[maybe_unused]] const std::exception &ex)
            {
                // Any other exception, just close the connection
                CP_DEBUG("KVPooledDBConnection::close - Exception: " << ex.what());
                if (m_conn)
                {
                    m_conn->close();
                }
            }
            catch (...)
            {
                // Unknown exception, just close the connection
                CP_DEBUG("KVPooledDBConnection::close - Unknown exception");
                if (m_conn)
                {
                    m_conn->close();
                }
            }
        }
    }

    bool KVPooledDBConnection::isClosed()
    {
        return m_closed || (m_conn && m_conn->isClosed());
    }

    void KVPooledDBConnection::returnToPool()
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
                    poolShared->returnConnection(std::static_pointer_cast<KVPooledDBConnection>(this->shared_from_this()));
                    m_closed.store(false);
                }
            }
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("KVPooledDBConnection::returnToPool - shared_from_this failed: " << ex.what());
        }
        catch ([[maybe_unused]] const std::exception &ex)
        {
            CP_DEBUG("KVPooledDBConnection::returnToPool - Exception: " << ex.what());
        }
    }

    bool KVPooledDBConnection::isPooled()
    {
        return true;
    }

    std::string KVPooledDBConnection::getURL() const
    {
        if (m_conn)
        {
            return m_conn->getURL();
        }
        return "";
    }

    // Forward all KVDBConnection methods to underlying connection

    bool KVPooledDBConnection::setString(const std::string &key, const std::string &value,
                                         std::optional<int64_t> expirySeconds)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setString(key, value, expirySeconds);
    }

    std::string KVPooledDBConnection::getString(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getString(key);
    }

    bool KVPooledDBConnection::exists(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->exists(key);
    }

    bool KVPooledDBConnection::deleteKey(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->deleteKey(key);
    }

    int64_t KVPooledDBConnection::deleteKeys(const std::vector<std::string> &keys)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->deleteKeys(keys);
    }

    bool KVPooledDBConnection::expire(const std::string &key, int64_t seconds)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->expire(key, seconds);
    }

    int64_t KVPooledDBConnection::getTTL(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getTTL(key);
    }

    int64_t KVPooledDBConnection::increment(const std::string &key, int64_t by)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->increment(key, by);
    }

    int64_t KVPooledDBConnection::decrement(const std::string &key, int64_t by)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->decrement(key, by);
    }

    int64_t KVPooledDBConnection::listPushLeft(const std::string &key, const std::string &value)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listPushLeft(key, value);
    }

    int64_t KVPooledDBConnection::listPushRight(const std::string &key, const std::string &value)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listPushRight(key, value);
    }

    std::string KVPooledDBConnection::listPopLeft(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listPopLeft(key);
    }

    std::string KVPooledDBConnection::listPopRight(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listPopRight(key);
    }

    std::vector<std::string> KVPooledDBConnection::listRange(const std::string &key, int64_t start, int64_t stop)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listRange(key, start, stop);
    }

    int64_t KVPooledDBConnection::listLength(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listLength(key);
    }

    bool KVPooledDBConnection::hashSet(const std::string &key, const std::string &field, const std::string &value)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashSet(key, field, value);
    }

    std::string KVPooledDBConnection::hashGet(const std::string &key, const std::string &field)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashGet(key, field);
    }

    bool KVPooledDBConnection::hashDelete(const std::string &key, const std::string &field)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashDelete(key, field);
    }

    bool KVPooledDBConnection::hashExists(const std::string &key, const std::string &field)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashExists(key, field);
    }

    std::map<std::string, std::string> KVPooledDBConnection::hashGetAll(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashGetAll(key);
    }

    int64_t KVPooledDBConnection::hashLength(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashLength(key);
    }

    bool KVPooledDBConnection::setAdd(const std::string &key, const std::string &member)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setAdd(key, member);
    }

    bool KVPooledDBConnection::setRemove(const std::string &key, const std::string &member)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setRemove(key, member);
    }

    bool KVPooledDBConnection::setIsMember(const std::string &key, const std::string &member)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setIsMember(key, member);
    }

    std::vector<std::string> KVPooledDBConnection::setMembers(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setMembers(key);
    }

    int64_t KVPooledDBConnection::setSize(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setSize(key);
    }

    bool KVPooledDBConnection::sortedSetAdd(const std::string &key, double score, const std::string &member)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->sortedSetAdd(key, score, member);
    }

    bool KVPooledDBConnection::sortedSetRemove(const std::string &key, const std::string &member)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->sortedSetRemove(key, member);
    }

    std::optional<double> KVPooledDBConnection::sortedSetScore(const std::string &key, const std::string &member)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->sortedSetScore(key, member);
    }

    std::vector<std::string> KVPooledDBConnection::sortedSetRange(const std::string &key, int64_t start, int64_t stop)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->sortedSetRange(key, start, stop);
    }

    int64_t KVPooledDBConnection::sortedSetSize(const std::string &key)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->sortedSetSize(key);
    }

    std::vector<std::string> KVPooledDBConnection::scanKeys(const std::string &pattern, int64_t count)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->scanKeys(pattern, count);
    }

    std::string KVPooledDBConnection::executeCommand(const std::string &command, const std::vector<std::string> &args)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->executeCommand(command, args);
    }

    bool KVPooledDBConnection::flushDB(bool async)
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->flushDB(async);
    }

    std::string KVPooledDBConnection::ping()
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->ping();
    }

    std::map<std::string, std::string> KVPooledDBConnection::getServerInfo()
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getServerInfo();
    }

    std::chrono::time_point<std::chrono::steady_clock> KVPooledDBConnection::getCreationTime() const
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> KVPooledDBConnection::getLastUsedTime() const
    {
        return m_lastUsedTime;
    }

    void KVPooledDBConnection::setActive(bool active)
    {
        m_active = active;
    }

    bool KVPooledDBConnection::isActive() const
    {
        return m_active;
    }

    std::shared_ptr<DBConnection> KVPooledDBConnection::getUnderlyingConnection()
    {
        return m_conn;
    }

    std::shared_ptr<KVDBConnection> KVPooledDBConnection::getUnderlyingKVConnection()
    {
        return m_conn;
    }

    // ============================================================================
    // KVPooledDBConnection - NOTHROW IMPLEMENTATIONS (delegate to underlying connection)
    // ============================================================================

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setString(
        std::nothrow_t,
        const std::string &key,
        const std::string &value,
        std::optional<int64_t> expirySeconds) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setString(std::nothrow, key, value, expirySeconds);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::getString(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getString(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::exists(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->exists(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::deleteKey(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->deleteKey(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::deleteKeys(
        std::nothrow_t, const std::vector<std::string> &keys) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->deleteKeys(std::nothrow, keys);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::expire(
        std::nothrow_t, const std::string &key, int64_t seconds) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->expire(std::nothrow, key, seconds);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::getTTL(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getTTL(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::increment(
        std::nothrow_t, const std::string &key, int64_t by) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->increment(std::nothrow, key, by);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::decrement(
        std::nothrow_t, const std::string &key, int64_t by) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->decrement(std::nothrow, key, by);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listPushLeft(
        std::nothrow_t, const std::string &key, const std::string &value) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listPushLeft(std::nothrow, key, value);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listPushRight(
        std::nothrow_t, const std::string &key, const std::string &value) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listPushRight(std::nothrow, key, value);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::listPopLeft(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listPopLeft(std::nothrow, key);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::listPopRight(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listPopRight(std::nothrow, key);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::listRange(
        std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listRange(std::nothrow, key, start, stop);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::listLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->listLength(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashSet(
        std::nothrow_t,
        const std::string &key,
        const std::string &field,
        const std::string &value) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashSet(std::nothrow, key, field, value);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::hashGet(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashGet(std::nothrow, key, field);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashDelete(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashDelete(std::nothrow, key, field);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::hashExists(
        std::nothrow_t, const std::string &key, const std::string &field) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashExists(std::nothrow, key, field);
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> KVPooledDBConnection::hashGetAll(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashGetAll(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::hashLength(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->hashLength(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setAdd(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setAdd(std::nothrow, key, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setRemove(std::nothrow, key, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::setIsMember(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setIsMember(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::setMembers(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setMembers(std::nothrow, key);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::setSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->setSize(std::nothrow, key);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::sortedSetAdd(
        std::nothrow_t, const std::string &key, double score, const std::string &member) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->sortedSetAdd(std::nothrow, key, score, member);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::sortedSetRemove(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->sortedSetRemove(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::optional<double>, DBException> KVPooledDBConnection::sortedSetScore(
        std::nothrow_t, const std::string &key, const std::string &member) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->sortedSetScore(std::nothrow, key, member);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::sortedSetRange(
        std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->sortedSetRange(std::nothrow, key, start, stop);
    }

    cpp_dbc::expected<int64_t, DBException> KVPooledDBConnection::sortedSetSize(
        std::nothrow_t, const std::string &key) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->sortedSetSize(std::nothrow, key);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> KVPooledDBConnection::scanKeys(
        std::nothrow_t, const std::string &pattern, int64_t count) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->scanKeys(std::nothrow, pattern, count);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::executeCommand(
        std::nothrow_t,
        const std::string &command,
        const std::vector<std::string> &args) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->executeCommand(std::nothrow, command, args);
    }

    cpp_dbc::expected<bool, DBException> KVPooledDBConnection::flushDB(
        std::nothrow_t, bool async) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->flushDB(std::nothrow, async);
    }

    cpp_dbc::expected<std::string, DBException> KVPooledDBConnection::ping(std::nothrow_t) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->ping(std::nothrow);
    }

    cpp_dbc::expected<std::map<std::string, std::string>, DBException> KVPooledDBConnection::getServerInfo(
        std::nothrow_t) noexcept
    {
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getServerInfo(std::nothrow);
    }

    // RedisConnectionPool implementation

    namespace Redis
    {
        RedisConnectionPool::RedisConnectionPool(const std::string &url,
                                                 const std::string &username,
                                                 const std::string &password)
            : KVDBConnectionPool(url, username, password, {}, 5, 20, 3, 5000, 5000, 300000, 1800000, true, false, "PING")
        {
        }

        RedisConnectionPool::RedisConnectionPool(const config::DBConnectionPoolConfig &config)
            : KVDBConnectionPool(config.getUrl(),
                                 config.getUsername(),
                                 config.getPassword(),
                                 config.getOptions(),
                                 config.getInitialSize(),
                                 config.getMaxSize(),
                                 config.getMinIdle(),
                                 config.getConnectionTimeout(),
                                 config.getValidationInterval(),
                                 config.getIdleTimeout(),
                                 config.getMaxLifetimeMillis(),
                                 config.getTestOnBorrow(),
                                 config.getTestOnReturn(),
                                 config.getValidationQuery().empty() || config.getValidationQuery() == "SELECT 1" ? "PING" : config.getValidationQuery(),
                                 config.getTransactionIsolation())
        {
            // Redis-specific initialization if needed
        }

        std::shared_ptr<RedisConnectionPool> RedisConnectionPool::create(const std::string &url,
                                                                         const std::string &username,
                                                                         const std::string &password)
        {
            auto pool = std::shared_ptr<RedisConnectionPool>(new RedisConnectionPool(url, username, password));  // NOSONAR - Cannot use make_shared with protected constructor (factory pattern)
            pool->initializePool();
            return pool;
        }

        std::shared_ptr<RedisConnectionPool> RedisConnectionPool::create(const config::DBConnectionPoolConfig &config)
        {
            auto pool = std::shared_ptr<RedisConnectionPool>(new RedisConnectionPool(config));  // NOSONAR - Cannot use make_shared with protected constructor (factory pattern)
            pool->initializePool();
            return pool;
        }
    }

} // namespace cpp_dbc
