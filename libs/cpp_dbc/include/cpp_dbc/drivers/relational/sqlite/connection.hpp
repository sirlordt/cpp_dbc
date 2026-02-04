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

namespace cpp_dbc::SQLite
{
    class SQLiteDBPreparedStatement; // Forward declaration
    class SQLiteDBResultSet;         // Forward declaration

    class SQLiteDBConnection final : public RelationalDBConnection, public std::enable_shared_from_this<SQLiteDBConnection>
    {
        friend class SQLiteDBPreparedStatement;
        friend class SQLiteDBResultSet;

    private:
        /**
         * @brief Smart pointer for sqlite3 connection - shared_ptr allows weak_ptr support
         *
         * Uses shared_ptr to allow PreparedStatements to use weak_ptr for safe
         * connection detection. The custom deleter ensures sqlite3_close_v2() is called.
         */
        SQLiteDbHandle m_db;

        bool m_closed{true};
        bool m_autoCommit{true};
        bool m_transactionActive{false};
        TransactionIsolationLevel m_isolationLevel;

        // Cached URL
        std::string m_url;

        // Registry of active prepared statements (weak pointers to avoid preventing destruction)
        std::set<std::weak_ptr<SQLiteDBPreparedStatement>, std::owner_less<std::weak_ptr<SQLiteDBPreparedStatement>>> m_activeStatements;
        std::mutex m_statementsMutex;

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared mutex for connection and all its prepared statements
         *
         * This mutex is shared with all PreparedStatements created from this connection.
         * Ensures that statement close operations (sqlite3_finalize) don't race
         * with other operations on the sqlite3* handle.
         */
        mutable SharedConnMutex m_connMutex;
#endif

        // Internal methods for statement registry
        void registerStatement(std::weak_ptr<SQLiteDBPreparedStatement> stmt);
        void unregisterStatement(std::weak_ptr<SQLiteDBPreparedStatement> stmt);
        void closeAllStatements();

    public:
        SQLiteDBConnection(const std::string &database,
                           const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
        ~SQLiteDBConnection() override;

        void close() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() override;

        std::shared_ptr<RelationalDBPreparedStatement> prepareStatement(const std::string &sql) override;
        std::shared_ptr<RelationalDBResultSet> executeQuery(const std::string &sql) override;
        uint64_t executeUpdate(const std::string &sql) override;

        void setAutoCommit(bool autoCommit) override;
        bool getAutoCommit() override;

        bool beginTransaction() override;
        bool transactionActive() override;

        void commit() override;
        void rollback() override;
        void prepareForPoolReturn() override;

        // Transaction isolation level methods
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

        // Get the connection URL
        std::string getURL() const override;

        // Nothrow API
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
    };

} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE
