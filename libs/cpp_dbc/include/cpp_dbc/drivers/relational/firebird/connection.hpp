#pragma once

#include "handles.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0
#endif

#if USE_FIREBIRD
#include <map>
#include <set>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>

namespace cpp_dbc::Firebird
{
    // Forward declarations
    class FirebirdDBPreparedStatement;
    class FirebirdDBResultSet;
    class FirebirdBlob;

    /**
     * @brief Firebird connection implementation
     *
     * Concrete RelationalDBConnection for Firebird databases.
     * Uses the cursor-based model where result iteration communicates
     * with the database handle on every call to next().
     *
     * ```cpp
     * auto conn = std::dynamic_pointer_cast<cpp_dbc::Firebird::FirebirdDBConnection>(
     *     cpp_dbc::DriverManager::getDBConnection(
     *         "cpp_dbc:firebird://localhost:3050/tmp/test.fdb", "SYSDBA", "masterkey"));
     * conn->executeUpdate("CREATE TABLE t (id INTEGER, name VARCHAR(100))");
     * auto rs = conn->executeQuery("SELECT * FROM t");
     * while (rs->next()) {
     *     std::cout << rs->getString("name") << std::endl;
     * }
     * conn->close();
     * ```
     *
     * @see FirebirdDBDriver, FirebirdDBPreparedStatement, FirebirdDBResultSet
     */
    class FirebirdDBConnection final : public RelationalDBConnection, public std::enable_shared_from_this<FirebirdDBConnection>
    {
        friend class FirebirdDBPreparedStatement;
        friend class FirebirdDBResultSet;
        friend class FirebirdBlob;

    private:
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
        FirebirdDbHandle m_db;
        isc_tr_handle m_tr;
        std::atomic<bool> m_closed{true};
        std::atomic<bool> m_resetting{false}; // True during reset() to prevent unregister deadlock
        bool m_autoCommit{true};
        bool m_transactionActive{false};
        bool m_counterIncremented{false};
        TransactionIsolationLevel m_isolationLevel;
        std::string m_uri;

        // Registry of active prepared statements and result sets
        std::set<std::weak_ptr<FirebirdDBPreparedStatement>, std::owner_less<std::weak_ptr<FirebirdDBPreparedStatement>>> m_activeStatements;
        std::set<std::weak_ptr<FirebirdDBResultSet>, std::owner_less<std::weak_ptr<FirebirdDBResultSet>>> m_activeResultSets;

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared mutex for connection and all its prepared statements
         *
         * This mutex is shared with all PreparedStatements created from this connection.
         * Ensures that statement close operations (isc_dsql_free_statement) don't race
         * with other operations on the database handle.
         */
        mutable SharedConnMutex m_connMutex;
#endif

        /**
         * @brief Flag indicating constructor initialization failed
         *
         * Set by the private nothrow constructor when connection setup fails.
         * Inspected by create(nothrow_t) to propagate the error via expected.
         */
        bool m_initFailed{false};

        /**
         * @brief Error captured when constructor initialization fails
         *
         * Holds the DBException that would have been thrown, for deferred delivery.
         */
        DBException m_initError{"RE8MSLXITHQ2", "", {}};

        // ── Private helper methods ────────────────────────────────────────────
        cpp_dbc::expected<void, DBException> registerStatement(std::nothrow_t, std::weak_ptr<FirebirdDBPreparedStatement> stmt) noexcept;
        cpp_dbc::expected<void, DBException> unregisterStatement(std::nothrow_t, std::weak_ptr<FirebirdDBPreparedStatement> stmt) noexcept;
        cpp_dbc::expected<void, DBException> registerResultSet(std::nothrow_t, std::weak_ptr<FirebirdDBResultSet> rs) noexcept;
        cpp_dbc::expected<void, DBException> unregisterResultSet(std::nothrow_t, std::weak_ptr<FirebirdDBResultSet> rs) noexcept;
        cpp_dbc::expected<void, DBException> startTransaction(std::nothrow_t) noexcept;
        cpp_dbc::expected<void, DBException> endTransaction(std::nothrow_t, bool commit) noexcept;
        cpp_dbc::expected<void, DBException> closeAllActiveResultSets(std::nothrow_t) noexcept;

        /**
         * @brief Close and invalidate all active prepared statements
         * Called before DDL operations to release metadata locks
         */
        cpp_dbc::expected<void, DBException> closeAllActivePreparedStatements(std::nothrow_t) noexcept;

        /**
         * @brief Execute a CREATE DATABASE statement using isc_dsql_execute_immediate
         * @param sql The CREATE DATABASE SQL statement
         * @return 0 (CREATE DATABASE doesn't return affected rows)
         */
        cpp_dbc::expected<uint64_t, DBException> executeCreateDatabase(std::nothrow_t, const std::string &sql) noexcept;

