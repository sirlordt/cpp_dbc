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
 * @file prepared_statement.hpp
 * @brief ScyllaDB prepared statement implementation
 */

#pragma once

#include "handles.hpp"

#if USE_SCYLLADB

#include <memory>
#include <mutex>
#include <vector>
#include <string>

#include "cpp_dbc/core/columnar/columnar_db_prepared_statement.hpp"
#include "cpp_dbc/core/db_exception.hpp"

namespace cpp_dbc::ScyllaDB
{
        class ScyllaDBConnection; // Forward declaration

        /**
         * @brief ScyllaDB prepared statement implementation
         *
         * Concrete ColumnarDBPreparedStatement for ScyllaDB/Cassandra.
         * Uses the Cassandra C/C++ driver prepared statement API for
         * parameter binding and execution. Supports batch operations.
         *
         * ```cpp
         * auto stmt = conn->prepareStatement(
         *     "INSERT INTO users (id, name) VALUES (?, ?)");
         * stmt->setUUID(1, "550e8400-e29b-41d4-a716-446655440000");
         * stmt->setString(2, "Alice");
         * stmt->executeUpdate();
         * stmt->close();
         * ```
         *
         * @see ScyllaDBConnection, ScyllaDBResultSet
         */
        class ScyllaDBPreparedStatement final : public cpp_dbc::ColumnarDBPreparedStatement
        {
            friend class ScyllaDBConnection;

        private:
            std::weak_ptr<CassSession> m_session;
            std::string m_query;
            CassPreparedHandle m_prepared;
            CassStatementHandle m_statement;

            // For batch operations
            struct BatchEntry
            {
                std::vector<std::pair<int, int>> intParams;
                std::vector<std::pair<int, int64_t>> longParams;
                std::vector<std::pair<int, double>> doubleParams;
                std::vector<std::pair<int, std::string>> stringParams;
                std::vector<std::pair<int, bool>> boolParams;
                std::vector<std::pair<int, std::vector<uint8_t>>> bytesParams;
                std::vector<std::pair<int, Types>> nullParams;
            };
            std::vector<BatchEntry> m_batchEntries;
            BatchEntry m_currentEntry;

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex;
#endif

            void checkSession();
            void recreateStatement();

        public:
            ScyllaDBPreparedStatement(std::weak_ptr<CassSession> session, const std::string &query, const CassPrepared *prepared);
            ~ScyllaDBPreparedStatement() override;

            void setInt(int parameterIndex, int value) override;
            void setLong(int parameterIndex, int64_t value) override;
            void setDouble(int parameterIndex, double value) override;
            void setString(int parameterIndex, const std::string &value) override;
            void setBoolean(int parameterIndex, bool value) override;
            void setNull(int parameterIndex, Types type) override;
            void setDate(int parameterIndex, const std::string &value) override;
            void setTimestamp(int parameterIndex, const std::string &value) override;
            void setUUID(int parameterIndex, const std::string &value) override;

            void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x) override;
            void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length) override;
            void setBytes(int parameterIndex, const std::vector<uint8_t> &x) override;
            void setBytes(int parameterIndex, const uint8_t *x, size_t length) override;

            std::shared_ptr<ColumnarDBResultSet> executeQuery() override;
            uint64_t executeUpdate() override;
            bool execute() override;

            void addBatch() override;
            void clearBatch() override;
            std::vector<uint64_t> executeBatch() override;
            void close() override;

            // Nothrow API
            [[nodiscard]] cpp_dbc::expected<void, DBException> setInt(std::nothrow_t, int parameterIndex, int value) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setLong(std::nothrow_t, int parameterIndex, int64_t value) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setDouble(std::nothrow_t, int parameterIndex, double value) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setNull(std::nothrow_t, int parameterIndex, Types type) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setUUID(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept override;

            [[nodiscard]] cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> executeQuery(std::nothrow_t) noexcept override;
            [[nodiscard]] cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t) noexcept override;
            [[nodiscard]] cpp_dbc::expected<bool, DBException> execute(std::nothrow_t) noexcept override;

            [[nodiscard]] cpp_dbc::expected<void, DBException> addBatch(std::nothrow_t) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> clearBatch(std::nothrow_t) noexcept override;
            [[nodiscard]] cpp_dbc::expected<std::vector<uint64_t>, DBException> executeBatch(std::nothrow_t) noexcept override;
            [[nodiscard]] cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        };
} // namespace cpp_dbc::ScyllaDB

#endif // USE_SCYLLADB
