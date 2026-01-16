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
 * @file db_types.hpp
 * @brief Common types and enums for the cpp_dbc library
 */

#ifndef CPP_DBC_CORE_DB_TYPES_HPP
#define CPP_DBC_CORE_DB_TYPES_HPP

namespace cpp_dbc
{

    /**
     * @brief Enum representing the type of database paradigm
     *
     * This enum is used to categorize databases by their data model:
     * - RELATIONAL: Traditional SQL databases (MySQL, PostgreSQL, SQLite, Firebird)
     * - DOCUMENT: Document-oriented databases (MongoDB)
     * - KEY_VALUE: Key-value stores (Redis)
     * - COLUMNAR: Column-oriented databases (ClickHouse)
     */
    enum class DBType
    {
        RELATIONAL, ///< SQL databases with tables, rows, and columns
        DOCUMENT,   ///< Document databases storing JSON/BSON documents
        KEY_VALUE,  ///< Key-value stores for simple data access
        COLUMNAR    ///< Column-oriented databases for analytics
    };

    /**
     * @brief Represents a SQL parameter type for relational databases
     */
    enum class Types
    {
        INTEGER,
        FLOAT,
        DOUBLE,
        VARCHAR,
        DATE,
        TIMESTAMP,
        BOOLEAN,
        BLOB,
        UUID,
        CHAR
    };

    /**
     * @brief Transaction isolation levels (following JDBC standard)
     *
     * These levels define the degree to which one transaction must be isolated
     * from resource or data modifications made by other transactions.
     */
    enum class TransactionIsolationLevel
    {
        TRANSACTION_NONE = 0,             ///< Transactions are not supported
        TRANSACTION_READ_UNCOMMITTED = 1, ///< Dirty reads, non-repeatable reads, and phantom reads can occur
        TRANSACTION_READ_COMMITTED = 2,   ///< Dirty reads are prevented; non-repeatable reads and phantom reads can occur
        TRANSACTION_REPEATABLE_READ = 4,  ///< Dirty reads and non-repeatable reads are prevented; phantom reads can occur
        TRANSACTION_SERIALIZABLE = 8      ///< Dirty reads, non-repeatable reads, and phantom reads are prevented
    };

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_TYPES_HPP