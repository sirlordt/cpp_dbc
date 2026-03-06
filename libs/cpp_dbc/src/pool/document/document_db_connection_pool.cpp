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
 * getDocumentDBConnection, factory methods, and the full
 * DocumentPooledDBConnection + MongoDB subclass implementations.
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
                                                       int initialSize,
                                                       int maxSize,
                                                       int minIdle,
                                                       long maxWaitMillis,
                                                       long validationTimeoutMillis,
                                                       long idleTimeoutMillis,
                                                       long maxLifetimeMillis,
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

    DocumentDBConnectionPool::~DocumentDBConnectionPool()
    {
        // DBConnectionPoolBase destructor handles close() and cleanup
    }

    // ── Private helpers ──────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnection>, DBException>
    DocumentDBConnectionPool::createDBConnection(std::nothrow_t) noexcept
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
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("DocumentDBConnectionPool::createPooledDBConnection - Pool not managed by shared_ptr: %s", ex.what());
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
                                                                                                               int initialSize,
                                                                                                               int maxSize,
                                                                                                               int minIdle,
                                                                                                               long maxWaitMillis,
                                                                                                               long validationTimeoutMillis,
                                                                                                               long idleTimeoutMillis,
                                                                                                               long maxLifetimeMillis,
                                                                                                               bool testOnBorrow,
                                                                                                               bool testOnReturn,
                                                                                                               TransactionIsolationLevel transactionIsolation) noexcept
    {
        try
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
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("TSH2S7H56QKI", "Failed to create connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("CQ827ZOIDL0H", "Failed to create connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBConnectionPool>, DBException> DocumentDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        try
        {
            auto pool = std::make_shared<DocumentDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }

            return pool;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("TSH2S7H56QKI", "Failed to create connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("CQ827ZOIDL0H", "Failed to create connection pool: unknown error", system_utils::captureCallStack()));
        }
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
        return std::dynamic_pointer_cast<DocumentDBConnection>(result.value());
    }

    // ════════════════════════════════════════════════════════════════════════
    // DocumentPooledDBConnection implementation
    // ════════════════════════════════════════════════════════════════════════

    DocumentPooledDBConnection::DocumentPooledDBConnection(
        std::shared_ptr<DocumentDBConnection> connection,
        std::weak_ptr<DocumentDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive) noexcept
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive)
    {
        // m_creationTime, m_lastUsedTimeNs, m_active, m_closed use in-class initializers
    }

    bool DocumentPooledDBConnection::isPoolValid(std::nothrow_t) const noexcept
    {
        return m_poolAlive && m_poolAlive->load(std::memory_order_acquire) && !m_pool.expired();
    }

    DocumentPooledDBConnection::~DocumentPooledDBConnection()
    {
        if (!m_closed.load(std::memory_order_acquire) && m_conn)
        {
            // CRITICAL: Never call returnToPool() or shared_from_this() from destructor.
            // When the destructor runs, refcount is already 0, so shared_from_this()
            // throws bad_weak_ptr. Instead, do direct cleanup:
            // 1. Decrement active counter if this connection was active
            // 2. Close the underlying physical connection
            if (m_active.load(std::memory_order_acquire))
            {
                if (auto pool = m_pool.lock())
                {
                    pool->decrementActiveCount(std::nothrow);
                }
                m_active.store(false, std::memory_order_release);
            }

            auto closeResult = m_conn->close(std::nothrow);
            if (!closeResult.has_value())
            {
                CP_DEBUG("DocumentPooledDBConnection::~DocumentPooledDBConnection - close failed: %s", closeResult.error().what_s().data());
            }
        }
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::close(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the close
        bool expected = false;
        if (m_closed.compare_exchange_strong(expected, true))
        {
            try
            {
                // Return to pool instead of actually closing
                updateLastUsedTime(std::nothrow);

                // Check if pool is still alive using the shared atomic flag
                if (isPoolValid(std::nothrow))
                {
                    // Try to obtain a shared_ptr from the weak_ptr
                    if (auto poolShared = m_pool.lock())
                    {
                        // ============================================================================
                        // CRITICAL FIX: Race Condition in Connection Pool Return Flow
                        // ============================================================================
                        //
                        // BUG DESCRIPTION:
                        // ----------------
                        // A race condition existed where m_closed was reset to false AFTER
                        // returnConnection() completed. This created a window where another thread
                        // could obtain the connection from the idle queue while m_closed was still
                        // true, causing spurious "Connection is closed" errors.
                        //
                        // SEQUENCE OF EVENTS (BUG):
                        // -------------------------
                        // Thread A (returning connection):
                        //   1. close() sets m_closed = true (compare_exchange)
                        //   2. returnConnection() is called
                        //   3. Inside returnConnection():
                        //      - LOCK(m_mutexPool)
                        //      - m_idleConnections.push(conn)  <-- Connection available with m_closed=TRUE
                        //      - UNLOCK(m_mutexPool) <-- Race window opens here
                        //   4. m_closed.store(false)  <-- Too late if Thread B already got the connection
                        //
                        // Thread B (acquiring connection) - executes between steps 3 and 4:
                        //   1. getDocumentDBConnection() acquires m_mutexPool
                        //   2. Pops connection from idle queue (m_closed is still TRUE)
                        //   3. Returns connection to caller
                        //   4. Caller calls ping() -> sees m_closed=true -> throws "Connection is closed"
                        //
                        // FIX:
                        // ----
                        // Reset m_closed to false BEFORE calling returnConnection(). This ensures
                        // that when the connection becomes available in the idle queue, m_closed
                        // is already false and any thread that obtains it will see the correct state.
                        //
                        // If returnConnection() fails or throws an exception, the catch blocks below
                        // will close the underlying connection, maintaining correct error handling.
                        // ============================================================================
                        m_closed.store(false, std::memory_order_release);
                        auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<DBConnectionPooled>(shared_from_this()));
                        if (!returnResult.has_value())
                        {
                            CP_DEBUG("DocumentPooledDBConnection::close - returnConnection failed: %s", returnResult.error().what_s().data());
                            return returnResult;
                        }
                    }
                }
                else if (m_conn)
                {
                    // If pool is invalid, actually close the connection
                    auto result = m_conn->close(std::nothrow);
                    if (!result.has_value())
                    {
                        CP_DEBUG("DocumentPooledDBConnection::close - Failed to close underlying connection: %s", result.error().what_s().data());
                        return result;
                    }
                }

                return {};
            }
            catch (const std::bad_weak_ptr &ex)
            {
                // shared_from_this failed, just close the connection
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    auto result = m_conn->close(std::nothrow);
                    if (!result.has_value())
                    {
                        return result;
                    }
                }
                return cpp_dbc::unexpected(DBException("B6ZEXDIEMWV7",
                                                        "Failed to obtain shared_from_this: " + std::string(ex.what()),
                                                        system_utils::captureCallStack()));
            }
            catch (const DBException &ex)
            {
                // DBException from underlying connection, just return it
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(ex);
            }
            catch (const std::exception &ex)
            {
                // Any other exception, close the connection and return error
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("8XN84Y8GGSQU",
                                                        "Exception in close: " + std::string(ex.what()),
                                                        system_utils::captureCallStack()));
            }
            catch (...) // NOSONAR - Catch-all to ensure m_closed is always set correctly on any exception
            {
                CP_DEBUG("DocumentPooledDBConnection::close - Unknown exception caught");
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("KK3USJBXJPR1",
                                                        "Unknown exception in close",
                                                        system_utils::captureCallStack()));
            }
        }

        return {};
    }

