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

 @file driver_mysql.hpp
 @brief MySQL database driver implementation

*/

#ifndef CPP_DBC_DRIVER_MYSQL_HPP
#define CPP_DBC_DRIVER_MYSQL_HPP

#include "../../cpp_dbc.hpp"
#include "mysql_blob.hpp"

#if USE_MYSQL
#include <mysql/mysql.h>
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

namespace cpp_dbc::MySQL
{
        /**
         * @brief Custom deleter for MYSQL_RES* to use with unique_ptr
         *
         * This deleter ensures that mysql_free_result() is called automatically
         * when the unique_ptr goes out of scope, preventing memory leaks.
         */
        struct MySQLResDeleter
        {
            void operator()(MYSQL_RES *res) const noexcept
            {
                if (res)
                {
                    mysql_free_result(res);
                }
            }
        };

        /**
         * @brief Type alias for the smart pointer managing MYSQL_RES
         *
         * Uses unique_ptr with custom deleter to ensure automatic cleanup
         * of MySQL result sets, even in case of exceptions.
         */
        using MySQLResHandle = std::unique_ptr<MYSQL_RES, MySQLResDeleter>;

        class MySQLDBResultSet final : public RelationalDBResultSet
        {
        private:
            /**
             * @brief Smart pointer for MYSQL_RES - automatically calls mysql_free_result
             *
             * This is an OWNING pointer that manages the lifecycle of the MySQL result set.
             * When this pointer is reset or destroyed, mysql_free_result() is called automatically.
             */
            MySQLResHandle m_result;

            /**
             * @brief Non-owning pointer to internal data within m_result
             *
             * IMPORTANT: This is intentionally a raw pointer, NOT a smart pointer, because:
             *
             * 1. MYSQL_ROW is a typedef for char** - it points to internal memory managed
             *    by the MYSQL_RES structure, not separately allocated memory.
             *
             * 2. This memory is automatically managed by the MySQL library:
             *    - It is invalidated when mysql_fetch_row() is called again
             *    - It is freed automatically when mysql_free_result() is called on m_result
             *
             * 3. Using a smart pointer here would cause DOUBLE-FREE errors because:
             *    - The smart pointer would try to free memory it doesn't own
             *    - mysql_free_result() would also try to free the same memory
             *
             * 4. Protection is provided through:
             *    - validateCurrentRow() method that checks both m_result and m_currentRow
             *    - Explicit nullification in close() and next() when appropriate
             *    - Exception throwing when accessing invalid state
             */
            MYSQL_ROW m_currentRow{nullptr};

            size_t m_rowPosition{0};
            size_t m_rowCount{0};
            size_t m_fieldCount{0};
            std::vector<std::string> m_columnNames;
            std::map<std::string, size_t> m_columnMap;

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex; // Mutex for thread-safe ResultSet operations
#endif

            /**
             * @brief Validates that the result set is still valid (not closed)
             * @throws DBException if m_result is nullptr
             */
            void validateResultState() const;

            /**
             * @brief Validates that there is a current row to read from
             * @throws DBException if m_result is nullptr or m_currentRow is nullptr
             */
            void validateCurrentRow() const;

        public:
            explicit MySQLDBResultSet(MYSQL_RES *res);
            ~MySQLDBResultSet() override;

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

        // Custom deleter for MYSQL_STMT* to use with unique_ptr
        struct MySQLStmtDeleter
        {
            void operator()(MYSQL_STMT *stmt) const noexcept
            {
                if (stmt)
                {
                    mysql_stmt_close(stmt);
                }
            }
        };

        // Type alias for the smart pointer managing MYSQL_STMT
        using MySQLStmtHandle = std::unique_ptr<MYSQL_STMT, MySQLStmtDeleter>;

        class MySQLDBPreparedStatement final : public RelationalDBPreparedStatement
        {
            friend class MySQLDBConnection;

