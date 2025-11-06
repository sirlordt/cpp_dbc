// CPPDBC_SQLite.cpp
// Implementation of SQLite classes for cpp_dbc

#include "cpp_dbc/drivers/driver_sqlite.hpp"
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>

#if USE_SQLITE

namespace cpp_dbc
{
    namespace SQLite
    {
        // Initialize static members
        std::set<SQLiteConnection *> SQLiteConnection::activeConnections;
        std::mutex SQLiteConnection::connectionsListMutex;

        // SQLiteResultSet implementation
        SQLiteResultSet::SQLiteResultSet(sqlite3_stmt *stmt, bool ownStatement)
            : stmt(stmt), ownStatement(ownStatement), rowPosition(0), rowCount(0), fieldCount(0), hasData(false), closed(false)
        {
            if (stmt)
            {
                // Get column count
                fieldCount = sqlite3_column_count(stmt);

                // Store column names and create column name to index mapping
                for (int i = 0; i < fieldCount; i++)
                {
                    std::string name = sqlite3_column_name(stmt, i);
                    columnNames.push_back(name);
                    columnMap[name] = i;
                }
            }
        }

        SQLiteResultSet::~SQLiteResultSet()
        {
            close();
        }

        bool SQLiteResultSet::next()
        {
            if (!stmt || closed)
            {
                return false;
            }

            int result = sqlite3_step(stmt);

            if (result == SQLITE_ROW)
            {
                rowPosition++;
                hasData = true;
                return true;
            }
            else if (result == SQLITE_DONE)
            {
                rowPosition++;
                hasData = false;
                return false;
            }
            else
            {
                // Error occurred
                throw SQLException("Error stepping through SQLite result set: " +
                                   std::string(sqlite3_errmsg(sqlite3_db_handle(stmt))));
            }
        }

        bool SQLiteResultSet::isBeforeFirst()
        {
            return rowPosition == 0;
        }

        bool SQLiteResultSet::isAfterLast()
        {
            return rowPosition > 0 && !hasData;
        }

        int SQLiteResultSet::getRow()
        {
            return rowPosition;
        }

        int SQLiteResultSet::getInt(int columnIndex)
        {
            if (!stmt || closed || !hasData || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw SQLException("Invalid column index or row position");
            }

            // SQLite column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = columnIndex - 1;

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return 0; // Return 0 for NULL values (similar to JDBC)
            }

            return sqlite3_column_int(stmt, idx);
        }

        int SQLiteResultSet::getInt(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return getInt(it->second + 1); // +1 because getInt(int) is 1-based
        }

        long SQLiteResultSet::getLong(int columnIndex)
        {
            if (!stmt || closed || !hasData || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw SQLException("Invalid column index or row position");
            }

            int idx = columnIndex - 1;

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return 0;
            }

