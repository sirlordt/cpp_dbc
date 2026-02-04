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
 * @file columnar_db_result_set.hpp
 * @brief Abstract class for columnar database result sets
 */

#ifndef CPP_DBC_COLUMNAR_DB_RESULT_SET_HPP
#define CPP_DBC_COLUMNAR_DB_RESULT_SET_HPP

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
     * @brief Abstract class for columnar database result sets
     *
     * Extends DBResultSet with columnar-specific types (UUID, date, timestamp).
     * Column access is available by 1-based index or by column name.
     *
     * ```cpp
     * auto rs = conn->executeQuery("SELECT id, name, created_at FROM users");
     * while (rs->next()) {
     *     std::string uuid = rs->getUUID("id");
     *     std::string name = rs->getString("name");
     *     std::string ts   = rs->getTimestamp("created_at");
     *     if (!rs->isNull("name")) {
     *         std::cout << uuid << ": " << name << " (" << ts << ")" << std::endl;
     *     }
     * }
     * ```
     *
     * Implementations: ScyllaDBResultSet, CassandraResultSet
     *
     * @see ColumnarDBConnection, ColumnarDBPreparedStatement
     */
    class ColumnarDBResultSet : public DBResultSet
    {
    public:
        ~ColumnarDBResultSet() override = default;

        // ====================================================================
        // Row navigation
        // ====================================================================

        /**
         * @brief Move to the next row in the result set
         * @return true if there is a next row, false if at end
         */
        virtual bool next() = 0;

        /** @brief Check if cursor is before the first row */
        virtual bool isBeforeFirst() = 0;

        /** @brief Check if cursor is after the last row */
        virtual bool isAfterLast() = 0;

        /** @brief Get the current row number (1-based) */
        virtual uint64_t getRow() = 0;

        // ====================================================================
        // Typed column access by index (1-based)
        // ====================================================================

        /** @brief Get an integer column value by 1-based index */
        virtual int getInt(size_t columnIndex) = 0;
        /** @brief Get a long integer column value by 1-based index */
        virtual long getLong(size_t columnIndex) = 0;
        /** @brief Get a double column value by 1-based index */
        virtual double getDouble(size_t columnIndex) = 0;
        /** @brief Get a string column value by 1-based index */
        virtual std::string getString(size_t columnIndex) = 0;
        /** @brief Get a boolean column value by 1-based index */
        virtual bool getBoolean(size_t columnIndex) = 0;
        /** @brief Check if a column value is NULL by 1-based index */
        virtual bool isNull(size_t columnIndex) = 0;
        /** @brief Get a UUID column value as string by 1-based index */
        virtual std::string getUUID(size_t columnIndex) = 0;
        /** @brief Get a date column value as ISO string by 1-based index */
        virtual std::string getDate(size_t columnIndex) = 0;
        /** @brief Get a timestamp column value as ISO string by 1-based index */
        virtual std::string getTimestamp(size_t columnIndex) = 0;

        // ====================================================================
        // Typed column access by name
        // ====================================================================

        /** @brief Get an integer column value by name */
        virtual int getInt(const std::string &columnName) = 0;
        /** @brief Get a long integer column value by name */
        virtual long getLong(const std::string &columnName) = 0;
        /** @brief Get a double column value by name */
        virtual double getDouble(const std::string &columnName) = 0;
        /** @brief Get a string column value by name */
        virtual std::string getString(const std::string &columnName) = 0;
        /** @brief Get a boolean column value by name */
        virtual bool getBoolean(const std::string &columnName) = 0;
        /** @brief Check if a column value is NULL by name */
        virtual bool isNull(const std::string &columnName) = 0;
        /** @brief Get a UUID column value as string by name */
        virtual std::string getUUID(const std::string &columnName) = 0;
        /** @brief Get a date column value as ISO string by name */
        virtual std::string getDate(const std::string &columnName) = 0;
        /** @brief Get a timestamp column value as ISO string by name */
        virtual std::string getTimestamp(const std::string &columnName) = 0;

        // ====================================================================
        // Metadata
        // ====================================================================

        /** @brief Get the names of all columns in the result set */
        virtual std::vector<std::string> getColumnNames() = 0;

        /** @brief Get the number of columns in the result set */
        virtual size_t getColumnCount() = 0;

        // ====================================================================
        // BLOB/Binary support
        // ====================================================================

        /** @brief Get a binary column as an InputStream by 1-based index */
        virtual std::shared_ptr<InputStream> getBinaryStream(size_t columnIndex) = 0;
        /** @brief Get a binary column as an InputStream by name */
        virtual std::shared_ptr<InputStream> getBinaryStream(const std::string &columnName) = 0;

        /** @brief Get a binary column as a byte vector by 1-based index */
        virtual std::vector<uint8_t> getBytes(size_t columnIndex) = 0;
        /** @brief Get a binary column as a byte vector by name */
        virtual std::vector<uint8_t> getBytes(const std::string &columnName) = 0;

        // ====================================================================
        // NOTHROW VERSIONS - Exception-free API
        // ====================================================================

        virtual cpp_dbc::expected<bool, DBException> next(std::nothrow_t) noexcept = 0;
        virtual cpp_dbc::expected<bool, DBException> isBeforeFirst(std::nothrow_t) noexcept = 0;
        virtual cpp_dbc::expected<bool, DBException> isAfterLast(std::nothrow_t) noexcept = 0;
        virtual cpp_dbc::expected<uint64_t, DBException> getRow(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, size_t columnIndex) noexcept = 0;
        virtual cpp_dbc::expected<long, DBException> getLong(std::nothrow_t, size_t columnIndex) noexcept = 0;
        virtual cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, size_t columnIndex) noexcept = 0;
        virtual cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, size_t columnIndex) noexcept = 0;
        virtual cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, size_t columnIndex) noexcept = 0;
        virtual cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, size_t columnIndex) noexcept = 0;
        virtual cpp_dbc::expected<std::string, DBException> getUUID(std::nothrow_t, size_t columnIndex) noexcept = 0;
        virtual cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, size_t columnIndex) noexcept = 0;
        virtual cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, size_t columnIndex) noexcept = 0;

        virtual cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, const std::string &columnName) noexcept = 0;
        virtual cpp_dbc::expected<long, DBException> getLong(std::nothrow_t, const std::string &columnName) noexcept = 0;
        virtual cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, const std::string &columnName) noexcept = 0;
        virtual cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string &columnName) noexcept = 0;
        virtual cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, const std::string &columnName) noexcept = 0;
        virtual cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, const std::string &columnName) noexcept = 0;
        virtual cpp_dbc::expected<std::string, DBException> getUUID(std::nothrow_t, const std::string &columnName) noexcept = 0;
        virtual cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, const std::string &columnName) noexcept = 0;
        virtual cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, const std::string &columnName) noexcept = 0;

        virtual cpp_dbc::expected<std::vector<std::string>, DBException> getColumnNames(std::nothrow_t) noexcept = 0;
        virtual cpp_dbc::expected<size_t, DBException> getColumnCount(std::nothrow_t) noexcept = 0;

        virtual cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept = 0;
        virtual cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept = 0;

        virtual cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t columnIndex) noexcept = 0;
        virtual cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, const std::string &columnName) noexcept = 0;
    };

} // namespace cpp_dbc

#endif // CPP_DBC_COLUMNAR_DB_RESULT_SET_HPP
