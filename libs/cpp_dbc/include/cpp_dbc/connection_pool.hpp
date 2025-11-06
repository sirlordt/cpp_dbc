// CPPDBC_ConnectionPool.hpp
// Connection Pool implementation for cpp_dbc with thread safety

#ifndef CPP_DBC_CONNECTION_POOL_HPP
#define CPP_DBC_CONNECTION_POOL_HPP

#include "cpp_dbc.hpp"

// Forward declarations for configuration classes
namespace cpp_dbc
{
    namespace config
    {
        class DatabaseConfig;
        class ConnectionPoolConfig;
    }
}
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <unordered_set>

namespace cpp_dbc
{

    // Forward declaration
    class PooledConnection;

    // Forward declaration of configuration classes
    namespace config
    {
        class ConnectionPoolConfig;
    }

    // Connection Pool class
    class ConnectionPool
    {
    private:
        friend class PooledConnection;

        // Connection parameters
        std::string url;
        std::string username;
        std::string password;
        int initialSize;              // Initial number of connections
        int maxSize;                  // Maximum number of connections
        int minIdle;                  // Minimum number of idle connections
        long maxWaitMillis;           // Maximum wait time for a connection in milliseconds
        long validationTimeoutMillis; // Timeout for connection validation
        long idleTimeoutMillis;       // Maximum time a connection can be idle before being closed
        long maxLifetimeMillis;       // Maximum lifetime of a connection
        bool testOnBorrow;            // Test connection before borrowing
        bool testOnReturn;            // Test connection when returning to pool
        std::string validationQuery;  // Query used to validate connections
        std::vector<std::shared_ptr<PooledConnection>> allConnections;
        std::queue<std::shared_ptr<PooledConnection>> idleConnections;
        // std::unordered_set<std::shared_ptr<PooledConnection>> idleConnectionsSet; // Track what's in the queue
        mutable std::mutex mutexGetConnection;
        mutable std::mutex mutexReturnConnection;
        mutable std::mutex mutexAllConnections;
        mutable std::mutex mutexIdleConnections;
        mutable std::mutex mutexMaintenance;
        // std::condition_variable condition;
        std::condition_variable maintenanceCondition;
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

        std::shared_ptr<PooledConnection> getIdleConnection();

    public:
        // Constructor that takes individual parameters
        ConnectionPool(const std::string &url,
                       const std::string &username,
                       const std::string &password,
                       int initialSize = 5,
                       int maxSize = 20,
                       int minIdle = 3,
                       long maxWaitMillis = 5000,
                       long validationTimeoutMillis = 5000,
                       long idleTimeoutMillis = 300000,
                       long maxLifetimeMillis = 1800000,
                       bool testOnBorrow = true,
                       bool testOnReturn = false,
                       const std::string &validationQuery = "SELECT 1");

        // Constructor that accepts a configuration object
        ConnectionPool(const config::ConnectionPoolConfig &config);

        // Static factory method
        static std::shared_ptr<ConnectionPool> create(const config::ConnectionPoolConfig &config);

        ~ConnectionPool();

        // Borrows a connection from the pool
        virtual std::shared_ptr<Connection> getConnection();

        // Gets current pool statistics
        int getActiveConnectionCount() const;
        int getIdleConnectionCount() const;
        int getTotalConnectionCount() const;

        // Closes the pool and all connections
        void close();

        // Check if pool is running
        bool isRunning() const;
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

        friend class ConnectionPool;

    public:
        PooledConnection(std::shared_ptr<Connection> conn, ConnectionPool *pool);
        ~PooledConnection() override;

        // Overridden Connection interface methods
        void close() override;
        bool isClosed() override;
        void returnToPool() override;
        bool isPooled() override;

        std::shared_ptr<PreparedStatement> prepareStatement(const std::string &sql) override;
        std::shared_ptr<ResultSet> executeQuery(const std::string &sql) override;
        int executeUpdate(const std::string &sql) override;

        void setAutoCommit(bool autoCommit) override;
        bool getAutoCommit() override;

        void commit() override;
        void rollback() override;

        // Transaction isolation level methods
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

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
            MySQLConnectionPool(const std::string &url,
                                const std::string &username,
                                const std::string &password);

            MySQLConnectionPool(const config::ConnectionPoolConfig &config);
        };
    }

    namespace PostgreSQL
    {
        class PostgreSQLConnectionPool : public ConnectionPool
        {
        public:
            PostgreSQLConnectionPool(const std::string &url,
                                     const std::string &username,
                                     const std::string &password);

            PostgreSQLConnectionPool(const config::ConnectionPoolConfig &config);
        };
    }

} // namespace cpp_dbc

#endif // CPP_DBC_CONNECTION_POOL_HPP
