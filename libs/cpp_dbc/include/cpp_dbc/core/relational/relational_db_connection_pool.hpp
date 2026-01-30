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

#ifndef CPP_DBC_RELATIONAL_DB_CONNECTION_POOL_HPP
#define CPP_DBC_RELATIONAL_DB_CONNECTION_POOL_HPP

#include "../../cpp_dbc.hpp"
#include "cpp_dbc/core/db_connection_pool.hpp"
#include "cpp_dbc/core/db_connection_pooled.hpp"
#include "cpp_dbc/core/relational/relational_db_connection.hpp"
#include "cpp_dbc/core/relational/relational_db_prepared_statement.hpp"
#include "cpp_dbc/core/relational/relational_db_result_set.hpp"

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
    class RelationalPooledDBConnection;

    // Forward declaration of configuration classes
    namespace config
    {
        class DatabaseConfig;
        class DBConnectionPoolConfig;
    }

    // Relational Database Connection Pool class
    class RelationalDBConnectionPool : public DBConnectionPool, public std::enable_shared_from_this<RelationalDBConnectionPool>
    {
    private:
        friend class RelationalPooledDBConnection;

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
        TransactionIsolationLevel m_transactionIsolation; // Transaction isolation level for connections
        std::vector<std::shared_ptr<RelationalPooledDBConnection>> m_allConnections;
        std::queue<std::shared_ptr<RelationalPooledDBConnection>> m_idleConnections;
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
        std::shared_ptr<RelationalDBConnection> createDBConnection();

        // Creates a new pooled connection wrapper
        std::shared_ptr<RelationalPooledDBConnection> createPooledDBConnection();

        // Validates a connection
        bool validateConnection(std::shared_ptr<RelationalDBConnection> conn) const;

        // Returns a connection to the pool
        void returnConnection(std::shared_ptr<RelationalPooledDBConnection> conn);

        // Maintenance thread function
        void maintenanceTask();

        std::shared_ptr<RelationalPooledDBConnection> getIdleDBConnection();

    protected:
        // Sets the transaction isolation level for the pool
        void setPoolTransactionIsolation(TransactionIsolationLevel level) override
        {
            m_transactionIsolation = level;
        }

        // Initialize the pool after construction (creates initial connections and starts maintenance thread)
        // This must be called after the pool is managed by a shared_ptr
        void initializePool();

    public:
        // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
        // Constructor that takes individual parameters
        RelationalDBConnectionPool(ConstructorTag,
                                   const std::string &url,
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
        explicit RelationalDBConnectionPool(ConstructorTag, const config::DBConnectionPoolConfig &config);

        // Static factory methods - use these to create pools
        static std::shared_ptr<RelationalDBConnectionPool> create(const std::string &url,
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

        static std::shared_ptr<RelationalDBConnectionPool> create(const config::DBConnectionPoolConfig &config);

        ~RelationalDBConnectionPool() override;

        // DBConnectionPool interface implementation
        std::shared_ptr<DBConnection> getDBConnection() override;

        // Specialized method for relational databases
        virtual std::shared_ptr<RelationalDBConnection> getRelationalDBConnection();

        // Gets current pool statistics
        size_t getActiveDBConnectionCount() const override;
        size_t getIdleDBConnectionCount() const override;
        size_t getTotalDBConnectionCount() const override;

        // Closes the pool and all connections
        void close() override;

        // Check if pool is running
        bool isRunning() const override;
    };

    // RelationalPooledDBConnection wraps a physical relational database connection
    class RelationalPooledDBConnection : public DBConnectionPooled, public RelationalDBConnection, public std::enable_shared_from_this<RelationalPooledDBConnection>
    {
    private:
        std::shared_ptr<RelationalDBConnection> m_conn;
        std::weak_ptr<RelationalDBConnectionPool> m_pool; // Weak pointer to connection pool
        std::shared_ptr<std::atomic<bool>> m_poolAlive;   // Shared flag to check if pool is still alive
        std::chrono::time_point<std::chrono::steady_clock> m_creationTime;
        std::chrono::time_point<std::chrono::steady_clock> m_lastUsedTime;
        std::atomic<bool> m_active{false};
        std::atomic<bool> m_closed{false};

        friend class RelationalDBConnectionPool;

        // Helper method to check if pool is still valid
        bool isPoolValid() const override;

    public:
        RelationalPooledDBConnection(std::shared_ptr<RelationalDBConnection> conn,
                                     std::weak_ptr<RelationalDBConnectionPool> pool,
                                     std::shared_ptr<std::atomic<bool>> poolAlive);
        ~RelationalPooledDBConnection() override;

        // Overridden DBConnection interface methods
        // DBConnection interface methods
        void close() override;
        bool isClosed() override;
        void returnToPool() override;
        bool isPooled() override;
        std::string getURL() const override;

        // Overridden RelationalDBConnection interface methods
        std::shared_ptr<RelationalDBPreparedStatement> prepareStatement(const std::string &sql) override;
        std::shared_ptr<RelationalDBResultSet> executeQuery(const std::string &sql) override;
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

        // Nothrow API
        cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> prepareStatement(std::nothrow_t, const std::string &sql) noexcept override;
        cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> executeQuery(std::nothrow_t, const std::string &sql) noexcept override;
        cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t, const std::string &sql) noexcept override;
        cpp_dbc::expected<void, DBException> setAutoCommit(std::nothrow_t, bool autoCommit) noexcept override;
        cpp_dbc::expected<bool, DBException> getAutoCommit(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> beginTransaction(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> transactionActive(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> commit(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> rollback(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override;
        cpp_dbc::expected<TransactionIsolationLevel, DBException> getTransactionIsolation(std::nothrow_t) noexcept override;

        // DBConnectionPooled interface methods
        std::chrono::time_point<std::chrono::steady_clock> getCreationTime() const override;
        std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime() const override;
        void setActive(bool active) override;
        bool isActive() const override;

        // Implementation of DBConnectionPooled interface
        std::shared_ptr<DBConnection> getUnderlyingConnection() override;

        // RelationalPooledDBConnection specific method
        std::shared_ptr<RelationalDBConnection> getUnderlyingRelationalConnection();
    };

    // Specialized connection pools for MySQL, PostgreSQL, SQLite, and Firebird
    namespace MySQL
    {
        class MySQLConnectionPool : public RelationalDBConnectionPool
        {
        public:
            // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
            MySQLConnectionPool(ConstructorTag,
                                const std::string &url,
                                const std::string &username,
                                const std::string &password);

            explicit MySQLConnectionPool(ConstructorTag, const config::DBConnectionPoolConfig &config);

            static std::shared_ptr<MySQLConnectionPool> create(const std::string &url,
                                                               const std::string &username,
                                                               const std::string &password);

            static std::shared_ptr<MySQLConnectionPool> create(const config::DBConnectionPoolConfig &config);
        };
    }

    namespace PostgreSQL
    {
        class PostgreSQLConnectionPool : public RelationalDBConnectionPool
        {
        public:
            // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
            PostgreSQLConnectionPool(ConstructorTag,
                                     const std::string &url,
                                     const std::string &username,
                                     const std::string &password);

            explicit PostgreSQLConnectionPool(ConstructorTag, const config::DBConnectionPoolConfig &config);

            static std::shared_ptr<PostgreSQLConnectionPool> create(const std::string &url,
                                                                    const std::string &username,
                                                                    const std::string &password);

            static std::shared_ptr<PostgreSQLConnectionPool> create(const config::DBConnectionPoolConfig &config);
        };
    }

    namespace SQLite
    {
        class SQLiteConnectionPool : public RelationalDBConnectionPool
        {
        public:
            // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
            SQLiteConnectionPool(ConstructorTag,
                                 const std::string &url,
                                 const std::string &username,
                                 const std::string &password);

            explicit SQLiteConnectionPool(ConstructorTag, const config::DBConnectionPoolConfig &config);

            static std::shared_ptr<SQLiteConnectionPool> create(const std::string &url,
                                                                const std::string &username,
                                                                const std::string &password);

            static std::shared_ptr<SQLiteConnectionPool> create(const config::DBConnectionPoolConfig &config);
        };
    }

    namespace Firebird
    {
        class FirebirdConnectionPool : public RelationalDBConnectionPool
        {
        public:
            // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
            FirebirdConnectionPool(ConstructorTag,
                                   const std::string &url,
                                   const std::string &username,
                                   const std::string &password);

            explicit FirebirdConnectionPool(ConstructorTag, const config::DBConnectionPoolConfig &config);

            static std::shared_ptr<FirebirdConnectionPool> create(const std::string &url,
                                                                  const std::string &username,
                                                                  const std::string &password);

            static std::shared_ptr<FirebirdConnectionPool> create(const config::DBConnectionPoolConfig &config);
        };
    }

} // namespace cpp_dbc

#endif // CPP_DBC_RELATIONAL_DB_CONNECTION_POOL_HPP
