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
 * @file result_set.hpp
 * @brief ScyllaDB result set implementation
 */

#pragma once

#include "handles.hpp"

#if USE_SCYLLADB

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#include "cpp_dbc/core/columnar/columnar_db_result_set.hpp"
#include "cpp_dbc/core/db_exception.hpp"

namespace cpp_dbc::ScyllaDB
{
        /**
         * @brief ScyllaDB result set implementation
         *
         * Concrete ColumnarDBResultSet for ScyllaDB/Cassandra queries.
         * Wraps CassResult and provides row-by-row iteration with typed
         * column accessors. The result data is fetched into client memory,
         * so the result set remains valid even if the connection is closed.
         *
         * ```cpp
         * auto rs = conn->executeQuery("SELECT id, name, age FROM users");
         * while (rs->next()) {
         *     auto uuid = rs->getUUID("id");
         *     auto name = rs->getString("name");
         *     auto age  = rs->getInt("age");
         * }
         * rs->close();
         * ```
         *
         * @see ScyllaDBConnection, ScyllaDBPreparedStatement
         */
        class ScyllaDBResultSet final : public cpp_dbc::ColumnarDBResultSet
        {
        private:
            CassResultHandle m_result;
            CassIteratorHandle m_iterator;
            const CassRow *m_currentRow{nullptr};

            size_t m_rowCount{0};
            size_t m_columnCount{0};
            std::vector<std::string> m_columnNames;
            std::map<std::string, size_t> m_columnMap;
            uint64_t m_rowPosition{0};

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex;
#endif

            cpp_dbc::expected<void, DBException> validateResultState(std::nothrow_t) const noexcept;
            cpp_dbc::expected<void, DBException> validateCurrentRow(std::nothrow_t) const noexcept;

        public:
            explicit ScyllaDBResultSet(const CassResult *res);
            ~ScyllaDBResultSet() override;

            ScyllaDBResultSet(const ScyllaDBResultSet &) = delete;
            ScyllaDBResultSet &operator=(const ScyllaDBResultSet &) = delete;
            ScyllaDBResultSet(ScyllaDBResultSet &&) = delete;
            ScyllaDBResultSet &operator=(ScyllaDBResultSet &&) = delete;

            // DBResultSet interface
            void close() override;
            bool isEmpty() override;

            // ColumnarDBResultSet interface
            bool next() override;
            bool isBeforeFirst() override;
            bool isAfterLast() override;
            uint64_t getRow() override;

            /**
             * @brief Typed getters - NULL handling semantics
             *
             * When a column contains NULL, typed getters return default values:
             * - getInt/getLong: returns 0
             * - getDouble: returns 0.0
             * - getBoolean: returns false
             * - getString: returns empty string ""
             *
             * @note Unlike JDBC which requires checking wasNull() after each call,
             *       callers should use isNull(columnIndex) BEFORE calling typed getters
             *       if they need to distinguish NULL from actual default values.
             *
             * @see isNull()
             */
            int getInt(size_t columnIndex) override;
            int getInt(const std::string &columnName) override;
            int64_t getLong(size_t columnIndex) override;
            int64_t getLong(const std::string &columnName) override;
            double getDouble(size_t columnIndex) override;
            double getDouble(const std::string &columnName) override;
            std::string getString(size_t columnIndex) override;
            std::string getString(const std::string &columnName) override;
            bool getBoolean(size_t columnIndex) override;
            bool getBoolean(const std::string &columnName) override;
            bool isNull(size_t columnIndex) override;
            bool isNull(const std::string &columnName) override;
            std::string getUUID(size_t columnIndex) override;
            std::string getUUID(const std::string &columnName) override;
            std::string getDate(size_t columnIndex) override;
            std::string getDate(const std::string &columnName) override;
            std::string getTimestamp(size_t columnIndex) override;
            std::string getTimestamp(const std::string &columnName) override;
            std::string getTime(size_t columnIndex) override;
            std::string getTime(const std::string &columnName) override;
            std::vector<std::string> getColumnNames() override;
            size_t getColumnCount() override;

            // BLOB/Binary support
            std::shared_ptr<InputStream> getBinaryStream(size_t columnIndex) override;
            std::shared_ptr<InputStream> getBinaryStream(const std::string &columnName) override;
            std::vector<uint8_t> getBytes(size_t columnIndex) override;
            std::vector<uint8_t> getBytes(const std::string &columnName) override;

            // Nothrow API
            cpp_dbc::expected<bool, DBException> next(std::nothrow_t) noexcept override;
            cpp_dbc::expected<bool, DBException> isBeforeFirst(std::nothrow_t) noexcept override;
            cpp_dbc::expected<bool, DBException> isAfterLast(std::nothrow_t) noexcept override;
            cpp_dbc::expected<uint64_t, DBException> getRow(std::nothrow_t) noexcept override;

            cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<int64_t, DBException> getLong(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::string, DBException> getUUID(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::string, DBException> getTime(std::nothrow_t, size_t columnIndex) noexcept override;

            cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<int64_t, DBException> getLong(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getUUID(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getTime(std::nothrow_t, const std::string &columnName) noexcept override;

            cpp_dbc::expected<std::vector<std::string>, DBException> getColumnNames(std::nothrow_t) noexcept override;
            cpp_dbc::expected<size_t, DBException> getColumnCount(std::nothrow_t) noexcept override;

            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept override;

            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, const std::string &columnName) noexcept override;
        };
} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
