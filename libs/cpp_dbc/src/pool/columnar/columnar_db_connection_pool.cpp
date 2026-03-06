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
 * @file columnar_db_connection_pool.cpp
 * @brief Thin columnar pool — delegates all pool infrastructure to DBConnectionPoolBase.
 *
 * Only contains: constructors, createDBConnection, createPooledDBConnection,
 * getColumnarDBConnection, factory methods, and the full
 * ColumnarPooledDBConnection + ScyllaDB subclass implementations.
 */

#include "cpp_dbc/pool/columnar/columnar_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include "cpp_dbc/common/system_utils.hpp"
#include "../connection_pool_internal.hpp"

namespace cpp_dbc
{
    // ── ColumnarDBConnectionPool ─────────────────────────────────────────────

    // ── Constructors (delegate to base) ──────────────────────────────────────

    ColumnarDBConnectionPool::ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag,
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

    ColumnarDBConnectionPool::ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : DBConnectionPoolBase(DBConnectionPool::ConstructorTag{}, config)
    {
        // All initialization handled by DBConnectionPoolBase
    }

    // ── Destructor ───────────────────────────────────────────────────────────

    ColumnarDBConnectionPool::~ColumnarDBConnectionPool()
    {
        // DBConnectionPoolBase destructor handles close() and cleanup
    }

    // ── Private helpers ──────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException>
    ColumnarDBConnectionPool::createDBConnection(std::nothrow_t) noexcept
    {
        auto dbConnResult = DriverManager::getDBConnection(std::nothrow, getUrl(), getUsername(), getPassword(), getOptions());
        if (!dbConnResult.has_value())
        {
            return cpp_dbc::unexpected(dbConnResult.error());
        }

        auto columnarConn = std::dynamic_pointer_cast<ColumnarDBConnection>(dbConnResult.value());
        if (!columnarConn)
        {
            return cpp_dbc::unexpected(DBException("TP7X7T9LHSDV", "Connection pool only supports columnar database connections", system_utils::captureCallStack()));
        }
        return columnarConn;
    }

    cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
    ColumnarDBConnectionPool::createPooledDBConnection(std::nothrow_t) noexcept
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
        // because ColumnarPooledDBConnection stores weak_ptr<ColumnarDBConnectionPool>.
        std::weak_ptr<ColumnarDBConnectionPool> weakPool;
        try
        {
            weakPool = std::static_pointer_cast<ColumnarDBConnectionPool>(shared_from_this());
        }
        catch ([[maybe_unused]] const std::bad_weak_ptr &ex)
        {
            CP_DEBUG("ColumnarDBConnectionPool::createPooledDBConnection - Pool not managed by shared_ptr: %s", ex.what());
        }

