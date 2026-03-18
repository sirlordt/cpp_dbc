#pragma once

#include "handles.hpp"

#if USE_MYSQL

#include <map>
#include <set>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>

namespace cpp_dbc::MySQL
{
    class MySQLDBPreparedStatement; // Forward declaration
    class MySQLDBResultSet;          // Forward declaration

    /**
     * @brief MySQL connection implementation
     *
     * Concrete RelationalDBConnection for MySQL/MariaDB databases.
     * Supports prepared statements, transactions, and connection pooling.
     *
     * ```cpp
     * auto conn = std::dynamic_pointer_cast<cpp_dbc::MySQL::MySQLDBConnection>(
     *     cpp_dbc::DriverManager::getDBConnection("cpp_dbc:mysql://localhost:3306/mydb", "root", "pass"));
     * auto rs = conn->executeQuery("SELECT * FROM users");
     * while (rs->next()) {
     *     std::cout << rs->getString("name") << std::endl;
     * }
     * conn->close();
     * ```
     *
     * @see MySQLDBDriver, MySQLDBPreparedStatement, MySQLDBResultSet
     */
    class MySQLDBConnection final : public RelationalDBConnection, public std::enable_shared_from_this<MySQLDBConnection>
    {
        friend class MySQLDBPreparedStatement;
        friend class MySQLDBResultSet;
        friend class MySQLBlob;
        friend class MySQLConnectionLock;

        /**
         * @brief Private tag for the passkey idiom — enables std::make_shared
         * from static factory methods while keeping the constructor
         * effectively private (external code cannot construct PrivateCtorTag).
         */
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        // ── Member variables ──────────────────────────────────────────────────
        MySQLHandle m_conn; // shared_ptr allows PreparedStatements to use weak_ptr
        std::atomic<bool> m_closed{true};
        std::atomic<bool> m_resetting{false}; // True during reset() to prevent unregister deadlock
        bool m_autoCommit{true};
        bool m_transactionActive{false};
        TransactionIsolationLevel m_isolationLevel{TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ}; // MySQL default

        // Self weak_ptr stored by create() factory after construction.
        // Used by close() to unregister this connection from MySQLDBDriver::s_connectionRegistry
        // via owner_less comparison — safe to call even after shared_ptr refcount drops to zero,
        // because owner_less compares control blocks, not the object pointer.
        std::weak_ptr<MySQLDBConnection> m_self;
        bool m_inGetTransactionIsolation{false}; // Recursion guard for getTransactionIsolation (per-instance, not static)

        /// @brief Cached URI string for getURI() method
        std::string m_uri;

        /**
         * @brief Registry of active prepared statements using weak_ptr
         *
         * @details
         * **DESIGN RATIONALE - Statement Lifecycle Management:**
         *
         * This registry uses `weak_ptr` instead of `shared_ptr` to track active statements.
         * This design decision addresses a complex threading issue in connection pooling scenarios.
         *
         * **THE PROBLEM:**
         *
         * When using `shared_ptr` for statement tracking:
         * - Statements remain alive as long as the connection exists
         * - Memory accumulates if users create many statements without explicitly closing them
         * - However, this prevents race conditions because statements don't get destroyed unexpectedly
         *
         * When using `weak_ptr` without proper synchronization:
         * - Statements can be destroyed at any time when user releases their reference
         * - The destructor calls `mysql_stmt_close()` which communicates with the MySQL server
         * - If another thread is using the same `MYSQL*` connection (e.g., connection pool validation),
         *   this causes a race condition leading to use-after-free memory corruption
         *
         * **THE SOLUTION:**
         *
         * We use `weak_ptr` combined with explicit statement cleanup in `returnToPool()`:
         *
         * 1. `weak_ptr` allows statements to be destroyed when the user releases them (no memory leak)
         * 2. Before returning a connection to the pool, `returnToPool()` explicitly closes ALL
         *    active statements while holding exclusive access to the connection
         * 3. This ensures no statement destruction can race with connection reuse by another thread
         * 4. The `close()` method also closes all statements before destroying the connection
         *
         * **LIFECYCLE GUARANTEE:**
         *
         * - Statement created → registered in this set (weak_ptr)
         * - User uses statement → statement remains valid
         * - User releases statement → destructor may run, calls `mysql_stmt_close()`
         * - Connection returned to pool → ALL remaining statements are explicitly closed first
         * - Connection closed → ALL remaining statements are explicitly closed first
         *
         * This ensures `mysql_stmt_close()` never races with other connection operations.
         *
         * @see returnToPool() - Closes all statements before making connection available
         * @see close() - Closes all statements before destroying connection
         * @see registerStatement() - Adds statement to registry
         * @see unregisterStatement() - Removes statement from registry (unused, kept for API symmetry)
         */
        std::set<std::weak_ptr<MySQLDBPreparedStatement>, std::owner_less<std::weak_ptr<MySQLDBPreparedStatement>>> m_activeStatements;
        std::set<std::weak_ptr<MySQLDBResultSet>, std::owner_less<std::weak_ptr<MySQLDBResultSet>>> m_activeResultSets;

