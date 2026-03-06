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
 * @brief Connection pool for document databases — thin wrapper around DBConnectionPoolBase
 *
 * All pool infrastructure (connection lifecycle, maintenance thread, direct handoff,
 * HikariCP validation skip, phase-based lock protocol) is inherited from
 * DBConnectionPoolBase. This class only provides:
 * - createPooledDBConnection() override (creates DocumentPooledDBConnection wrappers)
 * - getDocumentDBConnection() typed getter
 * - Factory methods
 */

#ifndef CPP_DBC_DOCUMENT_DB_CONNECTION_POOL_HPP
#define CPP_DBC_DOCUMENT_DB_CONNECTION_POOL_HPP

#include "../../cpp_dbc.hpp"
#include "cpp_dbc/pool/connection_pool.hpp"
#include "cpp_dbc/pool/pooled_db_connection_base.hpp"
#include "cpp_dbc/core/document/document_db_connection.hpp"

#include <chrono>
#include <atomic>

// Forward declaration of configuration classes
namespace cpp_dbc::config
{
    class DatabaseConfig;
    class DBConnectionPoolConfig;
} // namespace cpp_dbc::config

namespace cpp_dbc
{

    // Forward declaration
    class DocumentPooledDBConnection;

    /**
     * @brief Connection pool for document databases
     *
     * Thin derived class that overrides only createPooledDBConnection() to produce
     * DocumentPooledDBConnection wrappers, and adds the typed getter
     * getDocumentDBConnection().
     *
     * All pool infrastructure (acquisition, validation, maintenance, direct handoff)
     * is inherited from DBConnectionPoolBase.
     *
     * @see DBConnectionPoolBase, DocumentPooledDBConnection
     */
    class DocumentDBConnectionPool : public DBConnectionPoolBase
    {
    private:
        friend class DocumentPooledDBConnection;

        // Creates a physical document connection via DriverManager
        cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException> createDBConnection(std::nothrow_t) const noexcept;

        // Override from DBConnectionPoolBase — creates the document-specific pooled wrapper
        cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
            createPooledDBConnection(std::nothrow_t) noexcept override;

    public:
        // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
        DocumentDBConnectionPool(DBConnectionPool::ConstructorTag,
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
                                 TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED) noexcept;

        explicit DocumentDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept;

        ~DocumentDBConnectionPool() override;

        DocumentDBConnectionPool(const DocumentDBConnectionPool &) = delete;
        DocumentDBConnectionPool &operator=(const DocumentDBConnectionPool &) = delete;
        DocumentDBConnectionPool(DocumentDBConnectionPool &&) = delete;
        DocumentDBConnectionPool &operator=(DocumentDBConnectionPool &&) = delete;

        // Static factory methods
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
                                                                                                TransactionIsolationLevel transactionIsolation = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED) noexcept;

        static cpp_dbc::expected<std::shared_ptr<DocumentDBConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;

#ifdef __cpp_exceptions
        // Family-specific typed getter (throwing)
        virtual std::shared_ptr<DocumentDBConnection> getDocumentDBConnection();
#endif

