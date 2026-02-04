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
 * @file relational_db_result_set.hpp
 * @brief Abstract class for relational database result sets
 */

#ifndef CPP_DBC_RELATIONAL_DB_RESULT_SET_HPP
#define CPP_DBC_RELATIONAL_DB_RESULT_SET_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <new> // For std::nothrow_t
#include "cpp_dbc/core/db_result_set.hpp"
#include "cpp_dbc/core/db_expected.hpp"
#include "cpp_dbc/core/db_exception.hpp"

namespace cpp_dbc
{

    // Forward declarations
    class Blob;
    class InputStream;

    /**
     * @brief Abstract class for relational database result sets
     *
     * Provides row-based navigation and typed column access for SQL query results.
     * Columns can be accessed by 1-based index or by name. The cursor starts
     * before the first row; call next() to advance.
     *
     * ```cpp
     * auto rs = conn->executeQuery("SELECT id, name, salary FROM employees");
     * while (rs->next()) {
     *     int id = rs->getInt(1);                  // by index (1-based)
     *     std::string name = rs->getString("name"); // by name
     *     if (!rs->isNull("salary")) {
     *         double salary = rs->getDouble("salary");
     *     }
     * }
     * rs->close();
     * ```
     *
     * Implementations: MySQLDBResultSet, PostgreSQLDBResultSet, SQLiteDBResultSet, FirebirdDBResultSet
     */
    class RelationalDBResultSet : public DBResultSet
    {
    public:
        ~RelationalDBResultSet() override = default;

        // ====================================================================
        // Row navigation
        // ====================================================================

        /**
         * @brief Advance the cursor to the next row
         *
         * Must be called before reading the first row. Returns false when
         * all rows have been consumed.
         *
         * @return true if a new row is available, false if past the last row
         * @throws DBException if navigation fails
         *
         * ```cpp
         * auto rs = conn->executeQuery("SELECT name FROM users");
         * while (rs->next()) {
         *     std::cout << rs->getString(1) << std::endl;
         * }
         * ```
         */
        virtual bool next() = 0;

        /**
         * @brief Check if cursor is before the first row (initial state)
         * @return true if before first row
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * bool atStart = rs->isBeforeFirst(); // true initially
         * rs->next();
         * atStart = rs->isBeforeFirst(); // false after first next()
         * ```
         */
        virtual bool isBeforeFirst() = 0;

        /**
         * @brief Check if cursor is past the last row
         * @return true if after last row
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) { }
         * bool done = rs->isAfterLast(); // true after all rows consumed
         * ```
         */
        virtual bool isAfterLast() = 0;

        /**
         * @brief Get the current row number (1-based)
         * @return The current row number, 0 if before first
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     uint64_t rowNum = rs->getRow(); // 1, 2, 3, ...
         * }
         * ```
         */
        virtual uint64_t getRow() = 0;

        // ====================================================================
        // Typed column access by index (1-based)
        // ====================================================================

        /**
         * @brief Get an integer value by column index
         * @param columnIndex The column position (1-based)
         * @return The integer value
         * @throws DBException if the index is invalid or type conversion fails
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     int id = rs->getInt(1);
         * }
         * ```
         */
        virtual int getInt(size_t columnIndex) = 0;

        /**
         * @brief Get a long value by column index
         * @param columnIndex The column position (1-based)
         * @return The long value
         * @throws DBException if the index is invalid or type conversion fails
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     long bigId = rs->getLong(1);
         * }
         * ```
         */
        virtual long getLong(size_t columnIndex) = 0;

        /**
         * @brief Get a double value by column index
         * @param columnIndex The column position (1-based)
         * @return The double value
         * @throws DBException if the index is invalid or type conversion fails
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     double price = rs->getDouble(1);
         * }
         * ```
         */
        virtual double getDouble(size_t columnIndex) = 0;

        /**
         * @brief Get a string value by column index
         * @param columnIndex The column position (1-based)
         * @return The string value
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     std::string name = rs->getString(2);
         * }
         * ```
         */
        virtual std::string getString(size_t columnIndex) = 0;

        /**
         * @brief Get a boolean value by column index
         * @param columnIndex The column position (1-based)
         * @return The boolean value
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     bool active = rs->getBoolean(3);
         * }
         * ```
         */
        virtual bool getBoolean(size_t columnIndex) = 0;

        /**
         * @brief Check if a column value is SQL NULL by index
         * @param columnIndex The column position (1-based)
         * @return true if the column is NULL
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     if (!rs->isNull(3)) {
         *         double salary = rs->getDouble(3);
         *     }
         * }
         * ```
         */
        virtual bool isNull(size_t columnIndex) = 0;

        // ====================================================================
        // Typed column access by name
        // ====================================================================

        /**
         * @brief Get an integer value by column name
         * @param columnName The column name (case-sensitive)
         * @return The integer value
         * @throws DBException if the column is not found
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     int id = rs->getInt("user_id");
         * }
         * ```
         */
        virtual int getInt(const std::string &columnName) = 0;

