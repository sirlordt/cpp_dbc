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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file driver_postgresql.hpp
 @brief PostgreSQL database driver implementation

*/

#ifndef CPP_DBC_DRIVER_POSTGRESQL_HPP
#define CPP_DBC_DRIVER_POSTGRESQL_HPP

#include "../../cpp_dbc.hpp"
#include "postgresql_blob.hpp"

#if USE_POSTGRESQL
#include <libpq-fe.h>
#include <map>
#include <memory>
#include <set>
#include <mutex>

// Thread-safety macros for conditional mutex locking
// Using recursive_mutex to allow the same thread to acquire the lock multiple times
// This is needed when a method that holds the lock calls another method that also needs the lock
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
    namespace PostgreSQL
    {
        /**
         * @brief Custom deleter for PGresult* to use with unique_ptr
         *
         * This deleter ensures that PQclear() is called automatically
         * when the unique_ptr goes out of scope, preventing memory leaks.
         */
        struct PGresultDeleter
        {
            void operator()(PGresult *result) const noexcept
            {
                if (result)
                {
                    PQclear(result);
                }
            }
        };

        /**
         * @brief Custom deleter for PGconn* to use with shared_ptr
         *
         * This deleter ensures that PQfinish() is called automatically
         * when the shared_ptr reference count reaches zero, preventing resource leaks.
         */
        struct PGconnDeleter
        {
            void operator()(PGconn *conn) const noexcept
            {
                if (conn)
                {
                    PQfinish(conn);
                }
            }
        };

        /**
         * @brief Type alias for the smart pointer managing PGresult
         *
         * Uses unique_ptr with custom deleter to ensure automatic cleanup
         * of PostgreSQL result sets, even in case of exceptions.
         */
        using PGresultHandle = std::unique_ptr<PGresult, PGresultDeleter>;

        /**
         * @brief Type alias for the smart pointer managing PGconn connection (shared_ptr for weak_ptr support)
         *
         * Uses shared_ptr so that PreparedStatements can use weak_ptr to safely
         * detect when the connection has been closed.
         * Note: The deleter is passed to the constructor, not as a template parameter.
         */
        using PGconnHandle = std::shared_ptr<PGconn>;

        class PostgreSQLDBResultSet final : public RelationalDBResultSet
        {
        private:
            /**
             * @brief Smart pointer for PGresult - automatically calls PQclear
             *
             * This is an OWNING pointer that manages the lifecycle of the PostgreSQL result set.
             * When this pointer is reset or destroyed, PQclear() is called automatically.
             */
            PGresultHandle m_result;
            int m_rowPosition{0};
            int m_rowCount{0};
            int m_fieldCount{0};
            std::vector<std::string> m_columnNames;
            std::map<std::string, int> m_columnMap;

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex; // Mutex for thread-safe ResultSet operations
#endif

        public:
            PostgreSQLDBResultSet(PGresult *res);
            ~PostgreSQLDBResultSet() override;

            // DBResultSet interface
            void close() override;
            bool isEmpty() override;

            // RelationalDBResultSet interface
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

            std::vector<std::string> getColumnNames() override;
            size_t getColumnCount() override;

            // BLOB support methods
            std::shared_ptr<Blob> getBlob(size_t columnIndex) override;
            std::shared_ptr<Blob> getBlob(const std::string &columnName) override;

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
            cpp_dbc::expected<int, DBException> getInt(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<long, DBException> getLong(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<double, DBException> getDouble(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::string, DBException> getString(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<bool, DBException> getBoolean(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<bool, DBException> isNull(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::vector<std::string>, DBException> getColumnNames(std::nothrow_t) noexcept override;
            cpp_dbc::expected<size_t, DBException> getColumnCount(std::nothrow_t) noexcept override;
            cpp_dbc::expected<std::shared_ptr<Blob>, DBException> getBlob(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::shared_ptr<Blob>, DBException> getBlob(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::shared_ptr<InputStream>, DBException> getBinaryStream(std::nothrow_t, const std::string &columnName) noexcept override;
            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, size_t columnIndex) noexcept override;
            cpp_dbc::expected<std::vector<uint8_t>, DBException> getBytes(std::nothrow_t, const std::string &columnName) noexcept override;
        };

        class PostgreSQLDBPreparedStatement final : public RelationalDBPreparedStatement
        {
            friend class PostgreSQLDBConnection;

        private:
            std::weak_ptr<PGconn> m_conn; // Safe weak reference to connection - detects when connection is closed
            std::string m_sql;
            std::string m_stmtName;
            std::vector<std::string> m_paramValues;
            std::vector<size_t> m_paramLengths;
            std::vector<int> m_paramFormats;
            std::vector<Oid> m_paramTypes;
            bool m_prepared{false};
            int m_statementCounter{0};
            std::vector<std::vector<uint8_t>> m_blobValues;            // To keep blob values alive
            std::vector<std::shared_ptr<Blob>> m_blobObjects;          // To keep blob objects alive
            std::vector<std::shared_ptr<InputStream>> m_streamObjects; // To keep stream objects alive

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex; // Mutex for thread-safe PreparedStatement operations
#endif

            // Internal method called by connection when closing
            void notifyConnClosing();

            // Helper method to process SQL and count parameters
            int processSQL(std::string &sqlQuery) const;

            // Helper method to get PGconn* safely, throws if connection is closed
            PGconn *getPGConnection() const;

        public:
            PostgreSQLDBPreparedStatement(std::weak_ptr<PGconn> conn, const std::string &sql, const std::string &stmt_name);
            ~PostgreSQLDBPreparedStatement() override;

            void setInt(int parameterIndex, int value) override;
            void setLong(int parameterIndex, long value) override;
            void setDouble(int parameterIndex, double value) override;
            void setString(int parameterIndex, const std::string &value) override;
            void setBoolean(int parameterIndex, bool value) override;
            void setNull(int parameterIndex, Types type) override;
            void setDate(int parameterIndex, const std::string &value) override;
            void setTimestamp(int parameterIndex, const std::string &value) override;

            // BLOB support methods
            void setBlob(int parameterIndex, std::shared_ptr<Blob> x) override;
            void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x) override;
            void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length) override;
            void setBytes(int parameterIndex, const std::vector<uint8_t> &x) override;
            void setBytes(int parameterIndex, const uint8_t *x, size_t length) override;

            std::shared_ptr<RelationalDBResultSet> executeQuery() override;
            uint64_t executeUpdate() override;
            bool execute() override;
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
            cpp_dbc::expected<void, DBException> setBlob(std::nothrow_t, int parameterIndex, std::shared_ptr<Blob> x) noexcept override;
            cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x) noexcept override;
            cpp_dbc::expected<void, DBException> setBinaryStream(std::nothrow_t, int parameterIndex, std::shared_ptr<InputStream> x, size_t length) noexcept override;
            cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const std::vector<uint8_t> &x) noexcept override;
            cpp_dbc::expected<void, DBException> setBytes(std::nothrow_t, int parameterIndex, const uint8_t *x, size_t length) noexcept override;
            cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> executeQuery(std::nothrow_t) noexcept override;
            cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t) noexcept override;
            cpp_dbc::expected<bool, DBException> execute(std::nothrow_t) noexcept override;
            cpp_dbc::expected<void, DBException> close(std::nothrow_t) noexcept override;
        };

        class PostgreSQLDBConnection final : public RelationalDBConnection
        {
        private:
            PGconnHandle m_conn; // shared_ptr allows PreparedStatements to use weak_ptr
            bool m_closed{true};
            bool m_autoCommit{true};
            bool m_transactionActive{false};
            int m_statementCounter{0};
            TransactionIsolationLevel m_isolationLevel{TransactionIsolationLevel::TRANSACTION_READ_COMMITTED};

            // Cached URL string
            std::string m_url;

            // Registry of active prepared statements
            std::set<std::shared_ptr<PostgreSQLDBPreparedStatement>> m_activeStatements;
            std::mutex m_statementsMutex;

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_connMutex; // Mutex for thread-safe Connection operations
#endif

            // Internal methods for statement registry
            void registerStatement(std::shared_ptr<PostgreSQLDBPreparedStatement> stmt);
            void unregisterStatement(std::shared_ptr<PostgreSQLDBPreparedStatement> stmt);

        public:
            PostgreSQLDBConnection(const std::string &host,
                                   int port,
                                   const std::string &database,
                                   const std::string &user,
                                   const std::string &password,
                                   const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
            ~PostgreSQLDBConnection() override;

            // DBConnection interface
            void close() override;
            bool isClosed() override;
            void returnToPool() override;
            bool isPooled() override;
            std::string getURL() const override;

            // RelationalDBConnection interface
            std::shared_ptr<RelationalDBPreparedStatement> prepareStatement(const std::string &sql) override;
            std::shared_ptr<RelationalDBResultSet> executeQuery(const std::string &sql) override;
            uint64_t executeUpdate(const std::string &sql) override;

            void setAutoCommit(bool autoCommit) override;
            bool getAutoCommit() override;

            bool beginTransaction() override;
            bool transactionActive() override;

            void commit() override;
            void rollback() override;

            // Transaction isolation level methods
            void setTransactionIsolation(TransactionIsolationLevel level) override;
            TransactionIsolationLevel getTransactionIsolation() override;

            // Helper to generate unique statement names
            std::string generateStatementName();

            // Nothrow API
            cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException> prepareStatement(std::nothrow_t, const std::string &sql) noexcept override;
            cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException> executeQuery(std::nothrow_t, const std::string &sql) noexcept override;
            cpp_dbc::expected<uint64_t, DBException> executeUpdate(std::nothrow_t, const std::string &sql) noexcept override;
            cpp_dbc::expected<void, DBException> setAutoCommit(std::nothrow_t, bool autoCommit) noexcept override;
            cpp_dbc::expected<bool, DBException> getAutoCommit(std::nothrow_t) noexcept override;
            cpp_dbc::expected<bool, DBException> beginTransaction(std::nothrow_t) noexcept override;
            cpp_dbc::expected<bool, DBException> transactionActive(std::nothrow_t) noexcept override;
            cpp_dbc::expected<void, DBException> commit(std::nothrow_t) noexcept override;
            cpp_dbc::expected<void, DBException> rollback(std::nothrow_t) noexcept override;
            cpp_dbc::expected<void, DBException> setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override;
            cpp_dbc::expected<TransactionIsolationLevel, DBException> getTransactionIsolation(std::nothrow_t) noexcept override;
        };

        class PostgreSQLDBDriver final : public RelationalDBDriver
        {
        public:
            PostgreSQLDBDriver();
            ~PostgreSQLDBDriver() override;

            std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                      const std::string &user,
                                                                      const std::string &password,
                                                                      const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool acceptsURL(const std::string &url) override;

            // Parses a JDBC-like URL: jdbc:postgresql://host:port/database
            bool parseURL(const std::string &url,
                          std::string &host,
                          int &port,
                          std::string &database);

            // Nothrow API
            cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
                std::nothrow_t,
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

            std::string getName() const noexcept override;
        };

    } // namespace PostgreSQL
} // namespace cpp_dbc

#else // USE_POSTGRESQL

// Stub implementations when PostgreSQL is disabled
namespace cpp_dbc
{
    namespace PostgreSQL
    {
        // Forward declarations only
        class PostgreSQLDBDriver final : public RelationalDBDriver
        {
        public:
            PostgreSQLDBDriver()
            {
                throw DBException("3FE734D0BDE9", "PostgreSQL support is not enabled in this build");
            }
            ~PostgreSQLDBDriver() override = default;

            PostgreSQLDBDriver(const PostgreSQLDBDriver &) = delete;
            PostgreSQLDBDriver &operator=(const PostgreSQLDBDriver &) = delete;
            PostgreSQLDBDriver(PostgreSQLDBDriver &&) = delete;
            PostgreSQLDBDriver &operator=(PostgreSQLDBDriver &&) = delete;

            [[noreturn]] std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &,
                                                                      const std::string &,
                                                                      const std::string &,
                                                                      const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override
            {
                throw DBException("E39F6F23D06B", "PostgreSQL support is not enabled in this build");
            }

            bool acceptsURL(const std::string & /*url*/) override
            {
                return false;
            }
        };
    } // namespace PostgreSQL
} // namespace cpp_dbc

#endif // USE_POSTGRESQL

#endif // CPP_DBC_DRIVER_POSTGRESQL_HPP
