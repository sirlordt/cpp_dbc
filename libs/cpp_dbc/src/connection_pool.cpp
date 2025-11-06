// CPPDBC_ConnectionPool.cpp
// Implementation of connection pool for cpp_dbc

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
        : url(url),
          username(username),
          password(password),
          initialSize(initialSize),
          maxSize(maxSize),
          minIdle(minIdle),
          maxWaitMillis(maxWaitMillis),
          validationTimeoutMillis(validationTimeoutMillis),
          idleTimeoutMillis(idleTimeoutMillis),
          maxLifetimeMillis(maxLifetimeMillis),
          testOnBorrow(testOnBorrow),
          testOnReturn(testOnReturn),
          validationQuery(validationQuery),
          transactionIsolation(transactionIsolation),
          running(true),
          activeConnections(0)
    {

        allConnections.reserve(maxSize);

        // Create initial connections
        for (int i = 0; i < initialSize; i++)
        {
            auto pooledConn = createPooledConnection();
            idleConnections.push(pooledConn);
            allConnections.push_back(pooledConn);
        }

        // Start maintenance thread
        maintenanceThread = std::thread(&ConnectionPool::maintenanceTask, this);
    }

    ConnectionPool::ConnectionPool(const config::ConnectionPoolConfig &config)
        : url(config.getUrl()),
          username(config.getUsername()),
          password(config.getPassword()),
          initialSize(config.getInitialSize()),
          maxSize(config.getMaxSize()),
          minIdle(config.getMinIdle()),
          maxWaitMillis(config.getConnectionTimeout()),
          validationTimeoutMillis(config.getValidationInterval()),
          idleTimeoutMillis(config.getIdleTimeout()),
          maxLifetimeMillis(config.getMaxLifetimeMillis()),
          testOnBorrow(config.getTestOnBorrow()),
          testOnReturn(config.getTestOnReturn()),
          validationQuery(config.getValidationQuery()),
          transactionIsolation(config.getTransactionIsolation()),
          running(true),
          activeConnections(0)
    {

        allConnections.reserve(maxSize);

        // Create initial connections
        // cpp_dbc::system_utils::safePrint("A1B2C3D4", "ConnectionPool: Creating " + std::to_string(initialSize) + " initial connections...");
        for (int i = 0; i < initialSize; i++)
        {
            // cpp_dbc::system_utils::safePrint("E5F6A7B8", "ConnectionPool: Creating connection " + std::to_string(i + 1) + "/" + std::to_string(initialSize));
            auto pooledConn = createPooledConnection();
            // cpp_dbc::system_utils::safePrint("C9D0E1F2", "ConnectionPool: Connection " + std::to_string(i + 1) + " created successfully");
            idleConnections.push(pooledConn);
            allConnections.push_back(pooledConn);
        }
        // cpp_dbc::system_utils::safePrint("A3B4C5D6", "ConnectionPool: All initial connections created. Pool ready.");

        maintenanceThread = std::thread(&ConnectionPool::maintenanceTask, this);
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
        return DriverManager::getConnection(url, username, password);
    }

    std::shared_ptr<PooledConnection> ConnectionPool::createPooledConnection()
    {
        // cpp_dbc::system_utils::safePrint("A5B6C7D8", "createPooledConnection: Starting...");
        auto conn = createConnection();
        // cpp_dbc::system_utils::safePrint("E9F0A1B2", "createPooledConnection: Physical connection created");

        // Set transaction isolation level on the new connection
        conn->setTransactionIsolation(transactionIsolation);

        auto pooledConn = std::make_shared<PooledConnection>(conn, this);
        // cpp_dbc::system_utils::safePrint("C3D4E5F6", "createPooledConnection: PooledConnection wrapper created");
        return pooledConn;
    }

    bool ConnectionPool::validateConnection(std::shared_ptr<Connection> conn)
    {
        try
        {
            // Use validation query to check connection
            auto resultSet = conn->executeQuery(validationQuery);
            return true;
        }
        catch (const SQLException &e)
        {
            return false;
        }
    }

    void ConnectionPool::returnConnection(std::shared_ptr<PooledConnection> conn)
    {
        std::lock_guard<std::mutex> lock(mutexReturnConnection);
        // cpp_dbc::system_utils::safePrint("A7B8C9D0", "ConnectionPool::returnConnection - Starting, Active: " + std::to_string(activeConnections) +
        //                                                  ", Idle: " + std::to_string(idleConnections.size()) +
        //                                                  ", Total: " + std::to_string(allConnections.size()) +
        //                                                  ", Max: " + std::to_string(maxSize));

        if (!running.load())
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
        auto it = std::find(allConnections.begin(), allConnections.end(), conn);
        if (it == allConnections.end())
        {
            // cpp_dbc::system_utils::safePrint("A9B0C1D2", "ConnectionPool::returnConnection - Connection not found in pool, ignoring");
            return;
        }

        // Test connection before returning to pool if configured
        if (testOnReturn)
        {
            if (!validateConnection(conn->getUnderlyingConnection()))
            {
                // Remove from allConnections
                auto it = std::find(allConnections.begin(), allConnections.end(), conn);
                if (it != allConnections.end())
                {
                    allConnections.erase(it);
                }

                // Replace with new connection if pool is running
                if (running && allConnections.size() < minIdle)
                {
                    auto newConn = createPooledConnection();
                    idleConnections.push(newConn);
                    allConnections.push_back(newConn);
                }

                return;
            }
        }

        try
        {
            if (!conn->closed)
            {
                // Check if the connection's transaction isolation level is different from the pool's
                // If so, reset it to match the pool's level
                TransactionIsolationLevel connIsolation = conn->getTransactionIsolation();
                if (connIsolation != transactionIsolation)
                {
                    conn->setTransactionIsolation(transactionIsolation);
                }
            }
        }
        catch (...)
        // catch (const cpp_dbc::SQLException &e)
        {
            // std::cerr << "SQL Error: " << e.what() << std::endl;
        }

        // Mark as inactive and update last used time
        // cpp_dbc::system_utils::safePrint("E3F4A5B6", "ConnectionPool::returnConnection - Marking connection as inactive");
        conn->setActive(false);

        /*
        // Check if connection is already in idle set (prevent duplicates)
        if (idleConnectionsSet.find(conn) != idleConnectionsSet.end())
        {
            cpp_dbc::system_utils::safePrint("C7D8E9F0", "ConnectionPool::returnConnection - Connection already in idle set, ignoring duplicate");
            return;
        }
            */

        // Add to idle connections queue and tracking set
        // cpp_dbc::system_utils::safePrint("A1B2C3D4", "ConnectionPool::returnConnection - Adding to idle queue");
        {
            std::lock_guard<std::mutex> lock(mutexIdleConnections);
            idleConnections.push(conn);
            activeConnections--;
        }
        // cpp_dbc::system_utils::safePrint("E5F6A7B8", "ConnectionPool::returnConnection - Connection returned to pool, now Active: " +
        //                                                  std::to_string(activeConnections) + ", Idle: " + std::to_string(idleConnections.size()));

        // Notify waiting threads
        // cpp_dbc::system_utils::safePrint("C9D0E1F2", "ConnectionPool::returnConnection - Notifying waiting threads");
        // condition.notify_one();
        //     std::cout << "ConnectionPool::returnConnection - Notified waiting threads" << std::endl;
    }

    std::shared_ptr<PooledConnection> ConnectionPool::getIdleConnection()
    {
        // cpp_dbc::system_utils::safePrint("C1D2E3F4", "getIdleConnection: Checking idle connections. Count: " + std::to_string(idleConnections.size()));
        if (!idleConnections.empty())
        {
            // cpp_dbc::system_utils::safePrint("A5B6C7D8", "getIdleConnection: Found idle connection, retrieving...");
            auto conn = idleConnections.front();
            idleConnections.pop();

            // Remove from tracking set
            // idleConnectionsSet.erase(conn);

            // Test connection before use if configured
            if (testOnBorrow)
            {
                if (!validateConnection(conn->getUnderlyingConnection()))
                {
                    // Remove from allConnections
                    auto it = std::find(allConnections.begin(), allConnections.end(), conn);
                    if (it != allConnections.end())
                    {
                        allConnections.erase(it);
                    }

                    // Create new connection
                    return createPooledConnection();
                }
            }

            // cpp_dbc::system_utils::safePrint("E9F0A1B2", "getIdleConnection: Returning idle connection");
            return conn;
        }
        else if (allConnections.size() < maxSize)
        {
            // cpp_dbc::system_utils::safePrint("C3D4E5F6", "getIdleConnection: No idle connections, need to create new one. Current: " +
            //                                                  std::to_string(allConnections.size()) + ", Max: " + std::to_string(maxSize));
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
        std::unique_lock<std::mutex> lock(mutexGetConnection);

        // auto conn = createConnection();
        // // cpp_dbc::system_utils::safePrint("E9F0A1B2", "createPooledConnection: Physical connection created");
        // return std::make_shared<PooledConnection>(conn, this);

        // std::lock_guard<std::mutex> lock(mutex);
        // cpp_dbc::system_utils::safePrint("E7F8A9B0", "ConnectionPool::getConnection - Lock acquired. Active: " + std::to_string(activeConnections) +
        //                                                 ", Idle: " + std::to_string(idleConnections.size()) +
        //                                                 ", Total: " + std::to_string(allConnections.size()) +
        //                                                 ", Max: " + std::to_string(maxSize));

        if (!running.load())
        {
            // std::cout << "ConnectionPool::getConnection - Pool is closed" << std::endl;
            throw SQLException("Connection pool is closed");
        }

        /*
        // Try to get idle connection or create new one
        auto getIdleConnection = [this]() -> std::shared_ptr<PooledConnection>
        {
            // cpp_dbc::system_utils::safePrint("C1D2E3F4", "getIdleConnection: Checking idle connections. Count: " + std::to_string(idleConnections.size()));
            if (!idleConnections.empty())
            {
                // cpp_dbc::system_utils::safePrint("A5B6C7D8", "getIdleConnection: Found idle connection, retrieving...");
                auto conn = idleConnections.front();
                idleConnections.pop();

                // Remove from tracking set
                // idleConnectionsSet.erase(conn);

                // Test connection before use if configured
                if (testOnBorrow)
                {
                    if (!validateConnection(conn->getUnderlyingConnection()))
                    {
                        // Remove from allConnections
                        auto it = std::find(allConnections.begin(), allConnections.end(), conn);
                        if (it != allConnections.end())
                        {
                            allConnections.erase(it);
                        }

                        // Create new connection
                        return createPooledConnection();
                    }
                }

                // cpp_dbc::system_utils::safePrint("E9F0A1B2", "getIdleConnection: Returning idle connection");
                return conn;
            }
            else if (allConnections.size() < maxSize)
            {
                // cpp_dbc::system_utils::safePrint("C3D4E5F6", "getIdleConnection: No idle connections, need to create new one. Current: " +
                //                                                  std::to_string(allConnections.size()) + ", Max: " + std::to_string(maxSize));
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
        if (result == nullptr && allConnections.size() < maxSize)
        {
            // cpp_dbc::system_utils::safePrint("A9B0C1D2", "ConnectionPool::getConnection - Creating new connection outside lock...");
            //  Release lock temporarily to create new connection
            //  lock.unlock();

            // try
            // {
            result = createPooledConnection();

            {
                std::lock_guard<std::mutex> lock(mutexAllConnections);
                allConnections.push_back(result);
            }
            // auto newConn = createPooledConnection();
            //  // cpp_dbc::system_utils::safePrint("E3F4A5B6", "ConnectionPool::getConnection - New connection created, re-acquiring lock...");

            // // Re-acquire lock and check size again (another thread might have added connections)
            // // lock.lock();
            // if (allConnections.size() < maxSize)
            // {
            //     allConnections.push_back(newConn);
            //     result = newConn;
            //     // cpp_dbc::system_utils::safePrint("C7D8E9F0", "ConnectionPool::getConnection - New connection added to pool");

            //     // cpp_dbc::system_utils::safePrint("F7059E39E45F", "ConnectionPool::getConnection - Lock acquired. Active: " + std::to_string(activeConnections) +
            //     //                                                      ", Idle: " + std::to_string(idleConnections.size()) +
            //     //                                                      ", Total: " + std::to_string(allConnections.size()) +
            //     //                                                      ", Max: " + std::to_string(maxSize));
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

                // auto waitStatus = condition.wait_for(lock,
                //                                      std::chrono::milliseconds(maxWaitMillis)); //The condition create radom stop and seg fails
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                auto waitedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - waitStart).count();

                if (waitedMs >= maxWaitMillis)
                {
                    throw SQLException("88D90026A76A: Timeout waiting for connection from the pool");
                }

                // if (waitStatus == std::cv_status::timeout)
                // {
                //     // std::cout << "ConnectionPool::getConnection - TIMEOUT waiting for connection! Active: "
                //     //              << activeConnections << ", Idle: " << idleConnections.size()
                //     //              << ", Total: " << allConnections.size() << ", Max: " << maxSize << std::endl;

                //     throw SQLException("Timeout waiting for connection from the pool");
                // }

                if (!running.load())
                {
                    throw SQLException("58566A84D1A1: Connection pool is closed");
                }

                // Check if we've waited too long
                // now = std::chrono::steady_clock::now();

                // result = createPooledConnection(); // this->getIdleConnection();
                result = this->getIdleConnection();

                // waitedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - waitStart).count();

                // if (result == nullptr && waitedMs >= maxWaitMillis)
                // {
                //     throw SQLException("723F1992C1C7: Timeout waiting for connection from the pool");
                // }
            } while (result == nullptr);
        }

        // // Add connection to the pool if it's new
        // if (std::find(allConnections.begin(), allConnections.end(), result) == allConnections.end())
        // {
        //     allConnections.push_back(result);
        // }

        // Mark as active
        result->setActive(true);
        activeConnections++;
        // std::cout << "ConnectionPool::getConnection - Connection obtained, now Active: "
        //           << activeConnections << ", Idle: " << idleConnections.size() << std::endl;

        return result;
    }

    void ConnectionPool::maintenanceTask()
    {
        do
        {
            // Wait for 30 seconds or until notified (e.g., when close() is called)
            std::unique_lock<std::mutex> lock(mutexMaintenance);
            maintenanceCondition.wait_for(lock, std::chrono::seconds(30), [this]
                                          { return !running; });

            if (!running)
            {
                break;
            }

            auto now = std::chrono::steady_clock::now();

            // Ensure no body touch the allConnections and idleConnections variables when is used by this thread
            std::lock_guard<std::mutex> lockAllConnections(mutexAllConnections);
            std::lock_guard<std::mutex> lockIdleConnectons(mutexIdleConnections);

            // Check all connections for expired ones
            for (auto it = allConnections.begin(); it != allConnections.end();)
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

                bool expired = (idleTime > idleTimeoutMillis) ||
                               (lifeTime > maxLifetimeMillis);

                // Close and remove expired connections if we have more than minIdle
                if (expired && allConnections.size() > minIdle)
                {
                    // Remove from idle queue if present
                    std::queue<std::shared_ptr<PooledConnection>> tempQueue;
                    while (!idleConnections.empty())
                    {
                        auto conn = idleConnections.front();
                        idleConnections.pop();

                        if (conn != pooledConn)
                        {
                            tempQueue.push(conn);
                        }
                    }
                    idleConnections = tempQueue;

                    // Remove from allConnections
                    it = allConnections.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            // Ensure we have at least minIdle connections
            while (running && allConnections.size() < minIdle)
            {
                auto pooledConn = createPooledConnection();
                idleConnections.push(pooledConn);
                allConnections.push_back(pooledConn);
            }
        } while (running);
    }

    int ConnectionPool::getActiveConnectionCount() const
    {
        return activeConnections;
    }

    int ConnectionPool::getIdleConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(mutexIdleConnections);
        return idleConnections.size();
    }

    int ConnectionPool::getTotalConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(mutexAllConnections);
        return allConnections.size();
    }

    void ConnectionPool::close()
    {
        time_t start_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        CP_DEBUG("ConnectionPool::close - Starting close operation at " << start_time);

        if (!running.exchange(false))
        {
            CP_DEBUG("ConnectionPool::close - Already closed, returning");
            return; // Already closed
        }

        CP_DEBUG("ConnectionPool::close - Waiting for active operations to complete at "
                 << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

        // Wait for all active operations to complete
        {
            auto waitStart = std::chrono::steady_clock::now();
            int initialActiveConnections = activeConnections.load();
            CP_DEBUG("ConnectionPool::close - Initial active connections: " << initialActiveConnections);

            while (activeConnections.load() > 0)
            {
                CP_DEBUG("ConnectionPool::close - Waiting for " << activeConnections.load()
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
                    activeConnections.store(0);
                    break;
                }
            }
        }

        // cpp_dbc::system_utils::safePrint("F9A0B1C2", "ConnectionPool::close - Notifying waiting threads");
        //  Notify all waiting threads
        //  condition.notify_all();
        maintenanceCondition.notify_all();

        // Join maintenance thread (only if it was started)
        // cpp_dbc::system_utils::safePrint("D3E4F5A6", "ConnectionPool::close - Checking maintenance thread");
        if (maintenanceThread.joinable())
        {
            // cpp_dbc::system_utils::safePrint("B7C8D9E0", "ConnectionPool::close - Joining maintenance thread");
            maintenanceThread.join();
            // cpp_dbc::system_utils::safePrint("F1A2B3C4", "ConnectionPool::close - Maintenance thread joined");
        }

        // Close all connections
        // cpp_dbc::system_utils::safePrint("D5E6F7A8", "ConnectionPool::close - Acquiring lock to close connections");
        std::lock_guard<std::mutex> lockAllConnections(mutexAllConnections);
        std::lock_guard<std::mutex> lockIdleConnections(mutexIdleConnections);
        // cpp_dbc::system_utils::safePrint("B9C0D1E2", "ConnectionPool::close - Lock acquired, closing " + std::to_string(allConnections.size()) + " connections");

        for (size_t i = 0; i < allConnections.size(); ++i)
        {
            try
            {
                // cpp_dbc::system_utils::safePrint("F3A4B5C6", "ConnectionPool::close - Closing connection " + std::to_string(i + 1) + "/" + std::to_string(allConnections.size()));
                auto &conn = allConnections[i];
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
        while (!idleConnections.empty())
        {
            idleConnections.pop();
        }
        // cpp_dbc::system_utils::safePrint("F7A8B9C0", "ConnectionPool::close - Clearing idle connections set");
        //  idleConnectionsSet.clear();
        // cpp_dbc::system_utils::safePrint("D1E2F3A4", "ConnectionPool::close - Clearing all connections vector");
        allConnections.clear();
        // cpp_dbc::system_utils::safePrint("B5C6D7E8", "ConnectionPool::close - Close operation completed successfully");
    }

    bool ConnectionPool::isRunning() const
    {
        return running.load();
    }

    // PooledConnection implementation
    PooledConnection::PooledConnection(std::shared_ptr<Connection> connection, ConnectionPool *connectionPool)
        : conn(connection), pool(connectionPool), active(false), closed(false)
    {
        creationTime = std::chrono::steady_clock::now();
        lastUsedTime = creationTime;
    }

    PooledConnection::~PooledConnection()
    {
        // cpp_dbc::system_utils::safePrint("F9A0B1C2", "PooledConnection::~PooledConnection - Starting destructor");

        // Mark as closed to prevent any further operations
        closed = true;

        // Simply close the underlying connection without returning to pool
        if (conn)
        {
            try
            {
                // cpp_dbc::system_utils::safePrint("D3E4F5A6", "PooledConnection::~PooledConnection - Closing underlying connection");
                conn->close();
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
        if (!closed.compare_exchange_strong(expected, true))
        {
            // cpp_dbc::system_utils::safePrint("F3A4B5C6", "PooledConnection::close - Already closed by another thread, ignoring");
            return;
        }

        try
        {
            // cpp_dbc::system_utils::safePrint("D7E8F9A0", "PooledConnection::close - Updating last used time");
            //  Return to pool instead of actually closing
            lastUsedTime = std::chrono::steady_clock::now();

            // cpp_dbc::system_utils::safePrint("B1C2D3E4", "PooledConnection::close - Getting shared_from_this");
            //  Check if we can safely use shared_from_this
            //  auto self = shared_from_this();
            // cpp_dbc::system_utils::safePrint("F5A6B7C8", "PooledConnection::close - shared_from_this successful");

            if (pool)
            {
                // cpp_dbc::system_utils::safePrint("D9E0F1B2", "PooledConnection::close - Returning connection to pool");
                pool->returnConnection(std::static_pointer_cast<PooledConnection>(shared_from_this()));
                closed.store(false);
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
            closed = true;
        }
        catch (const std::exception &e)
        {
            // cpp_dbc::system_utils::safePrint("B5C6D7F8", "PooledConnection::close - Exception: " + std::string(e.what()));
            //  Any other exception, mark as closed
            closed = true;
        }
        catch (...)
        {
            // cpp_dbc::system_utils::safePrint("F9A0B1D2", "PooledConnection::close - Unknown exception");
            //  Any other exception, mark as closed
            closed = true;
        }

        // cpp_dbc::system_utils::safePrint("D3E4F5B6", "PooledConnection::close - Completed");
    }

    bool PooledConnection::isClosed()
    {
        return closed || conn->isClosed();
    }

    std::shared_ptr<PreparedStatement> PooledConnection::prepareStatement(const std::string &sql)
    {
        if (closed)
        {
            throw SQLException("95097A0AE22B: Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        return conn->prepareStatement(sql);
    }

    std::shared_ptr<ResultSet> PooledConnection::executeQuery(const std::string &sql)
    {
        if (closed)
        {
            throw SQLException("F9E32F56CFF4: Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        return conn->executeQuery(sql);
    }

    int PooledConnection::executeUpdate(const std::string &sql)
    {
        if (closed)
        {
            throw SQLException("3D5C6173E4D1: Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        return conn->executeUpdate(sql);
    }

    void PooledConnection::setAutoCommit(bool autoCommit)
    {
        if (closed)
        {
            throw SQLException("5742170C69B4: Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        conn->setAutoCommit(autoCommit);
    }

    bool PooledConnection::getAutoCommit()
    {
        if (closed)
        {
            throw SQLException("E3DEAB8A5E6D: Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        return conn->getAutoCommit();
    }

    void PooledConnection::commit()
    {
        if (closed)
        {
            throw SQLException("E32DBBC7316E: Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        conn->commit();
    }

    void PooledConnection::rollback()
    {
        if (closed)
        {
            throw SQLException("094612AE91B6: Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        conn->rollback();
    }

    void PooledConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        if (closed)
        {
            throw SQLException("F7A2B9C3D1E5: Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        conn->setTransactionIsolation(level);
    }

    TransactionIsolationLevel PooledConnection::getTransactionIsolation()
    {
        if (closed)
        {
            throw SQLException("A4B5C6D7E8F9: Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        return conn->getTransactionIsolation();
    }

    std::chrono::time_point<std::chrono::steady_clock> PooledConnection::getCreationTime() const
    {
        return creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> PooledConnection::getLastUsedTime() const
    {
        return lastUsedTime;
    }

    void PooledConnection::setActive(bool isActive)
    {
        active = isActive;
    }

    bool PooledConnection::isActive() const
    {
        return active;
    }

    void PooledConnection::returnToPool()
    {
        this->close();
    }

    bool PooledConnection::isPooled()
    {
        return this->isActive() == false;
    }

    std::shared_ptr<Connection> PooledConnection::getUnderlyingConnection()
    {
        return conn;
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

} // namespace cpp_dbc
