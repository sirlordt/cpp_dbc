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
 * @file columnar_db_connection_pool.hpp
 * @brief Connection pool implementation for columnar databases
 */

#ifndef CPP_DBC_COLUMNAR_DB_CONNECTION_POOL_HPP
#define CPP_DBC_COLUMNAR_DB_CONNECTION_POOL_HPP

#include "../../cpp_dbc.hpp"
#include "cpp_dbc/core/db_connection_pool.hpp"
#include "cpp_dbc/core/db_connection_pooled.hpp"
#include "cpp_dbc/core/columnar/columnar_db_connection.hpp"
#include "cpp_dbc/core/columnar/columnar_db_prepared_statement.hpp"
#include "cpp_dbc/core/columnar/columnar_db_result_set.hpp"

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
    class ColumnarPooledDBConnection;

    // Forward declaration of configuration classes
    namespace config
    {
        class DatabaseConfig;
        class DBConnectionPoolConfig;
    }

    /**
     * @brief Connection pool implementation for columnar databases
     *
     * This class manages a pool of columnar database connections, providing
     * efficient connection reuse, lifecycle management, and monitoring.
     */
    class ColumnarDBConnectionPool : public DBConnectionPool, public std::enable_shared_from_this<ColumnarDBConnectionPool>
    {
    private:
        friend class ColumnarPooledDBConnection;

        // Shared flag to indicate if the pool is still alive (shared with all pooled connections)
        std::shared_ptr<std::atomic<bool>> m_poolAlive;

        // Connection parameters
        std::string m_url;
        std::string m_username;
        std::string m_password;
        std::map<std::string, std::string> m_options;     // Connection options
        int m_initialSize{0};                             // Initial number of connections
        size_t m_maxSize{0};                              // Maximum number of connections
        size_t m_minIdle{0};                              // Minimum number of idle connections
        long m_maxWaitMillis{0};                          // Maximum wait time for a connection in milliseconds
        long m_validationTimeoutMillis{0};                // Timeout for connection validation
        long m_idleTimeoutMillis{0};                      // Maximum time a connection can be idle before being closed
        long m_maxLifetimeMillis{0};                      // Maximum lifetime of a connection
        bool m_testOnBorrow{false};                       // Test connection before borrowing
        bool m_testOnReturn{false};                       // Test connection when returning to pool
        std::string m_validationQuery;                    // Query used to validate connections
        TransactionIsolationLevel m_transactionIsolation; // Transaction isolation level (if supported)
        std::vector<std::shared_ptr<ColumnarPooledDBConnection>> m_allConnections;
        std::queue<std::shared_ptr<ColumnarPooledDBConnection>> m_idleConnections;
        mutable std::mutex m_mutexGetConnection;
        mutable std::mutex m_mutexReturnConnection;
        mutable std::mutex m_mutexAllConnections;
        mutable std::mutex m_mutexIdleConnections;
        mutable std::mutex m_mutexMaintenance;
        std::condition_variable m_maintenanceCondition;
        std::atomic<bool> m_running{true};
        std::atomic<int> m_activeConnections{0};
        std::jthread m_maintenanceThread;

        // Creates a new physical connection
        std::shared_ptr<ColumnarDBConnection> createDBConnection();

        // Creates a new pooled connection wrapper
        std::shared_ptr<ColumnarPooledDBConnection> createPooledDBConnection();

        // Validates a connection
        bool validateConnection(std::shared_ptr<ColumnarDBConnection> conn) const;

        // Returns a connection to the pool
        void returnConnection(std::shared_ptr<ColumnarPooledDBConnection> conn);

        // Maintenance thread function
        void maintenanceTask();

        std::shared_ptr<ColumnarPooledDBConnection> getIdleDBConnection();

    protected:
        // Sets the transaction isolation level for the pool
        void setPoolTransactionIsolation(TransactionIsolationLevel level) override
        {
            m_transactionIsolation = level;
        }

        // Initialize the pool after construction (creates initial connections and starts maintenance thread)
        // This must be called after the pool is managed by a shared_ptr
        void initializePool();

    protected:
        // Protected constructors - pools must be created via factory methods
        // Constructor that takes individual parameters
        ColumnarDBConnectionPool(const std::string &url,
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
                                 const std::string &validationQuery = "SELECT now() FROM system.local",
                                 TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);

        // Constructor that accepts a configuration object
        ColumnarDBConnectionPool(const config::DBConnectionPoolConfig &config);

    public:
        // Static factory methods - use these to create pools
        static std::shared_ptr<ColumnarDBConnectionPool> create(const std::string &url,
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
                                                                const std::string &validationQuery = "SELECT now() FROM system.local",
                                                                TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);

        static std::shared_ptr<ColumnarDBConnectionPool> create(const config::DBConnectionPoolConfig &config);

        ~ColumnarDBConnectionPool();

        // DBConnectionPool interface implementation
        std::shared_ptr<DBConnection> getDBConnection() override;

        // Specialized method for columnar databases
        virtual std::shared_ptr<ColumnarDBConnection> getColumnarDBConnection();

        // Gets current pool statistics
        size_t getActiveDBConnectionCount() const override;
        size_t getIdleDBConnectionCount() const override;
        size_t getTotalDBConnectionCount() const override;

        // Closes the pool and all connections
        void close() final;

        // Check if pool is running
        bool isRunning() const override;
    };

    /**
     * @brief Pooled connection implementation for columnar databases
     *
     * This class wraps a physical columnar database connection and provides
     * pooling functionality.
     */
    class ColumnarPooledDBConnection : public DBConnectionPooled, public ColumnarDBConnection, public std::enable_shared_from_this<ColumnarPooledDBConnection>
    {
    private:
        std::shared_ptr<ColumnarDBConnection> m_conn;
        std::weak_ptr<ColumnarDBConnectionPool> m_pool;
        std::shared_ptr<std::atomic<bool>> m_poolAlive; // Shared flag to check if pool is still alive
        std::chrono::time_point<std::chrono::steady_clock> m_creationTime;
        std::chrono::time_point<std::chrono::steady_clock> m_lastUsedTime;
        std::atomic<bool> m_active{false};
        std::atomic<bool> m_closed{false};

        friend class ColumnarDBConnectionPool;

        // Helper method to check if pool is still valid
        bool isPoolValid() const final;

    public:
        ColumnarPooledDBConnection(std::shared_ptr<ColumnarDBConnection> conn,
                                   std::weak_ptr<ColumnarDBConnectionPool> pool,
                                   std::shared_ptr<std::atomic<bool>> poolAlive);
        ~ColumnarPooledDBConnection() override;

        // Overridden DBConnection interface methods
        void close() final;
        bool isClosed() override;
        void returnToPool() final;
        bool isPooled() override;
        std::string getURL() const override;

        // Overridden ColumnarDBConnection interface methods
        std::shared_ptr<ColumnarDBPreparedStatement> prepareStatement(const std::string &query) override;
        std::shared_ptr<ColumnarDBResultSet> executeQuery(const std::string &query) override;
        uint64_t executeUpdate(const std::string &query) override;

        bool beginTransaction() override;
        void commit() override;
        void rollback() override;

        // Nothrow API
        cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> prepareStatement(std::nothrow_t, const std::string &query) noexcept override;
        cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> executeQuery(std::nothrow_t, const std::string &query) noexcept override;
        cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t, const std::string &query) noexcept override;
        cpp_dbc::expected<bool, DBException> beginTransaction(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> commit(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> rollback(std::nothrow_t) noexcept override;

        // DBConnectionPooled interface methods
        std::chrono::time_point<std::chrono::steady_clock> getCreationTime() const override;
        std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime() const override;
        void setActive(bool active) override;
        bool isActive() const override;

        // Implementation of DBConnectionPooled interface
        std::shared_ptr<DBConnection> getUnderlyingConnection() override;

        // ColumnarPooledDBConnection specific method
        std::shared_ptr<ColumnarDBConnection> getUnderlyingColumnarConnection();
    };

    // Specialized connection pool for ScyllaDB
    namespace ScyllaDB
    {
        /**
         * @brief ScyllaDB-specific connection pool implementation
         */
        class ScyllaConnectionPool : public ColumnarDBConnectionPool
        {
        protected:
            ScyllaConnectionPool(const std::string &url,
                                 const std::string &username,
                                 const std::string &password);

            ScyllaConnectionPool(const config::DBConnectionPoolConfig &config);

        public:
            static std::shared_ptr<ScyllaConnectionPool> create(const std::string &url,
                                                                const std::string &username,
                                                                const std::string &password);

            static std::shared_ptr<ScyllaConnectionPool> create(const config::DBConnectionPoolConfig &config);
        };
    }

} // namespace cpp_dbc

#endif // CPP_DBC_COLUMNAR_DB_CONNECTION_POOL_HPP