        // Family-specific typed getter (nothrow)
        cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException> getDocumentDBConnection(std::nothrow_t) noexcept;
    };

    // DocumentPooledDBConnection wraps a physical document database connection.
    // Common pool logic (close, returnToPool, destructor, metadata) is in PooledDBConnectionBase via CRTP.
    class DocumentPooledDBConnection final
        : public PooledDBConnectionBase<DocumentPooledDBConnection, DocumentDBConnection, DocumentDBConnectionPool>
        , public DocumentDBConnection
        , public std::enable_shared_from_this<DocumentPooledDBConnection>
    {
    private:
        using Base = PooledDBConnectionBase<DocumentPooledDBConnection, DocumentDBConnection, DocumentDBConnectionPool>;
        friend class DocumentDBConnectionPool;

    protected:
        expected<void, DBException> prepareForPoolReturn(std::nothrow_t,
                                                         TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override
        {
            return this->prepareForPoolReturnImpl(std::nothrow, isolationLevel);
        }
        expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override
        {
            return this->prepareForBorrowImpl(std::nothrow);
        }

    public:
        DocumentPooledDBConnection(
            std::shared_ptr<DocumentDBConnection> connection,
            std::weak_ptr<DocumentDBConnectionPool> connectionPool,
            std::shared_ptr<std::atomic<bool>> poolAlive) noexcept;
        ~DocumentPooledDBConnection() override = default;

#ifdef __cpp_exceptions
        // ── Diamond-resolving throwing delegators ──
        void close() override { this->closeThrow(); }
        bool isClosed() const override { return this->isClosedThrow(); }
        void returnToPool() override { this->returnToPoolThrow(); }
        bool isPooled() const override { return this->isPooledThrow(); }
        std::string getURL() const override { return this->getURLThrow(); }
        void reset() override { this->resetThrow(); }
        bool ping() override { return this->pingThrow(); }

        // ── Document-specific throwing methods ──
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

        std::string startSession() override;
        void endSession(const std::string &sessionId) override;
        void startTransaction(const std::string &sessionId) override;
        void commitTransaction(const std::string &sessionId) override;
        void abortTransaction(const std::string &sessionId) override;
        bool supportsTransactions() override;
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

#endif // __cpp_exceptions
        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        // ── Diamond-resolving nothrow delegators ──
        expected<void, DBException> close(std::nothrow_t) noexcept override { return this->closeImpl(std::nothrow); }
        expected<void, DBException> reset(std::nothrow_t) noexcept override { return this->resetImpl(std::nothrow); }
        expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override { return this->isClosedImpl(std::nothrow); }
        expected<void, DBException> returnToPool(std::nothrow_t) noexcept override { return this->returnToPoolImpl(std::nothrow); }
        expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override { return this->isPooledImpl(std::nothrow); }
        expected<std::string, DBException> getURL(std::nothrow_t) const noexcept override { return this->getURLImpl(std::nothrow); }
        expected<bool, DBException> ping(std::nothrow_t) noexcept override { return this->pingImpl(std::nothrow); }

        // ── Document-specific nothrow methods ──
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
        expected<std::string, DBException> startSession(std::nothrow_t) noexcept override;
        expected<void, DBException> endSession(std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<void, DBException> startTransaction(std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<void, DBException> commitTransaction(std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<void, DBException> abortTransaction(std::nothrow_t, const std::string &sessionId) noexcept override;
        expected<bool, DBException> supportsTransactions(std::nothrow_t) noexcept override;
        expected<void, DBException>
        setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override;
        expected<TransactionIsolationLevel, DBException>
            getTransactionIsolation(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc

// Specialized connection pool for MongoDB
namespace cpp_dbc::MongoDB
{
    /**
     * @brief MongoDB-specific connection pool implementation
     */
    class MongoDBConnectionPool final : public DocumentDBConnectionPool
    {
    public:
        // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
        MongoDBConnectionPool(DBConnectionPool::ConstructorTag,
                              const std::string &url,
                              const std::string &username,
                              const std::string &password) noexcept;

        explicit MongoDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept;

        ~MongoDBConnectionPool() override = default;

        MongoDBConnectionPool(const MongoDBConnectionPool &) = delete;
        MongoDBConnectionPool &operator=(const MongoDBConnectionPool &) = delete;
        MongoDBConnectionPool(MongoDBConnectionPool &&) = delete;
        MongoDBConnectionPool &operator=(MongoDBConnectionPool &&) = delete;

#ifdef __cpp_exceptions
        // Throwing static factory methods
        static std::shared_ptr<MongoDBConnectionPool> create(const std::string &url,
                                                              const std::string &username,
                                                              const std::string &password);

        static std::shared_ptr<MongoDBConnectionPool> create(const config::DBConnectionPoolConfig &config);
#endif // __cpp_exceptions

        // Nothrow static factory methods
        static cpp_dbc::expected<std::shared_ptr<MongoDBConnectionPool>, DBException> create(std::nothrow_t,
                                                                                              const std::string &url,
                                                                                              const std::string &username,
                                                                                              const std::string &password) noexcept;

        static cpp_dbc::expected<std::shared_ptr<MongoDBConnectionPool>, DBException> create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept;
    };
} // namespace cpp_dbc::MongoDB

#endif // CPP_DBC_DOCUMENT_DB_CONNECTION_POOL_HPP