        /**
         * @brief Mutex protecting m_activeStatements registry
         *
         * @note This mutex is ALWAYS present (not conditional on DB_DRIVER_THREAD_SAFE) because
         * statement registration/cleanup can occur from different execution paths even in
         * single-threaded builds (e.g., during returnToPool() or close()).
         */
        std::mutex m_statementsMutex;

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared connection mutex for thread-safe operations
         *
         * This mutex is shared with all PreparedStatements created from this connection.
         * This ensures that mysql_stmt_close() in PreparedStatement destructors is
         * serialized with all other connection operations, preventing race conditions.
         */
        SharedConnMutex m_connMutex = std::make_shared<std::recursive_mutex>();
#endif

        /**
         * @brief Flag indicating constructor initialization failed
         *
         * Set by the nothrow constructor when connection setup fails.
         * Inspected by create(nothrow_t) to propagate the error via expected.
         */
        bool m_initFailed{false};

        /**
         * @brief Error captured when constructor initialization fails
         *
         * Holds the DBException that would have been thrown, for deferred delivery.
         */
        std::unique_ptr<DBException> m_initError{nullptr};

        // ── Private helper methods ────────────────────────────────────────────

        /**
         * @brief Register a prepared statement in the active statements registry
         * @param stmt Weak pointer to the statement to register
         * @note Called automatically when a new PreparedStatement is created via prepareStatement()
         */
        cpp_dbc::expected<void, DBException> registerStatement(std::nothrow_t, std::weak_ptr<MySQLDBPreparedStatement> stmt) noexcept;

        /**
         * @brief Unregister a prepared statement from the active statements registry
         * @param stmt Weak pointer to the statement to unregister
         * @note Currently unused - statements are cleaned up via closeAllStatements() or expire naturally.
         *       Kept for API symmetry and potential future use.
         */
        cpp_dbc::expected<void, DBException> unregisterStatement(std::nothrow_t, std::weak_ptr<MySQLDBPreparedStatement> stmt) noexcept;

        /**
         * @brief Close all active prepared statements
         *
         * @details
         * This method iterates through all registered statements and explicitly closes them.
         * It is called by:
         * - `returnToPool()` before making the connection available for reuse
         * - `close()` before destroying the connection
         *
         * This ensures that `mysql_stmt_close()` is called while we have exclusive access
         * to the connection, preventing race conditions with other threads.
         *
         * @note Statements that have already expired (weak_ptr returns nullptr) are simply
         * removed from the registry without any action.
         */
        cpp_dbc::expected<void, DBException> closeAllStatements(std::nothrow_t) noexcept;

        /**
         * @brief Register a result set in the active result sets registry
         * @param rs Weak pointer to the result set to register
         * @note Called automatically when a new ResultSet is created via executeQuery()
         */
        cpp_dbc::expected<void, DBException> registerResultSet(std::nothrow_t, std::weak_ptr<MySQLDBResultSet> rs) noexcept;

        /**
         * @brief Unregister a result set from the active result sets registry
         * @param rs Weak pointer to the result set to unregister
         * @note Currently unused - result sets are cleaned up via closeAllResultSets()
         *       or expire naturally. Kept for API symmetry and potential future use.
         */
        cpp_dbc::expected<void, DBException> unregisterResultSet(std::nothrow_t, std::weak_ptr<MySQLDBResultSet> rs) noexcept;

        /**
         * @brief Close all active result sets
         *
         * @details
         * Iterates through all registered result sets and explicitly closes them.
         * Uses a two-phase pattern: collect shared_ptrs under lock, then close
         * outside the lock to prevent iterator invalidation.
         *
         * Called by returnToPool(), close(), and reset() before making the
         * connection available for reuse or destroying it.
         *
         * @note Unlike PreparedStatements, MySQL ResultSets are independent of the
         * connection after creation (store result model). Closing them only frees
         * local MYSQL_RES* memory and does not communicate with the server.
         */
        cpp_dbc::expected<void, DBException> closeAllResultSets(std::nothrow_t) noexcept;

        // ── Internal helpers for sibling types (PreparedStatement, ResultSet, MySQLConnectionLock) ──

