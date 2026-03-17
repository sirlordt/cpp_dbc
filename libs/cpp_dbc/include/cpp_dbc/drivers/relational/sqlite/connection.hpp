#pragma once

#include "handles.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0
#endif

#if USE_SQLITE

#include <map>
#include <set>
#include <string>
#include <mutex>
#include <memory>
#include <cpp_dbc/common/file_mutex_registry.hpp>

namespace cpp_dbc::SQLite
{
    class SQLiteDBPreparedStatement; // Forward declaration
    class SQLiteDBResultSet;         // Forward declaration

    /**
     * @brief SQLite connection implementation
     *
     * Concrete RelationalDBConnection for SQLite embedded databases.
     * Uses the cursor-based model where result iteration communicates
     * with the sqlite3* handle on every call to next().
     *
     * ```cpp
     * auto conn = std::dynamic_pointer_cast<cpp_dbc::SQLite::SQLiteDBConnection>(
     *     cpp_dbc::DriverManager::getDBConnection("cpp_dbc:sqlite::memory:", "", ""));
     * conn->executeUpdate("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT)");
     * conn->executeUpdate("INSERT INTO t (name) VALUES ('Alice')");
     * auto rs = conn->executeQuery("SELECT * FROM t");
     * while (rs->next()) {
     *     std::cout << rs->getString("name") << std::endl;
     * }
     * conn->close();
     * ```
     *
     * @see SQLiteDBDriver, SQLiteDBPreparedStatement, SQLiteDBResultSet
     */
    class SQLiteDBConnection final : public RelationalDBConnection, public std::enable_shared_from_this<SQLiteDBConnection>
    {
        friend class SQLiteDBPreparedStatement;
        friend class SQLiteDBResultSet;

        // ── PrivateCtorTag — prevents direct construction; use create() ──
        struct PrivateCtorTag
        {
            explicit PrivateCtorTag() = default;
        };

        /**
         * @brief Smart pointer for sqlite3 connection - shared_ptr allows weak_ptr support
         *
         * Uses shared_ptr to allow PreparedStatements to use weak_ptr for safe
         * connection detection. The custom deleter ensures sqlite3_close_v2() is called.
         */
        SQLiteDbHandle m_db;

        std::atomic<bool> m_closed{true};
        // Stored by the create() factory so close() can unregister from the driver registry
        // using owner_less comparison (raw 'this' won't work with the set's comparator).
        std::weak_ptr<SQLiteDBConnection> m_self;
        std::atomic<bool> m_autoCommit{true};
        std::atomic<bool> m_transactionActive{false};
        TransactionIsolationLevel m_isolationLevel;

        // Cached URI
        std::string m_uri;

        // Normalized database file path
        std::string m_dbPath;

        // ── Construction state ────────────────────────────────────────────────
        bool m_initFailed{false};
        std::unique_ptr<DBException> m_initError{nullptr};

        /**
         * @brief Global file-level mutex shared by all connections to the same database file
         *
         * This mutex is obtained from FileMutexRegistry and is shared among ALL connections
         * to the SAME database file. This ensures:
         * 1. Thread-safe access to sqlite3* handle across all operations
         * 2. Eliminates ThreadSanitizer false positives (SQLite's internal POSIX locks are invisible)
         * 3. Prevents "database is locked" errors under high concurrency
         *
         * The mutex is also shared with all PreparedStatements and ResultSets created from
         * any connection to this file, ensuring complete synchronization at the file level.
         */
        std::shared_ptr<std::recursive_mutex> m_globalFileMutex;

        // Registry of active prepared statements (weak pointers to avoid preventing destruction)
        // NOTE (2026-02-15): m_activeStatements is now protected by m_globalFileMutex in all methods
        // (registerStatement, unregisterStatement, closeAllStatements) to avoid lock order violations.
        std::set<std::weak_ptr<SQLiteDBPreparedStatement>, std::owner_less<std::weak_ptr<SQLiteDBPreparedStatement>>> m_activeStatements;
        // std::recursive_mutex m_statementsMutex;  // REMOVED (2026-02-15). Replaced by m_globalFileMutex to eliminate lock order violations.