        /**
         * @brief Get a long value by column name
         * @param columnName The column name
         * @return The long value
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     long fileSize = rs->getLong("file_size");
         * }
         * ```
         */
        virtual long getLong(const std::string &columnName) = 0;

        /**
         * @brief Get a double value by column name
         * @param columnName The column name
         * @return The double value
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     double salary = rs->getDouble("salary");
         * }
         * ```
         */
        virtual double getDouble(const std::string &columnName) = 0;

        /**
         * @brief Get a string value by column name
         * @param columnName The column name
         * @return The string value
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     std::string name = rs->getString("name");
         * }
         * ```
         */
        virtual std::string getString(const std::string &columnName) = 0;

        /**
         * @brief Get a boolean value by column name
         * @param columnName The column name
         * @return The boolean value
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     bool active = rs->getBoolean("is_active");
         * }
         * ```
         */
        virtual bool getBoolean(const std::string &columnName) = 0;

        /**
         * @brief Check if a column value is SQL NULL by name
         * @param columnName The column name
         * @return true if the column is NULL
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     if (!rs->isNull("email")) {
         *         std::string email = rs->getString("email");
         *     }
         * }
         * ```
         */
        virtual bool isNull(const std::string &columnName) = 0;

        // ====================================================================
        // Metadata
        // ====================================================================

        /**
         * @brief Get the names of all columns in the result set
         * @return Vector of column names in order
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * for (const auto& col : rs->getColumnNames()) {
         *     std::cout << col << " ";
         * }
         * std::cout << std::endl;
         * ```
         */
        virtual std::vector<std::string> getColumnNames() = 0;

        /**
         * @brief Get the number of columns in the result set
         * @return Number of columns
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * size_t cols = rs->getColumnCount();
         * std::cout << "Result has " << cols << " columns" << std::endl;
         * ```
         */
        virtual size_t getColumnCount() = 0;

        // ====================================================================
        // BLOB support methods
        // ====================================================================

        /**
         * @brief Get a BLOB by column index
         * @param columnIndex The column position (1-based)
         * @return The BLOB object
         * @throws DBException if the index is invalid
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     auto blob = rs->getBlob(1);
         *     auto bytes = blob->getBytes(0, blob->length());
         * }
         * ```
         */
        virtual std::shared_ptr<Blob> getBlob(size_t columnIndex) = 0;

        /**
         * @brief Get a BLOB by column name
         * @param columnName The column name
         * @return The BLOB object
         * @throws DBException if the column is not found
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     auto blob = rs->getBlob("photo");
         *     auto bytes = blob->getBytes(0, blob->length());
         * }
         * ```
         */
        virtual std::shared_ptr<Blob> getBlob(const std::string &columnName) = 0;

        /**
         * @brief Get a binary stream by column index
         * @param columnIndex The column position (1-based)
         * @return An input stream for reading binary data
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     auto stream = rs->getBinaryStream(1);
         *     std::vector<uint8_t> buffer(1024);
         *     auto bytesRead = stream->read(buffer.data(), buffer.size());
         * }
         * ```
         */
        virtual std::shared_ptr<InputStream> getBinaryStream(size_t columnIndex) = 0;

        /**
         * @brief Get a binary stream by column name
         * @param columnName The column name
         * @return An input stream for reading binary data
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     auto stream = rs->getBinaryStream("file_data");
         *     std::vector<uint8_t> buffer(1024);
         *     auto bytesRead = stream->read(buffer.data(), buffer.size());
         * }
         * ```
         */
        virtual std::shared_ptr<InputStream> getBinaryStream(const std::string &columnName) = 0;

        /**
         * @brief Get raw bytes by column index
         * @param columnIndex The column position (1-based)
         * @return The column data as a byte vector
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     std::vector<uint8_t> data = rs->getBytes(1);
         * }
         * ```
         */
        virtual std::vector<uint8_t> getBytes(size_t columnIndex) = 0;

        /**
         * @brief Get raw bytes by column name
         * @param columnName The column name
         * @return The column data as a byte vector
         *
         * ```cpp
         * auto rs = stmt->executeQuery();
         * while (rs->next()) {
         *     std::vector<uint8_t> data = rs->getBytes("binary_col");
         * }
         * ```
         */
        virtual std::vector<uint8_t> getBytes(const std::string &columnName) = 0;

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        /**
         * @brief Move to the next row in the result set (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing true if there is a next row, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
            next(std::nothrow_t) noexcept = 0;

        /**
         * @brief Check if cursor is before the first row (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing true if before first row, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
            isBeforeFirst(std::nothrow_t) noexcept = 0;

        /**
         * @brief Check if cursor is after the last row (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing true if after last row, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
            isAfterLast(std::nothrow_t) noexcept = 0;

        /**
         * @brief Get the current row number (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing the current row number (1-based), or DBException on failure
         */
        virtual cpp_dbc::expected<uint64_t, DBException>
            getRow(std::nothrow_t) noexcept = 0;

