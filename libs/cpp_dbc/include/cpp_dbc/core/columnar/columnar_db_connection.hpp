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
 * @file columnar_db_connection.hpp
 * @brief Abstract class for columnar database connections
 */

#ifndef CPP_DBC_COLUMNAR_DB_CONNECTION_HPP
#define CPP_DBC_COLUMNAR_DB_CONNECTION_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <new> // For std::nothrow_t
#include "cpp_dbc/core/db_connection.hpp"
#include "cpp_dbc/core/db_types.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "columnar_db_prepared_statement.hpp"
#include "columnar_db_result_set.hpp"

namespace cpp_dbc
{

    /**
     * @brief Abstract class for columnar database connections (ScyllaDB, Cassandra, etc.)
     *
     * Provides SQL/CQL execution, prepared statements with batching,
     * and transaction management for columnar/wide-column databases.
     *
     * ```cpp
     * auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
     *     cpp_dbc::DriverManager::getDBConnection("scylladb://localhost:9042/mykeyspace", "", ""));
     * auto rs = conn->executeQuery("SELECT id, name FROM users WHERE active = true");
     * while (rs->next()) {
     *     std::cout << rs->getString("name") << std::endl;
     * }
     * auto stmt = conn->prepareStatement("INSERT INTO users (id, name) VALUES (?, ?)");
     * stmt->setUUID(1, "550e8400-e29b-41d4-a716-446655440000");
     * stmt->setString(2, "Alice");
     * stmt->executeUpdate();
     * conn->close();
     * ```
     *
     * Implementations: ScyllaDBConnection, CassandraConnection
     *
     * @see ColumnarDBPreparedStatement, ColumnarDBResultSet
     */
    class ColumnarDBConnection : public DBConnection
    {
    public:
        ~ColumnarDBConnection() override = default;

        // ====================================================================
        // SQL/CQL execution
        // ====================================================================

        /**
         * @brief Prepare a statement for execution
         *
         * @param query The SQL/CQL statement with optional parameter placeholders
         * @return A prepared statement object
         *
         * ```cpp
         * auto stmt = conn->prepareStatement(
         *     "INSERT INTO events (id, ts, data) VALUES (?, ?, ?)");
         * std::string eventUuid = "550e8400-e29b-41d4-a716-446655440000";
         * stmt->setUUID(1, eventUuid);
         * stmt->setTimestamp(2, "2025-01-15T10:30:00Z");
         * stmt->setString(3, "{\"type\": \"click\"}");
         * stmt->executeUpdate();
         * ```
         */
        virtual std::shared_ptr<ColumnarDBPreparedStatement> prepareStatement(const std::string &query) = 0;

        /**
         * @brief Execute a query directly
         *
         * @param query The SQL/CQL query statement
         * @return A result set containing the query results
         *
         * ```cpp
         * auto rs = conn->executeQuery("SELECT * FROM events WHERE ts > '2025-01-01'");
         * while (rs->next()) {
         *     std::cout << rs->getUUID(1) << " " << rs->getTimestamp(2) << std::endl;
         * }
         * ```
         */
        virtual std::shared_ptr<ColumnarDBResultSet> executeQuery(const std::string &query) = 0;

        /**
         * @brief Execute an INSERT, UPDATE, DELETE or DDL statement directly
         * @param query The SQL/CQL statement
         * @return The number of affected rows (if applicable)
         */
        virtual uint64_t executeUpdate(const std::string &query) = 0;

        // ====================================================================
        // Transaction management (where supported)
        // ====================================================================
        // Note: Columnar databases may have limited or no ACID transaction support.
        // Implementations should throw DBException if transactions are not supported.

        /**
         * @brief Begin a transaction (if supported by the database)
         * @return true if the transaction was started
         * @throws DBException if transactions are not supported
         */
        virtual bool beginTransaction() = 0;

        /** @brief Commit the current transaction */
        virtual void commit() = 0;

        /** @brief Rollback the current transaction */
        virtual void rollback() = 0;

        /**
         * @brief Prepare the connection for return to pool
         *
         * This method is called when a connection is returned to the pool.
         * It should:
         * - Close all active prepared statements
         * - Rollback any active transaction
         *
         * The default implementation only rolls back any active transaction.
         * Subclasses should override to close statements.
         */
        virtual void prepareForPoolReturn()
        {
            // Default implementation: rollback any active transaction
            // Explicitly discard the [[nodiscard]] result - errors during pool return are intentionally ignored
            (void)rollback(std::nothrow);
        }

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        [[nodiscard]] virtual cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException>
        prepareStatement(std::nothrow_t, const std::string &query) noexcept = 0;

        [[nodiscard]] virtual cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException>
        executeQuery(std::nothrow_t, const std::string &query) noexcept = 0;

        [[nodiscard]] virtual cpp_dbc::expected<uint64_t, DBException>
        executeUpdate(std::nothrow_t, const std::string &query) noexcept = 0;

        [[nodiscard]] virtual cpp_dbc::expected<bool, DBException>
            beginTransaction(std::nothrow_t) noexcept = 0;

        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
            commit(std::nothrow_t) noexcept = 0;

        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
            rollback(std::nothrow_t) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_COLUMNAR_DB_CONNECTION_HPP
