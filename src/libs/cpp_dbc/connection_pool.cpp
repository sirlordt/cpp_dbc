// CPPDBC_ConnectionPool.cpp
// Implementation of connection pool for cpp_dbc

#include "connection_pool.hpp"
#include <algorithm>

namespace cpp_dbc
{

    // ConnectionPool implementation
    ConnectionPool::ConnectionPool(const ConnectionPoolConfig &config)
        : config(config), running(true), activeConnections(0)
    {

        // Create initial connections
        for (int i = 0; i < config.initialSize; i++)
        {
            auto pooledConn = createPooledConnection();
            idleConnections.push(pooledConn);
            allConnections.push_back(pooledConn);
        }

        // Start maintenance thread
        maintenanceThread = std::thread(&ConnectionPool::maintenanceTask, this);
    }

    ConnectionPool::~ConnectionPool()
    {
        close();
    }

    std::shared_ptr<Connection> ConnectionPool::createConnection()
    {
        return DriverManager::getConnection(config.url, config.username, config.password);
    }

    std::shared_ptr<PooledConnection> ConnectionPool::createPooledConnection()
    {
        auto conn = createConnection();
        return std::make_shared<PooledConnection>(conn, this);
    }

    bool ConnectionPool::validateConnection(std::shared_ptr<Connection> conn)
    {
        try
        {
            // Use validation query to check connection
            auto resultSet = conn->executeQuery(config.validationQuery);
            return true;
        }
        catch (const SQLException &e)
        {
            return false;
        }
    }

    void ConnectionPool::returnConnection(std::shared_ptr<PooledConnection> conn)
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (!running)
        {
            // If pool is shutting down, just discard the connection
            return;
        }

        // Test connection before returning to pool if configured
        if (config.testOnReturn)
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
                if (running && allConnections.size() < config.minIdle)
                {
                    auto newConn = createPooledConnection();
                    idleConnections.push(newConn);
                    allConnections.push_back(newConn);
                }

