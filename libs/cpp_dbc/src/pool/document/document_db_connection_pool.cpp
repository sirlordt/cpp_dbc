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
 * @file document_db_connection_pool.cpp
 * @brief Thin document pool — delegates all pool infrastructure to DBConnectionPoolBase.
 *
 * Only contains: constructors, createDBConnection, createPooledDBConnection,
 * getDocumentDBConnection, factory methods, and the document-specific
 * DocumentPooledDBConnection methods (getDatabaseName, getCollection, etc.).
 *
 * Common pooled-connection logic (close, returnToPool, destructor, pool metadata)
 * is inherited from PooledDBConnectionBase via CRTP.
 */

#include "cpp_dbc/pool/document/document_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "../connection_pool_internal.hpp"

namespace cpp_dbc
{
    // ── DocumentDBConnectionPool ──────────────────────────────────────────

    // ── Constructors (delegate to base) ──────────────────────────────────

    DocumentDBConnectionPool::DocumentDBConnectionPool(DBConnectionPool::ConstructorTag,
                                                       const std::string &url,
                                                       const std::string &username,
                                                       const std::string &password,
                                                       const std::map<std::string, std::string> &options,
                                                       size_t initialSize,
                                                       size_t maxSize,
                                                       size_t minIdle,
                                                       size_t maxWaitMillis,
                                                       size_t validationTimeoutMillis,
                                                       size_t idleTimeoutMillis,
                                                       size_t maxLifetimeMillis,
                                                       bool testOnBorrow,
                                                       bool testOnReturn,
                                                       TransactionIsolationLevel transactionIsolation) noexcept
        : DBConnectionPoolBase(DBConnectionPool::ConstructorTag{}, url, username, password, options,
                               initialSize, maxSize, minIdle, maxWaitMillis, validationTimeoutMillis,
                               idleTimeoutMillis, maxLifetimeMillis, testOnBorrow, testOnReturn,
                               transactionIsolation)
    {
        // All initialization handled by DBConnectionPoolBase
    }

    DocumentDBConnectionPool::DocumentDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : DBConnectionPoolBase(DBConnectionPool::ConstructorTag{}, config)
    {
        // All initialization handled by DBConnectionPoolBase
    }

    // ── Destructor ───────────────────────────────────────────────────────

    // DBConnectionPoolBase destructor handles close() and cleanup
    DocumentDBConnectionPool::~DocumentDBConnectionPool() = default;

    // ── Private helpers ──────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException>
    DocumentDBConnectionPool::createDBConnection(std::nothrow_t) const noexcept
    {
        auto dbConnResult = DriverManager::getDBConnection(std::nothrow, getUrl(), getUsername(), getPassword(), getOptions());
        if (!dbConnResult.has_value())
        {
            return cpp_dbc::unexpected(dbConnResult.error());
        }

        auto documentConn = std::dynamic_pointer_cast<DocumentDBConnection>(dbConnResult.value());
        if (!documentConn)
        {
            return cpp_dbc::unexpected(DBException("1EA1E853ED8F", "Connection pool only supports document database connections", system_utils::captureCallStack()));
        }
        return documentConn;
    }

    cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
    DocumentDBConnectionPool::createPooledDBConnection(std::nothrow_t) noexcept
    {
        auto connResult = createDBConnection(std::nothrow);
        if (!connResult.has_value())
        {
            return cpp_dbc::unexpected(connResult.error());
        }
        auto conn = connResult.value();

        // Set transaction isolation level on the new connection
        auto isoResult = conn->setTransactionIsolation(std::nothrow, getTransactionIsolation());
        if (!isoResult.has_value())
        {
            return cpp_dbc::unexpected(isoResult.error());
        }

        // Create pooled connection with weak_ptr to this pool.
        // shared_from_this() returns shared_ptr<DBConnectionPoolBase>; downcast to the concrete type
        // because DocumentPooledDBConnection stores weak_ptr<DocumentDBConnectionPool>.
        std::weak_ptr<DocumentDBConnectionPool> weakPool;
        try
        {
            weakPool = std::static_pointer_cast<DocumentDBConnectionPool>(shared_from_this());
        }
        catch (const std::bad_weak_ptr &ex)
        {
            // Pool not managed by shared_ptr — pooled connection would have no way to return to pool
            CP_DEBUG("DocumentDBConnectionPool::createPooledDBConnection - Pool not managed by shared_ptr: %s", ex.what());
            return cpp_dbc::unexpected(DBException("6ATHS3FZVDOD",
                                                   "Pool is not managed by shared_ptr, cannot create pooled connection",
                                                   system_utils::captureCallStack()));
        }

        return std::static_pointer_cast<DBConnectionPooled>(
            std::make_shared<DocumentPooledDBConnection>(conn, weakPool, getPoolAliveFlag()));
    }

