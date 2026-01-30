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
 * @file driver_scylla.hpp
 * @brief ScyllaDB (Cassandra) database driver implementation
 */

#ifndef CPP_DBC_DRIVER_SCYLLA_HPP
#define CPP_DBC_DRIVER_SCYLLA_HPP

#include "../../cpp_dbc.hpp"
#include "cpp_dbc/core/columnar/columnar_db_connection.hpp"
#include "cpp_dbc/core/columnar/columnar_db_driver.hpp"
#include "cpp_dbc/core/columnar/columnar_db_prepared_statement.hpp"
#include "cpp_dbc/core/columnar/columnar_db_result_set.hpp"
#include "cpp_dbc/core/db_exception.hpp"

#if USE_SCYLLADB
#include <cassandra.h>
#include <map>
#include <memory>
#include <set>
#include <mutex>
#include <vector>
#include <algorithm>
#include <cstring>

// Thread-safety macros for conditional mutex locking
#if DB_DRIVER_THREAD_SAFE
#define DB_DRIVER_MUTEX mutable std::recursive_mutex
#define DB_DRIVER_LOCK_GUARD(mutex) std::lock_guard<std::recursive_mutex> lock(mutex)
#define DB_DRIVER_UNIQUE_LOCK(mutex) std::unique_lock<std::recursive_mutex> lock(mutex)
#else
#define DB_DRIVER_MUTEX
#define DB_DRIVER_LOCK_GUARD(mutex) (void)0
#define DB_DRIVER_UNIQUE_LOCK(mutex) (void)0
#endif

namespace cpp_dbc
{
    namespace ScyllaDB
    {
        // Custom deleters for Cassandra objects
        struct CassClusterDeleter
        {
            void operator()(CassCluster *ptr) const
            {
                if (ptr)
                    cass_cluster_free(ptr);
            }
        };

        struct CassSessionDeleter
        {
            void operator()(CassSession *ptr) const
            {
                if (ptr)
                    cass_session_free(ptr);
            }
        };

        struct CassFutureDeleter
        {
            void operator()(CassFuture *ptr) const
            {
                if (ptr)
                    cass_future_free(ptr);
            }
        };

        struct CassStatementDeleter
        {
            void operator()(CassStatement *ptr) const
            {
                if (ptr)
                    cass_statement_free(ptr);
            }
        };

        struct CassPreparedDeleter
        {
            void operator()(const CassPrepared *ptr) const
            {
                if (ptr)
                    cass_prepared_free(ptr);
            }
        };

        struct CassResultDeleter
        {
            void operator()(const CassResult *ptr) const
            {
                if (ptr)
                    cass_result_free(ptr);
            }
        };

        struct CassIteratorDeleter
        {
            void operator()(CassIterator *ptr) const
            {
                if (ptr)
                    cass_iterator_free(ptr);
            }
        };

        using CassClusterHandle = std::unique_ptr<CassCluster, CassClusterDeleter>;
        using CassSessionHandle = std::unique_ptr<CassSession, CassSessionDeleter>;
        using CassFutureHandle = std::unique_ptr<CassFuture, CassFutureDeleter>;
        using CassStatementHandle = std::unique_ptr<CassStatement, CassStatementDeleter>;
        using CassPreparedHandle = std::shared_ptr<const CassPrepared>; // Shared because multiple statements can be created from one prepared
        using CassResultHandle = std::unique_ptr<const CassResult, CassResultDeleter>;
        using CassIteratorHandle = std::unique_ptr<CassIterator, CassIteratorDeleter>;

        class ScyllaMemoryInputStream final : public cpp_dbc::InputStream
        {
        private:
            std::vector<uint8_t> m_data;
            size_t m_position{0};

        public:
            explicit ScyllaMemoryInputStream(std::vector<uint8_t> data) : m_data(std::move(data)) {}

