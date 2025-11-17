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
 @brief Tests for database connections

*/

#include "cpp_dbc/connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include <algorithm>
#include <iostream>

// Debug output is controlled by -DDEBUG_CONNECTION_POOL=1 CMake option
#if defined(DEBUG_CONNECTION_POOL) && DEBUG_CONNECTION_POOL
#define CP_DEBUG(x) std::cout << x << std::endl
#else
#define CP_DEBUG(x)
#endif

namespace cpp_dbc
{

    // ConnectionPool implementation
    ConnectionPool::ConnectionPool(const std::string &url,
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
            auto pooledConn = createPooledConnection();
            m_idleConnections.push(pooledConn);
            m_allConnections.push_back(pooledConn);
        }

        // Start maintenance thread
        m_maintenanceThread = std::thread(&ConnectionPool::maintenanceTask, this);
    }

    ConnectionPool::ConnectionPool(const config::ConnectionPoolConfig &config)
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
        // cpp_dbc::system_utils::safePrint("A1B2C3D4", "ConnectionPool: Creating " + std::to_string(m_initialSize) + " initial connections...");
        for (int i = 0; i < m_initialSize; i++)
        {
            // cpp_dbc::system_utils::safePrint("E5F6A7B8", "ConnectionPool: Creating connection " + std::to_string(i + 1) + "/" + std::to_string(m_initialSize));
            auto pooledConn = createPooledConnection();
            // cpp_dbc::system_utils::safePrint("C9D0E1F2", "ConnectionPool: Connection " + std::to_string(i + 1) + " created successfully");
            m_idleConnections.push(pooledConn);
            m_allConnections.push_back(pooledConn);
        }
        // cpp_dbc::system_utils::safePrint("A3B4C5D6", "ConnectionPool: All initial connections created. Pool ready.");

        m_maintenanceThread = std::thread(&ConnectionPool::maintenanceTask, this);
    }

    std::shared_ptr<ConnectionPool> ConnectionPool::create(const config::ConnectionPoolConfig &config)
    {
        return std::make_shared<ConnectionPool>(config);
    }

    ConnectionPool::~ConnectionPool()
    {
        CP_DEBUG("ConnectionPool::~ConnectionPool - Starting destructor at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        close();

        CP_DEBUG("ConnectionPool::~ConnectionPool - Destructor completed at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
    }

    std::shared_ptr<Connection> ConnectionPool::createConnection()
    {
        return DriverManager::getConnection(m_url, m_username, m_password, m_options);
    }

    std::shared_ptr<PooledConnection> ConnectionPool::createPooledConnection()
    {
        // cpp_dbc::system_utils::safePrint("A5B6C7D8", "createPooledConnection: Starting...");
        auto conn = createConnection();
        // cpp_dbc::system_utils::safePrint("E9F0A1B2", "createPooledConnection: Physical connection created");

        // Set transaction isolation level on the new connection
        conn->setTransactionIsolation(m_transactionIsolation);

        auto pooledConn = std::make_shared<PooledConnection>(conn, this);
        // cpp_dbc::system_utils::safePrint("C3D4E5F6", "createPooledConnection: PooledConnection wrapper created");
        return pooledConn;
    }

    bool ConnectionPool::validateConnection(std::shared_ptr<Connection> conn)
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

    void ConnectionPool::returnConnection(std::shared_ptr<PooledConnection> conn)
    {
        std::lock_guard<std::mutex> lock(m_mutexReturnConnection);
        // cpp_dbc::system_utils::safePrint("A7B8C9D0", "ConnectionPool::returnConnection - Starting, Active: " + std::to_string(m_activeConnections) +
        //                                                  ", Idle: " + std::to_string(m_idleConnections.size()) +
        //                                                  ", Total: " + std::to_string(m_allConnections.size()) +
        //                                                  ", Max: " + std::to_string(m_maxSize));

        if (!m_running.load())
        {
            // cpp_dbc::system_utils::safePrint("E1F2A3B4", "ConnectionPool::returnConnection - Pool is closed, discarding connection");
            //  If pool is shutting down, just discard the connection
            return;
        }

        // Check if connection is already inactive (already returned to pool)
        if (!conn->isActive())
        {
            // cpp_dbc::system_utils::safePrint("C5D6E7F8", "ConnectionPool::returnConnection - Connection already inactive, ignoring duplicate return");
            return;
        }

        // Additional safety check: verify connection is in allConnections
        auto it = std::find(m_allConnections.begin(), m_allConnections.end(), conn);
        if (it == m_allConnections.end())
        {
            // cpp_dbc::system_utils::safePrint("A9B0C1D2", "ConnectionPool::returnConnection - Connection not found in pool, ignoring");
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
                    auto newConn = createPooledConnection();
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
        // catch (const cpp_dbc::DBException &e)
        {
            // std::cerr << "SQL Error: " << e.what_s() << std::endl;
        }

        // Mark as inactive and update last used time
        // cpp_dbc::system_utils::safePrint("E3F4A5B6", "ConnectionPool::returnConnection - Marking connection as inactive");
        conn->setActive(false);

        /*
        // Check if connection is already in idle set (prevent duplicates)
        if (m_idleConnectionsSet.find(conn) != m_idleConnectionsSet.end())
        {
            cpp_dbc::system_utils::safePrint("C7D8E9F0", "ConnectionPool::returnConnection - Connection already in idle set, ignoring duplicate");
            return;
        }
            */

        // Add to idle connections queue and tracking set
        // cpp_dbc::system_utils::safePrint("A1B2C3D4", "ConnectionPool::returnConnection - Adding to idle queue");
        {
            std::lock_guard<std::mutex> lock_idle(m_mutexIdleConnections);
            m_idleConnections.push(conn);
            m_activeConnections--;
        }
        // cpp_dbc::system_utils::safePrint("E5F6A7B8", "ConnectionPool::returnConnection - Connection returned to pool, now Active: " +
        //                                                  std::to_string(m_activeConnections) + ", Idle: " + std::to_string(m_idleConnections.size()));

        // Notify waiting threads
        // cpp_dbc::system_utils::safePrint("C9D0E1F2", "ConnectionPool::returnConnection - Notifying waiting threads");
        // m_condition.notify_one();
        //     std::cout << "ConnectionPool::returnConnection - Notified waiting threads" << std::endl;
    }

    std::shared_ptr<PooledConnection> ConnectionPool::getIdleConnection()
    {
        // cpp_dbc::system_utils::safePrint("C1D2E3F4", "getIdleConnection: Checking idle connections. Count: " + std::to_string(m_idleConnections.size()));
        if (!m_idleConnections.empty())
        {
            // cpp_dbc::system_utils::safePrint("A5B6C7D8", "getIdleConnection: Found idle connection, retrieving...");
            auto conn = m_idleConnections.front();
            m_idleConnections.pop();

            // Remove from tracking set
            // m_idleConnectionsSet.erase(conn);

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
                    return createPooledConnection();
                }
            }

            // cpp_dbc::system_utils::safePrint("E9F0A1B2", "getIdleConnection: Returning idle connection");
            return conn;
        }
        else if (m_allConnections.size() < m_maxSize)
        {
            // cpp_dbc::system_utils::safePrint("C3D4E5F6", "getIdleConnection: No idle connections, need to create new one. Current: " +
            //                                                  std::to_string(m_allConnections.size()) + ", Max: " + std::to_string(m_maxSize));
            //  We need to create a new connection, but we'll do it outside the lambda
            //  to avoid holding the lock too long
            return nullptr; // Signal that we need to create a new connection
        }

        // cpp_dbc::system_utils::safePrint("A7B8C9D0", "getIdleConnection: No connections available, returning nullptr");
        return nullptr;
    };

    std::shared_ptr<Connection> ConnectionPool::getConnection()
    {
        // cpp_dbc::system_utils::safePrint("A3B4C5D6", "ConnectionPool::getConnection - Starting...");
        std::unique_lock<std::mutex> lock(m_mutexGetConnection);

        // auto conn = createConnection();
        // // cpp_dbc::system_utils::safePrint("E9F0A1B2", "createPooledConnection: Physical connection created");
        // return std::make_shared<PooledConnection>(conn, this);

        // std::lock_guard<std::mutex> lock(mutex);
        // cpp_dbc::system_utils::safePrint("E7F8A9B0", "ConnectionPool::getConnection - Lock acquired. Active: " + std::to_string(m_activeConnections) +
        //                                                 ", Idle: " + std::to_string(m_idleConnections.size()) +
        //                                                 ", Total: " + std::to_string(m_allConnections.size()) +
        //                                                 ", Max: " + std::to_string(m_maxSize));

        if (!m_running.load())
        {
            // std::cout << "ConnectionPool::getConnection - Pool is closed" << std::endl;
            throw DBException("5Q6R7S8T9U0V", "Connection pool is closed", system_utils::captureCallStack());
        }

        /*
        // Try to get idle connection or create new one
        auto getIdleConnection = [this]() -> std::shared_ptr<PooledConnection>
        {
            // cpp_dbc::system_utils::safePrint("C1D2E3F4", "getIdleConnection: Checking idle connections. Count: " + std::to_string(m_idleConnections.size()));
            if (!m_idleConnections.empty())
            {
                // cpp_dbc::system_utils::safePrint("A5B6C7D8", "getIdleConnection: Found idle connection, retrieving...");
                auto conn = m_idleConnections.front();
                m_idleConnections.pop();

                // Remove from tracking set
                // m_idleConnectionsSet.erase(conn);

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
                        return createPooledConnection();
                    }
                }

                // cpp_dbc::system_utils::safePrint("E9F0A1B2", "getIdleConnection: Returning idle connection");
                return conn;
            }
            else if (m_allConnections.size() < m_maxSize)
            {
                // cpp_dbc::system_utils::safePrint("C3D4E5F6", "getIdleConnection: No idle connections, need to create new one. Current: " +
                //                                                  std::to_string(m_allConnections.size()) + ", Max: " + std::to_string(m_maxSize));
                //  We need to create a new connection, but we'll do it outside the lambda
                //  to avoid holding the lock too long
                return nullptr; // Signal that we need to create a new connection
            }

            // cpp_dbc::system_utils::safePrint("A7B8C9D0", "getIdleConnection: No connections available, returning nullptr");
            return nullptr;
        };
        */

        // cpp_dbc::system_utils::safePrint("E1F2A3B4", "ConnectionPool::getConnection - Calling getIdleConnection...");
        std::shared_ptr<PooledConnection> result = this->getIdleConnection();
        // cpp_dbc::system_utils::safePrint("C5D6E7F8", "ConnectionPool::getConnection - getIdleConnection returned: " +
        //                                                  std::string(result ? "connection" : "nullptr"));

        // If no connection available, check if we can create a new one
        if (result == nullptr && m_allConnections.size() < m_maxSize)
        {
            // cpp_dbc::system_utils::safePrint("A9B0C1D2", "ConnectionPool::getConnection - Creating new connection outside lock...");
            //  Release lock temporarily to create new connection
            //  lock.unlock();

            // try
            // {
            result = createPooledConnection();

            {
                std::lock_guard<std::mutex> lock_all(m_mutexAllConnections);
                m_allConnections.push_back(result);
            }
            // auto newConn = createPooledConnection();
            //  // cpp_dbc::system_utils::safePrint("E3F4A5B6", "ConnectionPool::getConnection - New connection created, re-acquiring lock...");

            // // Re-acquire lock and check size again (another thread might have added connections)
            // // lock.lock();
            // if (m_allConnections.size() < m_maxSize)
            // {
            //     m_allConnections.push_back(newConn);
            //     result = newConn;
            //     // cpp_dbc::system_utils::safePrint("C7D8E9F0", "ConnectionPool::getConnection - New connection added to pool");

            //     // cpp_dbc::system_utils::safePrint("F7059E39E45F", "ConnectionPool::getConnection - Lock acquired. Active: " + std::to_string(m_activeConnections) +
            //     //                                                      ", Idle: " + std::to_string(m_idleConnections.size()) +
            //     //                                                      ", Total: " + std::to_string(m_allConnections.size()) +
            //     //                                                      ", Max: " + std::to_string(m_maxSize));
            // }
            // else
            // {
            //     // cpp_dbc::system_utils::safePrint("A1B2C3E4", "ConnectionPool::getConnection - Pool full after re-acquiring lock, discarding new connection");
            //     //  Pool is full, discard the new connection and try to get an idle one
            //     result = getIdleConnection();
            // }
            // }
            // catch (...)
            // {
            //     // Re-acquire lock even if creation failed
            //     // lock.lock();
            //     // throw;
            // }
        }
        // If no connection available, wait until one becomes available
        else if (result == nullptr)
        {

            // cpp_dbc::system_utils::safePrint("F5A6B7C8", "ConnectionPool::getConnection - No connection available, entering wait loop...");
            auto waitStart = std::chrono::steady_clock::now();

            // Wait until a connection is returned to the pool or timeout
            do
            {
                auto now = std::chrono::steady_clock::now();

                // auto waitStatus = m_condition.wait_for(lock,
                //                                      std::chrono::milliseconds(m_maxWaitMillis)); //The condition create radom stop and seg fails
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                auto waitedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - waitStart).count();

                if (waitedMs >= m_maxWaitMillis)
                {
                    throw DBException("88D90026A76A", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                }

                // if (waitStatus == std::cv_status::timeout)
                // {
                //     // std::cout << "ConnectionPool::getConnection - TIMEOUT waiting for connection! Active: "
                //     //              << m_activeConnections << ", Idle: " << m_idleConnections.size()
                //     //              << ", Total: " << m_allConnections.size() << ", Max: " << m_maxSize << std::endl;

                //     throw DBException("DE04466AD05E", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                // }

                if (!m_running.load())
                {
                    throw DBException("58566A84D1A1", "Connection pool is closed", system_utils::captureCallStack());
                }

                // Check if we've waited too long
                // now = std::chrono::steady_clock::now();

                // result = createPooledConnection(); // this->getIdleConnection();
                result = this->getIdleConnection();

                // waitedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - waitStart).count();

                // if (result == nullptr && waitedMs >= m_maxWaitMillis)
                // {
                //     throw DBException("723F1992C1C7", "Timeout waiting for connection from the pool", system_utils::captureCallStack());
                // }
            } while (result == nullptr);
        }

        // // Add connection to the pool if it's new
        // if (std::find(m_allConnections.begin(), m_allConnections.end(), result) == m_allConnections.end())
        // {
        //     m_allConnections.push_back(result);
        // }

        // Mark as active
        result->setActive(true);
        m_activeConnections++;
        // std::cout << "ConnectionPool::getConnection - Connection obtained, now Active: "
        //           << m_activeConnections << ", Idle: " << m_idleConnections.size() << std::endl;

        return result;
    }

    void ConnectionPool::maintenanceTask()
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
                    std::queue<std::shared_ptr<PooledConnection>> tempQueue;
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
                auto pooledConn = createPooledConnection();
                m_idleConnections.push(pooledConn);
                m_allConnections.push_back(pooledConn);
            }
        } while (m_running);
    }

    int ConnectionPool::getActiveConnectionCount() const
    {
        return m_activeConnections;
    }

    size_t ConnectionPool::getIdleConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutexIdleConnections);
        return m_idleConnections.size();
    }

    size_t ConnectionPool::getTotalConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutexAllConnections);
        return m_allConnections.size();
    }

    void ConnectionPool::close()
    {
        // time_t start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        CP_DEBUG("ConnectionPool::close - Starting close operation at " << start_time);

        if (!m_running.exchange(false))
        {
            CP_DEBUG("ConnectionPool::close - Already closed, returning");
            return; // Already closed
        }

        CP_DEBUG("ConnectionPool::close - Waiting for active operations to complete at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            // int initialActiveConnections = m_activeConnections.load();
            CP_DEBUG("ConnectionPool::close - Initial active connections: " << initialActiveConnections);

            while (m_activeConnections.load() > 0)
            {
                CP_DEBUG("ConnectionPool::close - Waiting for " << m_activeConnections.load()
                                                                << " active connections to finish at "
                                                                << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                auto elapsed = std::chrono::steady_clock::now() - waitStart;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if (elapsed_seconds > 0 && elapsed_seconds % 1 == 0)
                {
                    CP_DEBUG("ConnectionPool::close - Waited " << elapsed_seconds
                                                               << " seconds for active connections");
                }

                if (elapsed > std::chrono::seconds(10))
                {
                    CP_DEBUG("ConnectionPool::close - Timeout waiting for active connections, forcing close at "
                             << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

                    // Force active connections to be marked as inactive
                    m_activeConnections.store(0);
                    break;
                }
            }
        }

        // cpp_dbc::system_utils::safePrint("F9A0B1C2", "ConnectionPool::close - Notifying waiting threads");
        //  Notify all waiting threads
        //  m_condition.notify_all();
        m_maintenanceCondition.notify_all();

        // Join maintenance thread (only if it was started)
        // cpp_dbc::system_utils::safePrint("D3E4F5A6", "ConnectionPool::close - Checking maintenance thread");
        if (m_maintenanceThread.joinable())
        {
            // cpp_dbc::system_utils::safePrint("B7C8D9E0", "ConnectionPool::close - Joining maintenance thread");
            m_maintenanceThread.join();
            // cpp_dbc::system_utils::safePrint("F1A2B3C4", "ConnectionPool::close - Maintenance thread joined");
        }

        // Close all connections
        // cpp_dbc::system_utils::safePrint("D5E6F7A8", "ConnectionPool::close - Acquiring lock to close connections");
        std::lock_guard<std::mutex> lockAllConnections(m_mutexAllConnections);
        std::lock_guard<std::mutex> lockIdleConnections(m_mutexIdleConnections);
        // cpp_dbc::system_utils::safePrint("B9C0D1E2", "ConnectionPool::close - Lock acquired, closing " + std::to_string(m_allConnections.size()) + " connections");

        for (size_t i = 0; i < m_allConnections.size(); ++i)
        {
            try
            {
                // cpp_dbc::system_utils::safePrint("F3A4B5C6", "ConnectionPool::close - Closing connection " + std::to_string(i + 1) + "/" + std::to_string(m_allConnections.size()));
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

                    // cpp_dbc::system_utils::safePrint("D7E8F9A0", "ConnectionPool::close - Connection " + std::to_string(i + 1) + " closed successfully");
                }
                else
                {
                    // cpp_dbc::system_utils::safePrint("B1C2D3E4", "ConnectionPool::close - Connection " + std::to_string(i + 1) + " is null, skipping");
                }
            }
            catch (...)
            {
                // cpp_dbc::system_utils::safePrint("F5A6B7C8", "ConnectionPool::close - Exception closing connection " + std::to_string(i + 1) + ", ignoring");
                //  Ignore errors during close
            }
        }
        // cpp_dbc::system_utils::safePrint("D9E0F1A2", "ConnectionPool::close - All connections processed");

        // Clear collections
        // cpp_dbc::system_utils::safePrint("B3C4D5E6", "ConnectionPool::close - Clearing idle connections queue");
        while (!m_idleConnections.empty())
        {
            m_idleConnections.pop();
        }
        // cpp_dbc::system_utils::safePrint("F7A8B9C0", "ConnectionPool::close - Clearing idle connections set");
        //  m_idleConnectionsSet.clear();
        // cpp_dbc::system_utils::safePrint("D1E2F3A4", "ConnectionPool::close - Clearing all connections vector");
        m_allConnections.clear();
        // cpp_dbc::system_utils::safePrint("B5C6D7E8", "ConnectionPool::close - Close operation completed successfully");
    }

    bool ConnectionPool::isRunning() const
    {
        return m_running.load();
    }

    // PooledConnection implementation
    PooledConnection::PooledConnection(std::shared_ptr<Connection> connection, ConnectionPool *connectionPool)
        : m_conn(connection), m_pool(connectionPool), m_active(false), m_closed(false)
    {
        m_creationTime = std::chrono::steady_clock::now();
        m_lastUsedTime = m_creationTime;
    }

    PooledConnection::~PooledConnection()
    {
        // cpp_dbc::system_utils::safePrint("F9A0B1C2", "PooledConnection::~PooledConnection - Starting destructor");

        // Mark as closed to prevent any further operations
        m_closed = true;

        // Simply close the underlying connection without returning to pool
        if (m_conn)
        {
            try
            {
                // cpp_dbc::system_utils::safePrint("D3E4F5A6", "PooledConnection::~PooledConnection - Closing underlying connection");
                m_conn->close();
                // cpp_dbc::system_utils::safePrint("B7C8D9E0", "PooledConnection::~PooledConnection - Underlying connection closed successfully");
            }
            catch (...)
            {
                // cpp_dbc::system_utils::safePrint("F1A2B3C4", "PooledConnection::~PooledConnection - Exception during close, ignoring");
                //  Ignore exceptions during destruction
            }
        }

        // cpp_dbc::system_utils::safePrint("D5E6F7A8", "PooledConnection::~PooledConnection - Destructor completed");
    }

    /*
    void PooledConnection::close()
    {
        // safe fallback if someone uses conn->close()
        close_(shared_from_this()); // ← aún la necesitas aquí, aunque idealmente se usará vía el pool
    }
    */
    void PooledConnection::close()
    {
        // cpp_dbc::system_utils::safePrint("B9C0D1E2", "PooledConnection::close - Starting, closed=" + std::to_string(closed));

        // Use atomic exchange to ensure only one thread processes the close
        bool expected = false;
        if (!m_closed.compare_exchange_strong(expected, true))
        {
            // cpp_dbc::system_utils::safePrint("F3A4B5C6", "PooledConnection::close - Already closed by another thread, ignoring");
            return;
        }

        try
        {
            // cpp_dbc::system_utils::safePrint("D7E8F9A0", "PooledConnection::close - Updating last used time");
            //  Return to pool instead of actually closing
            m_lastUsedTime = std::chrono::steady_clock::now();

            // cpp_dbc::system_utils::safePrint("B1C2D3E4", "PooledConnection::close - Getting shared_from_this");
            //  Check if we can safely use shared_from_this
            //  auto self = shared_from_this();
            // cpp_dbc::system_utils::safePrint("F5A6B7C8", "PooledConnection::close - shared_from_this successful");

            if (m_pool)
            {
                // cpp_dbc::system_utils::safePrint("D9E0F1B2", "PooledConnection::close - Returning connection to pool");
                m_pool->returnConnection(std::static_pointer_cast<PooledConnection>(shared_from_this()));
                m_closed.store(false);
                //  cpp_dbc::system_utils::safePrint("B3C4D5F6", "PooledConnection::close - Connection returned to pool successfully");
            }
            else
            {
                // cpp_dbc::system_utils::safePrint("F7A8B9D0", "PooledConnection::close - Cannot return to pool, connection will be destroyed");
            }
        }
        catch (const std::bad_weak_ptr &e)
        {
            // cpp_dbc::system_utils::safePrint("D1E2F3B4", "PooledConnection::close - bad_weak_ptr exception: " + std::string(e.what()));
            //  shared_from_this failed, mark as closed
            m_closed = true;
        }
        catch (const std::exception &e)
        {
            // cpp_dbc::system_utils::safePrint("B5C6D7F8", "PooledConnection::close - Exception: " + std::string(e.what()));
            //  Any other exception, mark as closed
            m_closed = true;
        }
        catch (...)
        {
            // cpp_dbc::system_utils::safePrint("F9A0B1D2", "PooledConnection::close - Unknown exception");
            //  Any other exception, mark as closed
            m_closed = true;
        }

        // cpp_dbc::system_utils::safePrint("D3E4F5B6", "PooledConnection::close - Completed");
    }

    bool PooledConnection::isClosed()
    {
        return m_closed || m_conn->isClosed();
    }

    std::shared_ptr<PreparedStatement> PooledConnection::prepareStatement(const std::string &sql)
    {
        if (m_closed)
        {
            throw DBException("95097A0AE22B", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->prepareStatement(sql);
    }

    std::shared_ptr<ResultSet> PooledConnection::executeQuery(const std::string &sql)
    {
        if (m_closed)
        {
            throw DBException("F9E32F56CFF4", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->executeQuery(sql);
    }

    uint64_t PooledConnection::executeUpdate(const std::string &sql)
    {
        if (m_closed)
        {
            throw DBException("3D5C6173E4D1", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->executeUpdate(sql);
    }

    void PooledConnection::setAutoCommit(bool autoCommit)
    {
        if (m_closed)
        {
            throw DBException("5742170C69B4", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->setAutoCommit(autoCommit);
    }

    bool PooledConnection::getAutoCommit()
    {
        if (m_closed)
        {
            throw DBException("E3DEAB8A5E6D", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getAutoCommit();
    }

    void PooledConnection::commit()
    {
        if (m_closed)
        {
            throw DBException("E32DBBC7316E", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->commit();
    }

    void PooledConnection::rollback()
    {
        if (m_closed)
        {
            throw DBException("094612AE91B6", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->rollback();
    }

    void PooledConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        if (m_closed)
        {
            throw DBException("F7A2B9C3D1E5", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        m_conn->setTransactionIsolation(level);
    }

    TransactionIsolationLevel PooledConnection::getTransactionIsolation()
    {
        if (m_closed)
        {
            throw DBException("A4B5C6D7E8F9", "Connection is closed", system_utils::captureCallStack());
        }
        m_lastUsedTime = std::chrono::steady_clock::now();
        return m_conn->getTransactionIsolation();
    }

    std::chrono::time_point<std::chrono::steady_clock> PooledConnection::getCreationTime() const
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> PooledConnection::getLastUsedTime() const
    {
        return m_lastUsedTime;
    }

    void PooledConnection::setActive(bool isActive)
    {
        m_active = isActive;
    }

    bool PooledConnection::isActive() const
    {
        return m_active;
    }

    void PooledConnection::returnToPool()
    {
        this->close();
    }

    bool PooledConnection::isPooled()
    {
        return this->m_active == false;
    }

    std::shared_ptr<Connection> PooledConnection::getUnderlyingConnection()
    {
        return m_conn;
    }

    // MySQL connection pool implementation
    namespace MySQL
    {
        MySQLConnectionPool::MySQLConnectionPool(const std::string &url,
                                                 const std::string &username,
                                                 const std::string &password)
            : ConnectionPool(url, username, password)
        {
            // MySQL-specific initialization if needed
        }

        MySQLConnectionPool::MySQLConnectionPool(const config::ConnectionPoolConfig &config)
            : ConnectionPool(config)
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
            : ConnectionPool(url, username, password)
        {
            // PostgreSQL-specific initialization if needed
        }

        PostgreSQLConnectionPool::PostgreSQLConnectionPool(const config::ConnectionPoolConfig &config)
            : ConnectionPool(config)
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
            : ConnectionPool(url, username, password)
        {
            // SQLite only supports SERIALIZABLE isolation level
            // Override the isolation level from the constructor
            this->setPoolTransactionIsolation(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
        }

        SQLiteConnectionPool::SQLiteConnectionPool(const config::ConnectionPoolConfig &config)
            : ConnectionPool(config)
        {
            // SQLite only supports SERIALIZABLE isolation level
            // Override the isolation level from the config
            this->setPoolTransactionIsolation(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
        }
    }

} // namespace cpp_dbc