        // Registry of active result sets (weak pointers to avoid preventing destruction)
        // NOTE (2026-02-15): m_activeResultSets is now protected by m_globalFileMutex in all methods
        // (registerResultSet, unregisterResultSet, closeAllResultSets) to avoid lock order violations.
        std::set<std::weak_ptr<SQLiteDBResultSet>, std::owner_less<std::weak_ptr<SQLiteDBResultSet>>> m_activeResultSets;
        // std::recursive_mutex m_resultSetsMutex;  // REMOVED (2026-02-15). Replaced by m_globalFileMutex to eliminate lock order violations.

        // Internal methods for statement registry
        cpp_dbc::expected<void, DBException> registerStatement(std::nothrow_t, std::weak_ptr<SQLiteDBPreparedStatement> stmt) noexcept;
        cpp_dbc::expected<void, DBException> unregisterStatement(std::nothrow_t, std::weak_ptr<SQLiteDBPreparedStatement> stmt) noexcept;
        cpp_dbc::expected<void, DBException> closeAllStatements(std::nothrow_t) noexcept;

        // Internal methods for result set registry
        cpp_dbc::expected<void, DBException> registerResultSet(std::nothrow_t, std::weak_ptr<SQLiteDBResultSet> rs) noexcept;
        cpp_dbc::expected<void, DBException> unregisterResultSet(std::nothrow_t, std::weak_ptr<SQLiteDBResultSet> rs) noexcept;
        cpp_dbc::expected<void, DBException> closeAllResultSets(std::nothrow_t) noexcept;

    protected:
        // Pool lifecycle overrides - only callable by pool infrastructure (via friend in RelationalDBConnection).
        cpp_dbc::expected<void, DBException> prepareForPoolReturn(std::nothrow_t,
            TransactionIsolationLevel isolationLevel = TransactionIsolationLevel::TRANSACTION_NONE) noexcept override;
        cpp_dbc::expected<void, DBException> prepareForBorrow(std::nothrow_t) noexcept override;

    public:
        // ── Public nothrow constructor (guarded by PrivateCtorTag) ─────────────
        SQLiteDBConnection(PrivateCtorTag,
                           std::nothrow_t,
                           const std::string &database,
                           const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept;

        ~SQLiteDBConnection() override;

        SQLiteDBConnection(const SQLiteDBConnection &) = delete;
        SQLiteDBConnection &operator=(const SQLiteDBConnection &) = delete;

#ifdef __cpp_exceptions
        static std::shared_ptr<SQLiteDBConnection>
        create(const std::string &database,
               const std::map<std::string, std::string> &options = std::map<std::string, std::string>())
        {
            auto r = create(std::nothrow, database, options);
            if (!r.has_value())
            {
                throw r.error();
            }
            return r.value();
        }
#endif

        static cpp_dbc::expected<std::shared_ptr<SQLiteDBConnection>, DBException>
        create(std::nothrow_t,
               const std::string &database,
               const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept
        {
            // std::make_shared may throw std::bad_alloc — death sentence, no try/catch.
            auto conn = std::make_shared<SQLiteDBConnection>(
                PrivateCtorTag{}, std::nothrow, database, options);
            if (conn->m_initFailed)
            {
                return cpp_dbc::unexpected(std::move(*conn->m_initError));
            }
            // Store a weak self-reference so close() can unregister from the driver's
            // connection registry via owner_less comparison without calling shared_from_this().
            conn->m_self = conn;
            return conn;
        }

#ifdef __cpp_exceptions
        void close() override;
        void reset() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() const override;

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

        // Get the connection URI
        std::string getURI() const override;
        bool ping() override;

        std::string getServerVersion() override;
        std::map<std::string, std::string> getServerInfo() override;

#endif // __cpp_exceptions
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

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
