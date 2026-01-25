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
 * @file kv_db_connection_pool.hpp
 * @brief Connection pool implementation for key-value databases
 */

#ifndef CPP_DBC_KV_DB_CONNECTION_POOL_HPP
#define CPP_DBC_KV_DB_CONNECTION_POOL_HPP

#include "../../cpp_dbc.hpp"
#include "cpp_dbc/core/db_connection_pool.hpp"
#include "cpp_dbc/core/db_connection_pooled.hpp"
#include "cpp_dbc/core/kv/kv_db_connection.hpp"

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
    class KVPooledDBConnection;

    // Forward declaration of configuration classes
    namespace config
    {
        class DatabaseConfig;
        class DBConnectionPoolConfig;
    }

    /**
     * @brief Connection pool implementation for key-value databases
     *
     * This class manages a pool of key-value database connections, providing
     * efficient connection reuse, lifecycle management, and monitoring.
     *
     * Features:
     * - Connection pooling with configurable size and behavior
     * - Connection validation and recycling
     * - Automatic maintenance of pool health
     * - Statistics tracking
     */
    class KVDBConnectionPool : public DBConnectionPool, public std::enable_shared_from_this<KVDBConnectionPool>
    {
    private:
        friend class KVPooledDBConnection;

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
        std::vector<std::shared_ptr<KVPooledDBConnection>> m_allConnections;
        std::queue<std::shared_ptr<KVPooledDBConnection>> m_idleConnections;
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
        std::shared_ptr<KVDBConnection> createDBConnection();

        // Creates a new pooled connection wrapper
        std::shared_ptr<KVPooledDBConnection> createPooledDBConnection();

        // Validates a connection
        bool validateConnection(std::shared_ptr<KVDBConnection> conn) const;

        // Returns a connection to the pool
        void returnConnection(std::shared_ptr<KVPooledDBConnection> conn);

        // Maintenance thread function
        void maintenanceTask();

        std::shared_ptr<KVPooledDBConnection> getIdleDBConnection();

    protected:
        // Sets the transaction isolation level for the pool
        void setPoolTransactionIsolation(TransactionIsolationLevel level) override
        {
            m_transactionIsolation = level;
        }

        // Initialize the pool after construction (creates initial connections and starts maintenance thread)
        // This must be called after the pool is managed by a shared_ptr
        void initializePool();

        // Protected constructors - pools must be created via factory methods
        // Constructor that takes individual parameters
        KVDBConnectionPool(const std::string &url,
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
                           const std::string &validationQuery = "PING",
                           TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);

        // Constructor that accepts a configuration object
        explicit KVDBConnectionPool(const config::DBConnectionPoolConfig &config);

    public:
        // Static factory methods - use these to create pools
        static std::shared_ptr<KVDBConnectionPool> create(const std::string &url,
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
                                                          const std::string &validationQuery = "PING",
                                                          TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);

        static std::shared_ptr<KVDBConnectionPool> create(const config::DBConnectionPoolConfig &config);

        ~KVDBConnectionPool() override;

        // DBConnectionPool interface implementation
        std::shared_ptr<DBConnection> getDBConnection() override;

        // Specialized method for key-value databases
        virtual std::shared_ptr<KVDBConnection> getKVDBConnection();

        // Gets current pool statisticsH
        size_t getActiveDBConnectionCount() const override;
        size_t getIdleDBConnectionCount() const override;
        size_t getTotalDBConnectionCount() const override;

        // Closes the pool and all connections
        void close() override;

        // Check if pool is running
        bool isRunning() const override;
    };

    /**
     * @brief Pooled connection implementation for key-value databases
     *
     * This class wraps a physical key-value database connection and provides
     * pooling functionality, returning the connection to the pool when closed
     * rather than actually closing the physical connection.
     */
    class KVPooledDBConnection : public DBConnectionPooled, public KVDBConnection, public std::enable_shared_from_this<KVPooledDBConnection>
    {
    private:
        std::shared_ptr<KVDBConnection> m_conn;
        std::weak_ptr<KVDBConnectionPool> m_pool;
        std::shared_ptr<std::atomic<bool>> m_poolAlive; // Shared flag to check if pool is still alive
        std::chrono::time_point<std::chrono::steady_clock> m_creationTime{std::chrono::steady_clock::now()};
        std::chrono::time_point<std::chrono::steady_clock> m_lastUsedTime{m_creationTime};
        std::atomic<bool> m_active{false};
        std::atomic<bool> m_closed{false};

        friend class KVDBConnectionPool;

        // Helper method to check if pool is still valid
        bool isPoolValid() const override;

    public:
        KVPooledDBConnection(std::shared_ptr<KVDBConnection> conn,
                             std::weak_ptr<KVDBConnectionPool> pool,
                             std::shared_ptr<std::atomic<bool>> poolAlive);
        ~KVPooledDBConnection() override;

        // Overridden DBConnection interface methods
        void close() override;
        bool isClosed() override;
        void returnToPool() override;
        bool isPooled() override;
        std::string getURL() const override;

        // KVDBConnection interface delegated methods
        bool setString(const std::string &key, const std::string &value,
                       std::optional<int64_t> expirySeconds = std::nullopt) override;
        std::string getString(const std::string &key) override;
        bool exists(const std::string &key) override;
        bool deleteKey(const std::string &key) override;
        int64_t deleteKeys(const std::vector<std::string> &keys) override;
        bool expire(const std::string &key, int64_t seconds) override;
        int64_t getTTL(const std::string &key) override;
        int64_t increment(const std::string &key, int64_t by = 1) override;
        int64_t decrement(const std::string &key, int64_t by = 1) override;
        int64_t listPushLeft(const std::string &key, const std::string &value) override;
        int64_t listPushRight(const std::string &key, const std::string &value) override;
        std::string listPopLeft(const std::string &key) override;
        std::string listPopRight(const std::string &key) override;
        std::vector<std::string> listRange(const std::string &key, int64_t start, int64_t stop) override;
        int64_t listLength(const std::string &key) override;
        bool hashSet(const std::string &key, const std::string &field, const std::string &value) override;
        std::string hashGet(const std::string &key, const std::string &field) override;
        bool hashDelete(const std::string &key, const std::string &field) override;
        bool hashExists(const std::string &key, const std::string &field) override;
        std::map<std::string, std::string> hashGetAll(const std::string &key) override;
        int64_t hashLength(const std::string &key) override;
        bool setAdd(const std::string &key, const std::string &member) override;
        bool setRemove(const std::string &key, const std::string &member) override;
        bool setIsMember(const std::string &key, const std::string &member) override;
        std::vector<std::string> setMembers(const std::string &key) override;
        int64_t setSize(const std::string &key) override;
        bool sortedSetAdd(const std::string &key, double score, const std::string &member) override;
        bool sortedSetRemove(const std::string &key, const std::string &member) override;
        std::optional<double> sortedSetScore(const std::string &key, const std::string &member) override;
        std::vector<std::string> sortedSetRange(const std::string &key, int64_t start, int64_t stop) override;
        int64_t sortedSetSize(const std::string &key) override;
        std::vector<std::string> scanKeys(const std::string &pattern, int64_t count = 10) override;
        std::string executeCommand(const std::string &command, const std::vector<std::string> &args = {}) override;
        bool flushDB(bool async = false) override;
        std::string ping() override;
        std::map<std::string, std::string> getServerInfo() override;

        // DBConnectionPooled interface methods
        std::chrono::time_point<std::chrono::steady_clock> getCreationTime() const override;
        std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime() const override;
        void setActive(bool active) override;
        bool isActive() const override;

        // Implementation of DBConnectionPooled interface
        std::shared_ptr<DBConnection> getUnderlyingConnection() override;

        // KVPooledDBConnection specific method
        std::shared_ptr<KVDBConnection> getUnderlyingKVConnection();

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API (delegated to underlying connection)
        // ====================================================================

        cpp_dbc::expected<bool, DBException> setString(
            std::nothrow_t,
            const std::string &key,
            const std::string &value,
            std::optional<int64_t> expirySeconds = std::nullopt) noexcept override;

        cpp_dbc::expected<std::string, DBException> getString(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<bool, DBException> exists(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<bool, DBException> deleteKey(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<int64_t, DBException> deleteKeys(
            std::nothrow_t, const std::vector<std::string> &keys) noexcept override;

        cpp_dbc::expected<bool, DBException> expire(
            std::nothrow_t, const std::string &key, int64_t seconds) noexcept override;

        cpp_dbc::expected<int64_t, DBException> getTTL(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<int64_t, DBException> increment(
            std::nothrow_t, const std::string &key, int64_t by = 1) noexcept override;

        cpp_dbc::expected<int64_t, DBException> decrement(
            std::nothrow_t, const std::string &key, int64_t by = 1) noexcept override;

        cpp_dbc::expected<int64_t, DBException> listPushLeft(
            std::nothrow_t, const std::string &key, const std::string &value) noexcept override;

        cpp_dbc::expected<int64_t, DBException> listPushRight(
            std::nothrow_t, const std::string &key, const std::string &value) noexcept override;

        cpp_dbc::expected<std::string, DBException> listPopLeft(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<std::string, DBException> listPopRight(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<std::vector<std::string>, DBException> listRange(
            std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept override;

        cpp_dbc::expected<int64_t, DBException> listLength(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<bool, DBException> hashSet(
            std::nothrow_t,
            const std::string &key,
            const std::string &field,
            const std::string &value) noexcept override;

        cpp_dbc::expected<std::string, DBException> hashGet(
            std::nothrow_t, const std::string &key, const std::string &field) noexcept override;

        cpp_dbc::expected<bool, DBException> hashDelete(
            std::nothrow_t, const std::string &key, const std::string &field) noexcept override;

        cpp_dbc::expected<bool, DBException> hashExists(
            std::nothrow_t, const std::string &key, const std::string &field) noexcept override;

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> hashGetAll(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<int64_t, DBException> hashLength(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<bool, DBException> setAdd(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept override;

        cpp_dbc::expected<bool, DBException> setRemove(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept override;

        cpp_dbc::expected<bool, DBException> setIsMember(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept override;

        cpp_dbc::expected<std::vector<std::string>, DBException> setMembers(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<int64_t, DBException> setSize(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<bool, DBException> sortedSetAdd(
            std::nothrow_t, const std::string &key, double score, const std::string &member) noexcept override;

        cpp_dbc::expected<bool, DBException> sortedSetRemove(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept override;

        cpp_dbc::expected<std::optional<double>, DBException> sortedSetScore(
            std::nothrow_t, const std::string &key, const std::string &member) noexcept override;

        cpp_dbc::expected<std::vector<std::string>, DBException> sortedSetRange(
            std::nothrow_t, const std::string &key, int64_t start, int64_t stop) noexcept override;

        cpp_dbc::expected<int64_t, DBException> sortedSetSize(
            std::nothrow_t, const std::string &key) noexcept override;

        cpp_dbc::expected<std::vector<std::string>, DBException> scanKeys(
            std::nothrow_t, const std::string &pattern, int64_t count = 10) noexcept override;

        cpp_dbc::expected<std::string, DBException> executeCommand(
            std::nothrow_t,
            const std::string &command,
            const std::vector<std::string> &args = {}) noexcept override;

        cpp_dbc::expected<bool, DBException> flushDB(
            std::nothrow_t, bool async = false) noexcept override;

        cpp_dbc::expected<std::string, DBException> ping(std::nothrow_t) noexcept override;

        cpp_dbc::expected<std::map<std::string, std::string>, DBException> getServerInfo(
            std::nothrow_t) noexcept override;
    };

    // Specialized connection pool for Redis
    namespace Redis
    {
        /**
         * @brief Redis-specific connection pool implementation
         *
         * This class extends the generic KVDBConnectionPool with Redis-specific
         * configuration and behaviors.
         */
        class RedisConnectionPool : public KVDBConnectionPool
        {
        protected:
            RedisConnectionPool(const std::string &url,
                                const std::string &username,
                                const std::string &password);

            explicit RedisConnectionPool(const config::DBConnectionPoolConfig &config);

        public:
            static std::shared_ptr<RedisConnectionPool> create(const std::string &url,
                                                               const std::string &username,
                                                               const std::string &password);

            static std::shared_ptr<RedisConnectionPool> create(const config::DBConnectionPoolConfig &config);
        };
    }

} // namespace cpp_dbc

#endif // CPP_DBC_KV_DB_CONNECTION_POOL_HPP
