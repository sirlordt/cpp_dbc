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

 @file driver_firebird.hpp
 @brief Firebird database driver implementation

 Required system package: firebird-dev (Debian/Ubuntu) or firebird-devel (RHEL/CentOS/Fedora)
 Install with: sudo apt-get install firebird-dev libfbclient2

*/

#ifndef CPP_DBC_DRIVER_FIREBIRD_HPP
#define CPP_DBC_DRIVER_FIREBIRD_HPP

#include "../../cpp_dbc.hpp"
#include "firebird_blob.hpp"

#ifndef USE_FIREBIRD
#define USE_FIREBIRD 0 // Default to disabled
#endif

#if USE_FIREBIRD
#include <ibase.h>
#include <map>
#include <memory>
#include <set>
#include <mutex>
#include <vector>
#include <string>
#include <atomic>
#include <cstring>

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
    namespace Firebird
    {
        // Forward declarations
        class FirebirdDBConnection;
        class FirebirdDBPreparedStatement;

        /**
         * @brief Helper function to interpret Firebird status vector
         * @param status The status vector from Firebird API calls
         * @return Error message string with detailed error information
         */
        inline std::string interpretStatusVector(const ISC_STATUS_ARRAY status)
        {
            std::string result;

            // First, get the SQL code for more specific error information
            ISC_LONG sqlcode = isc_sqlcode(status);
            if (sqlcode != 0)
            {
                char sqlMsg[256];
                isc_sql_interprete(static_cast<short>(sqlcode), sqlMsg, sizeof(sqlMsg));
                result = "SQLCODE " + std::to_string(sqlcode) + ": " + std::string(sqlMsg);
            }

            // Then get the detailed error messages from the status vector using fb_interpret
            // This is the primary and most reliable source of error information
            char buffer[1024];
            const ISC_STATUS *pvector = status;
            std::string details;

            while (fb_interpret(buffer, sizeof(buffer), &pvector))
            {
                if (!details.empty())
                    details += " - ";
                details += buffer;
            }

            // Combine the messages
            if (!details.empty())
            {
                if (!result.empty())
                    result += " | ";
                result += details;
                // fb_interpret gave us a good message, return it
                return result;
            }

            // Final fallback - only if fb_interpret didn't give us anything
            if (result.empty())
            {
                result = "Unknown Firebird error (status[0]=" + std::to_string(status[0]) +
                         ", status[1]=" + std::to_string(status[1]) + ")";
            }

            return result;
        }

        /**
         * @brief Custom deleter for Firebird statement handle
         * Note: This deleter only frees the pointer wrapper, NOT the statement itself.
         * The statement is freed by the PreparedStatement::close() or ResultSet::close() methods.
         */
        struct FirebirdStmtDeleter
        {
            void operator()(isc_stmt_handle *stmt) const noexcept
            {
                // Only delete the pointer wrapper, don't free the statement
                // The statement is managed by PreparedStatement or ResultSet
                delete stmt;
            }
        };

        /**
         * @brief Type alias for the smart pointer managing Firebird statement
         */
        using FirebirdStmtHandle = std::unique_ptr<isc_stmt_handle, FirebirdStmtDeleter>;

        /**
         * @brief Custom deleter for XSQLDA structure
         */
        struct XSQLDADeleter
        {
            void operator()(XSQLDA *sqlda) const noexcept
            {
                if (sqlda)
                {
                    free(sqlda);
                }
            }
        };

        /**
         * @brief Type alias for the smart pointer managing XSQLDA
         */
        using XSQLDAHandle = std::unique_ptr<XSQLDA, XSQLDADeleter>;

        /**
         * @brief Custom deleter for transaction handle (non-owning, just for type safety)
         * Note: Transaction handles are managed by FirebirdConnection, not by this deleter
         */
        struct FirebirdTrDeleter
        {
            void operator()(isc_tr_handle *tr) const noexcept
            {
                // Transaction handles are managed by FirebirdConnection
                // This deleter only frees the pointer wrapper, not the transaction itself
                delete tr;
            }
        };

        /**
         * @brief Type alias for transaction handle wrapper
         */
        using FirebirdTrHandle = std::unique_ptr<isc_tr_handle, FirebirdTrDeleter>;

        /**
         * @brief Firebird ResultSet implementation
         */
        class FirebirdDBResultSet : public RelationalDBResultSet
        {
            friend class FirebirdDBConnection;

        private:
            FirebirdStmtHandle m_stmt;
            XSQLDAHandle m_sqlda;
            bool m_ownStatement{false};
            size_t m_rowPosition{0};
            size_t m_fieldCount{0};
            std::vector<std::string> m_columnNames;
            std::map<std::string, size_t> m_columnMap;
            bool m_hasData{false};
            bool m_closed{true};
            bool m_fetchedFirst{false};
            std::weak_ptr<FirebirdDBConnection> m_connection;

            // Buffer for SQLDA data
            std::vector<std::vector<char>> m_dataBuffers;
            std::vector<short> m_nullIndicators;

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex;
#endif

            /**
             * @brief Initialize column metadata from SQLDA
             */
            void initializeColumns();

            /**
             * @brief Get column value as string
             */
            std::string getColumnValue(size_t columnIndex) const;

            /**
             * @brief Get raw statement handle pointer for Firebird API calls
             */
            isc_stmt_handle *getStmtPtr() { return m_stmt.get(); }

            /**
             * @brief Notify this ResultSet that the connection is closing
             */
            void notifyConnClosing();

        public:
            FirebirdDBResultSet(FirebirdStmtHandle stmt, XSQLDAHandle sqlda, bool ownStatement = true,
                                std::shared_ptr<FirebirdDBConnection> conn = nullptr);
            ~FirebirdDBResultSet() override;

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

            // ====================================================================
            // NOTHROW VERSIONS - Exception-free API
            // ====================================================================

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

        /**
         * @brief Firebird PreparedStatement implementation
         */
        // Forward declaration
        class FirebirdDBConnection;

        class FirebirdDBPreparedStatement : public RelationalDBPreparedStatement
        {
            friend class FirebirdDBConnection;

        private:
            std::weak_ptr<isc_db_handle> m_dbHandle;
            std::weak_ptr<FirebirdDBConnection> m_connection; // Reference to connection for autocommit
            isc_tr_handle *m_trPtr{nullptr};                  // Non-owning pointer to transaction handle (owned by Connection)
            isc_stmt_handle m_stmt;
            std::string m_sql;
            XSQLDAHandle m_inputSqlda;
            XSQLDAHandle m_outputSqlda;
            bool m_closed{true};
            bool m_prepared{false};

            // Parameter storage
            std::vector<std::vector<char>> m_paramBuffers;
            std::vector<short> m_paramNullIndicators;
            std::vector<std::vector<uint8_t>> m_blobValues;
            std::vector<std::shared_ptr<Blob>> m_blobObjects;
            std::vector<std::shared_ptr<InputStream>> m_streamObjects;

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_mutex;
#endif

            void notifyConnClosing();
            isc_db_handle *getFirebirdConnection() const;
            void prepareStatement();
            void allocateInputSqlda();
            void setParameter(int parameterIndex, const void *data, size_t length, short sqlType);

        public:
            FirebirdDBPreparedStatement(std::weak_ptr<isc_db_handle> db, isc_tr_handle *trPtr, const std::string &sql,
                                        std::weak_ptr<FirebirdDBConnection> conn = std::weak_ptr<FirebirdDBConnection>());
            ~FirebirdDBPreparedStatement() override;

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

            // ====================================================================
            // NOTHROW VERSIONS - Exception-free API
            // ====================================================================

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

        /**
         * @brief Custom deleter for Firebird database handle
         */
        struct FirebirdDbDeleter
        {
            void operator()(isc_db_handle *db) const noexcept
            {
                if (db && *db)
                {
                    ISC_STATUS_ARRAY status;
                    isc_detach_database(status, db);
                    delete db;
                }
            }
        };

        /**
         * @brief Type alias for the smart pointer managing Firebird connection
         */
        using FirebirdDbHandle = std::shared_ptr<isc_db_handle>;

        /**
         * @brief Firebird Connection implementation
         */
        class FirebirdDBConnection : public RelationalDBConnection, public std::enable_shared_from_this<FirebirdDBConnection>
        {
            friend class FirebirdDBPreparedStatement;
            friend class FirebirdDBResultSet;
            friend class FirebirdBlob;

        private:
            FirebirdDbHandle m_db;
            isc_tr_handle m_tr;
            bool m_closed{true};
            bool m_autoCommit{true};
            bool m_transactionActive{false};
            TransactionIsolationLevel m_isolationLevel;
            std::string m_url;

            // Registry of active prepared statements and result sets
            std::set<std::weak_ptr<FirebirdDBPreparedStatement>, std::owner_less<std::weak_ptr<FirebirdDBPreparedStatement>>> m_activeStatements;
            std::set<std::weak_ptr<FirebirdDBResultSet>, std::owner_less<std::weak_ptr<FirebirdDBResultSet>>> m_activeResultSets;
            std::mutex m_statementsMutex;
            std::mutex m_resultSetsMutex;

#if DB_DRIVER_THREAD_SAFE
            mutable std::recursive_mutex m_connMutex;
#endif

            void registerStatement(std::weak_ptr<FirebirdDBPreparedStatement> stmt);
            void unregisterStatement(std::weak_ptr<FirebirdDBPreparedStatement> stmt);
            void registerResultSet(std::weak_ptr<FirebirdDBResultSet> rs);
            void unregisterResultSet(std::weak_ptr<FirebirdDBResultSet> rs);
            void startTransaction();
            void endTransaction(bool commit);
            void closeAllActiveResultSets();

            /**
             * @brief Execute a CREATE DATABASE statement using isc_dsql_execute_immediate
             * @param sql The CREATE DATABASE SQL statement
             * @return 0 (CREATE DATABASE doesn't return affected rows)
             */
            uint64_t executeCreateDatabase(const std::string &sql);

        public:
            FirebirdDBConnection(const std::string &host,
                                 int port,
                                 const std::string &database,
                                 const std::string &user,
                                 const std::string &password,
                                 const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
            ~FirebirdDBConnection() override;

            void close() override;
            bool isClosed() override;
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

            // Transaction isolation level methods
            void setTransactionIsolation(TransactionIsolationLevel level) override;
            TransactionIsolationLevel getTransactionIsolation() override;

            // Get the connection URL
            std::string getURL() const override;

            // ====================================================================
            // NOTHROW VERSIONS - Exception-free API
            // ====================================================================

            cpp_dbc::expected<std::shared_ptr<RelationalDBPreparedStatement>, DBException>
            prepareStatement(std::nothrow_t, const std::string &sql) noexcept override;

            cpp_dbc::expected<std::shared_ptr<RelationalDBResultSet>, DBException>
            executeQuery(std::nothrow_t, const std::string &sql) noexcept override;

            cpp_dbc::expected<uint64_t, DBException>
            executeUpdate(std::nothrow_t, const std::string &sql) noexcept override;

            cpp_dbc::expected<void, DBException>
            setAutoCommit(std::nothrow_t, bool autoCommit) noexcept override;

            cpp_dbc::expected<bool, DBException>
                getAutoCommit(std::nothrow_t) noexcept override;

            cpp_dbc::expected<bool, DBException>
                beginTransaction(std::nothrow_t) noexcept override;

            cpp_dbc::expected<bool, DBException>
                transactionActive(std::nothrow_t) noexcept override;

            cpp_dbc::expected<void, DBException>
                commit(std::nothrow_t) noexcept override;

            cpp_dbc::expected<void, DBException>
                rollback(std::nothrow_t) noexcept override;

            cpp_dbc::expected<void, DBException>
            setTransactionIsolation(std::nothrow_t, TransactionIsolationLevel level) noexcept override;

            cpp_dbc::expected<TransactionIsolationLevel, DBException>
                getTransactionIsolation(std::nothrow_t) noexcept override;
        };

        /**
         * @brief Firebird Driver implementation
         */
        class FirebirdDBDriver : public RelationalDBDriver
        {
        private:
            static std::atomic<bool> s_initialized;
            static std::mutex s_initMutex;

        public:
            FirebirdDBDriver();
            ~FirebirdDBDriver() override;

            std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                      const std::string &user,
                                                                      const std::string &password,
                                                                      const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool acceptsURL(const std::string &url) override;

            /**
             * @brief Execute a driver-specific command
             *
             * Supported commands:
             * - "create_database": Creates a new Firebird database
             *   Required params: "url", "user", "password"
             *   Optional params: "page_size" (default: "4096"), "charset" (default: "UTF8")
             *
             * @param params Command parameters as a map of string to std::any
             * @return 0 on success
             * @throws DBException if the command fails
             */
            int command(const std::map<std::string, std::any> &params) override;

            /**
             * @brief Creates a new Firebird database
             *
             * This method creates a new database file using isc_dsql_execute_immediate.
             * It can be called without an existing connection.
             *
             * @param url The database URL (cpp_dbc:firebird://host:port/path/to/database.fdb)
             * @param user The database user (typically SYSDBA)
             * @param password The user's password
             * @param options Optional parameters:
             *                - "page_size": Database page size (default: 4096)
             *                - "charset": Default character set (default: UTF8)
             * @return true if database was created successfully
             * @throws DBException if database creation fails
             */
            bool createDatabase(const std::string &url,
                                const std::string &user,
                                const std::string &password,
                                const std::map<std::string, std::string> &options = std::map<std::string, std::string>());

            /**
             * @brief Parses a URL: cpp_dbc:firebird://host:port/path/to/database.fdb
             * @param url The URL to parse
             * @param host Output: the host name
             * @param port Output: the port number
             * @param database Output: the database path
             * @return true if parsing was successful
             */
            bool parseURL(const std::string &url, std::string &host, int &port, std::string &database);

            // ====================================================================
            // NOTHROW VERSIONS - Exception-free API
            // ====================================================================

            cpp_dbc::expected<std::shared_ptr<RelationalDBConnection>, DBException>
            connectRelational(
                std::nothrow_t,
                const std::string &url,
                const std::string &user,
                const std::string &password,
                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) noexcept override;

            std::string getName() const noexcept override;
        };

        // ============================================================================
        // FirebirdBlob inline method implementations
        // These must be defined after FirebirdConnection is fully defined
        // ============================================================================

        /**
         * @brief Get the database handle from the connection
         * @return Pointer to the database handle
         */
        inline isc_db_handle *FirebirdBlob::getDbHandle() const
        {
            auto conn = getConnection();
            return conn->m_db.get();
        }

        /**
         * @brief Get the transaction handle from the connection
         * @return Pointer to the transaction handle
         */
        inline isc_tr_handle *FirebirdBlob::getTrHandle() const
        {
            auto conn = getConnection();
            return &conn->m_tr;
        }

    } // namespace Firebird
} // namespace cpp_dbc

#else // USE_FIREBIRD

// Stub implementations when Firebird is disabled
namespace cpp_dbc
{
    namespace Firebird
    {
        class FirebirdDBDriver : public RelationalDBDriver
        {
        public:
            FirebirdDBDriver()
            {
                throw DBException("R9T3U5V1W7X4", "Firebird support is not enabled in this build", system_utils::captureCallStack());
            }
            ~FirebirdDBDriver() override = default;

            std::shared_ptr<RelationalDBConnection> connectRelational(const std::string &url,
                                                                      const std::string &user,
                                                                      const std::string &password,
                                                                      const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
            {
                throw DBException("S0U4V6W2X8Y5", "Firebird support is not enabled in this build", system_utils::captureCallStack());
            }

            bool acceptsURL(const std::string &url) override
            {
                return false;
            }
        };
    } // namespace Firebird
} // namespace cpp_dbc

#endif // USE_FIREBIRD

#endif // CPP_DBC_DRIVER_FIREBIRD_HPP
