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
     * @brief Abstract class for relational database connections
     *
     * This class extends DBConnection with methods specific to relational databases,
     * including SQL execution, prepared statements, and transaction management.
     *
     * Implementations: MySQLDBConnection, PostgreSQLDBConnection, SQLiteDBConnection, FirebirdDBConnection
     */
    class RelationalDBConnection : public DBConnection
    {
    public:
        ~RelationalDBConnection() override = default;

        // SQL execution
        /**
         * @brief Prepare a SQL statement for execution
         * @param sql The SQL statement with optional parameter placeholders
         * @return A prepared statement object
         */
        virtual std::shared_ptr<RelationalDBPreparedStatement> prepareStatement(const std::string &sql) = 0;

        /**
         * @brief Execute a SELECT query directly
         * @param sql The SQL SELECT statement
         * @return A result set containing the query results
         */
        virtual std::shared_ptr<RelationalDBResultSet> executeQuery(const std::string &sql) = 0;

        /**
         * @brief Execute an INSERT, UPDATE, or DELETE statement directly
         * @param sql The SQL statement
         * @return The number of affected rows
         */
        virtual uint64_t executeUpdate(const std::string &sql) = 0;

        // Auto-commit control
        /**
         * @brief Set the auto-commit mode
         * @param autoCommit true to enable auto-commit, false to disable
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
         * @return true if the transaction was started successfully
         */
        virtual bool beginTransaction() = 0;

        /**
         * @brief Check if a transaction is currently active
         * @return true if a transaction is active
         */
        virtual bool transactionActive() = 0;

        /**
         * @brief Commit the current transaction
         */
        virtual void commit() = 0;

        /**
         * @brief Rollback the current transaction
         */
        virtual void rollback() = 0;

        // Transaction isolation level
        /**
         * @brief Set the transaction isolation level
         * @param level The desired isolation level
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
         * @brief Prepare a SQL statement for execution (nothrow version)
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