    // ── Static factories ─────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnectionPool>, DBException> DocumentDBConnectionPool::create(std::nothrow_t,
                                                                                                               const std::string &url,
                                                                                                               const std::string &username,
                                                                                                               const std::string &password,
                                                                                                               const std::map<std::string, std::string> &options,
                                                                                                               size_t initialSize,
                                                                                                               size_t maxSize,
                                                                                                               size_t minIdle,
                                                                                                               size_t maxWaitMillis,
                                                                                                               size_t validationTimeoutMillis,
                                                                                                               size_t idleTimeoutMillis,
                                                                                                               size_t maxLifetimeMillis,
                                                                                                               bool testOnBorrow,
                                                                                                               bool testOnReturn,
                                                                                                               TransactionIsolationLevel transactionIsolation) noexcept
    {
        auto pool = std::make_shared<DocumentDBConnectionPool>(
            DBConnectionPool::ConstructorTag{}, url, username, password, options, initialSize, maxSize, minIdle,
            maxWaitMillis, validationTimeoutMillis, idleTimeoutMillis, maxLifetimeMillis,
            testOnBorrow, testOnReturn, transactionIsolation);

        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }

        return pool;
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnectionPool>, DBException> DocumentDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        auto pool = std::make_shared<DocumentDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }

        return pool;
    }

    // ── Family-specific getter ───────────────────────────────────────────