            int read(uint8_t *buffer, size_t length) override
            {
                if (m_position >= m_data.size())
                    return -1; // EOF

                size_t remaining = m_data.size() - m_position;
                size_t toRead = std::min(length, remaining);
                std::memcpy(buffer, m_data.data() + m_position, toRead);
                m_position += toRead;
                return static_cast<int>(toRead);
            }

            void skip(size_t n) override
            {
                m_position = std::min(m_position + n, m_data.size());
            }

            void close() override
            {
                // Nothing to close
            }
        };

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

            // DBResultSet interface
            void close() override;
            bool isEmpty() override;

            // ColumnarDBResultSet interface
            bool next() override;
            bool isBeforeFirst() override;
            bool isAfterLast() override;
            uint64_t getRow() override;

            int getInt(size_t columnIndex) override;
            int getInt(const std::string &columnName) override;
            long getLong(size_t columnIndex) override;
            long getLong(const std::string &columnName) override;
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
            cpp_dbc::expected<long, DBException> getLong(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::string, DBException> getUUID(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, size_t columnIndex) noexcept override;

            cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<long, DBException> getLong(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getUUID(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getDate(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getTimestamp(std::nothrow_t, const std::string &columnName) noexcept override;

            cpp_dbc::expected<std::vector<std::string>, DBException> getColumnNames(std::nothrow_t) noexcept override;
            cpp_dbc::expected<size_t, DBException> getColumnCount(std::nothrow_t) noexcept override;

            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept override;

            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, const std::string &columnName) noexcept override;
        };

        class ScyllaDBConnection; // Forward declaration

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
                std::vector<std::pair<int, long>> longParams;
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
            void setLong(int parameterIndex, long value) override;
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
            cpp_dbc::expected<void, DBException> setInt(std::nothrow_t, int parameterIndex, int value) noexcept override;
            cpp_dbc::expected<void, DBException> setLong(std::nothrow_t, int parameterIndex, long value) noexcept override;
            cpp_dbc::expected<void, DBException> setDouble(std::nothrow_t, int parameterIndex, double value) noexcept override;
            cpp_dbc::expected<void, DBException> setString(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
            cpp_dbc::expected<void, DBException> setBoolean(std::nothrow_t, int parameterIndex, bool value) noexcept override;
            cpp_dbc::expected<void, DBException> setNull(std::nothrow_t, int parameterIndex, Types type) noexcept override;
            cpp_dbc::expected<void, DBException> setDate(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
            cpp_dbc::expected<void, DBException> setTimestamp(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
            cpp_dbc::expected<void, DBException> setUUID(std::nothrow_t, int parameterIndex, const std::string &value) noexcept override;
            cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept override;
            cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept override;
            cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept override;
            cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept override;

            cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> executeQuery(std::nothrow_t) noexcept override;
            cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t) noexcept override;
            cpp_dbc::expected<bool, DBException> execute(std::nothrow_t) noexcept override;

            cpp_dbc::expected<void, DBException> addBatch(std::nothrow_t) noexcept override;
            cpp_dbc::expected<void, DBException> clearBatch(std::nothrow_t) noexcept override;
            cpp_dbc::expected<std::vector<uint64_t>, DBException> executeBatch(std::nothrow_t) noexcept override;
            cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        };

        class ScyllaDBConnection final : public cpp_dbc::ColumnarDBConnection
        {
        private:
            std::shared_ptr<CassCluster> m_cluster; // Shared to keep cluster config alive if needed
            std::shared_ptr<CassSession> m_session; // Shared for PreparedStatement weak_ptr
            std::string m_url;
            bool m_closed{true};

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_connMutex;
#endif

        public:
            ScyllaDBConnection(const std::string &host, int port, const std::string &keyspace,
                               const std::string &user, const std::string &password,
                               const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
            ~ScyllaDBConnection() override;

            // DBConnection interface
            void close() override;
            bool isClosed() override;
            void returnToPool() override;
            bool isPooled() override;
            std::string getURL() const override;

            // ColumnarDBConnection interface
            std::shared_ptr<ColumnarDBPreparedStatement> prepareStatement(const std::string &query) override;
            std::shared_ptr<ColumnarDBResultSet> executeQuery(const std::string &query) override;
            uint64_t executeUpdate(const std::string &query) override;

            bool beginTransaction() override;
            void commit() override;
            void rollback() override;

            // Nothrow API
            cpp_dbc::expected<std::shared_ptr<ColumnarDBPreparedStatement>, DBException> prepareStatement(std::nothrow_t, const std::string &query) noexcept override;
            cpp_dbc::expected<std::shared_ptr<ColumnarDBResultSet>, DBException> executeQuery(std::nothrow_t, const std::string &query) noexcept override;
            cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t, const std::string &query) noexcept override;
            cpp_dbc::expected<bool, DBException> beginTransaction(std::nothrow_t) noexcept override;
            cpp_dbc::expected<void, DBException> commit(std::nothrow_t) noexcept override;
            cpp_dbc::expected<void, DBException> rollback(std::nothrow_t) noexcept override;
        };

        class ScyllaDBDriver final : public cpp_dbc::ColumnarDBDriver
        {
        public:
            ScyllaDBDriver();
            ~ScyllaDBDriver() override = default;

            std::shared_ptr<ColumnarDBConnection> connectColumnar(
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            int getDefaultPort() const override;
            std::string getURIScheme() const override;
            std::map<std::string, std::string> parseURI(const std::string &uri) override;
            std::string buildURI(const std::string &host, int port, const std::string &database, const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;
            bool supportsClustering() const override;
            bool supportsAsync() const override;
            std::string getDriverVersion() const override;

            // Nothrow API
            cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> connectColumnar(
                std::nothrow_t,
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

            cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(std::nothrow_t, const std::string &uri) noexcept override;

            bool acceptsURL(const std::string &url) override;
            std::string getName() const noexcept override;
        };
    }
}

#else // USE_SCYLLADB

namespace cpp_dbc::ScyllaDB
{
    class ScyllaDBDriver final : public ColumnarDBDriver
    {
    public:
        [[noreturn]] ScyllaDBDriver() { throw DBException("5F7826C0D4F2", "ScyllaDB support is not enabled in this build"); }
        ~ScyllaDBDriver() override = default;

        ScyllaDBDriver(const ScyllaDBDriver &) = delete;
        ScyllaDBDriver &operator=(const ScyllaDBDriver &) = delete;
        ScyllaDBDriver(ScyllaDBDriver &&) = delete;
        ScyllaDBDriver &operator=(ScyllaDBDriver &&) = delete;

        [[noreturn]] std::shared_ptr<ColumnarDBConnection> connectColumnar(const std::string &, const std::string &, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override
        {
            throw DBException("C0414E6FE88D", "ScyllaDB support is not enabled in this build");
        }
        int getDefaultPort() const override { return 9042; }
        std::string getURIScheme() const override { return "scylladb"; }
        [[noreturn]] std::map<std::string, std::string> parseURI(const std::string &) override { throw DBException("SCYLLADB_DISABLED", "ScyllaDB support is not enabled"); }
        [[noreturn]] std::string buildURI(const std::string &, int, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override { throw DBException("SCYLLADB_DISABLED", "ScyllaDB support is not enabled"); }
        bool supportsClustering() const override { return false; }
        bool supportsAsync() const override { return false; }
        std::string getDriverVersion() const override { return "0.0.0"; }

        cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> connectColumnar(std::nothrow_t, const std::string &, const std::string &, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("SCYLLADB_DISABLED", "ScyllaDB support is not enabled"));
        }
        cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(std::nothrow_t, const std::string &) noexcept override
        {
            return cpp_dbc::unexpected(DBException("SCYLLADB_DISABLED", "ScyllaDB support is not enabled"));
        }
        bool acceptsURL(const std::string &) override { return false; }
        std::string getName() const noexcept override { return "scylladb"; }
    };
}

#endif // USE_SCYLLADB

#endif // CPP_DBC_DRIVER_SCYLLA_HPP