                return;
            }
        }

        // Mark as inactive and update last used time
        conn->setActive(false);

        // Add to idle connections queue
        idleConnections.push(conn);
        activeConnections--;

        // Notify waiting threads
        condition.notify_one();
    }

    std::shared_ptr<Connection> ConnectionPool::getConnection()
    {
        std::unique_lock<std::mutex> lock(mutex);

        if (!running)
        {
            throw SQLException("Connection pool is closed");
        }

        // Try to get idle connection or create new one
        auto getIdleConnection = [this]() -> std::shared_ptr<PooledConnection>
        {
            if (!idleConnections.empty())
            {
                auto conn = idleConnections.front();
                idleConnections.pop();

                // Test connection before use if configured
                if (config.testOnBorrow)
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

                return conn;
            }
            else if (allConnections.size() < config.maxSize)
            {
                // Create new connection since we haven't reached max size
                return createPooledConnection();
            }

            return nullptr;
        };

        std::shared_ptr<PooledConnection> result = getIdleConnection();

        // If no connection available, wait until one becomes available
        if (result == nullptr)
        {
            auto waitStart = std::chrono::steady_clock::now();

            // Wait until a connection is returned to the pool or timeout
            while (result == nullptr)
            {
                auto waitStatus = condition.wait_for(lock,
                                                     std::chrono::milliseconds(config.maxWaitMillis));

                if (waitStatus == std::cv_status::timeout)
                {
                    throw SQLException("Timeout waiting for connection from the pool");
                }

                if (!running)
                {
                    throw SQLException("Connection pool is closed");
                }

                result = getIdleConnection();

                // Check if we've waited too long
                auto now = std::chrono::steady_clock::now();
                auto waitedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - waitStart).count();
                if (result == nullptr && waitedMs >= config.maxWaitMillis)
                {
                    throw SQLException("Timeout waiting for connection from the pool");
                }
            }
        }

        // Add connection to the pool if it's new
        if (std::find(allConnections.begin(), allConnections.end(), result) == allConnections.end())
        {
            allConnections.push_back(result);
        }

        // Mark as active
        result->setActive(true);
        activeConnections++;

        return result;
    }

    void ConnectionPool::maintenanceTask()
    {
        while (running)
        {
            // Sleep for a while before checking connections
            std::this_thread::sleep_for(std::chrono::seconds(30));

            if (!running)
            {
                break;
            }

            std::lock_guard<std::mutex> lock(mutex);

            auto now = std::chrono::steady_clock::now();

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

                bool expired = (idleTime > config.idleTimeoutMillis) ||
                               (lifeTime > config.maxLifetimeMillis);

                // Close and remove expired connections if we have more than minIdle
                if (expired && allConnections.size() > config.minIdle)
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
            while (running && allConnections.size() < config.minIdle)
            {
                auto pooledConn = createPooledConnection();
                idleConnections.push(pooledConn);
                allConnections.push_back(pooledConn);
            }
        }
    }

    int ConnectionPool::getActiveConnectionCount() const
    {
        return activeConnections;
    }

    int ConnectionPool::getIdleConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return idleConnections.size();
    }

    int ConnectionPool::getTotalConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return allConnections.size();
    }

    void ConnectionPool::close()
    {
        if (!running.exchange(false))
        {
            return; // Already closed
        }

        // Notify all waiting threads
        condition.notify_all();

        // Join maintenance thread
        if (maintenanceThread.joinable())
        {
            maintenanceThread.join();
        }

        // Close all connections
        std::lock_guard<std::mutex> lock(mutex);
        for (auto &conn : allConnections)
        {
            try
            {
                conn->getUnderlyingConnection()->close();
            }
            catch (...)
            {
                // Ignore errors during close
            }
        }

        // Clear collections
        while (!idleConnections.empty())
        {
            idleConnections.pop();
        }
        allConnections.clear();
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
        if (!closed && conn)
        {
            try
            {
                conn->close();
            }
            catch (...)
            {
                // Ignore exceptions during destruction
            }
        }
    }

    void PooledConnection::close()
    {
        if (!closed)
        {
            // Return to pool instead of actually closing
            lastUsedTime = std::chrono::steady_clock::now();
            pool->returnConnection(std::static_pointer_cast<PooledConnection>(shared_from_this()));
        }
    }

    bool PooledConnection::isClosed()
    {
        return closed || conn->isClosed();
    }

    std::shared_ptr<PreparedStatement> PooledConnection::prepareStatement(const std::string &sql)
    {
        if (closed)
        {
            throw SQLException("Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        return conn->prepareStatement(sql);
    }

    std::shared_ptr<ResultSet> PooledConnection::executeQuery(const std::string &sql)
    {
        if (closed)
        {
            throw SQLException("Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        return conn->executeQuery(sql);
    }

    int PooledConnection::executeUpdate(const std::string &sql)
    {
        if (closed)
        {
            throw SQLException("Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        return conn->executeUpdate(sql);
    }

    void PooledConnection::setAutoCommit(bool autoCommit)
    {
        if (closed)
        {
            throw SQLException("Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        conn->setAutoCommit(autoCommit);
    }

    bool PooledConnection::getAutoCommit()
    {
        if (closed)
        {
            throw SQLException("Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        return conn->getAutoCommit();
    }

    void PooledConnection::commit()
    {
        if (closed)
        {
            throw SQLException("Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        conn->commit();
    }

    void PooledConnection::rollback()
    {
        if (closed)
        {
            throw SQLException("Connection is closed");
        }
        lastUsedTime = std::chrono::steady_clock::now();
        conn->rollback();
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

    std::shared_ptr<Connection> PooledConnection::getUnderlyingConnection()
    {
        return conn;
    }

    // MySQL connection pool implementation
    namespace MySQL
    {
        MySQLConnectionPool::MySQLConnectionPool(const ConnectionPoolConfig &config)
            : ConnectionPool(config)
        {
            // MySQL-specific initialization if needed
        }
    }

    // PostgreSQL connection pool implementation
    namespace PostgreSQL
    {
        PostgreSQLConnectionPool::PostgreSQLConnectionPool(const ConnectionPoolConfig &config)
            : ConnectionPool(config)
        {
            // PostgreSQL-specific initialization if needed
        }
    }

} // namespace cpp_dbc