        /**
         * @brief Get an int value by column index (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnIndex The column index (1-based)
         * @return expected containing the int value, or DBException on failure
         */
        virtual cpp_dbc::expected<int, DBException>
        getInt(std::nothrow_t, size_t columnIndex) noexcept = 0;

        /**
         * @brief Get a long value by column index (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnIndex The column index (1-based)
         * @return expected containing the long value, or DBException on failure
         */
        virtual cpp_dbc::expected<long, DBException>
        getLong(std::nothrow_t, size_t columnIndex) noexcept = 0;

        /**
         * @brief Get a double value by column index (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnIndex The column index (1-based)
         * @return expected containing the double value, or DBException on failure
         */
        virtual cpp_dbc::expected<double, DBException>
        getDouble(std::nothrow_t, size_t columnIndex) noexcept = 0;

        /**
         * @brief Get a string value by column index (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnIndex The column index (1-based)
         * @return expected containing the string value, or DBException on failure
         */
        virtual cpp_dbc::expected<std::string, DBException>
        getString(std::nothrow_t, size_t columnIndex) noexcept = 0;

        /**
         * @brief Get a boolean value by column index (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnIndex The column index (1-based)
         * @return expected containing the boolean value, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
        getBoolean(std::nothrow_t, size_t columnIndex) noexcept = 0;

        /**
         * @brief Check if a column is NULL by index (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnIndex The column index (1-based)
         * @return expected containing true if the column is NULL, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
        isNull(std::nothrow_t, size_t columnIndex) noexcept = 0;

        /**
         * @brief Get an int value by column name (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnName The column name
         * @return expected containing the int value, or DBException on failure
         */
        virtual cpp_dbc::expected<int, DBException>
        getInt(std::nothrow_t, const std::string &columnName) noexcept = 0;

        /**
         * @brief Get a long value by column name (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnName The column name
         * @return expected containing the long value, or DBException on failure
         */
        virtual cpp_dbc::expected<long, DBException>
        getLong(std::nothrow_t, const std::string &columnName) noexcept = 0;

        /**
         * @brief Get a double value by column name (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnName The column name
         * @return expected containing the double value, or DBException on failure
         */
        virtual cpp_dbc::expected<double, DBException>
        getDouble(std::nothrow_t, const std::string &columnName) noexcept = 0;

        /**
         * @brief Get a string value by column name (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnName The column name
         * @return expected containing the string value, or DBException on failure
         */
        virtual cpp_dbc::expected<std::string, DBException>
        getString(std::nothrow_t, const std::string &columnName) noexcept = 0;

        /**
         * @brief Get a boolean value by column name (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnName The column name
         * @return expected containing the boolean value, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
        getBoolean(std::nothrow_t, const std::string &columnName) noexcept = 0;

        /**
         * @brief Check if a column is NULL by name (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnName The column name
         * @return expected containing true if the column is NULL, or DBException on failure
         */
        virtual cpp_dbc::expected<bool, DBException>
        isNull(std::nothrow_t, const std::string &columnName) noexcept = 0;

        /**
         * @brief Get the names of all columns (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing vector of column names, or DBException on failure
         */
        virtual cpp_dbc::expected<std::vector<std::string>, DBException>
            getColumnNames(std::nothrow_t) noexcept = 0;

        /**
         * @brief Get the number of columns (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @return expected containing number of columns, or DBException on failure
         */
        virtual cpp_dbc::expected<size_t, DBException>
            getColumnCount(std::nothrow_t) noexcept = 0;

        /**
         * @brief Get a BLOB by column index (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnIndex The column index (1-based)
         * @return expected containing the BLOB, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<Blob>, DBException>
        getBlob(std::nothrow_t, size_t columnIndex) noexcept = 0;

        /**
         * @brief Get a BLOB by column name (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnName The column name
         * @return expected containing the BLOB, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<Blob>, DBException>
        getBlob(std::nothrow_t, const std::string &columnName) noexcept = 0;

        /**
         * @brief Get a binary stream by column index (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnIndex The column index (1-based)
         * @return expected containing the input stream, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<InputStream>, DBException>
        getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept = 0;

        /**
         * @brief Get a binary stream by column name (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnName The column name
         * @return expected containing the input stream, or DBException on failure
         */
        virtual cpp_dbc::expected<std::shared_ptr<InputStream>, DBException>
        getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept = 0;

        /**
         * @brief Get bytes by column index (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnIndex The column index (1-based)
         * @return expected containing the bytes vector, or DBException on failure
         */
        virtual cpp_dbc::expected<std::vector<uint8_t>, DBException>
        getBytes(std::nothrow_t, size_t columnIndex) noexcept = 0;

        /**
         * @brief Get bytes by column name (nothrow version)
         * @param nothrow std::nothrow tag to indicate exception-free operation
         * @param columnName The column name
         * @return expected containing the bytes vector, or DBException on failure
         */
        virtual cpp_dbc::expected<std::vector<uint8_t>, DBException>
        getBytes(std::nothrow_t, const std::string &columnName) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_RELATIONAL_DB_RESULT_SET_HPP
