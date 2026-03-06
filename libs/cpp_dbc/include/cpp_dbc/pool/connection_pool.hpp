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
 * @file connection_pool.hpp
 * @brief Unified connection pool base class for all database families
 *
 * DBConnectionPoolBase contains ALL pool infrastructure (connection lifecycle,
 * maintenance thread, direct handoff, HikariCP validation skip, phase-based
 * lock protocol). Family-specific pool classes (RelationalDBConnectionPool,
 * KVDBConnectionPool, DocumentDBConnectionPool, ColumnarDBConnectionPool)
 * become thin derived classes that override only createPooledDBConnection().
 *
 * Reference implementation: ColumnarDBConnectionPool (columnar_db_connection_pool.cpp)
 * See: research/connection_pool_unified.md
 */

#ifndef CPP_DBC_POOL_CONNECTION_POOL_HPP
#define CPP_DBC_POOL_CONNECTION_POOL_HPP

#include "cpp_dbc/pool/db_connection_pool.hpp"
#include "cpp_dbc/pool/db_connection_pooled.hpp"

#include <queue>
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <string>
#include <map>

// Forward declaration of configuration classes
namespace cpp_dbc::config
{
    class DatabaseConfig;
    class DBConnectionPoolConfig;
} // namespace cpp_dbc::config

namespace cpp_dbc
{

    /**
     * @brief Unified base class for all database connection pools
     *
     * Contains the complete pool algorithm: connection acquisition with
     * direct handoff, HikariCP-style validation skip, phase-based lock
     * protocol (acquire under lock, validate/close outside lock), and
     * maintenance thread with 30s wake cycle for idle eviction and
     * minIdle replenishment.
     *
     * Derived classes override only createPooledDBConnection() to produce
     * family-specific pooled connection wrappers, and add a typed getter
     * (e.g., getRelationalDBConnection) that casts the result.
     *
     * ```cpp
     * // Derived class is thin — only creates the right pooled connection type:
     * class RelationalDBConnectionPool : public DBConnectionPoolBase
     * {
     * private:
     *     cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
     *     createPooledDBConnection(std::nothrow_t) noexcept override;
     * public:
     *     cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>
     *     getRelationalDBConnection(std::nothrow_t) noexcept;
     * };
     * ```
     *
     * @see DBConnectionPool, ColumnarDBConnectionPool, RelationalDBConnectionPool
     */
    class DBConnectionPoolBase : public DBConnectionPool, public std::enable_shared_from_this<DBConnectionPoolBase>
    {
    private:
        // Shared flag to indicate if the pool is still alive (shared with all pooled connections)
        std::shared_ptr<std::atomic<bool>> m_poolAlive{std::make_shared<std::atomic<bool>>(true)};

        // Connection parameters
        std::string m_url;
        std::string m_username;
        std::string m_password;
        std::map<std::string, std::string> m_options;
        int m_initialSize{0};
        size_t m_maxSize{0};
        size_t m_minIdle{0};
        long m_maxWaitMillis{0};
        long m_validationTimeoutMillis{0};
        long m_idleTimeoutMillis{0};
        long m_maxLifetimeMillis{0};
        bool m_testOnBorrow{false};
        bool m_testOnReturn{false};
        TransactionIsolationLevel m_transactionIsolation{TransactionIsolationLevel::TRANSACTION_READ_COMMITTED};
        std::vector<std::shared_ptr<DBConnectionPooled>> m_allConnections;
        std::queue<std::shared_ptr<DBConnectionPooled>> m_idleConnections;
        mutable std::mutex m_mutexPool; // Protects m_allConnections + m_idleConnections + m_waitQueue + m_pendingCreations + m_replenishNeeded
        std::condition_variable m_maintenanceCondition; // Wakes maintenance thread on close() or replenish needed
        std::atomic<bool> m_running{true};
        std::atomic<int> m_activeConnections{0};
        size_t m_pendingCreations{0}; // Connections being created outside lock (guarded by m_mutexPool)
        size_t m_replenishNeeded{0};  // Replacement connections requested by returnConnection() (guarded by m_mutexPool)
        std::jthread m_maintenanceThread;

        // Direct handoff mechanism: eliminates "stolen wakeup" race condition.
        // Each waiter has its own condition_variable so that handoffs wake only the
        // targeted waiter — not every sleeping thread (eliminates thundering herd).
        // Multiple std::condition_variable instances sharing the same std::mutex is
        // explicitly safe per the C++ standard; the CV-mutex association exists only
        // for the duration of a single wait() call.
        struct ConnectionRequest
        {
            std::shared_ptr<DBConnectionPooled> conn;
            bool fulfilled{false};
            std::condition_variable cv; // Per-waiter: only this waiter is woken on handoff
        };
        std::deque<ConnectionRequest *> m_waitQueue;

        // Notifies the first waiter in the queue, or no-op if the queue is empty.
        // PRECONDITION: m_mutexPool must be held by the caller.
        void notifyFirstWaiter(std::nothrow_t) noexcept;

