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
     * Prepared statements allow binding typed parameters to SQL placeholders ('?')
     * and executing the statement efficiently, potentially multiple times with
     * different parameter values. Parameter indices are **1-based**.
     *
     * ```cpp
     * // Query with parameters
     * auto stmt = conn->prepareStatement("SELECT * FROM users WHERE age > ? AND name LIKE ?");
     * stmt->setInt(1, 21);
     * stmt->setString(2, "%Smith%");
     * auto rs = stmt->executeQuery();
     * while (rs->next()) {
     *     std::cout << rs->getString("name") << std::endl;
     * }
     * stmt->close();
     * ```
     *
     * ```cpp
     * // Insert with various types
     * auto stmt = conn->prepareStatement(
     *     "INSERT INTO products (name, price, stock, created) VALUES (?, ?, ?, ?)");
     * stmt->setString(1, "Widget");
     * stmt->setDouble(2, 29.99);
     * stmt->setInt(3, 100);
     * stmt->setTimestamp(4, "2025-01-15 10:30:00");
     * stmt->executeUpdate();
     * ```
     *
     * Implementations: MySQLDBPreparedStatement, PostgreSQLDBPreparedStatement,
     *                  SQLiteDBPreparedStatement, FirebirdDBPreparedStatement
     */
    class RelationalDBPreparedStatement
    {
    public:
        virtual ~RelationalDBPreparedStatement() = default;

        // ====================================================================
        // Parameter binding (1-based index)
        // ====================================================================

        /**
         * @brief Bind an integer value to a parameter
         * @param parameterIndex The parameter position (1-based)
         * @param value The integer value
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("INSERT INTO users (age) VALUES (?)");
         * stmt->setInt(1, 30);
         * stmt->executeUpdate();
         * ```
         */
        virtual void setInt(int parameterIndex, int value) = 0;

        /**
         * @brief Bind a long value to a parameter
         * @param parameterIndex The parameter position (1-based)
         * @param value The long value
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("INSERT INTO logs (timestamp_ms) VALUES (?)");
         * stmt->setLong(1, 9876543210L);
         * stmt->executeUpdate();
         * ```
         */
        virtual void setLong(int parameterIndex, int64_t value) = 0;

        /**
         * @brief Bind a double value to a parameter
         * @param parameterIndex The parameter position (1-based)
         * @param value The double value
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("UPDATE products SET price = ? WHERE id = ?");
         * stmt->setDouble(1, 29.99);
         * stmt->setInt(2, 42);
         * ```
         */
        virtual void setDouble(int parameterIndex, double value) = 0;

        /**
         * @brief Bind a string value to a parameter
         * @param parameterIndex The parameter position (1-based)
         * @param value The string value
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("SELECT * FROM users WHERE name = ?");
         * stmt->setString(1, "Alice");
         * auto rs = stmt->executeQuery();
         * ```
         */
        virtual void setString(int parameterIndex, const std::string &value) = 0;

        /**
         * @brief Bind a boolean value to a parameter
         * @param parameterIndex The parameter position (1-based)
         * @param value The boolean value
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("UPDATE users SET active = ? WHERE id = ?");
         * stmt->setBoolean(1, true);
         * stmt->setInt(2, 42);
         * ```
         */
        virtual void setBoolean(int parameterIndex, bool value) = 0;

        /**
         * @brief Set a parameter to SQL NULL
         * @param parameterIndex The parameter position (1-based)
         * @param type The SQL type of the parameter (for driver compatibility)
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("UPDATE users SET email = ? WHERE id = ?");
         * stmt->setNull(1, cpp_dbc::Types::VARCHAR);
         * stmt->setInt(2, 42);
         * ```
         */
        virtual void setNull(int parameterIndex, Types type) = 0;

        /**
         * @brief Bind a date string to a parameter (format: "YYYY-MM-DD")
         * @param parameterIndex The parameter position (1-based)
         * @param value The date string
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("INSERT INTO events (event_date) VALUES (?)");
         * stmt->setDate(1, "2025-01-15");
         * stmt->executeUpdate();
         * ```
         */
        virtual void setDate(int parameterIndex, const std::string &value) = 0;

        /**
         * @brief Bind a timestamp string to a parameter (format: "YYYY-MM-DD HH:MM:SS")
         * @param parameterIndex The parameter position (1-based)
         * @param value The timestamp string
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("INSERT INTO logs (created_at) VALUES (?)");
         * stmt->setTimestamp(1, "2025-01-15 10:30:00");
         * stmt->executeUpdate();
         * ```
         */
        virtual void setTimestamp(int parameterIndex, const std::string &value) = 0;

        /**
         * @brief Bind a time string to a parameter (format: "HH:MM:SS")
         * @param parameterIndex The parameter position (1-based)
         * @param value The time string
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("INSERT INTO schedule (meeting_time) VALUES (?)");
         * stmt->setTime(1, "14:30:00");
         * stmt->executeUpdate();
         * ```
         */
        virtual void setTime(int parameterIndex, const std::string &value) = 0;

        // ====================================================================
        // BLOB support methods
        // ====================================================================

        /**
         * @brief Bind a BLOB object to a parameter
         * @param parameterIndex The parameter position (1-based)
         * @param x The BLOB object (cpp_dbc::Blob or derived class like cpp_dbc::MemoryBlob)
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * // Binary data to store (e.g., image bytes, serialized data)
         * std::vector<uint8_t> binaryData = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A};
         * std::shared_ptr<cpp_dbc::Blob> blob = std::make_shared<cpp_dbc::MemoryBlob>(binaryData);
         * stmt->setBlob(1, blob);
         * ```
         */
        virtual void setBlob(int parameterIndex, std::shared_ptr<Blob> x) = 0;

        /**
         * @brief Bind a binary input stream to a parameter
         * @param parameterIndex The parameter position (1-based)
         * @param x The input stream to read binary data from
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stream = std::make_shared<cpp_dbc::FileInputStream>("/path/to/file.bin");
         * stmt->setBinaryStream(1, stream);
         * ```
         */
        virtual void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x) = 0;

        /**
         * @brief Bind a binary input stream with explicit length to a parameter
         * @param parameterIndex The parameter position (1-based)
         * @param x The input stream
         * @param length The number of bytes to read from the stream
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto stream = std::make_shared<cpp_dbc::FileInputStream>("/path/to/file.bin");
         * stmt->setBinaryStream(1, stream, 4096);
         * ```
         */
        virtual void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length) = 0;

        /**
         * @brief Bind raw bytes to a parameter
         * @param parameterIndex The parameter position (1-based)
         * @param x The byte vector
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * std::vector<uint8_t> data = {0x01, 0x02, 0x03};
         * stmt->setBytes(1, data);
         * ```
         */
        virtual void setBytes(int parameterIndex, const std::vector<uint8_t> &x) = 0;

        /**
         * @brief Bind raw bytes from a pointer to a parameter
         * @param parameterIndex The parameter position (1-based)
         * @param x Pointer to the byte data
         * @param length Number of bytes
         * @throws DBException if the index is invalid
         */
        virtual void setBytes(int parameterIndex, const uint8_t *x, size_t length) = 0;

        // ====================================================================
        // Execution
        // ====================================================================

        /**
         * @brief Execute this prepared statement as a SELECT query
         *
         * @return A result set containing the query results
         * @throws DBException if execution fails
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("SELECT name FROM users WHERE id = ?");
         * stmt->setInt(1, 42);
         * auto rs = stmt->executeQuery();
         * if (rs->next()) {
         *     std::cout << rs->getString("name") << std::endl;
         * }
         * ```
         */
        virtual std::shared_ptr<RelationalDBResultSet> executeQuery() = 0;

        /**
         * @brief Execute this prepared statement as an INSERT, UPDATE, or DELETE
         *
         * @return The number of affected rows
         * @throws DBException if execution fails
         *
         * ```cpp
         * auto stmt = conn->prepareStatement("UPDATE users SET active = ? WHERE id = ?");
         * stmt->setBoolean(1, false);
         * stmt->setInt(2, 42);
         * uint64_t affected = stmt->executeUpdate();
         * ```
         */
        virtual uint64_t executeUpdate() = 0;

        /**
         * @brief Execute any SQL statement
         * @return true if the result is a result set, false if it's an update count
         * @throws DBException if execution fails
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
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setInt(std::nothrow_t, int parameterIndex, int value) noexcept = 0;

        /**
         * @brief Set a long parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The long value
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setLong(std::nothrow_t, int parameterIndex, int64_t value) noexcept = 0;

        /**
         * @brief Set a double parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The double value
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setDouble(std::nothrow_t, int parameterIndex, double value) noexcept = 0;

        /**
         * @brief Set a string parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The string value
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;

        /**
         * @brief Set a boolean parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The boolean value
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept = 0;

        /**
         * @brief Set a parameter to NULL (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param type The SQL type
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setNull(std::nothrow_t, int parameterIndex, Types type) noexcept = 0;

        /**
         * @brief Set a date parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The date value
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;

        /**
         * @brief Set a timestamp parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param value The timestamp value
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;

        /**
         * @brief Bind a time string to a parameter (nothrow version)
         * @param parameterIndex The parameter position (1-based)
         * @param value The time string
         * @return cpp_dbc::expected<void, DBException>
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setTime(std::nothrow_t, int parameterIndex, const std::string &value) noexcept = 0;

        /**
         * @brief Set a BLOB parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param x The BLOB value
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept = 0;

        /**
         * @brief Set a binary stream parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param x The input stream
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept = 0;

        /**
         * @brief Set a binary stream parameter with length (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param x The input stream
         * @param length The length of the stream
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept = 0;

        /**
         * @brief Set a bytes parameter (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param x The bytes vector
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept = 0;

        /**
         * @brief Set a bytes parameter from raw pointer (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param parameterIndex The parameter index (1-based)
         * @param x Pointer to the bytes
         * @param length The length of the data
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
        setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept = 0;

        /**
         * @brief Execute a SELECT query (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing a result set, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>
            executeQuery(std::nothrow_t) noexcept = 0;

        /**
         * @brief Execute an INSERT, UPDATE, or DELETE statement (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing the number of affected rows, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<uint64_t, DBException>
            executeUpdate(std::nothrow_t) noexcept = 0;

        /**
         * @brief Execute any SQL statement (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing true if result is a result set, false if update count, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<bool, DBException>
            execute(std::nothrow_t) noexcept = 0;

        /**
         * @brief Close the prepared statement and release resources (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing void on success, or DBException on failure
         */
        [[nodiscard]] virtual cpp_dbc::expected<void, DBException>
            close(std::nothrow_t) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_RELATIONAL_DB_PREPARED_STATEMENT_HPP
