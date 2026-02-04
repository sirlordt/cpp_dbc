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
        FirebirdDbHandle m_db;
        isc_tr_handle m_tr;
        bool m_closed{true};
        bool m_autoCommit{true};
        bool m_transactionActive{false};
        TransactionIsolationLevel m_isolationLevel;
        std::string m_url;

        // Registry of active prepared statements and result sets
        std::set<std::weak_ptr<FirebirdDBPreparedStatement>, std::owner_less<std::weak_ptr<FirebirdDBPreparedStatement>>> m_activeStatements;
        std::set<std::weak_ptr<FirebirdDBResultSet>, std::owner_less<std::weak_ptr<FirebirdDBResultSet>>> m_activeResultSets;
        std::mutex m_statementsMutex;
        std::mutex m_resultSetsMutex;

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

        void registerStatement(std::weak_ptr<FirebirdDBPreparedStatement> stmt);
        void unregisterStatement(std::weak_ptr<FirebirdDBPreparedStatement> stmt);
        void registerResultSet(std::weak_ptr<FirebirdDBResultSet> rs);
        void unregisterResultSet(std::weak_ptr<FirebirdDBResultSet> rs);
        void startTransaction();
        void endTransaction(bool commit);
        void closeAllActiveResultSets();

        /**
         * @brief Close and invalidate all active prepared statements
         * Called before DDL operations to release metadata locks
         */
        void closeAllActivePreparedStatements();

        /**
         * @brief Execute a CREATE DATABASE statement using isc_dsql_execute_immediate
         * @param sql The CREATE DATABASE SQL statement
         * @return 0 (CREATE DATABASE doesn't return affected rows)
         */
        uint64_t executeCreateDatabase(const std::string &sql);

    public:
        FirebirdDBConnection(const std::string &host,
                             int port,
                             const std::string &database,
                             const std::string &user,
                             const std::string &password,
                             const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
        ~FirebirdDBConnection() override;

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
        void prepareForBorrow() override;

        // Transaction isolation level methods
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

        // Get the connection URL
        std::string getURL() const override;

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>
        prepareStatement(std::nothrow_t, const std::string &sql) noexcept override;

        cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>
        executeQuery(std::nothrow_t, const std::string &sql) noexcept override;

        cpp_dbc::expected<uint64_t, DBException>
        executeUpdate(std::nothrow_t, const std::string &sql) noexcept override;

        cpp_dbc::expected<void, DBException>
        setAutoCommit(std::nothrow_t, bool autoCommit) noexcept override;

        cpp_dbc::expected<bool, DBException>
            getAutoCommit(std::nothrow_t) noexcept override;

        cpp_dbc::expected<bool, DBException>
            beginTransaction(std::nothrow_t) noexcept override;

        cpp_dbc::expected<bool, DBException>
            transactionActive(std::nothrow_t) noexcept override;

        cpp_dbc::expected<void, DBException>
            commit(std::nothrow_t) noexcept override;

        cpp_dbc::expected<void, DBException>
            rollback(std::nothrow_t) noexcept override;

        cpp_dbc::expected<void, DBException>
        setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override;

        cpp_dbc::expected<TransactionIsolationLevel, DBException>
            getTransactionIsolation(std::nothrow_t) noexcept override;
    };

} // namespace cpp_dbc::Firebird

#endif // USE_FIREBIRD
