#pragma once

#include "handles.hpp"

#if USE_POSTGRESQL

#include <atomic>
#include <map>
#include <set>
#include <string>
#include <mutex>
#include <memory>

namespace cpp_dbc::PostgreSQL
{
    class PostgreSQLDBPreparedStatement;
    class PostgreSQLDBResultSet;

    /**
     * @brief PostgreSQL connection implementation
     *
     * Concrete RelationalDBConnection for PostgreSQL databases.
     * Supports prepared statements, transactions, and connection pooling.
     * Uses the "store result" model where all query results are fetched into client memory.
     *
     * ```cpp
     * auto conn = std::dynamic_pointer_cast<cpp_dbc::PostgreSQL::PostgreSQLDBConnection>(
     *     cpp_dbc::DriverManager::getDBConnection(
     *         "cpp_dbc:postgresql://localhost:5432/mydb", "postgres", "pass"));
     * auto rs = conn->executeQuery("SELECT * FROM users");
     * while (rs->next()) {
     *     std::cout << rs->getString("name") << std::endl;
     * }
     * conn->close();
     * ```
     *
     * @see PostgreSQLDBDriver, PostgreSQLDBPreparedStatement, PostgreSQLDBResultSet
     */
    class PostgreSQLDBConnection final : public RelationalDBConnection, public std::enable_shared_from_this<PostgreSQLDBConnection>
    {
        friend class PostgreSQLDBPreparedStatement;
        friend class PostgreSQLDBResultSet;

        /**
         * @brief Private tag for the passkey idiom — enables std::make_shared
         * from static factory methods while keeping the constructor
         * effectively private (external code cannot construct PrivateCtorTag).
         */
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        PGconnHandle m_conn; // shared_ptr allows PreparedStatements to use weak_ptr
        std::atomic<bool> m_closed{true};

        // Stored by the create() factory so close() can unregister from the driver registry
        // using owner_less comparison (raw 'this' won't work with the set's comparator).
        std::weak_ptr<PostgreSQLDBConnection> m_self;
        bool m_autoCommit{true};
        bool m_transactionActive{false};
        int m_statementCounter{0};
        TransactionIsolationLevel m_isolationLevel{TransactionIsolationLevel::TRANSACTION_READ_COMMITTED};

        /// @brief Cached URI string for getURI() method
        std::string m_uri;

        /**
         * @brief Registry of active prepared statements using weak_ptr
         *
         * @details
         * Uses `weak_ptr` to track active statements. Statement lifecycle is managed as follows:
         * - Statement created -> registered in this set
         * - User releases statement -> destructor may run, deallocates statement
         * - Connection returned to pool -> ALL remaining statements are explicitly closed first
         * - Connection closed -> ALL remaining statements are explicitly closed first
         *
         * @note Mutex asymmetry by design: m_statementsMutex is ALWAYS present (unconditional)
         * because statement registration/cleanup can occur from different execution paths even
         * in single-threaded builds. m_connMutex is only defined under DB_DRIVER_THREAD_SAFE.
         *
         * @see returnToPool() - Closes all statements before making connection available
         * @see close() - Closes all statements before destroying connection
         */
        std::set<std::weak_ptr<PostgreSQLDBPreparedStatement>, std::owner_less<std::weak_ptr<PostgreSQLDBPreparedStatement>>> m_activeStatements;

        /**
         * @brief Registry of active result sets using weak_ptr
         *
         * @details
         * Mirrors the statement registry pattern. When the connection closes,
         * all active result sets are notified and closed. This ensures consistent
         * lifecycle management across all drivers (Firebird, MySQL, SQLite, PostgreSQL).
         */
        std::set<std::weak_ptr<PostgreSQLDBResultSet>, std::owner_less<std::weak_ptr<PostgreSQLDBResultSet>>> m_activeResultSets;

        /**
         * @brief Mutex protecting m_activeStatements and m_activeResultSets registries
         *
         * @note This mutex is ALWAYS present (not conditional on DB_DRIVER_THREAD_SAFE) because
         * registration/cleanup can occur from different execution paths even in
         * single-threaded builds (e.g., during returnToPool() or close()).
         */
        std::mutex m_statementsMutex;

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Connection mutex for thread-safe operations
         *
         * This mutex is shared with all PreparedStatements and ResultSets created from
         * this connection, accessed via getConnectionMutex(). This ensures that all
         * operations on the connection and its children are serialized, preventing
         * race conditions.
         */
        std::recursive_mutex m_connMutex;
#endif

        cpp_dbc::expected<void, DBException> registerStatement(std::nothrow_t, std::weak_ptr<PostgreSQLDBPreparedStatement> stmt) noexcept;
        cpp_dbc::expected<void, DBException> unregisterStatement(std::nothrow_t, std::weak_ptr<PostgreSQLDBPreparedStatement> stmt) noexcept;
        cpp_dbc::expected<void, DBException> closeAllStatements(std::nothrow_t) noexcept;

        cpp_dbc::expected<void, DBException> registerResultSet(std::nothrow_t, std::weak_ptr<PostgreSQLDBResultSet> rs) noexcept;
        cpp_dbc::expected<void, DBException> unregisterResultSet(std::nothrow_t, std::weak_ptr<PostgreSQLDBResultSet> rs) noexcept;
        cpp_dbc::expected<void, DBException> closeAllResultSets(std::nothrow_t) noexcept;

        /**
         * @brief Format a PQserverVersion() integer into a human-readable version string.
         * @param version The integer returned by PQserverVersion() (e.g. 160004 → "16.4").
         * @return Formatted version string: "major.minor" for PG >= 10, "major.minor.patch" for older.
         */
        std::string formatServerVersion(std::nothrow_t, int version) const noexcept;

        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

    protected:
        // Pool lifecycle overrides - only callable by pool infrastructure (via friend in RelationalDBConnection).
        cpp_dbc::expected<void, DBException> prepareForPoolReturn(std::nothrow_t,
            TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override;
        cpp_dbc::expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override;

    public:
        PostgreSQLDBConnection(PrivateCtorTag,
                               std::nothrow_t,
                               const std::string &host,
                               int port,
                               const std::string &database,
                               const std::string &user,
                               const std::string &password,
                               const std::map<std::string, std::string> &options) noexcept;
        ~PostgreSQLDBConnection() override;

        // Rule of 5: Non-copyable and non-movable (mutex member prevents copying/moving)
        PostgreSQLDBConnection(const PostgreSQLDBConnection &) = delete;
        PostgreSQLDBConnection &operator=(const PostgreSQLDBConnection &) = delete;
        PostgreSQLDBConnection(PostgreSQLDBConnection &&) = delete;
        PostgreSQLDBConnection &operator=(PostgreSQLDBConnection &&) = delete;

#ifdef __cpp_exceptions
        static std::shared_ptr<PostgreSQLDBConnection>
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
#endif

        static cpp_dbc::expected<std::shared_ptr<PostgreSQLDBConnection>, DBException>
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
            auto obj = std::make_shared<PostgreSQLDBConnection>(PrivateCtorTag{}, std::nothrow, host, port, database, user, password, options);
            if (obj->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*obj->m_initError));
            }
            // Store a weak self-reference so close() can unregister from the driver's
            // connection registry via owner_less comparison without calling shared_from_this().
            obj->m_self = obj;
            return obj;
        }

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Get a reference to the connection's recursive_mutex
         *
         * Used by PreparedStatement and ResultSet via PostgreSQLConnectionLock to acquire
         * the connection's mutex through the weak_ptr<Connection> pattern.
         *
         * @return Reference to the connection's recursive_mutex
         */
        std::recursive_mutex &getConnectionMutex() noexcept
        {
            return m_connMutex;
        }
#endif

// DBConnection interface
#ifdef __cpp_exceptions
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

        /** @brief Generate a unique name for server-side prepared statements */
        std::string generateStatementName();

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================
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

} // namespace cpp_dbc::PostgreSQL

#endif // USE_POSTGRESQL
