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
 @brief Tests for SQLite database operations

*/

#ifndef CPP_DBC_DRIVER_SQLITE_HPP
#define CPP_DBC_DRIVER_SQLITE_HPP

#include "../cpp_dbc.hpp"
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

namespace cpp_dbc
{
    namespace SQLite
    {

        class SQLiteResultSet : public ResultSet
        {
        private:
            sqlite3_stmt *stmt;
            bool ownStatement;
            int rowPosition;
            int rowCount;
            int fieldCount;
            std::vector<std::string> columnNames;
            std::map<std::string, int> columnMap;
            bool hasData;
            bool closed;
            std::weak_ptr<SQLiteConnection> connection; // Referencia débil a la conexión

        public:
            SQLiteResultSet(sqlite3_stmt *stmt, bool ownStatement = true, std::shared_ptr<SQLiteConnection> conn = nullptr);
            ~SQLiteResultSet() override;

            bool next() override;
            bool isBeforeFirst() override;
            bool isAfterLast() override;
            int getRow() override;

            int getInt(int columnIndex) override;
            int getInt(const std::string &columnName) override;

            long getLong(int columnIndex) override;
            long getLong(const std::string &columnName) override;

            double getDouble(int columnIndex) override;
            double getDouble(const std::string &columnName) override;

            std::string getString(int columnIndex) override;
            std::string getString(const std::string &columnName) override;

            bool getBoolean(int columnIndex) override;
            bool getBoolean(const std::string &columnName) override;

            bool isNull(int columnIndex) override;
            bool isNull(const std::string &columnName) override;

            std::vector<std::string> getColumnNames() override;
            int getColumnCount() override;
            void close() override;

            // BLOB support methods
            std::shared_ptr<Blob> getBlob(int columnIndex) override;
            std::shared_ptr<Blob> getBlob(const std::string &columnName) override;

            std::shared_ptr<InputStream> getBinaryStream(int columnIndex) override;
            std::shared_ptr<InputStream> getBinaryStream(const std::string &columnName) override;

            std::vector<uint8_t> getBytes(int columnIndex) override;
            std::vector<uint8_t> getBytes(const std::string &columnName) override;
        };

        // Forward declaration
        class SQLiteConnection;

        class SQLitePreparedStatement : public PreparedStatement, public std::enable_shared_from_this<SQLitePreparedStatement>
        {
            friend class SQLiteConnection;

        private:
            sqlite3 *db;
            std::string sql;
            sqlite3_stmt *stmt;
            bool closed;
            std::vector<std::vector<uint8_t>> blobValues;            // To keep blob values alive
            std::vector<std::shared_ptr<Blob>> blobObjects;          // To keep blob objects alive
            std::vector<std::shared_ptr<InputStream>> streamObjects; // To keep stream objects alive

            // Internal method called by connection when closing
            void notifyConnClosing();

        public:
            SQLitePreparedStatement(sqlite3 *db, const std::string &sql);
            ~SQLitePreparedStatement() override;

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

            std::shared_ptr<ResultSet> executeQuery() override;
            int executeUpdate() override;
            bool execute() override;
            void close() override;
        };

        class SQLiteConnection : public Connection, public std::enable_shared_from_this<SQLiteConnection>
        {
            friend class SQLitePreparedStatement;
            friend class SQLiteResultSet;

        private:
            sqlite3 *db;
            bool closed;
            bool autoCommit;
            TransactionIsolationLevel isolationLevel;

            // Registry of active prepared statements
            std::set<std::shared_ptr<SQLitePreparedStatement>> activeStatements;
            std::mutex statementsMutex;

            // Internal methods for statement registry
            void registerStatement(std::shared_ptr<SQLitePreparedStatement> stmt);
            void unregisterStatement(std::shared_ptr<SQLitePreparedStatement> stmt);

            // Static list of active connections for statement cleanup
            static std::set<std::weak_ptr<SQLiteConnection>, std::owner_less<std::weak_ptr<SQLiteConnection>>> activeConnections;
            static std::mutex connectionsListMutex;

        public:
            SQLiteConnection(const std::string &database,
                             const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
            ~SQLiteConnection() override;

            void close() override;
            bool isClosed() override;
            void returnToPool() override;
            bool isPooled() override;

            std::shared_ptr<PreparedStatement> prepareStatement(const std::string &sql) override;
            std::shared_ptr<ResultSet> executeQuery(const std::string &sql) override;
            int executeUpdate(const std::string &sql) override;

            void setAutoCommit(bool autoCommit) override;
            bool getAutoCommit() override;

            void commit() override;
            void rollback() override;

            // Transaction isolation level methods
            void setTransactionIsolation(TransactionIsolationLevel level) override;
            TransactionIsolationLevel getTransactionIsolation() override;
        };

        class SQLiteDriver : public Driver
        {
        public:
            SQLiteDriver();
            ~SQLiteDriver() override;

            std::shared_ptr<Connection> connect(const std::string &url,
                                                const std::string &user,
                                                const std::string &password,
                                                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool acceptsURL(const std::string &url) override;

            // Parses a URL: cpp_dbc:sqlite:/path/to/database.db or cpp_dbc:sqlite::memory:
            bool parseURL(const std::string &url, std::string &database);
        };

    } // namespace SQLite
} // namespace cpp_dbc

#else // USE_SQLITE

// Stub implementations when SQLite is disabled
namespace cpp_dbc
{
    namespace SQLite
    {
        // Forward declarations only
        class SQLiteDriver : public Driver
        {
        public:
            SQLiteDriver()
            {
                throw DBException("C27AD46A860B", "SQLite support is not enabled in this build", system_utils::captureCallStack());
            }
            ~SQLiteDriver() override = default;

            std::shared_ptr<Connection> connect(const std::string &url,
                                                const std::string &user,
                                                const std::string &password,
                                                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
            {
                throw DBException("269CC140F035", "SQLite support is not enabled in this build", system_utils::captureCallStack());
            }

            bool acceptsURL(const std::string &url) override
            {
                return false;
            }
        };
    } // namespace SQLite
} // namespace cpp_dbc

#endif // USE_SQLITE

#endif // CPP_DBC_DRIVER_SQLITE_HPP