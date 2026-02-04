#pragma once

#include "handles.hpp"

#if USE_MYSQL

#include <map>
#include <set>
#include <string>
#include <mutex>
#include <memory>

namespace cpp_dbc::MySQL
{
        class MySQLDBPreparedStatement; // Forward declaration

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
        private:
            MySQLHandle m_mysql; // shared_ptr allows PreparedStatements to use weak_ptr
            bool m_closed{true};
            bool m_autoCommit{true};
            bool m_transactionActive{false};
            TransactionIsolationLevel m_isolationLevel{TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ}; // MySQL default

            /// @brief Cached URL string for getURL() method
            std::string m_url;

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
             * @brief Register a prepared statement in the active statements registry
             * @param stmt Weak pointer to the statement to register
             * @note Called automatically when a new PreparedStatement is created via prepareStatement()
             */
            void registerStatement(std::weak_ptr<MySQLDBPreparedStatement> stmt);

            /**
             * @brief Unregister a prepared statement from the active statements registry
             * @param stmt Weak pointer to the statement to unregister
             * @note Currently unused - statements are cleaned up via closeAllStatements() or expire naturally.
             *       Kept for API symmetry and potential future use.
             */
            void unregisterStatement(std::weak_ptr<MySQLDBPreparedStatement> stmt);

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
            void closeAllStatements();

        public:
            MySQLDBConnection(const std::string &host,
                              int port,
                              const std::string &database,
                              const std::string &user,
                              const std::string &password,
                              const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
            ~MySQLDBConnection() override;

            // Rule of 5: Non-copyable and non-movable (mutex member prevents copying/moving)
            MySQLDBConnection(const MySQLDBConnection &) = delete;
            MySQLDBConnection &operator=(const MySQLDBConnection &) = delete;
            MySQLDBConnection(MySQLDBConnection &&) = delete;
            MySQLDBConnection &operator=(MySQLDBConnection &&) = delete;

            // DBConnection interface
            void close() override;
            bool isClosed() const override;
            void returnToPool() override;
            bool isPooled() override;
            std::string getURL() const override;

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
            void prepareForPoolReturn() override;

            // Transaction isolation level methods
            void setTransactionIsolation(TransactionIsolationLevel level) override;
            TransactionIsolationLevel getTransactionIsolation() override;

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

} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL
