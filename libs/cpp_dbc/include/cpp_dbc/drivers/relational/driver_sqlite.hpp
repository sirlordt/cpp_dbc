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

 @file driver_sqlite.hpp
 @brief SQLite database driver implementation

*/

#ifndef CPP_DBC_DRIVER_SQLITE_HPP
#define CPP_DBC_DRIVER_SQLITE_HPP

#include "../../cpp_dbc.hpp"
#include "sqlite_blob.hpp"

#ifndef USE_SQLITE
#define USE_SQLITE 0 // Default to disabled
#endif

#if USE_SQLITE
#include <sqlite3.h>
#include <map>
#include <memory>
#include <set>
#include <mutex>
#include <vector>
#include <string>
#include <atomic>

namespace cpp_dbc::SQLite
{
#if DB_DRIVER_THREAD_SAFE
    /**
     * @brief Shared mutex type for connection and prepared statements
     *
     * This shared_ptr to a recursive_mutex ensures that the Connection and all its
     * PreparedStatements share the SAME mutex. This prevents race conditions when:
     * - PreparedStatement destructor calls sqlite3_finalize()
     * - Another thread uses the connection for queries or pool validation
     *
     * Although SQLite is an embedded database without network protocol concerns,
     * concurrent access to the sqlite3* handle from multiple threads is still unsafe.
     */
    using SharedConnMutex = std::shared_ptr<std::recursive_mutex>;
#endif

    /**
     * @brief Custom deleter for sqlite3_stmt* to use with unique_ptr
     *
     * This deleter ensures that sqlite3_finalize() is called automatically
     * when the unique_ptr goes out of scope, preventing memory leaks.
     */
    struct SQLiteStmtDeleter
    {
        void operator()(sqlite3_stmt *stmt) const noexcept
        {
            if (stmt)
            {
                sqlite3_finalize(stmt);
            }
        }
    };

    /**
     * @brief Type alias for the smart pointer managing sqlite3_stmt
     *
     * Uses unique_ptr with custom deleter to ensure automatic cleanup
     * of SQLite statements, even in case of exceptions.
     */
    using SQLiteStmtHandle = std::unique_ptr<sqlite3_stmt, SQLiteStmtDeleter>;

    /**
     * @brief Custom deleter for sqlite3* to use with shared_ptr
     *
     * This deleter ensures that sqlite3_close_v2() is called automatically
     * when the shared_ptr reference count reaches zero, preventing resource leaks.
     */
    struct SQLiteDbDeleter
    {
        void operator()(sqlite3 *db) const noexcept
        {
            if (db)
            {
                // Finalize all remaining statements before closing
                sqlite3_stmt *stmt;
                while ((stmt = sqlite3_next_stmt(db, nullptr)) != nullptr)
                {
                    sqlite3_finalize(stmt);
                }
                sqlite3_close_v2(db);
            }
        }
    };

    // Forward declaration
    class SQLiteDBConnection;

    /**
     * @brief Type alias for the smart pointer managing sqlite3 connection (shared_ptr for weak_ptr support)
     *
     * Uses shared_ptr to allow PreparedStatements to use weak_ptr for safe connection detection.
     * Note: The deleter is passed to the constructor, not as a template parameter.
     */
    using SQLiteDbHandle = std::shared_ptr<sqlite3>;

    /**
     * @brief Factory helper to create SQLiteDbHandle with proper deleter
     *
     * This ensures that sqlite3_close_v2() is always called correctly when
     * the handle is destroyed. Use this instead of constructing SQLiteDbHandle directly.
     *
     * @param db Raw pointer to sqlite3 connection (takes ownership)
     * @return SQLiteDbHandle with proper deleter attached
     */
    inline SQLiteDbHandle makeSQLiteDbHandle(sqlite3 *db)
    {
        return SQLiteDbHandle(db, SQLiteDbDeleter{});
    }

    /**
     * @brief SQLite ResultSet implementation
     *
     * IMPORTANT: Thread-Safety and Shared Mutex Design
     * ================================================
     *
     * Unlike MySQL and PostgreSQL, SQLite ResultSets REQUIRE a shared mutex with the
     * Connection because SQLite uses a CURSOR-BASED model that maintains ongoing
     * communication with the database connection.
     *
     * WHY SQLITE/FIREBIRD NEED SharedConnMutex (but MySQL/PostgreSQL don't):
     *
     * MySQL/PostgreSQL ("Store Result" model):
     * - mysql_store_result() / PQexec() fetch ALL data into client memory (MYSQL_RES* / PGresult*)
     * - next() just reads from in-memory structures, no DB communication
     * - close() only frees client memory (mysql_free_result / PQclear)
     * - The result is completely INDEPENDENT of the connection handle
     * - Therefore, no shared mutex is needed
     *
     * SQLite/Firebird ("Cursor" model):
     * - sqlite3_step() / isc_dsql_fetch() communicate with DB for EACH row
     * - sqlite3_column_*() functions access the connection handle internally
     * - sqlite3_finalize() / isc_dsql_free_statement() access the connection handle
     * - Concurrent access from multiple threads causes undefined behavior/crashes
     *
     * RACE CONDITION SCENARIO (without SharedConnMutex):
     * Thread A: resultSet->next() -> sqlite3_step() [uses sqlite3* handle]
     * Thread B: connection->isValid() -> SELECT 1 [uses same sqlite3* handle]
     * Result: Memory corruption, crashes, undefined behavior
     *
     * SOLUTION:
     * ResultSet shares the SAME mutex as Connection and PreparedStatements,
     * ensuring all operations on the sqlite3* handle are serialized.
     */
    class SQLiteDBResultSet final : public RelationalDBResultSet
    {
    private:
        /**
         * @brief Raw pointer to sqlite3_stmt
         *
         * IMPORTANT: This is intentionally a raw pointer, NOT a smart pointer, because:
         *
         * 1. When m_ownStatement is true, we own the statement and must finalize it
         *    BUT only if the connection is still valid (not closed).
         *
         * 2. When m_ownStatement is false, the statement is owned by a PreparedStatement
         *    and should not be finalized by the ResultSet.
         *
         * 3. The connection's close() method uses sqlite3_next_stmt() to finalize ALL
         *    statements, so if we try to finalize after connection close, we get double-free.
         *
         * 4. Protection is provided through:
         *    - m_ownStatement flag that determines ownership
         *    - m_connection weak_ptr to check if connection is still valid
         *    - Only finalize if we own AND connection is still valid
         */
        sqlite3_stmt *m_stmt{nullptr};

        bool m_ownStatement;
        size_t m_rowPosition{0};
        size_t m_rowCount{0};
        size_t m_fieldCount{0};
        std::vector<std::string> m_columnNames;
        std::map<std::string, size_t> m_columnMap;
        bool m_hasData{false};
        bool m_closed{true};
        std::weak_ptr<SQLiteDBConnection> m_connection; // Weak reference to the connection

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared mutex with the parent Connection
         *
         * CRITICAL: This is shared with Connection and PreparedStatements because
         * SQLite uses cursor-based iteration. Unlike MySQL/PostgreSQL where results
         * are fully loaded into client memory, SQLite's sqlite3_step() and
         * sqlite3_column_*() functions communicate with the sqlite3* connection
         * handle on every call. Without this shared mutex, concurrent operations
         * (e.g., pool validation while iterating results) would cause race conditions.
         *
         * See class documentation above for detailed explanation.
         */
        SharedConnMutex m_connMutex;
#endif

        /**
         * @brief Helper method to get the active statement pointer
         * @return The active statement pointer
         */
        sqlite3_stmt *getStmt() const
        {
            return m_stmt;
        }

    public:
#if DB_DRIVER_THREAD_SAFE
        SQLiteDBResultSet(sqlite3_stmt *stmt, bool ownStatement, std::shared_ptr<SQLiteDBConnection> conn, SharedConnMutex connMutex);
#else
        SQLiteDBResultSet(sqlite3_stmt *stmt, bool ownStatement = true, std::shared_ptr<SQLiteDBConnection> conn = nullptr);
#endif
        ~SQLiteDBResultSet() override;

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
        void close() override;
        bool isEmpty() override;

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

    class SQLiteDBPreparedStatement final : public RelationalDBPreparedStatement
    {
        friend class SQLiteDBConnection;

    private:
        /**
         * @brief Safe weak reference to connection - detects when connection is closed
         *
         * Uses weak_ptr to safely detect when the connection has been closed,
         * preventing use-after-free errors.
         */
        std::weak_ptr<sqlite3> m_db;

        std::string m_sql;

        /**
         * @brief Smart pointer for sqlite3_stmt - automatically calls sqlite3_finalize
         *
         * This is an OWNING pointer that manages the lifecycle of the SQLite statement.
         * When this pointer is reset or destroyed, sqlite3_finalize() is called automatically.
         */
        SQLiteStmtHandle m_stmt;

        bool m_closed{true};
        std::vector<std::vector<uint8_t>> m_blobValues;            // To keep blob values alive
        std::vector<std::shared_ptr<Blob>> m_blobObjects;          // To keep blob objects alive
        std::vector<std::shared_ptr<InputStream>> m_streamObjects; // To keep stream objects alive

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared mutex with the parent Connection
         *
         * This mutex is shared between the Connection and all PreparedStatements created from it.
         * This ensures that when close() calls sqlite3_finalize(), no other thread can
         * simultaneously use the same sqlite3* handle.
         */
        SharedConnMutex m_connMutex;
#endif

        // Internal method called by connection when closing
        void notifyConnClosing();

        // Helper method to get sqlite3* safely, throws if connection is closed
        sqlite3 *getSQLiteConnection() const;

    public:
#if DB_DRIVER_THREAD_SAFE
        SQLiteDBPreparedStatement(std::weak_ptr<sqlite3> db, SharedConnMutex connMutex, const std::string &sql);
#else
        SQLiteDBPreparedStatement(std::weak_ptr<sqlite3> db, const std::string &sql);
#endif
        ~SQLiteDBPreparedStatement() override;

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