#ifdef __cpp_exceptions

    void DocumentPooledDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool DocumentPooledDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return true;
        }
        return m_conn->isClosed(std::nothrow);
    }

    std::chrono::time_point<std::chrono::steady_clock> DocumentPooledDBConnection::getCreationTime(std::nothrow_t) const noexcept
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> DocumentPooledDBConnection::getLastUsedTime(std::nothrow_t) const noexcept
    {
        return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{m_lastUsedTimeNs.load(std::memory_order_acquire)}};
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::setActive(std::nothrow_t, bool isActive) noexcept
    {
        m_active.store(isActive, std::memory_order_release);
        return {};
    }

    bool DocumentPooledDBConnection::isActive(std::nothrow_t) const noexcept
    {
        return m_active.load(std::memory_order_acquire);
    }

    void DocumentPooledDBConnection::markPoolClosed(std::nothrow_t, bool closed) noexcept
    {
        m_closed.store(closed, std::memory_order_release);
    }

    bool DocumentPooledDBConnection::isPoolClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_acquire);
    }

    void DocumentPooledDBConnection::updateLastUsedTime(std::nothrow_t) noexcept
    {
        m_lastUsedTimeNs.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the return
        bool expected = false;
        if (!m_closed.compare_exchange_strong(expected, true))
        {
            return {};
        }

        try
        {
            // Return to pool instead of actually closing
            updateLastUsedTime(std::nothrow);

            // Check if pool is still alive using the shared atomic flag
            // Use qualified call to avoid virtual dispatch issues when called from destructor
            if (DocumentPooledDBConnection::isPoolValid(std::nothrow))
            {
                // Try to obtain a shared_ptr from the weak_ptr
                if (auto poolShared = m_pool.lock())
                {
                    // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                    // (see close(nothrow) method for full explanation of the bug)
                    m_closed.store(false, std::memory_order_release);
                    auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<DBConnectionPooled>(this->shared_from_this()));
                    if (!returnResult.has_value())
                    {
                        CP_DEBUG("DocumentPooledDBConnection::returnToPool - returnConnection failed: %s", returnResult.error().what_s().data());
                        return returnResult;
                    }
                }
            }
            return {};
        }
        catch (const std::exception &ex)
        {
            // Only shared_from_this() can throw here (std::bad_weak_ptr).
            // All other inner calls are nothrow. Close the connection and return error.
            CP_DEBUG("DocumentPooledDBConnection::returnToPool - shared_from_this failed: %s", ex.what());
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("JS9EDG2CRO9T",
                                                   "Exception in returnToPool: " + std::string(ex.what()),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions
        {
            CP_DEBUG("DocumentPooledDBConnection::returnToPool - Unknown exception caught");
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("O7RKPD89YPAW",
                                                   "Unknown exception in returnToPool",
                                                   system_utils::captureCallStack()));
        }
    }

