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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file connection_pool.hpp
 @brief Tests for database connections

*/

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
        std::string m_url;
        std::string m_username;
        std::string m_password;
        std::map<std::string, std::string> m_options;     // Connection options
        int m_initialSize;                                // Initial number of connections
        size_t m_maxSize;                                 // Maximum number of connections
        size_t m_minIdle;                                 // Minimum number of idle connections
        long m_maxWaitMillis;                             // Maximum wait time for a connection in milliseconds
        long m_validationTimeoutMillis;                   // Timeout for connection validation
        long m_idleTimeoutMillis;                         // Maximum time a connection can be idle before being closed
        long m_maxLifetimeMillis;                         // Maximum lifetime of a connection
        bool m_testOnBorrow;                              // Test connection before borrowing
        bool m_testOnReturn;                              // Test connection when returning to pool
        std::string m_validationQuery;                    // Query used to validate connections
        TransactionIsolationLevel m_transactionIsolation; // Transaction isolation level for connections
        std::vector<std::shared_ptr<PooledConnection>> m_allConnections;
        std::queue<std::shared_ptr<PooledConnection>> m_idleConnections;
        // std::unordered_set<std::shared_ptr<PooledConnection>> m_idleConnectionsSet; // Track what's in the queue
        mutable std::mutex m_mutexGetConnection;
        mutable std::mutex m_mutexReturnConnection;
        mutable std::mutex m_mutexAllConnections;
        mutable std::mutex m_mutexIdleConnections;
        mutable std::mutex m_mutexMaintenance;
        // std::condition_variable m_condition;
        std::condition_variable m_maintenanceCondition;
        std::atomic<bool> m_running;
        std::atomic<int> m_activeConnections;
        std::thread m_maintenanceThread;

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

    protected:
        // Sets the transaction isolation level for the pool
        void setPoolTransactionIsolation(TransactionIsolationLevel level)
        {
            m_transactionIsolation = level;
        }

    public:
        // Constructor that takes individual parameters
        ConnectionPool(const std::string &url,
                       const std::string &username,
                       const std::string &password,
                       const std::map<std::string, std::string> &options = std::map<std::string, std::string>(),
                       int initialSize = 5,
                       int maxSize = 20,
                       int minIdle = 3,
                       long maxWaitMillis = 5000,
                       long validationTimeoutMillis = 5000,
                       long idleTimeoutMillis = 300000,
                       long maxLifetimeMillis = 1800000,
                       bool testOnBorrow = true,
                       bool testOnReturn = false,
                       const std::string &validationQuery = "SELECT 1",
                       TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);

        // Constructor that accepts a configuration object
        ConnectionPool(const config::ConnectionPoolConfig &config);

        // Static factory method
        static std::shared_ptr<ConnectionPool> create(const config::ConnectionPoolConfig &config);

        ~ConnectionPool();

        // Borrows a connection from the pool
        virtual std::shared_ptr<Connection> getConnection();

        // Gets current pool statistics
        int getActiveConnectionCount() const;
        size_t getIdleConnectionCount() const;
        size_t getTotalConnectionCount() const;

        // Closes the pool and all connections
        void close();

        // Check if pool is running
        bool isRunning() const;
    };

    // PooledConnection wraps a physical connection
    class PooledConnection : public Connection, public std::enable_shared_from_this<PooledConnection>
    {
    private:
        std::shared_ptr<Connection> m_conn;
        ConnectionPool *m_pool;
        std::chrono::time_point<std::chrono::steady_clock> m_creationTime;
        std::chrono::time_point<std::chrono::steady_clock> m_lastUsedTime;
        std::atomic<bool> m_active;
        std::atomic<bool> m_closed;

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
        uint64_t executeUpdate(const std::string &sql) override;

        void setAutoCommit(bool autoCommit) override;
        bool getAutoCommit() override;

        void commit() override;
        void rollback() override;

        // Transaction management methods
        bool beginTransaction() override;
        bool transactionActive() override;

        // Transaction isolation level methods
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

        // Get the connection URL
        std::string getURL() const override;

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

    namespace SQLite
    {
        class SQLiteConnectionPool : public ConnectionPool
        {
        public:
            SQLiteConnectionPool(const std::string &url,
                                 const std::string &username,
                                 const std::string &password);

            SQLiteConnectionPool(const config::ConnectionPoolConfig &config);
        };
    }

} // namespace cpp_dbc

#endif // CPP_DBC_CONNECTION_POOL_HPP