    class SQLiteDBConnection final : public RelationalDBConnection, public std::enable_shared_from_this<SQLiteDBConnection>
    {
        friend class SQLiteDBPreparedStatement;
        friend class SQLiteDBResultSet;

    private:
        /**
         * @brief Smart pointer for sqlite3 connection - shared_ptr allows weak_ptr support
         *
         * Uses shared_ptr to allow PreparedStatements to use weak_ptr for safe
         * connection detection. The custom deleter ensures sqlite3_close_v2() is called.
         */
        SQLiteDbHandle m_db;

        bool m_closed{true};
        bool m_autoCommit{true};
        bool m_transactionActive{false};
        TransactionIsolationLevel m_isolationLevel;

        // Cached URL
        std::string m_url;

        // Registry of active prepared statements (weak pointers to avoid preventing destruction)
        std::set<std::weak_ptr<SQLiteDBPreparedStatement>, std::owner_less<std::weak_ptr<SQLiteDBPreparedStatement>>> m_activeStatements;
        std::mutex m_statementsMutex;

#if DB_DRIVER_THREAD_SAFE
        /**
         * @brief Shared mutex for connection and all its prepared statements
         *
         * This mutex is shared with all PreparedStatements created from this connection.
         * Ensures that statement close operations (sqlite3_finalize) don't race
         * with other operations on the sqlite3* handle.
         */
        mutable SharedConnMutex m_connMutex;
#endif

        // Internal methods for statement registry
        void registerStatement(std::weak_ptr<SQLiteDBPreparedStatement> stmt);
        void unregisterStatement(std::weak_ptr<SQLiteDBPreparedStatement> stmt);
        void closeAllStatements();

    public:
        SQLiteDBConnection(const std::string &database,
                           const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
        ~SQLiteDBConnection() override;

        void close() override;
        bool isClosed() const override;
        void returnToPool() override;
        bool isPooled() override;

        std::shared_ptr<RelationalDBPreparedStatement> prepareStatement(const std::string &sql) override;
        std::shared_ptr<RelationalDBResultSet> executeQuery(const std::string &sql) override;
        uint64_t executeUpdate(const std::string &sql) override;

        void setAutoCommit(bool autoCommit) override;
        bool getAutoCommit() override;

        bool beginTransaction() override;
        bool transactionActive() override;

        void commit() override;
        void rollback() override;
        void prepareForPoolReturn() override;

        // Transaction isolation level methods
        void setTransactionIsolation(TransactionIsolationLevel level) override;
        TransactionIsolationLevel getTransactionIsolation() override;

        // Get the connection URL
        std::string getURL() const override;

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

    class SQLiteDBDriver final : public RelationalDBDriver
    {
    private:
        // Static members to ensure SQLite is configured only once
        static std::atomic<bool> s_initialized;
        static std::mutex s_initMutex;

    public:
        SQLiteDBDriver();
        ~SQLiteDBDriver() override;

        std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                  const std::string &user,
                                                                  const std::string &password,
                                                                  const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

        bool acceptsURL(const std::string &url) override;

        // Parses a URL: cpp_dbc:sqlite:/path/to/database.db or cpp_dbc:sqlite::memory:
        bool parseURL(const std::string &url, std::string &database);

        // Nothrow API
        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string &url,
            const std::string &user,
            const std::string &password,
            const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

        std::string getName() const noexcept override;
    };

} // namespace cpp_dbc::SQLite

#else // USE_SQLITE

// Stub implementations when SQLite is disabled
namespace cpp_dbc::SQLite
{
    // Forward declarations only
    class SQLiteDBDriver final : public RelationalDBDriver
    {
    public:
        [[noreturn]] SQLiteDBDriver()
        {
            throw DBException("C27AD46A860B", "SQLite support is not enabled in this build", system_utils::captureCallStack());
        }
        ~SQLiteDBDriver() override = default;

        SQLiteDBDriver(const SQLiteDBDriver &) = delete;
        SQLiteDBDriver &operator=(const SQLiteDBDriver &) = delete;
        SQLiteDBDriver(SQLiteDBDriver &&) = delete;
        SQLiteDBDriver &operator=(SQLiteDBDriver &&) = delete;

        [[noreturn]] std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                               const std::string &user,
                                                                               const std::string &password,
                                                                               const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
        {
            throw DBException("269CC140F035", "SQLite support is not enabled in this build", system_utils::captureCallStack());
        }

        bool acceptsURL(const std::string & /*url*/) override
        {
            return false;
        }

        cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
            std::nothrow_t,
            const std::string & /*url*/,
            const std::string & /*user*/,
            const std::string & /*password*/,
            const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
        {
            return cpp_dbc::unexpected(DBException("269CC140F036", "SQLite support is not enabled in this build", system_utils::captureCallStack()));
        }

        std::string getName() const noexcept override
        {
            return "SQLite (disabled)";
        }
    };
} // namespace cpp_dbc::SQLite

#endif // USE_SQLITE

#endif // CPP_DBC_DRIVER_SQLITE_HPP