        return std::static_pointer_cast<DBConnectionPooled>(
            std::make_shared<ColumnarPooledDBConnection>(conn, weakPool, getPoolAliveFlag()));
    }

    // ── Static factories ─────────────────────────────────────────────────────

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnectionPool>, DBException> ColumnarDBConnectionPool::create(std::nothrow_t,
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
            auto pool = std::make_shared<ColumnarDBConnectionPool>(
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
            return cpp_dbc::unexpected(DBException("B52CIJ0FMNQL", "Failed to create connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("MYF6RTSKIFDW", "Failed to create connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnectionPool>, DBException> ColumnarDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        try
        {
            auto pool = std::make_shared<ColumnarDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);

            auto initResult = pool->initializePool(std::nothrow);
            if (!initResult.has_value())
            {
                return cpp_dbc::unexpected(initResult.error());
            }

            return pool;
        }
        catch (const std::exception &ex)
        {
            return cpp_dbc::unexpected(DBException("B52CIJ0FMNQL", "Failed to create connection pool: " + std::string(ex.what()), system_utils::captureCallStack()));
        }
        catch (...)
        {
            return cpp_dbc::unexpected(DBException("MYF6RTSKIFDW", "Failed to create connection pool: unknown error", system_utils::captureCallStack()));
        }
    }

    // ── Family-specific getter ───────────────────────────────────────────────

#ifdef __cpp_exceptions

    std::shared_ptr<ColumnarDBConnection> ColumnarDBConnectionPool::getColumnarDBConnection()
    {
        auto result = getColumnarDBConnection(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException>
    ColumnarDBConnectionPool::getColumnarDBConnection(std::nothrow_t) noexcept
    {
        auto result = acquireConnection(std::nothrow);
        if (!result.has_value())
        {
            return cpp_dbc::unexpected(result.error());
        }
        // acquireConnection() returns shared_ptr<DBConnectionPooled>. The underlying object
        // is a ColumnarPooledDBConnection which inherits from both DBConnectionPooled and
        // ColumnarDBConnection. dynamic_pointer_cast navigates the diamond correctly.
        return std::dynamic_pointer_cast<ColumnarDBConnection>(result.value());
    }

    // ════════════════════════════════════════════════════════════════════════
    // ColumnarPooledDBConnection implementation
    // ════════════════════════════════════════════════════════════════════════

    ColumnarPooledDBConnection::ColumnarPooledDBConnection(
        std::shared_ptr<ColumnarDBConnection> connection,
        std::weak_ptr<ColumnarDBConnectionPool> connectionPool,
        std::shared_ptr<std::atomic<bool>> poolAlive) noexcept
        : m_conn(connection), m_pool(connectionPool), m_poolAlive(poolAlive)
    {
        // m_creationTime and m_lastUsedTimeNs use in-class initializers
    }

    bool ColumnarPooledDBConnection::isPoolValid(std::nothrow_t) const noexcept
    {
        return m_poolAlive && m_poolAlive->load(std::memory_order_acquire) && !m_pool.expired();
    }

    ColumnarPooledDBConnection::~ColumnarPooledDBConnection()
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
                CP_DEBUG("ColumnarPooledDBConnection::~ColumnarPooledDBConnection - close failed: %s", closeResult.error().what_s().data());
            }
        }
    }

#ifdef __cpp_exceptions

    void ColumnarPooledDBConnection::close()
    {
        auto result = close(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool ColumnarPooledDBConnection::isClosed() const
    {
        auto result = isClosed(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void ColumnarPooledDBConnection::returnToPool()
    {
        auto result = returnToPool(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool ColumnarPooledDBConnection::isPooled() const
    {
        auto result = isPooled(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::string ColumnarPooledDBConnection::getURL() const
    {
        auto result = getURL(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void ColumnarPooledDBConnection::reset()
    {
        auto result = reset(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    bool ColumnarPooledDBConnection::ping()
    {
        auto result = ping(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<ColumnarDBPreparedStatement> ColumnarPooledDBConnection::prepareStatement(const std::string &query)
    {
        auto result = prepareStatement(std::nothrow, query);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    std::shared_ptr<ColumnarDBResultSet> ColumnarPooledDBConnection::executeQuery(const std::string &query)
    {
        auto result = executeQuery(std::nothrow, query);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    uint64_t ColumnarPooledDBConnection::executeUpdate(const std::string &query)
    {
        auto result = executeUpdate(std::nothrow, query);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    bool ColumnarPooledDBConnection::beginTransaction()
    {
        auto result = beginTransaction(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

    void ColumnarPooledDBConnection::commit()
    {
        auto result = commit(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ColumnarPooledDBConnection::rollback()
    {
        auto result = rollback(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    void ColumnarPooledDBConnection::setTransactionIsolation(TransactionIsolationLevel level)
    {
        auto result = setTransactionIsolation(std::nothrow, level);
        if (!result.has_value())
        {
            throw result.error();
        }
    }

    TransactionIsolationLevel ColumnarPooledDBConnection::getTransactionIsolation()
    {
        auto result = getTransactionIsolation(std::nothrow);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::close(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the close
        bool expected = false;
        if (m_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
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
                        auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<ColumnarPooledDBConnection>(shared_from_this()));
                        if (!returnResult.has_value())
                        {
                            CP_DEBUG("ColumnarPooledDBConnection::close - returnConnection failed: %s", returnResult.error().what_s().data());
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
                        CP_DEBUG("ColumnarPooledDBConnection::close - Failed to close underlying connection: %s", result.error().what_s().data());
                        return result;
                    }
                }

                return {};
            }
            catch (const std::exception &ex)
            {
                // Only shared_from_this() can throw here (std::bad_weak_ptr).
                // All other inner calls are nothrow. Close the connection and return error.
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("6J52BKYDQQOM",
                                                       "Exception in close: " + std::string(ex.what()),
                                                       system_utils::captureCallStack()));
            }
            catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions
            {
                CP_DEBUG("ColumnarPooledDBConnection::close - Unknown exception caught");
                m_closed.store(true, std::memory_order_release);
                if (m_conn)
                {
                    m_conn->close(std::nothrow);
                }
                return cpp_dbc::unexpected(DBException("UQQSB03ZV8CR",
                                                       "Unknown exception in close",
                                                       system_utils::captureCallStack()));
            }
        }

        return {};
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::reset(std::nothrow_t) noexcept
    {
        if (!m_conn)
        {
            return cpp_dbc::unexpected(DBException("W9QZE5CW0T1C",
                                                   "Underlying connection is null",
                                                   system_utils::captureCallStack()));
        }

        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("GCMQQLPI0A3Y",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }

        updateLastUsedTime(std::nothrow);
        return m_conn->reset(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::isClosed(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return true;
        }
        return m_conn->isClosed(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::returnToPool(std::nothrow_t) noexcept
    {
        // Use atomic exchange to ensure only one thread processes the return
        bool expected = false;
        if (!m_closed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return {};
        }

        try
        {
            // Return to pool instead of actually closing
            updateLastUsedTime(std::nothrow);

            // Check if pool is still alive using the shared atomic flag
            // Use qualified call to avoid virtual dispatch issues when called from destructor
            if (ColumnarPooledDBConnection::isPoolValid(std::nothrow))
            {
                // Try to obtain a shared_ptr from the weak_ptr
                if (auto poolShared = m_pool.lock())
                {
                    // FIX: Reset m_closed BEFORE returnConnection() to prevent race condition
                    // (see close(nothrow) method for full explanation of the bug)
                    m_closed.store(false, std::memory_order_release);
                    auto returnResult = poolShared->returnConnection(std::nothrow, std::static_pointer_cast<ColumnarPooledDBConnection>(this->shared_from_this()));
                    if (!returnResult.has_value())
                    {
                        CP_DEBUG("ColumnarPooledDBConnection::returnToPool - returnConnection failed: %s", returnResult.error().what_s().data());
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
            CP_DEBUG("ColumnarPooledDBConnection::returnToPool - shared_from_this failed: %s", ex.what());
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("6OOO8USCUNVM",
                                                   "Exception in returnToPool: " + std::string(ex.what()),
                                                   system_utils::captureCallStack()));
        }
        catch (...) // NOSONAR(cpp:S2738) — fallback for non-std exceptions
        {
            CP_DEBUG("ColumnarPooledDBConnection::returnToPool - Unknown exception caught");
            m_closed.store(true, std::memory_order_release);
            if (m_conn)
            {
                m_conn->close(std::nothrow);
            }
            return cpp_dbc::unexpected(DBException("C5TI8P7TLY82",
                                                   "Unknown exception in returnToPool",
                                                   system_utils::captureCallStack()));
        }
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::isPooled(std::nothrow_t) const noexcept
    {
        return !m_active.load(std::memory_order_acquire);
    }

    cpp_dbc::expected<std::string, DBException> ColumnarPooledDBConnection::getURL(std::nothrow_t) const noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("2I2FYW64DQJV",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->getURL(std::nothrow);
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::ping(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("FMPOXQZ16S31",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->ping(std::nothrow);
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> ColumnarPooledDBConnection::prepareStatement(std::nothrow_t, const std::string &query) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("T3SWMWQ4CMKE",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->prepareStatement(std::nothrow, query);
    }

    cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> ColumnarPooledDBConnection::executeQuery(std::nothrow_t, const std::string &query) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("PTUV21WCDWB7",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->executeQuery(std::nothrow, query);
    }

    cpp_dbc::expected<uint64_t, DBException> ColumnarPooledDBConnection::executeUpdate(std::nothrow_t, const std::string &query) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("5OY4J9KQ4RWE",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->executeUpdate(std::nothrow, query);
    }

    cpp_dbc::expected<bool, DBException> ColumnarPooledDBConnection::beginTransaction(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("XJ11KQTI03DD",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->beginTransaction(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::commit(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("N7OGZJNMWCQ4",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->commit(std::nothrow);
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::rollback(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("PEY7OGNBFRQE",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->rollback(std::nothrow);
    }

    cpp_dbc::expected<void, DBException>
    ColumnarPooledDBConnection::setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("WQQNTF57OIB3",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->setTransactionIsolation(std::nothrow, level);
    }

    cpp_dbc::expected<TransactionIsolationLevel, DBException>
    ColumnarPooledDBConnection::getTransactionIsolation(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("RL7UNKR8KXVW",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        updateLastUsedTime(std::nothrow);
        return m_conn->getTransactionIsolation(std::nothrow);
    }

    std::chrono::time_point<std::chrono::steady_clock> ColumnarPooledDBConnection::getCreationTime(std::nothrow_t) const noexcept
    {
        return m_creationTime;
    }

    std::chrono::time_point<std::chrono::steady_clock> ColumnarPooledDBConnection::getLastUsedTime(std::nothrow_t) const noexcept
    {
        return std::chrono::steady_clock::time_point{std::chrono::nanoseconds{m_lastUsedTimeNs.load(std::memory_order_acquire)}};
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::setActive(std::nothrow_t, bool isActive) noexcept
    {
        m_active.store(isActive, std::memory_order_release);
        return {};
    }

    bool ColumnarPooledDBConnection::isActive(std::nothrow_t) const noexcept
    {
        return m_active.load(std::memory_order_acquire);
    }

    void ColumnarPooledDBConnection::markPoolClosed(std::nothrow_t, bool closed) noexcept
    {
        m_closed.store(closed, std::memory_order_release);
    }

    bool ColumnarPooledDBConnection::isPoolClosed(std::nothrow_t) const noexcept
    {
        return m_closed.load(std::memory_order_acquire);
    }

    void ColumnarPooledDBConnection::updateLastUsedTime(std::nothrow_t) noexcept
    {
        m_lastUsedTimeNs.store(std::chrono::steady_clock::now().time_since_epoch().count(), std::memory_order_release);
    }

    std::shared_ptr<DBConnection> ColumnarPooledDBConnection::getUnderlyingConnection(std::nothrow_t) noexcept
    {
        return std::static_pointer_cast<DBConnection>(m_conn);
    }

    cpp_dbc::expected<void, DBException>
    ColumnarPooledDBConnection::prepareForPoolReturn(std::nothrow_t, TransactionIsolationLevel isolationLevel) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("ERB7A9KN4OZ9",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForPoolReturn(std::nothrow, isolationLevel);
    }

    cpp_dbc::expected<void, DBException> ColumnarPooledDBConnection::prepareForBorrow(std::nothrow_t) noexcept
    {
        if (m_closed.load(std::memory_order_acquire))
        {
            return cpp_dbc::unexpected(DBException("FQYTSNOGDGZE",
                                                   "Connection is closed",
                                                   system_utils::captureCallStack()));
        }
        return m_conn->prepareForBorrow(std::nothrow);
    }

} // namespace cpp_dbc

// ScyllaDB connection pool implementation
namespace cpp_dbc::ScyllaDB
{
    ScyllaDBConnectionPool::ScyllaDBConnectionPool(DBConnectionPool::ConstructorTag,
                                                   const std::string &url,
                                                   const std::string &username,
                                                   const std::string &password) noexcept
        : ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag{}, url, username, password)
    {
        // Scylla-specific initialization if needed
    }

    ScyllaDBConnectionPool::ScyllaDBConnectionPool(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept
        : ColumnarDBConnectionPool(DBConnectionPool::ConstructorTag{}, config)
    {
        // Scylla-specific initialization if needed
    }

#ifdef __cpp_exceptions

    std::shared_ptr<ScyllaDBConnectionPool> ScyllaDBConnectionPool::create(const std::string &url,
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

    std::shared_ptr<ScyllaDBConnectionPool> ScyllaDBConnectionPool::create(const config::DBConnectionPoolConfig &config)
    {
        auto result = create(std::nothrow, config);
        if (!result.has_value())
        {
            throw result.error();
        }
        return result.value();
    }

#endif // __cpp_exceptions

    cpp_dbc::expected<std::shared_ptr<ScyllaDBConnectionPool>, DBException> ScyllaDBConnectionPool::create(std::nothrow_t,
                                                                                                           const std::string &url,
                                                                                                           const std::string &username,
                                                                                                           const std::string &password) noexcept
    {
        auto pool = std::make_shared<ScyllaDBConnectionPool>(DBConnectionPool::ConstructorTag{}, url, username, password);
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return pool;
    }

    cpp_dbc::expected<std::shared_ptr<ScyllaDBConnectionPool>, DBException> ScyllaDBConnectionPool::create(std::nothrow_t, const config::DBConnectionPoolConfig &config) noexcept
    {
        auto pool = std::make_shared<ScyllaDBConnectionPool>(DBConnectionPool::ConstructorTag{}, config);
        auto initResult = pool->initializePool(std::nothrow);
        if (!initResult.has_value())
        {
            return cpp_dbc::unexpected(initResult.error());
        }
        return pool;
    }
} // namespace cpp_dbc::ScyllaDB
