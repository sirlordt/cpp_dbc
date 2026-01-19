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
     * @brief Abstract class for columnar database connections
     *
     * This class extends DBConnection with methods specific to columnar databases.
     * It provides support for SQL/CQL execution, prepared statements with batching,
     * and specific connection parameters for analytical workloads.
     *
     * Implementations: ClickHouseConnection, ScyllaDBConnection, CassandraConnection
     */
    class ColumnarDBConnection : public DBConnection
    {
    public:
        virtual ~ColumnarDBConnection() = default;

        // SQL/CQL execution
        /**
         * @brief Prepare a statement for execution
         * @param query The SQL/CQL statement with optional parameter placeholders
         * @return A prepared statement object
         */
        virtual std::shared_ptr<ColumnarDBPreparedStatement> prepareStatement(const std::string &query) = 0;

        /**
         * @brief Execute a query directly
         * @param query The SQL/CQL query statement
         * @return A result set containing the query results
         */
        virtual std::shared_ptr<ColumnarDBResultSet> executeQuery(const std::string &query) = 0;

        /**
         * @brief Execute an INSERT, UPDATE, DELETE or DDL statement directly
         * @param query The SQL/CQL statement
         * @return The number of affected rows (if applicable)
         */
        virtual uint64_t executeUpdate(const std::string &query) = 0;

        // Transaction management (where supported)
        // Note: Columnar databases may have limited or no ACID transaction support.
        // Implementations should throw DBException if transactions are not supported.

        virtual bool beginTransaction() = 0;
        virtual void commit() = 0;
        virtual void rollback() = 0;

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        virtual cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException>
        prepareStatement(std::nothrow_t, const std::string &query) noexcept = 0;

        virtual cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException>
        executeQuery(std::nothrow_t, const std::string &query) noexcept = 0;

        virtual cpp_dbc::expected<uint64_t, DBException>
        executeUpdate(std::nothrow_t, const std::string &query) noexcept = 0;

        virtual cpp_dbc::expected<bool, DBException>
            beginTransaction(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<void, DBException>
            commit(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<void, DBException>
            rollback(std::nothrow_t) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_COLUMNAR_DB_CONNECTION_HPP
