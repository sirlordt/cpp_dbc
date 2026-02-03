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

        /**
         * @brief MySQL ResultSet implementation using the "Store Result" model
         *
         * @details
         * **IMPORTANT ARCHITECTURAL NOTE - "Store Result" Model:**
         *
         * MySQL uses a "store result" model where mysql_store_result() fetches ALL rows
         * from the server into client memory at query execution time. This is fundamentally
         * different from SQLite/Firebird's cursor-based iteration.
         *
         * **HOW IT WORKS:**
         *
         * 1. Query execution calls mysql_store_result() which:
         *    - Fetches ALL rows from the MySQL server
         *    - Stores them in a client-side MYSQL_RES* structure
         *    - This structure is INDEPENDENT of the MYSQL* connection handle
         *
         * 2. ResultSet operations (next(), getString(), etc.):
         *    - mysql_fetch_row() reads from local memory (MYSQL_RES*), NOT from server
         *    - mysql_data_seek() repositions within local memory
         *    - These operations do NOT communicate with the database connection
         *
         * 3. ResultSet close:
         *    - mysql_free_result() only frees the local MYSQL_RES* memory
         *    - Does NOT communicate with the connection or server
         *
         * **WHY THE MUTEX IS INDEPENDENT (NOT SHARED WITH CONNECTION):**
         *
         * Unlike SQLite/Firebird where each next() call communicates with the connection,
         * MySQL ResultSet operations are purely local memory operations on MYSQL_RES*.
         * Therefore:
         *
         * - No race condition with connection operations (pool validation, queries, etc.)
         * - The ResultSet mutex (m_mutex) only protects internal state consistency
         * - It does NOT need to be the same mutex as the connection's m_connMutex
         *
         * **WHAT HAPPENS IF THE CONNECTION IS CLOSED:**
         *
         * If the parent connection is closed while a ResultSet is still open:
         *
         * 1. The ResultSet REMAINS FULLY VALID and usable
         * 2. All data is already in the MYSQL_RES* structure (client memory)
         * 3. next(), getString(), getInt(), etc. continue to work normally
         * 4. close() still works (just frees MYSQL_RES* memory)
         *
         * This is in stark contrast to SQLite/Firebird where closing the connection
         * would invalidate the ResultSet because cursor iteration requires the connection.
         *
         * **COMPARISON WITH CURSOR-BASED DRIVERS (SQLite/Firebird):**
         *
         * | Aspect                    | MySQL/PostgreSQL          | SQLite/Firebird           |
         * |---------------------------|---------------------------|---------------------------|
         * | Data location             | Client memory (MYSQL_RES*)| Server-side cursor        |
         * | next() communication      | Local memory read         | Connection handle call    |
         * | Connection dependency     | Only at query time        | Throughout iteration      |
         * | Shared mutex needed       | NO                        | YES                       |
         * | Valid after conn close    | YES (data in memory)      | NO (cursor invalidated)   |
         *
         * @see MySQLDBConnection - Creates ResultSets via executeQuery()
         * @see SQLiteDBResultSet - Contrast: Uses shared mutex due to cursor model
         * @see FirebirdDBResultSet - Contrast: Uses shared mutex due to cursor model
         */
        class MySQLDBResultSet final : public RelationalDBResultSet
        {
        private:
            /**
             * @brief Smart pointer for MYSQL_RES - automatically calls mysql_free_result
             *
             * This is an OWNING pointer that manages the lifecycle of the MySQL result set.
             * When this pointer is reset or destroyed, mysql_free_result() is called automatically.
             *
             * @note This structure contains ALL result data in client memory, independent
             * of the MYSQL* connection handle. The connection can be closed and this
             * ResultSet remains valid.
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
            /**
             * @brief Independent mutex for thread-safe ResultSet operations
             *
             * @details
             * This mutex is INDEPENDENT of the connection's mutex (m_connMutex) because:
             *
             * 1. **No connection communication**: All ResultSet operations (next(), getString(),
             *    etc.) only access the MYSQL_RES* structure in client memory. They do NOT
             *    communicate with the MYSQL* connection handle.
             *
             * 2. **No race condition possible**: Since we never touch the connection, there's
             *    no risk of racing with connection operations (pool validation, new queries, etc.)
             *
             * 3. **Self-contained protection**: This mutex only needs to protect the internal
             *    state of THIS ResultSet (m_currentRow, m_rowPosition) from concurrent access
             *    to THIS same ResultSet instance.
             *
             * **CONTRAST WITH SQLite/Firebird:**
             *
             * SQLite and Firebird use cursor-based iteration where each next() call invokes
             * sqlite3_step() or isc_dsql_fetch() which communicate with the connection handle.
             * Those drivers MUST share the connection mutex to prevent race conditions.
             *
             * MySQL's "store result" model eliminates this coupling entirely.
             */
            mutable std::recursive_mutex m_mutex;
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

        /**
         * @brief Type alias for shared connection mutex
         *
         * @details
         * This shared_ptr<recursive_mutex> is shared between a MySQLDBConnection and all
         * its PreparedStatements. This ensures that ALL operations that use the MYSQL*
         * connection (including mysql_stmt_close() in PreparedStatement destructors) are
         * serialized through the same mutex.
         *
         * **THE PROBLEM IT SOLVES:**
         *
         * Without a shared mutex, PreparedStatement uses its own mutex (m_mutex) while
         * Connection uses m_connMutex. When a PreparedStatement destructor runs (calling
         * mysql_stmt_close), it only locks its own mutex - NOT the connection's mutex.
         * This allows the destructor to run concurrently with connection operations in
         * other threads (like pool validation queries), causing use-after-free corruption.
         *
         * **HOW IT WORKS:**
         *
         * 1. Connection creates a shared mutex: m_connMutex = std::make_shared<std::recursive_mutex>()
         * 2. When creating a PreparedStatement, Connection passes its shared mutex
         * 3. PreparedStatement stores the same shared mutex
         * 4. ALL operations on both Connection and PreparedStatement lock the SAME mutex
         * 5. This includes PreparedStatement destructor calling mysql_stmt_close()
         * 6. Result: No race conditions possible
         */
        using SharedConnMutex = std::shared_ptr<std::recursive_mutex>;

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
            /**
             * @brief Shared mutex with the parent connection
             *
             * This is the SAME mutex instance as the connection's m_connMutex.
             * All operations on both Connection and PreparedStatement lock this mutex,
             * ensuring mysql_stmt_close() in the destructor never races with other
             * connection operations.
             */
            SharedConnMutex m_connMutex;
#endif

            // Internal method called by connection when closing
            void notifyConnClosing();

            // Helper method to get MYSQL* safely, throws if connection is closed
            MYSQL *getMySQLConnection() const;

        public:
#if DB_DRIVER_THREAD_SAFE
            MySQLDBPreparedStatement(std::weak_ptr<MYSQL> mysql, SharedConnMutex connMutex, const std::string &sql);
#else
            MySQLDBPreparedStatement(std::weak_ptr<MYSQL> mysql, const std::string &sql);
#endif
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

        class MySQLDBConnection final : public RelationalDBConnection, public std::enable_shared_from_this<MySQLDBConnection>
        {
        private:
            MySQLHandle m_mysql; // shared_ptr allows PreparedStatements to use weak_ptr
            bool m_closed{true};
            bool m_autoCommit{true};
            bool m_transactionActive{false};
            TransactionIsolationLevel m_isolationLevel{TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ}; // MySQL default

            /// @brief Cached URL string for getURL() method
            std::string m_url;

            /**
             * @brief Registry of active prepared statements using weak_ptr
             *
             * @details
             * **DESIGN RATIONALE - Statement Lifecycle Management:**
             *
             * This registry uses `weak_ptr` instead of `shared_ptr` to track active statements.
             * This design decision addresses a complex threading issue in connection pooling scenarios.
             *
             * **THE PROBLEM:**
             *
             * When using `shared_ptr` for statement tracking:
             * - Statements remain alive as long as the connection exists
             * - Memory accumulates if users create many statements without explicitly closing them
             * - However, this prevents race conditions because statements don't get destroyed unexpectedly
             *
             * When using `weak_ptr` without proper synchronization:
             * - Statements can be destroyed at any time when user releases their reference
             * - The destructor calls `mysql_stmt_close()` which communicates with the MySQL server
             * - If another thread is using the same `MYSQL*` connection (e.g., connection pool validation),
             *   this causes a race condition leading to use-after-free memory corruption
             *
             * **THE SOLUTION:**
             *
             * We use `weak_ptr` combined with explicit statement cleanup in `returnToPool()`:
             *
             * 1. `weak_ptr` allows statements to be destroyed when the user releases them (no memory leak)
             * 2. Before returning a connection to the pool, `returnToPool()` explicitly closes ALL
             *    active statements while holding exclusive access to the connection
             * 3. This ensures no statement destruction can race with connection reuse by another thread
             * 4. The `close()` method also closes all statements before destroying the connection
             *
             * **LIFECYCLE GUARANTEE:**
             *
             * - Statement created → registered in this set (weak_ptr)
             * - User uses statement → statement remains valid
             * - User releases statement → destructor may run, calls `mysql_stmt_close()`
             * - Connection returned to pool → ALL remaining statements are explicitly closed first
             * - Connection closed → ALL remaining statements are explicitly closed first
             *
             * This ensures `mysql_stmt_close()` never races with other connection operations.
             *
             * @see returnToPool() - Closes all statements before making connection available
             * @see close() - Closes all statements before destroying connection
             * @see registerStatement() - Adds statement to registry
             * @see unregisterStatement() - Removes statement from registry (unused, kept for API symmetry)
             */
            std::set<std::weak_ptr<MySQLDBPreparedStatement>, std::owner_less<std::weak_ptr<MySQLDBPreparedStatement>>> m_activeStatements;

            /**
             * @brief Mutex protecting m_activeStatements registry
             *
             * @note This mutex is ALWAYS present (not conditional on DB_DRIVER_THREAD_SAFE) because
             * statement registration/cleanup can occur from different execution paths even in
             * single-threaded builds (e.g., during returnToPool() or close()).
             */
            std::mutex m_statementsMutex;

#if DB_DRIVER_THREAD_SAFE
            /**
             * @brief Shared connection mutex for thread-safe operations
             *
             * This mutex is shared with all PreparedStatements created from this connection.
             * This ensures that mysql_stmt_close() in PreparedStatement destructors is
             * serialized with all other connection operations, preventing race conditions.
             */
            SharedConnMutex m_connMutex = std::make_shared<std::recursive_mutex>();
#endif

            /**
             * @brief Register a prepared statement in the active statements registry
             * @param stmt Weak pointer to the statement to register
             * @note Called automatically when a new PreparedStatement is created via prepareStatement()
             */
            void registerStatement(std::weak_ptr<MySQLDBPreparedStatement> stmt);

            /**
             * @brief Unregister a prepared statement from the active statements registry
             * @param stmt Weak pointer to the statement to unregister
             * @note Currently unused - statements are cleaned up via closeAllStatements() or expire naturally.
             *       Kept for API symmetry and potential future use.
             */
            void unregisterStatement(std::weak_ptr<MySQLDBPreparedStatement> stmt);

            /**
             * @brief Close all active prepared statements
             *
             * @details
             * This method iterates through all registered statements and explicitly closes them.
             * It is called by:
             * - `returnToPool()` before making the connection available for reuse
             * - `close()` before destroying the connection
             *
             * This ensures that `mysql_stmt_close()` is called while we have exclusive access
             * to the connection, preventing race conditions with other threads.
             *
             * @note Statements that have already expired (weak_ptr returns nullptr) are simply
             * removed from the registry without any action.
             */
            void closeAllStatements();

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
            bool isClosed() const override;
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
            void prepareForPoolReturn() override;

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

            // Rule of 5: disable copy and move operations
            MySQLDBDriver(const MySQLDBDriver &) = delete;
            MySQLDBDriver &operator=(const MySQLDBDriver &) = delete;
            MySQLDBDriver(MySQLDBDriver &&) = delete;
            MySQLDBDriver &operator=(MySQLDBDriver &&) = delete;

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

            MySQLDBDriver(const MySQLDBDriver &) = delete;
            MySQLDBDriver &operator=(const MySQLDBDriver &) = delete;
            MySQLDBDriver(MySQLDBDriver &&) = delete;
            MySQLDBDriver &operator=(MySQLDBDriver &&) = delete;

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

            cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException> connectRelational(
                std::nothrow_t,
                const std::string & /*url*/,
                const std::string & /*user*/,
                const std::string & /*password*/,
                const std::map<std::string, std::string> & /*options*/ = std::map<std::string, std::string>()) noexcept override
            {
                return cpp_dbc::unexpected(DBException("23D2107DA650", "MySQL support is not enabled in this build"));
            }

            std::string getName() const noexcept override
            {
                return "MySQL (disabled)";
            }
        };
} // namespace cpp_dbc::MySQL

#endif // USE_MYSQL

#endif // CPP_DBC_DRIVER_MYSQL_HPP
