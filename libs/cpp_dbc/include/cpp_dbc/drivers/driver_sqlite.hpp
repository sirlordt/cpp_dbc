// CPPDBC_SQLite.hpp
// SQLite implementation for cpp_dbc

#ifndef CPP_DBC_DRIVER_SQLITE_HPP
#define CPP_DBC_DRIVER_SQLITE_HPP

#include "../cpp_dbc.hpp"

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

        public:
            SQLiteResultSet(sqlite3_stmt *stmt, bool ownStatement = true);
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

            std::shared_ptr<ResultSet> executeQuery() override;
            int executeUpdate() override;
            bool execute() override;
            void close() override;
        };

        class SQLiteConnection : public Connection
        {
            friend class SQLitePreparedStatement;

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
            static std::set<SQLiteConnection *> activeConnections;
            static std::mutex connectionsListMutex;

        public:
            SQLiteConnection(const std::string &database);
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
                                                const std::string &password) override;

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
                throw SQLException("SQLite support is not enabled in this build");
            }
            ~SQLiteDriver() override = default;

            std::shared_ptr<Connection> connect(const std::string &url,
                                                const std::string &user,
                                                const std::string &password) override
            {
                throw SQLException("SQLite support is not enabled in this build");
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