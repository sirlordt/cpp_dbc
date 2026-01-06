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
 * @file relational_db_prepared_statement.hpp
 * @brief Abstract class for relational database prepared statements
 */

#ifndef CPP_DBC_RELATIONAL_DB_PREPARED_STATEMENT_HPP
#define CPP_DBC_RELATIONAL_DB_PREPARED_STATEMENT_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <new> // For std::nothrow_t
#include "cpp_dbc/core/db_types.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "cpp_dbc/core/db_exception.hpp"
#include "relational_db_result_set.hpp"

namespace cpp_dbc
{

    // Forward declarations
    class Blob;
    class InputStream;

    /**
     * @brief Abstract class for relational database prepared statements
     *
     * This class provides the interface for prepared SQL statements with
     * parameter binding and execution capabilities.
     *
     * Implementations: MySQLDBPreparedStatement, PostgreSQLDBPreparedStatement,
     *                  SQLiteDBPreparedStatement, FirebirdDBPreparedStatement
     */
    class RelationalDBPreparedStatement
    {
    public:
        virtual ~RelationalDBPreparedStatement() = default;

        // Parameter binding (1-based index)
        virtual void setInt(int parameterIndex, int value) = 0;
        virtual void setLong(int parameterIndex, long value) = 0;
        virtual void setDouble(int parameterIndex, double value) = 0;
        virtual void setString(int parameterIndex, const std::string &value) = 0;
        virtual void setBoolean(int parameterIndex, bool value) = 0;
        virtual void setNull(int parameterIndex, Types type) = 0;
        virtual void setDate(int parameterIndex, const std::string &value) = 0;
        virtual void setTimestamp(int parameterIndex, const std::string &value) = 0;

        // BLOB support methods
        virtual void setBlob(int parameterIndex, std::shared_ptr<Blob> x) = 0;
        virtual void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x) = 0;
        virtual void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length) = 0;
        virtual void setBytes(int parameterIndex, const std::vector<uint8_t> &x) = 0;
        virtual void setBytes(int parameterIndex, const uint8_t *x, size_t length) = 0;

        // Execution
        /**
         * @brief Execute a SELECT query
         * @return A result set containing the query results
         */
        virtual std::shared_ptr<RelationalDBResultSet> executeQuery() = 0;

        /**
         * @brief Execute an INSERT, UPDATE, or DELETE statement
         * @return The number of affected rows
         */
        virtual uint64_t executeUpdate() = 0;

        /**
         * @brief Execute any SQL statement
         * @return true if the result is a result set, false if it's an update count
         */
        virtual bool execute() = 0;

        /**
         * @brief Close the prepared statement and release resources
         */
        virtual void close() = 0;

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        /**
         * @brief Set an int parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The int value
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setInt(std::nothrow_t, int parameterIndex, int value) noexcept = 0;

        /**
         * @brief Set a long parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The long value
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setLong(std::nothrow_t, int parameterIndex, long value) noexcept = 0;

        /**
         * @brief Set a double parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The double value
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setDouble(std::nothrow_t, int parameterIndex, double value) noexcept = 0;

        /**
         * @brief Set a string parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The string value
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;

        /**
         * @brief Set a boolean parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The boolean value
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept = 0;

        /**
         * @brief Set a parameter to NULL (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param type The SQL type
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setNull(std::nothrow_t, int parameterIndex, Types type) noexcept = 0;

        /**
         * @brief Set a date parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The date value
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;

        /**
         * @brief Set a timestamp parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The timestamp value
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;

        /**
         * @brief Set a BLOB parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param x The BLOB value
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept = 0;

        /**
         * @brief Set a binary stream parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param x The input stream
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept = 0;

        /**
         * @brief Set a binary stream parameter with length (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param x The input stream
         * @param length The length of the stream
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept = 0;

        /**
         * @brief Set a bytes parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param x The bytes vector
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept = 0;

        /**
         * @brief Set a bytes parameter from raw pointer (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param x Pointer to the bytes
         * @param length The length of the data
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
        setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept = 0;

        /**
         * @brief Execute a SELECT query (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing a result set, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>
            executeQuery(std::nothrow_t) noexcept = 0;

        /**
         * @brief Execute an INSERT, UPDATE, or DELETE statement (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing the number of affected rows, or DBException on failure
         */
        virtual cpp_dbc::expected<uint64_t, DBException>
            executeUpdate(std::nothrow_t) noexcept = 0;

        /**
         * @brief Execute any SQL statement (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing true if result is a result set, false if update count, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
            execute(std::nothrow_t) noexcept = 0;

        /**
         * @brief Close the prepared statement and release resources (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing void on success, or DBException on failure
         */
        virtual cpp_dbc::expected<void, DBException>
            close(std::nothrow_t) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_RELATIONAL_DB_PREPARED_STATEMENT_HPP