        // Validates a connection by pinging the server
        cpp_dbc::expected<bool, DBException> validateConnection(std::nothrow_t, std::shared_ptr<DBConnection> conn) const noexcept;

        // Maintenance thread function (thread entry point — never throws, no caller to check return value)
        void maintenanceTask(std::nothrow_t) noexcept;

    protected:
        // Returns a connection to the pool (called by pooled connections' close/returnToPool).
        // Protected so that derived pool classes' friend pooled connections can access it.
        cpp_dbc::expected<void, DBException> returnConnection(std::nothrow_t, std::shared_ptr<DBConnectionPooled> conn) noexcept;

        // Decrement active connection count — for use by pooled connection destructors
        // when a connection is abandoned without being properly returned to the pool.
        // Cannot use returnToPool() from destructor because shared_from_this() throws
        // bad_weak_ptr when refcount is already 0.
        void decrementActiveCount(std::nothrow_t) noexcept;

        // Core borrow logic — returns a pooled connection from the pool.
        // Protected so derived classes can implement typed getters (e.g., getRelationalDBConnection).
        cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException> acquireConnection(std::nothrow_t) noexcept;
        // Sets the transaction isolation level for the pool
        void setPoolTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override
        {
            m_transactionIsolation = level;
        }

        // Initialize the pool after construction (creates initial connections and starts maintenance thread).
        // This must be called after the pool is managed by a shared_ptr.
        cpp_dbc::expected<void, DBException> initializePool(std::nothrow_t) noexcept;

        // ====================================================================
        // PURE VIRTUAL — Derived classes must override
        // ====================================================================

        /**
         * @brief Create a new pooled connection wrapper
         *
         * Derived classes implement this to:
         * 1. Create a raw DB connection via DriverManager
         * 2. Cast it to the family-specific type
         * 3. Set transaction isolation level
         * 4. Wrap it in the family-specific pooled connection class
         *
         * The weak_ptr to this pool and the m_poolAlive flag are available
         * via getPoolWeakPtr() and getPoolAliveFlag().
         *
         * @return expected containing a shared_ptr to the new pooled connection
         */
        virtual cpp_dbc::expected<std::shared_ptr<DBConnectionPooled>, DBException>
            createPooledDBConnection(std::nothrow_t) noexcept = 0;

        // ====================================================================
        // ACCESSORS — For use by derived classes in createPooledDBConnection()
        // ====================================================================

        const std::string &getUrl() const noexcept { return m_url; }
        const std::string &getUsername() const noexcept { return m_username; }
        const std::string &getPassword() const noexcept { return m_password; }
        const std::map<std::string, std::string> &getOptions() const noexcept { return m_options; }
        TransactionIsolationLevel getTransactionIsolation() const noexcept { return m_transactionIsolation; }
        std::shared_ptr<std::atomic<bool>> getPoolAliveFlag() const noexcept { return m_poolAlive; }

        // Returns a weak_ptr to this pool. Called by derived createPooledDBConnection()
        // to pass to the pooled connection wrapper. Uses shared_from_this() internally.
        // PRECONDITION: the pool must be managed by a shared_ptr.
        std::weak_ptr<DBConnectionPoolBase> getPoolWeakPtr(std::nothrow_t) noexcept;

    public:
        // Public constructors with ConstructorTag - enables std::make_shared while enforcing factory pattern
        DBConnectionPoolBase(DBConnectionPool::ConstructorTag,
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

        explicit DBConnectionPoolBase(DBConnectionPool::ConstructorTag, const config::DBConnectionPoolConfig &config) noexcept;

        ~DBConnectionPoolBase() override;

        DBConnectionPoolBase(const DBConnectionPoolBase &) = delete;
        DBConnectionPoolBase &operator=(const DBConnectionPoolBase &) = delete;
        DBConnectionPoolBase(DBConnectionPoolBase &&) = delete;
        DBConnectionPoolBase &operator=(DBConnectionPoolBase &&) = delete;

#ifdef __cpp_exceptions
        // DBConnectionPool interface implementation
        std::shared_ptr<DBConnection> getDBConnection() override;

        // Gets current pool statistics
        size_t getActiveDBConnectionCount() const override;
        size_t getIdleDBConnectionCount() const override;
        size_t getTotalDBConnectionCount() const override;

        // Closes the pool and all connections
        void close() final;

        // Check if pool is running
        bool isRunning() const override;

#endif // __cpp_exceptions
       // ====================================================================
       // NOTHROW VERSIONS - Exception-free API
       // ====================================================================

        cpp_dbc::expected<std::shared_ptr<DBConnection>, DBException> getDBConnection(std::nothrow_t) noexcept override;
        cpp_dbc::expected<size_t, DBException> getActiveDBConnectionCount(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<size_t, DBException> getIdleDBConnectionCount(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<size_t, DBException> getTotalDBConnectionCount(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isRunning(std::nothrow_t) const noexcept override;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_POOL_CONNECTION_POOL_HPP
