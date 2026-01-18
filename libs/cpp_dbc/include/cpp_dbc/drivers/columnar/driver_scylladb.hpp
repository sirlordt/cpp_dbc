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
            /**
             * @brief Frees a Cassandra cluster handle if the pointer is non-null.
             *
             * @param ptr Pointer to a `CassCluster` to be freed; no action is taken if `ptr` is `nullptr`.
             */
            void operator()(CassCluster *ptr) const
            {
                if (ptr)
                    cass_cluster_free(ptr);
            }
        };

        struct CassSessionDeleter
        {
            /**
             * @brief Frees a Cassandra session handle.
             *
             * Releases the resources associated with the provided `CassSession` pointer.
             *
             * @param ptr Pointer to a `CassSession`. If `nullptr`, no action is taken.
             */
            void operator()(CassSession *ptr) const
            {
                if (ptr)
                    cass_session_free(ptr);
            }
        };

        struct CassFutureDeleter
        {
            /**
             * @brief Releases a Cassandra future handle if it is not null.
             *
             * @param ptr Pointer to a `CassFuture` to be freed; no action is taken if `ptr` is `nullptr`.
             */
            void operator()(CassFuture *ptr) const
            {
                if (ptr)
                    cass_future_free(ptr);
            }
        };

        struct CassStatementDeleter
        {
            /**
             * @brief Releases a Cassandra statement object.
             *
             * Frees the given `CassStatement` if `ptr` is not null.
             *
             * @param ptr Pointer to the `CassStatement` to free.
             */
            void operator()(CassStatement *ptr) const
            {
                if (ptr)
                    cass_statement_free(ptr);
            }
        };

        struct CassPreparedDeleter
        {
            /**
             * @brief Releases a `CassPrepared` instance if present.
             *
             * @param ptr Pointer to the `CassPrepared` object to free; no action is taken if `ptr` is null.
             */
            void operator()(const CassPrepared *ptr) const
            {
                if (ptr)
                    cass_prepared_free(ptr);
            }
        };

        struct CassResultDeleter
        {
            /**
             * @brief Frees a CassResult resource if one is provided.
             *
             * @param ptr Pointer to the CassResult to release; may be nullptr.
             */
            void operator()(const CassResult *ptr) const
            {
                if (ptr)
                    cass_result_free(ptr);
            }
        };

        struct CassIteratorDeleter
        {
            /**
             * @brief Releases a Cassandra iterator handle if non-null.
             *
             * @param ptr Pointer to the CassIterator to free; ignored if `nullptr`.
             */
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

        class ScyllaMemoryInputStream : public cpp_dbc::InputStream
        {
        private:
            std::vector<uint8_t> m_data;
            size_t m_position{0};

        public:
            /**
 * @brief Constructs an in-memory input stream from a byte buffer.
 *
 * Creates a stream that reads from the provided byte vector. The constructor
 * takes ownership of the contents of `data`.
 *
 * @param data Byte buffer to be used as the stream source; contents are moved into the stream.
 */
explicit ScyllaMemoryInputStream(std::vector<uint8_t> data) : m_data(std::move(data)) {}

            /**
             * @brief Reads up to `length` bytes from the in-memory stream into `buffer`.
             *
             * Reads at most `length` bytes starting at the current stream position, advances
             * the stream position by the number of bytes read, and signals end-of-file when
             * no bytes remain.
             *
             * @param buffer Destination buffer to receive read bytes.
             * @param length Maximum number of bytes to read.
             * @return int Number of bytes actually read, or `-1` if the stream is at end-of-file.
             */
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

            /**
             * @brief Advances the read position by up to n bytes.
             *
             * Moves the stream cursor forward by `n` bytes; if `n` is larger than the remaining bytes, the cursor is set to the end of the stream.
             *
             * @param n Number of bytes to skip.
             */
            void skip(size_t n) override
            {
                m_position = std::min(m_position + n, m_data.size());
            }

            /**
             * @brief Closes the stream.
             *
             * No-op for in-memory streams; there are no external resources to release.
             */
            void close() override
            {
                // Nothing to close
            }
        };

        class ScyllaDBResultSet : public cpp_dbc::ColumnarDBResultSet
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

            void validateResultState() const;
            void validateCurrentRow() const;

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

        class ScyllaDBPreparedStatement : public cpp_dbc::ColumnarDBPreparedStatement
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

        class ScyllaDBConnection : public cpp_dbc::ColumnarDBConnection
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

        class ScyllaDBDriver : public cpp_dbc::ColumnarDBDriver
        {
        public:
            ScyllaDBDriver();
            /**
 * @brief Destroy the ScyllaDBDriver and release any resources it holds.
 *
 * Defaulted destructor; performs any necessary cleanup of driver state.
 */
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

namespace cpp_dbc
{
    namespace Scylla
    {
        class ScyllaDBDriver : public ColumnarDBDriver
        {
        public:
            /**
 * @brief Constructor that signals ScyllaDB support is unavailable in this build.
 *
 * @throws DBException Always thrown with code "SCYLLA_DISABLED" and message
 * "ScyllaDB support is not enabled in this build".
 */
ScyllaDBDriver() { throw DBException("SCYLLA_DISABLED", "ScyllaDB support is not enabled in this build"); }

            /**
             * @brief Create a columnar connection to a ScyllaDB instance using the provided URL and credentials.
             *
             * @param url Connection URL specifying host(s), port, and any database/keyspace information.
             * @param user Username for authentication.
             * @param password Password for authentication.
             * @param options Optional map of driver-specific connection options.
             * @return std::shared_ptr<ColumnarDBConnection> A shared pointer to the established columnar connection.
             *
             * @throws DBException Thrown with code "SCYLLA_DISABLED" when ScyllaDB support is not enabled in this build.
             */
            std::shared_ptr<ColumnarDBConnection> connectColumnar(const std::string &, const std::string &, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override
            {
                throw DBException("SCYLLA_DISABLED", "ScyllaDB support is not enabled in this build");
            }
            /**
 * @brief Provides the default network port used by the ScyllaDB driver.
 *
 * @return int The default port number (9042).
 */
int getDefaultPort() const override { return 9042; }
            /**
 * @brief Provides the URI scheme used by this driver.
 *
 * @return std::string The URI scheme "scylladb".
 */
std::string getURIScheme() const override { return "scylladb"; }
            /**
 * @brief Parse a ScyllaDB connection URI into its components.
 *
 * Parses the provided connection URI and returns a map of component names to their values
 * (for example: host, port, database, user, and options).
 *
 * @param uri The connection URI to parse.
 * @return std::map<std::string, std::string> Map of URI component names to values.
 * @throws DBException Thrown with code "SCYLLA_DISABLED" when ScyllaDB support is not enabled.
 */
std::map<std::string, std::string> parseURI(const std::string &) override { throw DBException("SCYLLA_DISABLED", "ScyllaDB support is not enabled"); }
            /**
 * @brief Constructs a ScyllaDB connection URI from host, port, database, and options.
 *
 * @param host Hostname or IP of the ScyllaDB server.
 * @param port TCP port number.
 * @param database Database/keyspace name.
 * @param options Optional key/value map of URI options to include.
 * @return std::string The constructed URI.
 *
 * @throws DBException Thrown with code "SCYLLA_DISABLED" when ScyllaDB support is not enabled in this build.
 */
std::string buildURI(const std::string &, int, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override { throw DBException("SCYLLA_DISABLED", "ScyllaDB support is not enabled"); }
            /**
 * @brief Indicates whether the driver supports clustered (multi-node) deployments.
 *
 * @return `true` if clustering is supported, `false` otherwise.
 */
bool supportsClustering() const override { return false; }
            /**
 * @brief Indicates whether the driver supports asynchronous operations.
 *
 * @return `true` if asynchronous operations are supported, `false` otherwise.
 */
bool supportsAsync() const override { return false; }
            /**
 * @brief Gets the driver's version string.
 *
 * @return std::string The driver version string (e.g., "0.0.0").
 */
std::string getDriverVersion() const override { return "0.0.0"; }

            /**
             * @brief Attempt to create a columnar ScyllaDB connection when Scylla support is disabled.
             *
             * When compiled without ScyllaDB support, this function always fails to create a connection and
             * returns an unexpected `DBException` indicating the feature is disabled.
             *
             * @param nw Placeholder to select nothrow overload (ignored).
             * @param url Connection URL or host specification.
             * @param user Username for authentication.
             * @param password Password for authentication.
             * @param options Optional connection options map.
             * @return cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException>
             *         An unexpected `DBException` with code `"SCYLLA_DISABLED"` and message
             *         `"ScyllaDB support is not enabled"`.
             */
            cpp_dbc::expected<std::shared_ptr<ColumnarDBConnection>, DBException> connectColumnar(std::nothrow_t, const std::string &, const std::string &, const std::string &, const std::map<std::string, std::string> & = std::map<std::string, std::string>()) noexcept override
            {
                return cpp_dbc::unexpected(DBException("SCYLLA_DISABLED", "ScyllaDB support is not enabled"));
            }
            /**
             * @brief Parses a ScyllaDB connection URI into its components.
             *
             * In builds where ScyllaDB support is disabled this overload always fails.
             *
             * @param uri The connection URI to parse.
             * @return cpp_dbc::expected<std::map<std::string, std::string>, DBException>
             *         An unexpected `DBException` with code `"SCYLLA_DISABLED"` and message
             *         `"ScyllaDB support is not enabled"`.
             */
            cpp_dbc::expected<std::map<std::string, std::string>, DBException> parseURI(std::nothrow_t, const std::string &) noexcept override
            {
                return cpp_dbc::unexpected(DBException("SCYLLA_DISABLED", "ScyllaDB support is not enabled"));
            }
            /**
 * @brief Checks whether the driver accepts the given connection URL.
 *
 * @param url The connection URL to test.
 * @return `true` if the driver accepts `url`, `false` otherwise.
 */
bool acceptsURL(const std::string &) override { return false; }
            /**
 * @brief Canonical name of the driver.
 *
 * @return std::string The driver name "scylladb".
 */
std::string getName() const noexcept override { return "scylladb"; }
        };
    }
}

#endif // USE_SCYLLADB

#endif // CPP_DBC_DRIVER_SCYLLA_HPP