            return sqlite3_column_int64(stmt, idx);
        }

        long SQLiteResultSet::getLong(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return getLong(it->second + 1);
        }

        double SQLiteResultSet::getDouble(int columnIndex)
        {
            if (!stmt || closed || !hasData || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw SQLException("Invalid column index or row position");
            }

            int idx = columnIndex - 1;

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return 0.0;
            }

            return sqlite3_column_double(stmt, idx);
        }

        double SQLiteResultSet::getDouble(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return getDouble(it->second + 1);
        }

        std::string SQLiteResultSet::getString(int columnIndex)
        {
            if (!stmt || closed || !hasData || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw SQLException("Invalid column index or row position");
            }

            int idx = columnIndex - 1;

            if (sqlite3_column_type(stmt, idx) == SQLITE_NULL)
            {
                return "";
            }

            const char *text = reinterpret_cast<const char *>(sqlite3_column_text(stmt, idx));
            return text ? std::string(text) : "";
        }

        std::string SQLiteResultSet::getString(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return getString(it->second + 1);
        }

        bool SQLiteResultSet::getBoolean(int columnIndex)
        {
            return getInt(columnIndex) != 0;
        }

        bool SQLiteResultSet::getBoolean(const std::string &columnName)
        {
            return getInt(columnName) != 0;
        }

        bool SQLiteResultSet::isNull(int columnIndex)
        {
            if (!stmt || closed || !hasData || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw SQLException("Invalid column index or row position");
            }

            int idx = columnIndex - 1;
            return sqlite3_column_type(stmt, idx) == SQLITE_NULL;
        }

        bool SQLiteResultSet::isNull(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return isNull(it->second + 1);
        }

        std::vector<std::string> SQLiteResultSet::getColumnNames()
        {
            return columnNames;
        }

        int SQLiteResultSet::getColumnCount()
        {
            return fieldCount;
        }

        void SQLiteResultSet::close()
        {
            if (!closed && stmt && ownStatement)
            {
                sqlite3_finalize(stmt);
                stmt = nullptr;
            }
            closed = true;
        }

        // SQLitePreparedStatement implementation
        SQLitePreparedStatement::SQLitePreparedStatement(sqlite3 *db, const std::string &sql)
            : db(db), sql(sql), stmt(nullptr), closed(false)
        {
            if (!db)
            {
                throw SQLException("Invalid SQLite database connection");
            }

            int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (result != SQLITE_OK)
            {
                throw SQLException("Failed to prepare SQLite statement: " + std::string(sqlite3_errmsg(db)));
            }
        }

        SQLitePreparedStatement::~SQLitePreparedStatement()
        {
            try
            {
                // Make sure to close the statement and clean up resources
                close();
            }
            catch (...)
            {
                // Ignore exceptions during destruction
            }

            // If close() failed or wasn't called, ensure we clean up
            if (!closed && stmt)
            {
                sqlite3_finalize(stmt);
                stmt = nullptr;
            }
        }

        void SQLitePreparedStatement::close()
        {
            if (!closed && stmt)
            {
                sqlite3_finalize(stmt);
                stmt = nullptr;

                // Unregister from the connection if it's still valid
                if (db)
                {
                    // Find the connection object that owns this statement
                    for (auto &conn : SQLiteConnection::activeConnections)
                    {
                        if (conn->db == db)
                        {
                            conn->unregisterStatement(shared_from_this());
                            break;
                        }
                    }
                }
            }
            closed = true;
        }

        void SQLitePreparedStatement::notifyConnClosing()
        {
            // Connection is closing, invalidate the statement without calling sqlite3_finalize
            // since the connection is already being destroyed
            stmt = nullptr;
            closed = true;
        }

        void SQLitePreparedStatement::setInt(int parameterIndex, int value)
        {
            if (closed || !stmt)
            {
                throw SQLException("Statement is closed");
            }

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw SQLException("Invalid parameter index: " + std::to_string(parameterIndex));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(stmt);
            if (parameterIndex > paramCount)
            {
                throw SQLException("Parameter index out of range: " + std::to_string(parameterIndex) +
                                   " (statement has " + std::to_string(paramCount) + " parameters)");
            }

            // Debug output
            std::cout << "Binding integer parameter " << parameterIndex << " with value " << value
                      << " (statement has " << paramCount << " parameters)" << std::endl;

            int result = sqlite3_bind_int(stmt, parameterIndex, value);
            if (result != SQLITE_OK)
            {
                throw SQLException("Failed to bind integer parameter: " + std::string(sqlite3_errmsg(db)) +
                                   " (index=" + std::to_string(parameterIndex) +
                                   ", value=" + std::to_string(value) +
                                   ", result=" + std::to_string(result) + ")");
            }
        }

        void SQLitePreparedStatement::setLong(int parameterIndex, long value)
        {
            if (closed || !stmt)
            {
                throw SQLException("Statement is closed");
            }

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw SQLException("Invalid parameter index: " + std::to_string(parameterIndex));
            }

            int result = sqlite3_bind_int64(stmt, parameterIndex, value);
            if (result != SQLITE_OK)
            {
                throw SQLException("Failed to bind long parameter: " + std::string(sqlite3_errmsg(db)));
            }
        }

        void SQLitePreparedStatement::setDouble(int parameterIndex, double value)
        {
            if (closed || !stmt)
            {
                throw SQLException("Statement is closed");
            }

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw SQLException("Invalid parameter index: " + std::to_string(parameterIndex));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(stmt);
            if (parameterIndex > paramCount)
            {
                throw SQLException("Parameter index out of range: " + std::to_string(parameterIndex) +
                                   " (statement has " + std::to_string(paramCount) + " parameters)");
            }

            // Debug output
            std::cout << "Binding double parameter " << parameterIndex << " with value " << value
                      << " (statement has " << paramCount << " parameters)" << std::endl;

            int result = sqlite3_bind_double(stmt, parameterIndex, value);
            if (result != SQLITE_OK)
            {
                throw SQLException("Failed to bind double parameter: " + std::string(sqlite3_errmsg(db)) +
                                   " (index=" + std::to_string(parameterIndex) +
                                   ", value=" + std::to_string(value) +
                                   ", result=" + std::to_string(result) + ")");
            }
        }

        void SQLitePreparedStatement::setString(int parameterIndex, const std::string &value)
        {
            if (closed || !stmt)
            {
                throw SQLException("Statement is closed");
            }

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw SQLException("Invalid parameter index: " + std::to_string(parameterIndex));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(stmt);
            if (parameterIndex > paramCount)
            {
                throw SQLException("Parameter index out of range: " + std::to_string(parameterIndex) +
                                   " (statement has " + std::to_string(paramCount) + " parameters)");
            }

            // Debug output
            std::cout << "Binding string parameter " << parameterIndex << " with value '" << value
                      << "' (statement has " << paramCount << " parameters)" << std::endl;

            // SQLITE_TRANSIENT tells SQLite to make its own copy of the data
            int result = sqlite3_bind_text(stmt, parameterIndex, value.c_str(), -1, SQLITE_TRANSIENT);
            if (result != SQLITE_OK)
            {
                throw SQLException("Failed to bind string parameter: " + std::string(sqlite3_errmsg(db)) +
                                   " (index=" + std::to_string(parameterIndex) +
                                   ", value='" + value + "'" +
                                   ", result=" + std::to_string(result) + ")");
            }
        }

        void SQLitePreparedStatement::setBoolean(int parameterIndex, bool value)
        {
            if (closed || !stmt)
            {
                throw SQLException("Statement is closed");
            }

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw SQLException("Invalid parameter index: " + std::to_string(parameterIndex));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(stmt);
            if (parameterIndex > paramCount)
            {
                throw SQLException("Parameter index out of range: " + std::to_string(parameterIndex) +
                                   " (statement has " + std::to_string(paramCount) + " parameters)");
            }

            // Debug output
            std::cout << "Binding boolean parameter " << parameterIndex << " with value " << (value ? "true" : "false")
                      << " (statement has " << paramCount << " parameters)" << std::endl;

            int intValue = value ? 1 : 0;
            int result = sqlite3_bind_int(stmt, parameterIndex, intValue);
            if (result != SQLITE_OK)
            {
                throw SQLException("Failed to bind boolean parameter: " + std::string(sqlite3_errmsg(db)) +
                                   " (index=" + std::to_string(parameterIndex) +
                                   ", value=" + (value ? "true" : "false") +
                                   ", result=" + std::to_string(result) + ")");
            }
        }

        void SQLitePreparedStatement::setNull(int parameterIndex, Types type)
        {
            if (closed || !stmt)
            {
                throw SQLException("Statement is closed");
            }

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw SQLException("Invalid parameter index: " + std::to_string(parameterIndex));
            }

            int result = sqlite3_bind_null(stmt, parameterIndex);
            if (result != SQLITE_OK)
            {
                throw SQLException("Failed to bind null parameter: " + std::string(sqlite3_errmsg(db)));
            }
        }

        void SQLitePreparedStatement::setDate(int parameterIndex, const std::string &value)
        {
            // SQLite doesn't have a specific date type, so we store it as text
            setString(parameterIndex, value);
        }

        void SQLitePreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
        {
            // SQLite doesn't have a specific timestamp type, so we store it as text
            setString(parameterIndex, value);
        }

        std::shared_ptr<ResultSet> SQLitePreparedStatement::executeQuery()
        {
            if (closed || !stmt)
            {
                throw SQLException("Statement is closed");
            }

            // Reset the statement to ensure it's ready for execution
            int resetResult = sqlite3_reset(stmt);
            if (resetResult != SQLITE_OK)
            {
                throw SQLException("Failed to reset statement: " + std::string(sqlite3_errmsg(db)) +
                                   " (result=" + std::to_string(resetResult) + ")");
            }

            // Debug output
            int paramCount = sqlite3_bind_parameter_count(stmt);
            std::cout << "Executing query with " << paramCount << " parameters" << std::endl;

            // Print the SQL statement
            std::cout << "SQL: " << sql << std::endl;

            // Create a result set that will own the statement
            // We don't execute the first step here - let the ResultSet do it when next() is called
            auto resultSet = std::make_shared<SQLiteResultSet>(stmt, false);

            // Note: We don't clear bindings here because the ResultSet needs them
            // The bindings will be cleared when the statement is reset for the next use

            return resultSet;
        }

        int SQLitePreparedStatement::executeUpdate()
        {
            if (closed || !stmt)
            {
                throw SQLException("Statement is closed");
            }

            // Reset the statement to ensure it's ready for execution
            int resetResult = sqlite3_reset(stmt);
            if (resetResult != SQLITE_OK)
            {
                throw SQLException("Failed to reset statement: " + std::string(sqlite3_errmsg(db)) +
                                   " (result=" + std::to_string(resetResult) + ")");
            }

            // Execute the statement
            int result = sqlite3_step(stmt);
            if (result != SQLITE_DONE)
            {
                throw SQLException("Failed to execute update: " + std::string(sqlite3_errmsg(db)) +
                                   " (result=" + std::to_string(result) + ")");
            }

            // Get the number of affected rows
            int changes = sqlite3_changes(db);

            // Reset the statement after execution to allow reuse
            resetResult = sqlite3_reset(stmt);
            if (resetResult != SQLITE_OK)
            {
                throw SQLException("Failed to reset statement after execution: " + std::string(sqlite3_errmsg(db)) +
                                   " (result=" + std::to_string(resetResult) + ")");
            }

            // Note: We don't clear bindings here anymore
            // This allows reusing the statement with the same parameters

            return changes;
        }

        bool SQLitePreparedStatement::execute()
        {
            if (closed || !stmt)
            {
                throw SQLException("Statement is closed");
            }

            // Reset the statement to ensure it's ready for execution
            sqlite3_reset(stmt);

            // Execute the statement
            int result = sqlite3_step(stmt);

            if (result == SQLITE_ROW)
            {
                // There are rows to fetch, so this is a query
                sqlite3_reset(stmt);
                return true;
            }
            else if (result == SQLITE_DONE)
            {
                // No rows to fetch, so this is an update
                return false;
            }
            else
            {
                throw SQLException("Failed to execute statement: " + std::string(sqlite3_errmsg(db)));
            }
        }

        // SQLiteConnection implementation

        SQLiteConnection::SQLiteConnection(const std::string &database)
            : db(nullptr), closed(false), autoCommit(true),
              isolationLevel(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE) // SQLite default
        {
            int result = sqlite3_open_v2(database.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
            if (result != SQLITE_OK)
            {
                std::string error = sqlite3_errmsg(db);
                sqlite3_close_v2(db);
                db = nullptr;
                throw SQLException("Failed to connect to SQLite database: " + error);
            }

            // Enable foreign keys
            executeUpdate("PRAGMA foreign_keys = ON");

            // Register this connection in the active connections list
            {
                std::lock_guard<std::mutex> lock(connectionsListMutex);
                activeConnections.insert(this);
            }
        }

        SQLiteConnection::~SQLiteConnection()
        {
            // Make sure to close the connection and clean up resources
            try
            {
                close();
            }
            catch (...)
            {
                // Ignore exceptions during destruction
            }

            // Ensure this connection is removed from the active connections list
            // even if close() wasn't called or failed
            {
                std::lock_guard<std::mutex> lock(connectionsListMutex);
                activeConnections.erase(this);
            }
        }

        void SQLiteConnection::close()
        {
            if (!closed && db)
            {
                // Notify all active statements that connection is closing
                {
                    std::lock_guard<std::mutex> lock(statementsMutex);
                    for (auto &stmt : activeStatements)
                    {
                        if (stmt)
                        {
                            stmt->notifyConnClosing();
                        }
                    }
                    activeStatements.clear();
                }

                // Use sqlite3_close_v2 instead of sqlite3_close
                // sqlite3_close_v2 will handle unfinalized prepared statements gracefully
                sqlite3_close_v2(db);
                db = nullptr;
                closed = true;

                // Remove this connection from the active connections list
                {
                    std::lock_guard<std::mutex> lock(connectionsListMutex);
                    activeConnections.erase(this);
                }
            }
        }

        bool SQLiteConnection::isClosed()
        {
            return closed;
        }

        void SQLiteConnection::returnToPool()
        {
            // Don't physically close the connection, just mark it as available
            // so it can be reused by the pool

            // Reset the connection state if necessary
            try
            {
                // Make sure autocommit is enabled for the next time the connection is used
                if (!autoCommit)
                {
                    setAutoCommit(true);
                }

                // We don't set closed = true because we want to keep the connection open
                // Just mark it as available for reuse
            }
            catch (...)
            {
                // Ignore errors during cleanup
            }
        }

        bool SQLiteConnection::isPooled()
        {
            return false;
        }

        std::shared_ptr<PreparedStatement> SQLiteConnection::prepareStatement(const std::string &sql)
        {
            if (closed || !db)
            {
                throw SQLException("Connection is closed");
            }

            auto stmt = std::make_shared<SQLitePreparedStatement>(db, sql);
            registerStatement(stmt);
            return stmt;
        }

        std::shared_ptr<ResultSet> SQLiteConnection::executeQuery(const std::string &sql)
        {
            if (closed || !db)
            {
                throw SQLException("Connection is closed");
            }

            sqlite3_stmt *stmt = nullptr;
            int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (result != SQLITE_OK)
            {
                throw SQLException("Failed to prepare query: " + std::string(sqlite3_errmsg(db)));
            }

            return std::make_shared<SQLiteResultSet>(stmt);
        }

        int SQLiteConnection::executeUpdate(const std::string &sql)
        {
            if (closed || !db)
            {
                throw SQLException("Connection is closed");
            }

            char *errmsg = nullptr;
            int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);

            if (result != SQLITE_OK)
            {
                std::string error = errmsg ? errmsg : "Unknown error";
                sqlite3_free(errmsg);
                throw SQLException("Failed to execute update: " + error);
            }

            return sqlite3_changes(db);
        }

        void SQLiteConnection::setAutoCommit(bool autoCommit)
        {
            if (closed || !db)
            {
                throw SQLException("Connection is closed");
            }

            if (this->autoCommit && !autoCommit)
            {
                // Start a transaction
                executeUpdate("BEGIN TRANSACTION");
            }
            else if (!this->autoCommit && autoCommit)
            {
                // Commit the current transaction
                executeUpdate("COMMIT");
            }

            this->autoCommit = autoCommit;
        }

        bool SQLiteConnection::getAutoCommit()
        {
            return autoCommit;
        }

        void SQLiteConnection::commit()
        {
            if (closed || !db)
            {
                throw SQLException("Connection is closed");
            }

            executeUpdate("COMMIT");

            // Start a new transaction if auto-commit is disabled
            if (!autoCommit)
            {
                executeUpdate("BEGIN TRANSACTION");
            }
        }

        void SQLiteConnection::rollback()
        {
            if (closed || !db)
            {
                throw SQLException("Connection is closed");
            }

            executeUpdate("ROLLBACK");

            // Start a new transaction if auto-commit is disabled
            if (!autoCommit)
            {
                executeUpdate("BEGIN TRANSACTION");
            }
        }

        void SQLiteConnection::setTransactionIsolation(TransactionIsolationLevel level)
        {
            if (closed || !db)
            {
                throw SQLException("Connection is closed");
            }

            // SQLite only supports SERIALIZABLE isolation level
            if (level != TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
            {
                throw SQLException("SQLite only supports SERIALIZABLE isolation level");
            }

            this->isolationLevel = level;
        }

        TransactionIsolationLevel SQLiteConnection::getTransactionIsolation()
        {
            return isolationLevel;
        }

        void SQLiteConnection::registerStatement(std::shared_ptr<SQLitePreparedStatement> stmt)
        {
            if (!stmt)
                return;

            std::lock_guard<std::mutex> lock(statementsMutex);
            activeStatements.insert(stmt);
        }

        void SQLiteConnection::unregisterStatement(std::shared_ptr<SQLitePreparedStatement> stmt)
        {
            if (!stmt)
                return;

            std::lock_guard<std::mutex> lock(statementsMutex);
            activeStatements.erase(stmt);
        }

        // SQLiteDriver implementation
        SQLiteDriver::SQLiteDriver()
        {
            // SQLite doesn't require explicit initialization
        }

        SQLiteDriver::~SQLiteDriver()
        {
            // SQLite doesn't require explicit cleanup
        }

        std::shared_ptr<Connection> SQLiteDriver::connect(const std::string &url,
                                                          const std::string &user,
                                                          const std::string &password)
        {
            std::string database;

            // Check if the URL is in the expected format
            if (acceptsURL(url))
            {
                // URL is in the format cpp_dbc:sqlite:/path/to/database.db or cpp_dbc:sqlite::memory:
                if (!parseURL(url, database))
                {
                    throw SQLException("Invalid SQLite connection URL: " + url);
                }
            }
            else
            {
                // Try to extract database path directly
                size_t dbStart = url.find("://");
                if (dbStart != std::string::npos)
                {
                    database = url.substr(dbStart + 3);
                }
                else
                {
                    throw SQLException("Invalid SQLite connection URL: " + url);
                }
            }

            return std::make_shared<SQLiteConnection>(database);
        }

        bool SQLiteDriver::acceptsURL(const std::string &url)
        {
            return url.substr(0, 18) == "cpp_dbc:sqlite://";
        }

        bool SQLiteDriver::parseURL(const std::string &url, std::string &database)
        {
            // Parse URL of format: cpp_dbc:sqlite:/path/to/database.db or cpp_dbc:sqlite::memory:
            if (!acceptsURL(url))
            {
                return false;
            }

            // Extract database path
            database = url.substr(18);
            return true;
        }

    } // namespace SQLite
} // namespace cpp_dbc

#endif // USE_SQLITE