#ifdef __cpp_exceptions

    void DocumentPooledDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool DocumentPooledDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string DocumentPooledDBConnection::getURL() const
    {
        auto result = getURL(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void DocumentPooledDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
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

    // Implementation of DocumentDBConnection interface methods
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

    std::shared_ptr<DocumentDBData> DocumentPooledDBConnection::getServerInfo()
    {
        auto result = getServerInfo(std::nothrow);
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

    bool DocumentPooledDBConnection::ping()
    {
        auto result = ping(std::nothrow);
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

#endif // __cpp_exceptions

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return !m_active.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<std::string, DBException> DocumentPooledDBConnection::getURL(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("KV03UAMVXQX7", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::reset(std::nothrow_t) noexcept
    {
        if (!m_conn)
        {
            return cpp_dbc::unexpected(DBException("0CUYVUE519T6",
                                                    "Underlying connection is null",
                                                    system_utils::captureCallStack()));
        }

        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("DVVC2P4912ZE",
                                                    "Connection is closed",
                                                    system_utils::captureCallStack()));
        }

        updateLastUsedTime(std::nothrow);
        return m_conn->reset(std::nothrow);
    }

    cpp_dbc::expected<void, DBException>
    DocumentPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("YXMF43OOV7WP", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->setTransactionIsolation(std::nothrow, level);
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    DocumentPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("3ZNBTS4N04WD", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getTransactionIsolation(std::nothrow);
    }

    cpp_dbc::expected<void, DBException>
    DocumentPooledDBConnection::prepareForPoolReturn(std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("IHKK4HM07XXA",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForPoolReturn(std::nothrow, isolationLevel);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("LRG1PLMORCRX",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForBorrow(std::nothrow);
    }

    std::shared_ptr<DBConnection> DocumentPooledDBConnection::getUnderlyingConnection(std::nothrow_t) noexcept
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

    // ====================================================================
    // NOTHROW VERSIONS - delegate to underlying connection
    // ====================================================================

    cpp_dbc::expected<std::string, DBException> DocumentPooledDBConnection::getDatabaseName(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("QI6DG2WF85Q2", "Connection is closed", system_utils::captureCallStack()));
        }
        return m_conn->getDatabaseName(std::nothrow);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listDatabases(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("U6THWPYMYN4L", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->listDatabases(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::databaseExists(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("FABBX2QR62GT", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->databaseExists(std::nothrow, databaseName);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::useDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("CFZMARADNIPZ", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->useDatabase(std::nothrow, databaseName);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::dropDatabase(
        std::nothrow_t, const std::string &databaseName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("6C55O75AJHS5", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->dropDatabase(std::nothrow, databaseName);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::getCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("G5BB749801OM", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getCollection(std::nothrow, collectionName);
    }

    cpp_dbc::expected<std::vector<std::string>, DBException> DocumentPooledDBConnection::listCollections(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("5C1WAY0V1SOL", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->listCollections(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::collectionExists(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("AEDVQNDH5OBR", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->collectionExists(std::nothrow, collectionName);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBCollection>, DBException> DocumentPooledDBConnection::createCollection(
        std::nothrow_t,
        const std::string &collectionName,
        const std::string &options) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("ZU34XKPX5NXO", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createCollection(std::nothrow, collectionName, options);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::dropCollection(
        std::nothrow_t, const std::string &collectionName) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("7239S0NFTH6X", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->dropCollection(std::nothrow, collectionName);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("92MQ8YF1833Z", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createDocument(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::createDocument(
        std::nothrow_t, const std::string &json) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("X02O1QY16VJ2", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->createDocument(std::nothrow, json);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::runCommand(
        std::nothrow_t, const std::string &command) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("GYIGUFYPTNB0", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->runCommand(std::nothrow, command);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerInfo(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("VC47QN01W5WH", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerInfo(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<DocumentDBData>, DBException> DocumentPooledDBConnection::getServerStatus(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("737UOJCBRKJ0", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getServerStatus(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::ping(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("218DK7FXM1U4", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->ping(std::nothrow);
    }

    cpp_dbc::expected<std::string, DBException> DocumentPooledDBConnection::startSession(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("WTWINKFWO8WZ", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->startSession(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::endSession(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("X7VNI3HTM5NN", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->endSession(std::nothrow, sessionId);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::startTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("YYJXBAMEI07O", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->startTransaction(std::nothrow, sessionId);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::commitTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("VQ2C4Y9YU1LX", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->commitTransaction(std::nothrow, sessionId);
    }

    cpp_dbc::expected<void, DBException> DocumentPooledDBConnection::abortTransaction(
        std::nothrow_t, const std::string &sessionId) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("A7VGP008305O", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->abortTransaction(std::nothrow, sessionId);
    }

    cpp_dbc::expected<bool, DBException> DocumentPooledDBConnection::supportsTransactions(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("PLV8XFKGJ0PO", "Connection is closed", system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->supportsTransactions(std::nothrow);
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
        try
        {
            auto pool = std::make_shared<MongoDBConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("WBORKFD9SLKV", "Failed to create MongoDB connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("1YU3JTHO2WBL", "Failed to create MongoDB connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<MongoDBConnectionPool>, DBException> MongoDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        try
        {
            auto pool = std::make_shared<MongoDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }
            return pool;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("WBORKFD9SLKV", "Failed to create MongoDB connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("1YU3JTHO2WBL", "Failed to create MongoDB connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

} // namespace cpp_dbc::MongoDB
