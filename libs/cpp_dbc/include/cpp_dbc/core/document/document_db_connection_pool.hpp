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
 * @file document_db_connection_pool.hpp
 * @brief Connection pool implementation for document databases
 */

#ifndef CPP_DBC_DOCUMENT_DB_CONNECTION_POOL_HPP
#define CPP_DBC_DOCUMENT_DB_CONNECTION_POOL_HPP

#include "../../cpp_dbc.hpp"
#include "cpp_dbc/core/db_connection_pool.hpp"
#include "cpp_dbc/core/db_connection_pooled.hpp"
#include "cpp_dbc/core/document/document_db_connection.hpp"

#include <queue>
#include <deque>
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
    class DocumentPooledDBConnection;

    // Forward declaration of configuration classes
    namespace config
    {
        class DatabaseConfig;
        class DBConnectionPoolConfig;
    }

    /**
     * @brief Connection pool implementation for document databases
     *
     * Manages a pool of document database connections with configurable size,
     * validation, and lifecycle management. Create pools via the static
     * `create()` factory method.
     *
     * ```cpp
     * auto pool = cpp_dbc::MongoDB::MongoDBConnectionPool::create(
     *     "mongodb://localhost:27017/mydb", "user", "pass");
     * auto conn = std::dynamic_pointer_cast<cpp_dbc::DocumentDBConnection>(
     *     pool->getDBConnection());
     * auto coll = conn->getCollection("users");
     * // ... use connection ...
     * conn->returnToPool();
     * pool->close();
     * ```
     *
     * @see DocumentDBConnection, DBConnectionPool
     */
    class DocumentDBConnectionPool : public DBConnectionPool, public std::enable_shared_from_this<DocumentDBConnectionPool>
    {
    private:
        friend class DocumentPooledDBConnection;

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
        std::vector<std::shared_ptr<DocumentPooledDBConnection>> m_allConnections;
        std::queue<std::shared_ptr<DocumentPooledDBConnection>> m_idleConnections;
        mutable std::mutex m_mutexPool;                 // Protects m_allConnections + m_idleConnections + CVs + m_waitQueue
        std::condition_variable m_maintenanceCondition; // Wakes maintenance thread on close()
        std::condition_variable m_connectionAvailable;  // Wakes borrowers (direct handoff or state change)
        std::atomic<bool> m_running{true};
        std::atomic<int> m_activeConnections{0};
        size_t m_pendingCreations{0}; // Connections being created outside lock (guarded by m_mutexPool)
        std::jthread m_maintenanceThread;

        // Direct handoff mechanism: eliminates "stolen wakeup" race condition.
        struct ConnectionRequest
        {
            std::shared_ptr<DocumentPooledDBConnection> conn;
            bool fulfilled{false};
        };
        std::deque<ConnectionRequest *> m_waitQueue;

        // Creates a new physical connection
        cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException> createDBConnection(std::nothrow_t) noexcept;

        // Creates a new pooled connection wrapper
        cpp_dbc::expected<std::shared_ptr<DocumentPooledDBConnection>, DBException> createPooledDBConnection(std::nothrow_t) noexcept;

        // Validates a connection
        cpp_dbc::expected<bool, DBException> validateConnection(std::nothrow_t, std::shared_ptr<DocumentDBConnection> conn) const noexcept;

        // Returns a connection to the pool
        cpp_dbc::expected<void, DBException> returnConnection(std::nothrow_t, std::shared_ptr<DocumentPooledDBConnection> conn) noexcept;

        // Maintenance thread function
        void maintenanceTask();

    protected:
        // Sets the transaction isolation level for the pool
        void setPoolTransactionIsolation(TransactionIsolationLevel level) noexcept override
        {
            m_transactionIsolation = level;
        }

        // Initialize the pool after construction (creates initial connections and starts maintenance thread)
        // This must be called after the pool is managed by a shared_ptr
        cpp_dbc::expected<void, DBException> initializePool(std::nothrow_t) noexcept;

    public:
        // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
        // Constructor that takes individual parameters
        DocumentDBConnectionPool(ConstructorTag,
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
                                 const std::string &validationQuery = "{\"ping\": 1}",
                                 TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);

        // Constructor that accepts a configuration object
        explicit DocumentDBConnectionPool(ConstructorTag, const config::DBConnectionPoolConfig &config);

        // Static factory methods - use these to create pools
        static cpp_dbc::expected<std::shared_ptr<DocumentDBConnectionPool>, DBException> create(std::nothrow_t,
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
                                                                                                const std::string &validationQuery = "{\"ping\": 1}",
                                                                                                TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED) noexcept;

        static cpp_dbc::expected<std::shared_ptr<DocumentDBConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;

        ~DocumentDBConnectionPool() override;

        #ifdef __cpp_exceptions
        // DBConnectionPool interface implementation
        std::shared_ptr<DBConnection> getDBConnection() override;

        // Specialized method for document databases
        virtual std::shared_ptr<DocumentDBConnection> getDocumentDBConnection();

        // Gets current pool statistics
        size_t getActiveDBConnectionCount() const override;
        size_t getIdleDBConnectionCount() const override;
        size_t getTotalDBConnectionCount() const override;

        // Closes the pool and all connections
        void close() override;

        // Check if pool is running
        bool isRunning() const override;

        #endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> getDBConnection(std::nothrow_t) noexcept override;
        cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException> getDocumentDBConnection(std::nothrow_t) noexcept;
        cpp_dbc::expected<size_t, DBException> getActiveDBConnectionCount(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<size_t, DBException> getIdleDBConnectionCount(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<size_t, DBException> getTotalDBConnectionCount(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isRunning(std::nothrow_t) const noexcept override;
    };

    /**
     * @brief Pooled connection implementation for document databases
     *
     * This class wraps a physical document database connection and provides
     * pooling functionality, returning the connection to the pool when closed
     * rather than actually closing the physical connection.
     */
    class DocumentPooledDBConnection : public DBConnectionPooled, public DocumentDBConnection, public std::enable_shared_from_this<DocumentPooledDBConnection>
    {
    private:
        std::shared_ptr<DocumentDBConnection> m_conn;
        std::weak_ptr<DocumentDBConnectionPool> m_pool;
        std::shared_ptr<std::atomic<bool>> m_poolAlive; // Shared flag to check if pool is still alive
        std::chrono::time_point<std::chrono::steady_clock> m_creationTime{std::chrono::steady_clock::now()};
        // Store last-used time as nanoseconds since epoch in an atomic int64_t.
        // std::atomic<int64_t> is lock-free on every supported 64-bit platform,
        // unlike std::atomic<time_point> which is not portable to ARM32/MIPS.
        static_assert(std::atomic<int64_t>::is_always_lock_free,
                      "int64_t atomic must be lock-free on this platform");
        std::atomic<int64_t> m_lastUsedTimeNs{m_creationTime.time_since_epoch().count()};
        std::atomic<bool> m_active{false};
        std::atomic<bool> m_closed{false};

        friend class DocumentDBConnectionPool;

        // Helper method to check if pool is still valid
        bool isPoolValid(std::nothrow_t) const noexcept override;

        // Helper method to safely update last used time
        inline void updateLastUsedTime(std::nothrow_t) noexcept
        {
            m_lastUsedTimeNs.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_relaxed);
        }

    public:
        DocumentPooledDBConnection(std::shared_ptr<DocumentDBConnection> conn,
                                   std::weak_ptr<DocumentDBConnectionPool> pool,
                                   std::shared_ptr<std::atomic<bool>> poolAlive);
        ~DocumentPooledDBConnection() override;

        #ifdef __cpp_exceptions
        // Overridden DBConnection interface methods
        void close() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() const override;
        std::string getURL() const override;
        void reset() override;

        // Overridden DocumentDBConnection interface methods
        std::string getDatabaseName() const override;
        std::vector<std::string> listDatabases() override;
        bool databaseExists(const std::string &databaseName) override;
        void useDatabase(const std::string &databaseName) override;
        void dropDatabase(const std::string &databaseName) override;

        std::shared_ptr<DocumentDBCollection> getCollection(const std::string &collectionName) override;
        std::vector<std::string> listCollections() override;
        bool collectionExists(const std::string &collectionName) override;
        std::shared_ptr<DocumentDBCollection> createCollection(const std::string &collectionName, const std::string &options = "") override;
        void dropCollection(const std::string &collectionName) override;

        std::shared_ptr<DocumentDBData> createDocument() override;
        std::shared_ptr<DocumentDBData> createDocument(const std::string &json) override;
        std::shared_ptr<DocumentDBData> runCommand(const std::string &command) override;
        std::shared_ptr<DocumentDBData> getServerInfo() override;
        std::shared_ptr<DocumentDBData> getServerStatus() override;
        bool ping() override;

        std::string startSession() override;
        void endSession(const std::string &sessionId) override;
        void startTransaction(const std::string &sessionId) override;
        void commitTransaction(const std::string &sessionId) override;
        void abortTransaction(const std::string &sessionId) override;
        bool supportsTransactions() override;
        void prepareForPoolReturn() override;

        // DocumentPooledDBConnection specific method
        std::shared_ptr<DocumentDBConnection> getUnderlyingDocumentConnection();

        #endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        // DBConnection nothrow interface
        expected<void, DBException> close(std::nothrow_t) noexcept override;
        expected<void, DBException> reset(std::nothrow_t) noexcept override;
        expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override;
        expected<void, DBException> returnToPool(std::nothrow_t) noexcept override;
        expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override;
        expected<std::string, DBException> getURL(std::nothrow_t) const noexcept override;

        // DocumentDBConnection nothrow interface - delegate to underlying connection
        expected<std::string, DBException> getDatabaseName(std::nothrow_t) const noexcept override;
        expected<std::vector<std::string>, DBException> listDatabases(std::nothrow_t) noexcept override;
        expected<std::shared_ptr<DocumentDBCollection>, DBException> getCollection(
            std::nothrow_t, const std::string &collectionName) noexcept override;
        expected<std::vector<std::string>, DBException> listCollections(std::nothrow_t) noexcept override;
        expected<std::shared_ptr<DocumentDBCollection>, DBException> createCollection(
            std::nothrow_t,
            const std::string &collectionName,
            const std::string &options = "") noexcept override;
        expected<void, DBException> dropCollection(
            std::nothrow_t, const std::string &collectionName) noexcept override;
        expected<void, DBException> dropDatabase(
            std::nothrow_t, const std::string &databaseName) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> createDocument(std::nothrow_t) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> createDocument(
            std::nothrow_t, const std::string &json) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> runCommand(
            std::nothrow_t, const std::string &command) noexcept override;
        expected<bool, DBException> databaseExists(std::nothrow_t, const std::string &databaseName) noexcept override;
        expected<void, DBException> useDatabase(std::nothrow_t, const std::string &databaseName) noexcept override;
        expected<bool, DBException> collectionExists(std::nothrow_t, const std::string &collectionName) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> getServerInfo(std::nothrow_t) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> getServerStatus(std::nothrow_t) noexcept override;
        expected<bool, DBException> ping(std::nothrow_t) noexcept override;
        expected<std::string, DBException> startSession(std::nothrow_t) noexcept override;
        expected<void, DBException> endSession(std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<void, DBException> startTransaction(std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<void, DBException> commitTransaction(std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<void, DBException> abortTransaction(std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<bool, DBException> supportsTransactions(std::nothrow_t) noexcept override;
        expected<void, DBException> prepareForPoolReturn(std::nothrow_t) noexcept override;

        // DBConnectionPooled interface methods
        std::chrono::time_point<std::chrono::steady_clock> getCreationTime(std::nothrow_t) const noexcept override;
        std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime(std::nothrow_t) const noexcept override;
        expected<void, DBException> setActive(std::nothrow_t, bool active) noexcept override;
        bool isActive(std::nothrow_t) const noexcept override;

        // Implementation of DBConnectionPooled interface
        std::shared_ptr<DBConnection> getUnderlyingConnection(std::nothrow_t) noexcept override;
    };

    // Specialized connection pool for MongoDB
    namespace MongoDB
    {
        /**
         * @brief MongoDB-specific connection pool implementation
         *
         * This class extends the generic DocumentDBConnectionPool with MongoDB-specific
         * configuration and behaviors.
         */
        class MongoDBConnectionPool : public DocumentDBConnectionPool
        {
        public:
            // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
            MongoDBConnectionPool(ConstructorTag,
                                  const std::string &url,
                                  const std::string &username,
                                  const std::string &password);

            explicit MongoDBConnectionPool(ConstructorTag, const config::DBConnectionPoolConfig &config);

            static cpp_dbc::expected<std::shared_ptr<MongoDBConnectionPool>, DBException> create(std::nothrow_t,
                                                                                                 const std::string &url,
                                                                                                 const std::string &username,
                                                                                                 const std::string &password) noexcept;

            static cpp_dbc::expected<std::shared_ptr<MongoDBConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;
        };
    }

} // namespace cpp_dbc

#endif // CPP_DBC_DOCUMENT_DB_CONNECTION_POOL_HPP
