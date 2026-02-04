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
 * @file relational_db_connection.hpp
 * @brief Abstract class for relational database connections
 */

#ifndef CPP_DBC_RELATIONAL_DB_CONNECTION_HPP
#define CPP_DBC_RELATIONAL_DB_CONNECTION_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <new> // For std::nothrow_t
#include "cpp_dbc/core/db_connection.hpp"
#include "cpp_dbc/core/db_types.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "relational_db_prepared_statement.hpp"
#include "relational_db_result_set.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract class for relational (SQL) database connections
     *
     * Provides SQL execution, prepared statements, and transaction management
     * for relational databases. Obtain via DriverManager::getDBConnection() and
     * cast with std::dynamic_pointer_cast.
     *
     * ```cpp
     * auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
     *     cpp_dbc::DriverManager::getDBConnection("jdbc:mysql://localhost/mydb", "user", "pass"));
     * auto rs = conn->executeQuery("SELECT id, name FROM users");
     * while (rs->next()) {
     *     std::cout << rs->getInt("id") << ": " << rs->getString("name") << std::endl;
     * }
     * rs->close();
     * conn->close();
     * ```
     *
     * ```cpp
     * // Nothrow API pattern
     * auto result = conn->executeQuery(std::nothrow, "SELECT 1");
     * if (result.has_value()) {
     *     auto rs = result.value();
     *     // ... use result set ...
     * } else {
     *     std::cerr << result.error().what_s() << std::endl;
     * }
     * ```
     *
     * Implementations: MySQLDBConnection, PostgreSQLDBConnection, SQLiteDBConnection, FirebirdDBConnection
     *
     * @see RelationalDBPreparedStatement, RelationalDBResultSet
     */
    class RelationalDBConnection : public DBConnection
    {
    public:
        ~RelationalDBConnection() override = default;

        // SQL execution
        /**
         * @brief Prepare a SQL statement for execution with parameter placeholders
         *
         * @param sql The SQL statement (use '?' for parameter placeholders)
         * @return A prepared statement object for binding parameters and execution
         * @throws DBException if the SQL is invalid or preparation fails
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("INSERT INTO users (name, age) VALUES (?, ?)");
         * stmt->setString(1, "Alice");
         * stmt->setInt(2, 30);
         * stmt->executeUpdate();
         * stmt->close();
         * ```
         */
        virtual std::shared_ptr<RelationalDBPreparedStatement> prepareStatement(const std::string &sql) = 0;

        /**
         * @brief Execute a SELECT query directly (without parameter binding)
         *
         * @param sql The SQL SELECT statement
         * @return A result set containing the query results
         * @throws DBException if the query fails
         *
         * ```cpp
         * auto rs = conn->executeQuery("SELECT id, name FROM users WHERE active = 1");
         * while (rs->next()) {
         *     std::cout << rs->getString("name") << std::endl;
         * }
         * rs->close();
         * ```
         */
        virtual std::shared_ptr<RelationalDBResultSet> executeQuery(const std::string &sql) = 0;

        /**
         * @brief Execute an INSERT, UPDATE, or DELETE statement directly
         *
         * @param sql The SQL statement
         * @return The number of affected rows
         * @throws DBException if the statement fails
         *
         * ```cpp
         * uint64_t deleted = conn->executeUpdate("DELETE FROM sessions WHERE expired = 1");
         * std::cout << "Deleted " << deleted << " expired sessions" << std::endl;
         * ```
         */
        virtual uint64_t executeUpdate(const std::string &sql) = 0;

        // Auto-commit control
        /**
         * @brief Set the auto-commit mode
         *
         * When auto-commit is disabled, changes are only persisted after calling commit().
         *
         * @param autoCommit true to enable auto-commit, false to disable
         * @throws DBException if the mode change fails
         */
        virtual void setAutoCommit(bool autoCommit) = 0;

        /**
         * @brief Get the current auto-commit mode
         * @return true if auto-commit is enabled
         */
        virtual bool getAutoCommit() = 0;

        // Transaction management
        /**
         * @brief Begin a new transaction
         *
         * @return true if the transaction was started successfully
         * @throws DBException if a transaction is already active or start fails
         *
         * ```cpp
         * conn->beginTransaction();
         * try {
         *     conn->executeUpdate("UPDATE accounts SET balance = balance - 100 WHERE id = 1");
         *     conn->executeUpdate("UPDATE accounts SET balance = balance + 100 WHERE id = 2");
         *     conn->commit();
         * } catch (const cpp_dbc::DBException &e) {
         *     conn->rollback();
         *     throw;
         * }
         * ```
         */
        virtual bool beginTransaction() = 0;

        /**
         * @brief Check if a transaction is currently active
         * @return true if a transaction is active
         */
        virtual bool transactionActive() = 0;

        /**
         * @brief Commit the current transaction, persisting all changes
         * @throws DBException if no transaction is active or commit fails
         */
        virtual void commit() = 0;

        /**
         * @brief Rollback the current transaction, discarding all changes
         * @throws DBException if no transaction is active or rollback fails
         */
        virtual void rollback() = 0;

        /**
         * @brief Prepare the connection for return to pool
         *
         * This method is called when a connection is returned to the pool.
         * It should:
         * - Close all active prepared statements and result sets
         * - Rollback any active transaction
         * - Reset auto-commit to true
         *
         * The default implementation only rolls back any active transaction.
         * Subclasses should override to close statements/result sets.
         */
        virtual void prepareForPoolReturn()
        {
            // Default implementation: rollback if transaction is active
            auto txActive = transactionActive(std::nothrow);
            if (txActive.has_value() && txActive.value())
            {
                rollback(std::nothrow);
            }
            // Reset auto-commit to true
            setAutoCommit(std::nothrow, true);
        }

        /**
         * @brief Prepare the connection for borrowing from pool
         *
         * This method is called when a connection is borrowed from the pool.
         * It ensures the connection has a fresh transaction snapshot for
         * databases that use MVCC (like Firebird).
         *
         * The default implementation is a no-op for most databases.
         * Firebird overrides this to refresh its transaction.
         */
        virtual void prepareForBorrow()
        {
            // Default: no-op for most databases
            // Firebird overrides to ensure fresh transaction snapshot
        }

        // Transaction isolation level
        /**
         * @brief Set the transaction isolation level
         *
         * @param level The desired isolation level
         * @throws DBException if the level is not supported or change fails
         *
         * ```cpp
         * conn->setTransactionIsolation(
         *     cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
         * conn->beginTransaction();
         * // ... operations with serializable isolation ...
         * conn->commit();
         * ```
         */
        virtual void setTransactionIsolation(TransactionIsolationLevel level) = 0;

        /**
         * @brief Get the current transaction isolation level
         * @return The current isolation level
         */
        virtual TransactionIsolationLevel getTransactionIsolation() = 0;

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        /**
         * @brief Prepare a SQL statement (nothrow version)
         * @note Nothrow version of prepareStatement(). @see expected
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param sql The SQL statement with optional parameter placeholders
         * @return expected containing a prepared statement object, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>
        prepareStatement(std::nothrow_t, const std::string &sql) noexcept = 0;

        /**
         * @brief Execute a SELECT query directly (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param sql The SQL SELECT statement
         * @return expected containing a result set, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>
        executeQuery(std::nothrow_t, const std::string &sql) noexcept = 0;

        /**
         * @brief Execute an INSERT, UPDATE, or DELETE statement directly (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param sql The SQL statement
         * @return expected containing the number of affected rows, or DBException on failure
         */
        virtual cpp_dbc::expected<uint64_t, DBException>
        executeUpdate(std::nothrow_t, const std::string &sql) noexcept = 0;

        /**
         * @brief Set the auto-commit mode (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param autoCommit true to enable auto-commit, false to disable
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setAutoCommit(std::nothrow_t, bool autoCommit) noexcept = 0;

        /**
         * @brief Get the current auto-commit mode (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing true if auto-commit is enabled, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
            getAutoCommit(std::nothrow_t) noexcept = 0;

        /**
         * @brief Begin a new transaction (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing true if the transaction was started successfully, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
            beginTransaction(std::nothrow_t) noexcept = 0;

        /**
         * @brief Check if a transaction is currently active (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing true if a transaction is active, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
            transactionActive(std::nothrow_t) noexcept = 0;

        /**
         * @brief Commit the current transaction (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
            commit(std::nothrow_t) noexcept = 0;

        /**
         * @brief Rollback the current transaction (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
            rollback(std::nothrow_t) noexcept = 0;

        /**
         * @brief Set the transaction isolation level (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param level The desired isolation level
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept = 0;

        /**
         * @brief Get the current transaction isolation level (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing the current isolation level, or DBException on failure
         */
        virtual cpp_dbc::expected<TransactionIsolationLevel, DBException>
            getTransactionIsolation(std::nothrow_t) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_RELATIONAL_DB_CONNECTION_HPP
