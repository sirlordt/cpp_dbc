// CPPDBC_ConnectionPool.hpp
// Connection Pool implementation for cpp_dbc with thread safety

#ifndef CPP_DBC_CONNECTION_POOL_HPP
#define CPP_DBC_CONNECTION_POOL_HPP

#include "cpp_dbc.hpp"
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

namespace cpp_dbc
{

    // Forward declaration
    class PooledConnection;

    // Configuration for connection pools
    struct ConnectionPoolConfig
    {
        std::string url;
        std::string username;
        std::string password;
        int initialSize = 5;                      // Initial number of connections
        int maxSize = 20;                         // Maximum number of connections
        int minIdle = 3;                          // Minimum number of idle connections
        long maxWaitMillis = 30000;               // Maximum wait time for a connection in milliseconds
        long validationTimeoutMillis = 5000;      // Timeout for connection validation
        long idleTimeoutMillis = 300000;          // Maximum time a connection can be idle before being closed
        long maxLifetimeMillis = 1800000;         // Maximum lifetime of a connection
        bool testOnBorrow = true;                 // Test connection before borrowing
        bool testOnReturn = false;                // Test connection when returning to pool
        std::string validationQuery = "SELECT 1"; // Query used to validate connections
    };

    // Connection Pool class
    class ConnectionPool
    {
    private:
        friend class PooledConnection;

        ConnectionPoolConfig config;
        std::vector<std::shared_ptr<PooledConnection>> allConnections;
        std::queue<std::shared_ptr<PooledConnection>> idleConnections;
        mutable std::mutex mutex;
        std::condition_variable condition;
        std::atomic<bool> running;
        std::atomic<int> activeConnections;
        std::thread maintenanceThread;

        // Creates a new physical connection
        std::shared_ptr<Connection> createConnection();

        // Creates a new pooled connection wrapper
        std::shared_ptr<PooledConnection> createPooledConnection();

        // Validates a connection
        bool validateConnection(std::shared_ptr<Connection> conn);

        // Returns a connection to the pool
        void returnConnection(std::shared_ptr<PooledConnection> conn);

        // Maintenance thread function
        void maintenanceTask();

    public:
        ConnectionPool(const ConnectionPoolConfig &config);
        ~ConnectionPool();

        // Borrows a connection from the pool
        std::shared_ptr<Connection> getConnection();

        // Gets current pool statistics
        int getActiveConnectionCount() const;
        int getIdleConnectionCount() const;
        int getTotalConnectionCount() const;

        // Closes the pool and all connections
        void close();
    };

    // PooledConnection wraps a physical connection
    class PooledConnection : public Connection, public std::enable_shared_from_this<PooledConnection>
    {
    private:
        std::shared_ptr<Connection> conn;
        ConnectionPool *pool;
        std::chrono::time_point<std::chrono::steady_clock> creationTime;
        std::chrono::time_point<std::chrono::steady_clock> lastUsedTime;
        std::atomic<bool> active;
        std::atomic<bool> closed;

    public:
        PooledConnection(std::shared_ptr<Connection> conn, ConnectionPool *pool);
        ~PooledConnection() override;

        // Overridden Connection interface methods
        void close() override;
        bool isClosed() override;

        std::shared_ptr<PreparedStatement> prepareStatement(const std::string &sql) override;
        std::shared_ptr<ResultSet> executeQuery(const std::string &sql) override;
        int executeUpdate(const std::string &sql) override;

        void setAutoCommit(bool autoCommit) override;
        bool getAutoCommit() override;

        void commit() override;
        void rollback() override;

        // PooledConnection specific methods
        std::chrono::time_point<std::chrono::steady_clock> getCreationTime() const;
        std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime() const;
        void setActive(bool active);
        bool isActive() const;

        // Returns the underlying physical connection
        std::shared_ptr<Connection> getUnderlyingConnection();
    };

    // Specialized connection pools for MySQL and PostgreSQL
    namespace MySQL
    {
        class MySQLConnectionPool : public ConnectionPool
        {
        public:
            MySQLConnectionPool(const ConnectionPoolConfig &config);
        };
    }

    namespace PostgreSQL
    {
        class PostgreSQLConnectionPool : public ConnectionPool
        {
        public:
            PostgreSQLConnectionPool(const ConnectionPoolConfig &config);
        };
    }

} // namespace cpp_dbc

#endif // CPP_DBC_CONNECTION_POOL_HPP