            std::weak_ptr<MYSQL> m_mysql; // Safe weak reference to connection - detects when connection is closed
            std::string m_sql;
            MySQLStmtHandle m_stmt{nullptr}; // Smart pointer for MYSQL_STMT - automatically calls mysql_stmt_close
            std::vector<MYSQL_BIND> m_binds;
            std::vector<std::string> m_stringValues;                   // To keep string values alive
            std::vector<std::string> m_parameterValues;                // To store parameter values for query reconstruction
            std::vector<int> m_intValues;                              // To keep int values alive
            std::vector<long> m_longValues;                            // To keep long values alive
            std::vector<double> m_doubleValues;                        // To keep double values alive
            std::vector<char> m_nullFlags;                             // To keep null flags alive (char instead of bool for pointer access)
            std::vector<std::vector<uint8_t>> m_blobValues;            // To keep blob values alive
            std::vector<std::shared_ptr<Blob>> m_blobObjects;          // To keep blob objects alive
            std::vector<std::shared_ptr<InputStream>> m_streamObjects; // To keep stream objects alive

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex; // Mutex for thread-safe PreparedStatement operations
#endif

            // Internal method called by connection when closing
            void notifyConnClosing();

            // Helper method to get MYSQL* safely, throws if connection is closed
            MYSQL *getMySQLConnection() const;

        public:
            MySQLDBPreparedStatement(std::weak_ptr<MYSQL> mysql, const std::string &sql);
            ~MySQLDBPreparedStatement() override;

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

        // Custom deleter for MYSQL* to use with shared_ptr
        struct MySQLDeleter
        {
            void operator()(MYSQL *mysql) const noexcept
            {
                if (mysql)
                {
                    mysql_close(mysql);
                }
            }
        };

        // Type alias for the smart pointer managing MYSQL connection (shared_ptr for weak_ptr support)
        // Note: The deleter is passed to the constructor, not as a template parameter
        using MySQLHandle = std::shared_ptr<MYSQL>;

        class MySQLDBConnection final : public RelationalDBConnection
        {
        private:
            MySQLHandle m_mysql; // shared_ptr allows PreparedStatements to use weak_ptr
            bool m_closed{true};
            bool m_autoCommit{true};
            bool m_transactionActive{false};
            TransactionIsolationLevel m_isolationLevel{TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ}; // MySQL default

            // Cached URL string
            std::string m_url;

            // Registry of active prepared statements
            std::set<std::shared_ptr<MySQLDBPreparedStatement>> m_activeStatements;
            std::mutex m_statementsMutex;

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_connMutex; // Mutex for thread-safe Connection operations
#endif

            // Internal methods for statement registry
            void registerStatement(std::shared_ptr<MySQLDBPreparedStatement> stmt);
            void unregisterStatement(std::shared_ptr<MySQLDBPreparedStatement> stmt);

        public:
            MySQLDBConnection(const std::string &host,
                              int port,
                              const std::string &database,
                              const std::string &user,
                              const std::string &password,
                              const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
            ~MySQLDBConnection() override;

            // Rule of 5: Non-copyable and non-movable (mutex member prevents copying/moving)
            MySQLDBConnection(const MySQLDBConnection &) = delete;
            MySQLDBConnection &operator=(const MySQLDBConnection &) = delete;
            MySQLDBConnection(MySQLDBConnection &&) = delete;
            MySQLDBConnection &operator=(MySQLDBConnection &&) = delete;

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

        class MySQLDBDriver final : public RelationalDBDriver
        {
        public:
            MySQLDBDriver();
            ~MySQLDBDriver() override;

            std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                      const std::string &user,
                                                                      const std::string &password,
                                                                      const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool acceptsURL(const std::string &url) override;

            // Parses a JDBC-like URL: cpp_dbc:mysql://host:port/database
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

} // namespace cpp_dbc::MySQL

#else // USE_MYSQL

// Stub implementations when MySQL is disabled
namespace cpp_dbc::MySQL
{
        // Forward declarations only
        class MySQLDBDriver final : public RelationalDBDriver
        {
        public:
            MySQLDBDriver()
            {
                throw DBException("4FE1EBBEA99F", "MySQL support is not enabled in this build");
            }
            ~MySQLDBDriver() override = default;

            [[noreturn]] std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &,
                                                                      const std::string &,
                                                                      const std::string &,
                                                                      const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override
            {
                throw DBException("23D2107DA64F", "MySQL support is not enabled in this build");
            }

            bool acceptsURL(const std::string & /*url*/) override
            {
                return false;
            }
        };
} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL

#endif // CPP_DBC_DRIVER_MYSQL_HPP