#ifdef __cpp_exceptions

    std::shared_ptr<DocumentDBConnection> DocumentDBConnectionPool::getDocumentDBConnection()
    {
        auto result = getDocumentDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException>
    DocumentDBConnectionPool::getDocumentDBConnection(std::nothrow_t) noexcept
    {
        auto result = acquireConnection(std::nothrow);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        // acquireConnection() returns shared_ptr<DBConnectionPooled>. The underlying object
        // is a DocumentPooledDBConnection which inherits from both DBConnectionPooled and
        // DocumentDBConnection. dynamic_pointer_cast navigates the diamond correctly.
        auto conn = std::dynamic_pointer_cast<DocumentDBConnection>(result.value());
        if (!conn)
        {
            return cpp_dbc::unexpected(DBException("SYDEA2804SA8",
                                                   "Failed to cast pooled connection to DocumentDBConnection",
                                                   system_utils::captureCallStack()));
        }
        return conn;
    }

    // ════════════════════════════════════════════════════════════════════════
    // DocumentPooledDBConnection implementation
    // ════════════════════════════════════════════════════════════════════════

    DocumentPooledDBConnection::DocumentPooledDBConnection(
        std::shared_ptr<DocumentDBConnection> connection,
        std::weak_ptr<DocumentDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive) noexcept
        : Base(std::move(connection), std::move(connectionPool), std::move(poolAlive))
    {
        // All members initialized by CRTP base
    }

    // ── Document-specific throwing methods ────────────────────────────────

#ifdef __cpp_exceptions

    std::string DocumentPooledDBConnection::getDatabaseName() const
    {
        auto result = getDatabaseName(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> DocumentPooledDBConnection::listDatabases()
    {
        auto result = listDatabases(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool DocumentPooledDBConnection::databaseExists(const std::string &databaseName)
    {
        auto result = databaseExists(std::nothrow, databaseName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void DocumentPooledDBConnection::useDatabase(const std::string &databaseName)
    {
        auto result = useDatabase(std::nothrow, databaseName);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void DocumentPooledDBConnection::dropDatabase(const std::string &databaseName)
    {
        auto result = dropDatabase(std::nothrow, databaseName);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::getCollection(const std::string &collectionName)
    {
        auto result = getCollection(std::nothrow, collectionName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::vector<std::string> DocumentPooledDBConnection::listCollections()
    {
        auto result = listCollections(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool DocumentPooledDBConnection::collectionExists(const std::string &collectionName)
    {
        auto result = collectionExists(std::nothrow, collectionName);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DocumentDBCollection> DocumentPooledDBConnection::createCollection(
        const std::string &collectionName, const std::string &options)
    {
        auto result = createCollection(std::nothrow, collectionName, options);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void DocumentPooledDBConnection::dropCollection(const std::string &collectionName)
    {
        auto result = dropCollection(std::nothrow, collectionName);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument()
    {
        auto result = createDocument(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::createDocument(const std::string &json)
    {
        auto result = createDocument(std::nothrow, json);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::runCommand(const std::string &command)
    {
        auto result = runCommand(std::nothrow, command);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerInfoAsDocument()
    {
        auto result = getServerInfoAsDocument(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerStatus()
    {
        auto result = getServerStatus(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string DocumentPooledDBConnection::startSession()
    {
        auto result = startSession(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void DocumentPooledDBConnection::endSession(const std::string &sessionId)
    {
        auto result = endSession(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void DocumentPooledDBConnection::startTransaction(const std::string &sessionId)
    {
        auto result = startTransaction(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void DocumentPooledDBConnection::commitTransaction(const std::string &sessionId)
    {
        auto result = commitTransaction(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void DocumentPooledDBConnection::abortTransaction(const std::string &sessionId)
    {
        auto result = abortTransaction(std::nothrow, sessionId);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool DocumentPooledDBConnection::supportsTransactions()
    {
        auto result = supportsTransactions(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void DocumentPooledDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel DocumentPooledDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    // ── Document-specific nothrow methods ─────────────────────────────────

    cpp_dbc::expected<void, DBException>
    DocumentPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("YXMF43OOV7WP", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->setTransactionIsolation(std::nothrow, level);
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    DocumentPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("3ZNBTS4N04WD", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getTransactionIsolation(std::nothrow);
    }

    cpp_dbc::expected<std::string, DBException> DocumentPooledDBConnection::getDatabaseName(std::nothrow_t) const noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("QI6DG2WF85Q2", "Connection is closed", system_utils::captureCallStack()));
        }
        return getConn(std::nothrow)->getDatabaseName(std::nothrow);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listDatabases(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("U6THWPYMYN4L", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->listDatabases(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::databaseExists(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("FABBX2QR62GT", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->databaseExists(std::nothrow, databaseName);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::useDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("CFZMARADNIPZ", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->useDatabase(std::nothrow, databaseName);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::dropDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("6C55O75AJHS5", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->dropDatabase(std::nothrow, databaseName);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::getCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("G5BB749801OM", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getCollection(std::nothrow, collectionName);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listCollections(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("5C1WAY0V1SOL", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->listCollections(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::collectionExists(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("AEDVQNDH5OBR", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->collectionExists(std::nothrow, collectionName);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::createCollection(
        std::nothrow_t,
        const std::string &collectionName,
        const std::string &options) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("ZU34XKPX5NXO", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->createCollection(std::nothrow, collectionName, options);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::dropCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("7239S0NFTH6X", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->dropCollection(std::nothrow, collectionName);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("92MQ8YF1833Z", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->createDocument(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(
        std::nothrow_t, const std::string &json) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("X02O1QY16VJ2", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->createDocument(std::nothrow, json);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::runCommand(
        std::nothrow_t, const std::string &command) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("GYIGUFYPTNB0", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->runCommand(std::nothrow, command);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerInfoAsDocument(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("VC47QN01W5WH", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getServerInfoAsDocument(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerStatus(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("737UOJCBRKJ0", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->getServerStatus(std::nothrow);
    }

    cpp_dbc::expected<std::string, DBException> DocumentPooledDBConnection::startSession(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("WTWINKFWO8WZ", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->startSession(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::endSession(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("X7VNI3HTM5NN", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->endSession(std::nothrow, sessionId);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::startTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("YYJXBAMEI07O", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->startTransaction(std::nothrow, sessionId);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::commitTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("VQ2C4Y9YU1LX", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->commitTransaction(std::nothrow, sessionId);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::abortTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("A7VGP008305O", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->abortTransaction(std::nothrow, sessionId);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::supportsTransactions(std::nothrow_t) noexcept
    {
        if (isLocalClosed(std::nothrow))
        {
            return cpp_dbc::unexpected(DBException("PLV8XFKGJ0PO", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return getConn(std::nothrow)->supportsTransactions(std::nothrow);
    }

} // namespace cpp_dbc

// ── MongoDB connection pool implementation ───────────────────────────────

namespace cpp_dbc::MongoDB
{
    MongoDBConnectionPool::MongoDBConnectionPool(DBConnectionPool::ConstructorTag tag,
                                                 const std::string &url,
                                                 const std::string &username,
                                                 const std::string &password) noexcept
        : DocumentDBConnectionPool(tag, url, username, password)
    {
        // MongoDB-specific initialization if needed
    }

    MongoDBConnectionPool::MongoDBConnectionPool(DBConnectionPool::ConstructorTag tag, const config::DBConnectionPoolConfig &config) noexcept
        : DocumentDBConnectionPool(tag, config)
    {
        // MongoDB-specific initialization if needed
    }

#ifdef __cpp_exceptions

    std::shared_ptr<MongoDBConnectionPool> MongoDBConnectionPool::create(const std::string &url,
                                                                          const std::string &username,
                                                                          const std::string &password)
    {
        auto result = create(std::nothrow, url, username, password);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<MongoDBConnectionPool> MongoDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto result = create(std::nothrow, config);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<MongoDBConnectionPool>, DBException> MongoDBConnectionPool::create(std::nothrow_t,
                                                                                                          const std::string &url,
                                                                                                          const std::string &username,
                                                                                                          const std::string &password) noexcept
    {
        auto pool = std::make_shared<MongoDBConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return pool;
    }

    cpp_dbc::expected<std::shared_ptr<MongoDBConnectionPool>, DBException> MongoDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        auto pool = std::make_shared<MongoDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return pool;
    }

} // namespace cpp_dbc::MongoDB