        /**
         * @brief Get the native MYSQL* handle for PreparedStatement operations
         *
         * Allows PreparedStatement to access the underlying MYSQL* connection handle
         * via their weak_ptr<MySQLDBConnection>, without storing the handle directly.
         *
         * @return Raw MYSQL* pointer (may be nullptr if connection was closed)
         */
        MYSQL *getMySQLNativeHandle(std::nothrow_t) const noexcept
        {
            return m_conn.get();
        }

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Get the connection mutex for PreparedStatement/ResultSet access
         *
         * Allows PreparedStatement and ResultSet to serialize their operations through
         * the connection mutex via their weak_ptr<MySQLDBConnection>, without storing
         * the mutex directly.
         *
         * @return Reference to the connection's recursive_mutex
         */
        std::recursive_mutex &getConnectionMutex(std::nothrow_t) noexcept
        {
            return *m_connMutex;
        }
#endif

        /**
         * @brief Check if connection is currently in reset() operation
         * @return true if reset() is active, false otherwise
         *
         * Used by ResultSet/PreparedStatement to avoid unregister deadlock during closeAll*()
         */
        bool isResetting(std::nothrow_t) const noexcept
        {
            return m_resetting.load(std::memory_order_seq_cst);
        }

    protected:
        // Pool lifecycle overrides - only callable by pool infrastructure (via friend in RelationalDBConnection).
        cpp_dbc::expected<void, DBException> prepareForPoolReturn(std::nothrow_t,
            TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override;
        cpp_dbc::expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override;

    public:
        /**
         * @brief Nothrow constructor — contains all connection logic.
         *
         * All mysql_init, mysql_real_connect, and initial autocommit logic
         * lives here. On failure, sets m_initFailed and m_initError instead of throwing.
         *
         * Public for std::make_shared access, but effectively private:
         * external code cannot construct PrivateCtorTag.
         */
        MySQLDBConnection(PrivateCtorTag,
                          std::nothrow_t,
                          const std::string &host,
                          int port,
                          const std::string &database,
                          const std::string &user,
                          const std::string &password,
                          const std::map<std::string, std::string> &options) noexcept;

        // ── Destructor ────────────────────────────────────────────────────────
        ~MySQLDBConnection() override;

        // ── Deleted copy/move — non-copyable, non-movable: owns mutexes and a live DB connection ──
        MySQLDBConnection(const MySQLDBConnection &) = delete;
        MySQLDBConnection &operator=(const MySQLDBConnection &) = delete;
        MySQLDBConnection(MySQLDBConnection &&) = delete;
        MySQLDBConnection &operator=(MySQLDBConnection &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        static std::shared_ptr<MySQLDBConnection>
        create(const std::string &host,
               int port,
               const std::string &database,
               const std::string &user,
               const std::string &password,
               const std::map<std::string, std::string> &options = std::map<std::string, std::string>())
        {
            auto r = create(std::nothrow, host, port, database, user, password, options);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }

        void close() override;
        void reset() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() const override;
        std::string getURI() const override;
        bool ping() override;

        // RelationalDBConnection interface
        std::shared_ptr<RelationalDBPreparedStatement> prepareStatement(const std::string &sql) override;
        std::shared_ptr<RelationalDBResultSet> executeQuery(const std::string &sql) override;
        uint64_t executeUpdate(const std::string &sql) override;

        void setAutoCommit(bool autoCommit) override;
        bool getAutoCommit() override;

        bool beginTransaction() override;
        bool transactionActive() override;

        void commit() override;
        void rollback() override;

        // Transaction isolation level methods
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

        std::string getServerVersion() override;
        std::map<std::string, std::string> getServerInfo() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<MySQLDBConnection>, DBException>
        create(std::nothrow_t,
               const std::string &host,
               int port,
               const std::string &database,
               const std::string &user,
               const std::string &password,
               const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept
        {
            // No try/catch: std::make_shared can only throw std::bad_alloc, which is a
            // death-sentence exception — the heap is exhausted and no meaningful recovery
            // is possible. Catching it would hide a catastrophic failure as a silent error
            // return. Letting std::terminate fire is safer and more debuggable.
            auto obj = std::make_shared<MySQLDBConnection>(
                PrivateCtorTag{}, std::nothrow, host, port, database, user, password, options);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            // Store a weak self-reference so close() can unregister from the driver's
            // connection registry via owner_less comparison without calling shared_from_this().
            obj->m_self = obj;
            return obj;
        }

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

        cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> reset(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> returnToPool(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::string, DBException> getURI(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<bool, DBException> ping(std::nothrow_t) noexcept override;
        cpp_dbc::expected<std::string, DBException> getServerVersion(std::nothrow_t) noexcept override;
        cpp_dbc::expected<std::map<std::string, std::string>, DBException> getServerInfo(std::nothrow_t) noexcept override;

    };

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
