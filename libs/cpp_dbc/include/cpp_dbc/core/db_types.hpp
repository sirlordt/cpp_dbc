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

#include <string_view>

namespace cpp_dbc
{

    /**
     * @brief Enum representing the type of database paradigm
     *
     * Used by drivers to identify what kind of database they support,
     * and by application code to determine how to cast a DBConnection.
     *
     * ```cpp
     * auto driver = cpp_dbc::DriverManager::getDriver("mysql");
     * if (driver && (*driver)->getDBType() == cpp_dbc::DBType::RELATIONAL) {
     *     // Safe to cast connection to RelationalDBConnection
     * }
     * ```
     */
    enum class DBType
    {
        RELATIONAL, ///< SQL databases with tables, rows, and columns (MySQL, PostgreSQL, SQLite, Firebird)
        DOCUMENT,   ///< Document databases storing JSON/BSON documents (MongoDB)
        KEY_VALUE,  ///< Key-value stores for simple data access (Redis)
        COLUMNAR    ///< Column-oriented databases for analytics (ScyllaDB, Cassandra)
    };

    /**
     * @brief SQL parameter types for use with setNull() in prepared statements
     *
     * ```cpp
     * auto stmt = conn->prepareStatement("INSERT INTO t (a, b) VALUES (?, ?)");
     * stmt->setString(1, "hello");
     * stmt->setNull(2, cpp_dbc::Types::INTEGER);
     * stmt->executeUpdate();
     * ```
     */
    enum class Types
    {
        INTEGER,   ///< Integer numeric type
        FLOAT,     ///< Single-precision floating point
        DOUBLE,    ///< Double-precision floating point
        VARCHAR,   ///< Variable-length string
        DATE,      ///< Date (YYYY-MM-DD)
        TIMESTAMP, ///< Timestamp (YYYY-MM-DD HH:MM:SS)
        BOOLEAN,   ///< Boolean (true/false)
        BLOB,      ///< Binary large object
        UUID,      ///< Universally unique identifier
        CHAR       ///< Fixed-length character
    };

    /**
     * @brief Transaction isolation levels (following JDBC standard)
     *
     * Controls the degree of isolation between concurrent transactions.
     * Higher isolation prevents more anomalies but may reduce concurrency.
     *
     * ```cpp
     * conn->setTransactionIsolation(
     *     cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);
     * ```
     */
    enum class TransactionIsolationLevel
    {
        TRANSACTION_NONE = 0,             ///< Transactions are not supported
        TRANSACTION_READ_UNCOMMITTED = 1, ///< Dirty reads, non-repeatable reads, and phantom reads can occur
        TRANSACTION_READ_COMMITTED = 2,   ///< Dirty reads are prevented; non-repeatable reads and phantom reads can occur
        TRANSACTION_REPEATABLE_READ = 4,  ///< Dirty reads and non-repeatable reads are prevented; phantom reads can occur
        TRANSACTION_SERIALIZABLE = 8      ///< Dirty reads, non-repeatable reads, and phantom reads are prevented
    };

    /**
     * @brief Convert a TransactionIsolationLevel to its human-readable name
     *
     * ```cpp
     * auto name = cpp_dbc::toStringView(TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);
     * // name = "READ_COMMITTED"
     * ```
     *
     * @param level The isolation level
     * @return A string_view with the level name, or "UNKNOWN" for invalid values
     */
    constexpr std::string_view toStringView(TransactionIsolationLevel level) noexcept
    {
        switch (level)
        {
        case TransactionIsolationLevel::TRANSACTION_NONE:
            return "NONE";
        case TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED:
            return "READ_UNCOMMITTED";
        case TransactionIsolationLevel::TRANSACTION_READ_COMMITTED:
            return "READ_COMMITTED";
        case TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ:
            return "REPEATABLE_READ";
        case TransactionIsolationLevel::TRANSACTION_SERIALIZABLE:
            return "SERIALIZABLE";
        default:
            return "UNKNOWN";
        }
    }

} // namespace cpp_dbc

#endif // CPP_DBC_CORE_DB_TYPES_HPP