    protected:
        // Pool lifecycle overrides — only callable by pool infrastructure (via friend in RelationalDBConnection).
        cpp_dbc::expected<void, DBException> prepareForPoolReturn(std::nothrow_t,
            TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override;
        cpp_dbc::expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override;

    public:
        /**
         * @brief Nothrow constructor — contains all connection logic.
         *
         * All DPB construction, isc_attach_database, and initial transaction logic
         * lives here. On failure, sets m_initFailed and m_initError instead of throwing.
         *
         * Public for std::make_shared access, but effectively private:
         * external code cannot construct PrivateCtorTag.
         */
        FirebirdDBConnection(PrivateCtorTag,
                             std::nothrow_t,
                             const std::string &host,
                             int port,
                             const std::string &database,
                             const std::string &user,
                             const std::string &password,
                             const std::map<std::string, std::string> &options) noexcept;

        // ── Destructor ────────────────────────────────────────────────────────
        ~FirebirdDBConnection() override;

        // ── Deleted copy/move — non-copyable, non-movable: owns mutexes and a live DB connection ──
        FirebirdDBConnection(const FirebirdDBConnection &) = delete;
        FirebirdDBConnection &operator=(const FirebirdDBConnection &) = delete;
        FirebirdDBConnection(FirebirdDBConnection &&) = delete;
        FirebirdDBConnection &operator=(FirebirdDBConnection &&) = delete;

        // ====================================================================
        // THROWING API — requires exception support
        // ====================================================================

#ifdef __cpp_exceptions
        static std::shared_ptr<FirebirdDBConnection>
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

        std::shared_ptr<RelationalDBPreparedStatement> prepareStatement(const std::string &sql) override;
        std::shared_ptr<RelationalDBResultSet> executeQuery(const std::string &sql) override;
        uint64_t executeUpdate(const std::string &sql) override;

        void setAutoCommit(bool autoCommit) override;
        bool getAutoCommit() override;
        bool beginTransaction() override;
        bool transactionActive() override;
        void commit() override;
        void rollback() override;
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

        std::string getServerVersion() override;
        std::map<std::string, std::string> getServerInfo() override;

#endif // __cpp_exceptions

        // ====================================================================
        // NOTHROW API — exception-free, always available
        // ====================================================================

        static cpp_dbc::expected<std::shared_ptr<FirebirdDBConnection>, DBException>
        create(std::nothrow_t,
               const std::string &host,
               int port,
               const std::string &database,
               const std::string &user,
               const std::string &password,
               const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept
        {
            // The nothrow constructor stores init errors in m_initFailed/m_initError
            // rather than throwing, so no try/catch is needed here.
            auto obj = std::make_shared<FirebirdDBConnection>(
                PrivateCtorTag{}, std::nothrow, host, port, database, user, password, options);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(obj->m_initError);
            }
            return obj;
        }

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Get the connection mutex for PreparedStatement/ResultSet access
         *
         * Allows PreparedStatement and ResultSet to serialize their operations through
         * the connection mutex via their weak_ptr<FirebirdDBConnection>, without storing
         * the mutex directly.
         *
         * @return Reference to the connection's recursive_mutex
         */
        std::recursive_mutex &getConnectionMutex() noexcept
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
        bool isResetting() const noexcept { return m_resetting.load(std::memory_order_acquire); }

        cpp_dbc::expected<void, cpp_dbc::DBException> close(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, cpp_dbc::DBException> reset(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isClosed(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<void, DBException> returnToPool(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> isPooled(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<std::string, DBException> getURI(std::nothrow_t) const noexcept override;
        cpp_dbc::expected<bool, DBException> ping(std::nothrow_t) noexcept override;

        cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>
        prepareStatement(std::nothrow_t, const std::string &sql) noexcept override;

        cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>
        executeQuery(std::nothrow_t, const std::string &sql) noexcept override;

        cpp_dbc::expected<uint64_t, DBException>
        executeUpdate(std::nothrow_t, const std::string &sql) noexcept override;

        cpp_dbc::expected<void, DBException> setAutoCommit(std::nothrow_t, bool autoCommit) noexcept override;
        cpp_dbc::expected<bool, DBException> getAutoCommit(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> beginTransaction(std::nothrow_t) noexcept override;
        cpp_dbc::expected<bool, DBException> transactionActive(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> commit(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> rollback(std::nothrow_t) noexcept override;
        cpp_dbc::expected<void, DBException> setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override;
        cpp_dbc::expected<TransactionIsolationLevel, DBException> getTransactionIsolation(std::nothrow_t) noexcept override;
        cpp_dbc::expected<std::string, DBException> getServerVersion(std::nothrow_t) noexcept override;
        cpp_dbc::expected<std::map<std::string, std::string>, DBException> getServerInfo(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
