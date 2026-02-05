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
        std::shared_ptr<DocumentDBConnection> createDBConnection();

        // Creates a new pooled connection wrapper
        std::shared_ptr<DocumentPooledDBConnection> createPooledDBConnection();

        // Validates a connection
        bool validateConnection(std::shared_ptr<DocumentDBConnection> conn) const;

        // Returns a connection to the pool
        void returnConnection(std::shared_ptr<DocumentPooledDBConnection> conn);

        // Maintenance thread function
        void maintenanceTask();

        std::shared_ptr<DocumentPooledDBConnection> getIdleDBConnection();

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
        static std::shared_ptr<DocumentDBConnectionPool> create(const std::string &url,
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

        static std::shared_ptr<DocumentDBConnectionPool> create(const config::DBConnectionPoolConfig &config);

        ~DocumentDBConnectionPool() override;

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
        std::chrono::time_point<std::chrono::steady_clock> m_creationTime;
        std::chrono::time_point<std::chrono::steady_clock> m_lastUsedTime;
        std::atomic<bool> m_active{false};
        std::atomic<bool> m_closed{false};

        friend class DocumentDBConnectionPool;

        // Helper method to check if pool is still valid
        bool isPoolValid() const override;

    public:
        DocumentPooledDBConnection(std::shared_ptr<DocumentDBConnection> conn,
                                   std::weak_ptr<DocumentDBConnectionPool> pool,
                                   std::shared_ptr<std::atomic<bool>> poolAlive);
        ~DocumentPooledDBConnection() override;

        // Overridden DBConnection interface methods
        void close() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() override;
        std::string getURL() const override;

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

        // Nothrow versions - delegate to underlying connection
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
        expected<std::shared_ptr<DocumentDBData>, DBException> getServerInfo(std::nothrow_t) noexcept override;
        expected<std::shared_ptr<DocumentDBData>, DBException> getServerStatus(std::nothrow_t) noexcept override;

        // DBConnectionPooled interface methods
        std::chrono::time_point<std::chrono::steady_clock> getCreationTime() const override;
        std::chrono::time_point<std::chrono::steady_clock> getLastUsedTime() const override;
        void setActive(bool active) override;
        bool isActive() const override;

        // Implementation of DBConnectionPooled interface
        std::shared_ptr<DBConnection> getUnderlyingConnection() override;

        // DocumentPooledDBConnection specific method
        std::shared_ptr<DocumentDBConnection> getUnderlyingDocumentConnection();
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

            static std::shared_ptr<MongoDBConnectionPool> create(const std::string &url,
                                                                 const std::string &username,
                                                                 const std::string &password);

            static std::shared_ptr<MongoDBConnectionPool> create(const config::DBConnectionPoolConfig &config);
        };
    }

} // namespace cpp_dbc

#endif // CPP_DBC_DOCUMENT_DB_CONNECTION_POOL_HPP
