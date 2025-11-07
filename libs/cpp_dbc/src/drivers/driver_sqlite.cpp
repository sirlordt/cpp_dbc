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
        std::set<std::weak_ptr<SQLiteConnection>, std::owner_less<std::weak_ptr<SQLiteConnection>>> SQLiteConnection::activeConnections;
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
                throw DBException("A1B2C3D4E5F6: Error stepping through SQLite result set: " +
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
                throw DBException("7A8B9C0D1E2F: Invalid column index or row position");
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
                throw DBException("3G4H5I6J7K8L: Column not found: " + columnName);
            }

            return getInt(it->second + 1); // +1 because getInt(int) is 1-based
        }

        long SQLiteResultSet::getLong(int columnIndex)
        {
            if (!stmt || closed || !hasData || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw DBException("DDAABD02C9D3: Invalid column index or row position");
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
                throw DBException("20C1324B8D71: Column not found: " + columnName);
            }

            return getLong(it->second + 1);
        }

        double SQLiteResultSet::getDouble(int columnIndex)
        {
            if (!stmt || closed || !hasData || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw DBException("9M0N1O2P3Q4R: Invalid column index or row position");
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
                throw DBException("5S6T7U8V9W0X: Column not found: " + columnName);
            }

            return getDouble(it->second + 1);
        }

        std::string SQLiteResultSet::getString(int columnIndex)
        {
            if (!stmt || closed || !hasData || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw DBException("1Y2Z3A4B5C6D: Invalid column index or row position");
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
                throw DBException("93A82C42FA7B: Column not found: " + columnName);
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
                throw DBException("407EBCBBE843: Invalid column index or row position");
            }

            int idx = columnIndex - 1;
            return sqlite3_column_type(stmt, idx) == SQLITE_NULL;
        }

        bool SQLiteResultSet::isNull(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw DBException("8BAE4B58A947: Column not found: " + columnName);
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
                throw DBException("7E8F9G0H1I2J: Invalid SQLite database connection");
            }

            int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (result != SQLITE_OK)
            {
                throw DBException("3K4L5M6N7O8P: Failed to prepare SQLite statement: " + std::string(sqlite3_errmsg(db)));
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
                    for (auto &weak_conn : SQLiteConnection::activeConnections)
                    {
                        auto conn = weak_conn.lock();
                        if (conn && conn->db == db)
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
                throw DBException("9Q0R1S2T3U4V: Statement is closed");
            }

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("5W6X7Y8Z9A0B: Invalid parameter index: " + std::to_string(parameterIndex));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(stmt);
            if (parameterIndex > paramCount)
            {
                throw DBException("1C2D3E4F5G6H: Parameter index out of range: " + std::to_string(parameterIndex) +
                                  " (statement has " + std::to_string(paramCount) + " parameters)");
            }

            // Debug output
            std::cout << "Binding integer parameter " << parameterIndex << " with value " << value
                      << " (statement has " << paramCount << " parameters)" << std::endl;

            int result = sqlite3_bind_int(stmt, parameterIndex, value);
            if (result != SQLITE_OK)
            {
                throw DBException("7I8J9K0L1M2N: Failed to bind integer parameter: " + std::string(sqlite3_errmsg(db)) +
                                  " (index=" + std::to_string(parameterIndex) +
                                  ", value=" + std::to_string(value) +
                                  ", result=" + std::to_string(result) + ")");
            }
        }

        void SQLitePreparedStatement::setLong(int parameterIndex, long value)
        {
            if (closed || !stmt)
            {
                throw DBException("3O4P5Q6R7S8T: Statement is closed");
            }

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("9U0V1W2X3Y4Z: Invalid parameter index: " + std::to_string(parameterIndex));
            }

            int result = sqlite3_bind_int64(stmt, parameterIndex, value);
            if (result != SQLITE_OK)
            {
                throw DBException("5A6B7C8D9E0F: Failed to bind long parameter: " + std::string(sqlite3_errmsg(db)));
            }
        }

        void SQLitePreparedStatement::setDouble(int parameterIndex, double value)
        {
            if (closed || !stmt)
            {
                throw DBException("1G2H3I4J5K6L: Statement is closed");
            }

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("7M8N9O0P1Q2R: Invalid parameter index: " + std::to_string(parameterIndex));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(stmt);
            if (parameterIndex > paramCount)
            {
                throw DBException("3S4T5U6V7W8X: Parameter index out of range: " + std::to_string(parameterIndex) +
                                  " (statement has " + std::to_string(paramCount) + " parameters)");
            }

            // Debug output
            std::cout << "Binding double parameter " << parameterIndex << " with value " << value
                      << " (statement has " << paramCount << " parameters)" << std::endl;

            int result = sqlite3_bind_double(stmt, parameterIndex, value);
            if (result != SQLITE_OK)
            {
                throw DBException("9Y0Z1A2B3C4D: Failed to bind double parameter: " + std::string(sqlite3_errmsg(db)) +
                                  " (index=" + std::to_string(parameterIndex) +
                                  ", value=" + std::to_string(value) +
                                  ", result=" + std::to_string(result) + ")");
            }
        }

        void SQLitePreparedStatement::setString(int parameterIndex, const std::string &value)
        {
            if (closed || !stmt)
            {
                throw DBException("5E6F7G8H9I0J: Statement is closed");
            }

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("1K2L3M4N5O6P: Invalid parameter index: " + std::to_string(parameterIndex));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(stmt);
            if (parameterIndex > paramCount)
            {
                throw DBException("7Q8R9S0T1U2V: Parameter index out of range: " + std::to_string(parameterIndex) +
                                  " (statement has " + std::to_string(paramCount) + " parameters)");
            }

            // Debug output
            std::cout << "Binding string parameter " << parameterIndex << " with value '" << value
                      << "' (statement has " << paramCount << " parameters)" << std::endl;

            // SQLITE_TRANSIENT tells SQLite to make its own copy of the data
            int result = sqlite3_bind_text(stmt, parameterIndex, value.c_str(), -1, SQLITE_TRANSIENT);
            if (result != SQLITE_OK)
            {
                throw DBException("3W4X5Y6Z7A8B: Failed to bind string parameter: " + std::string(sqlite3_errmsg(db)) +
                                  " (index=" + std::to_string(parameterIndex) +
                                  ", value='" + value + "'" +
                                  ", result=" + std::to_string(result) + ")");
            }
        }

        void SQLitePreparedStatement::setBoolean(int parameterIndex, bool value)
        {
            if (closed || !stmt)
            {
                throw DBException("9C0D1E2F3G4H: Statement is closed");
            }

            // Do not reset or clear bindings here
            // This would erase previously set parameters

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("5I6J7K8L9M0N: Invalid parameter index: " + std::to_string(parameterIndex));
            }

            // Get the number of parameters in the statement
            int paramCount = sqlite3_bind_parameter_count(stmt);
            if (parameterIndex > paramCount)
            {
                throw DBException("1O2P3Q4R5S6T: Parameter index out of range: " + std::to_string(parameterIndex) +
                                  " (statement has " + std::to_string(paramCount) + " parameters)");
            }

            // Debug output
            std::cout << "Binding boolean parameter " << parameterIndex << " with value " << (value ? "true" : "false")
                      << " (statement has " << paramCount << " parameters)" << std::endl;

            int intValue = value ? 1 : 0;
            int result = sqlite3_bind_int(stmt, parameterIndex, intValue);
            if (result != SQLITE_OK)
            {
                throw DBException("7U8V9W0X1Y2Z: Failed to bind boolean parameter: " + std::string(sqlite3_errmsg(db)) +
                                  " (index=" + std::to_string(parameterIndex) +
                                  ", value=" + (value ? "true" : "false") +
                                  ", result=" + std::to_string(result) + ")");
            }
        }

        void SQLitePreparedStatement::setNull(int parameterIndex, Types type)
        {
            if (closed || !stmt)
            {
                throw DBException("3A4B5C6D7E8F: Statement is closed");
            }

            // SQLite parameter indices are 1-based, which matches our API
            // Make sure parameterIndex is valid
            if (parameterIndex <= 0)
            {
                throw DBException("9G0H1I2J3K4L: Invalid parameter index: " + std::to_string(parameterIndex));
            }

            int result = sqlite3_bind_null(stmt, parameterIndex);
            if (result != SQLITE_OK)
            {
                throw DBException("5M6N7O8P9Q0R: Failed to bind null parameter: " + std::string(sqlite3_errmsg(db)));
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
                throw DBException("1S2T3U4V5W6X: Statement is closed");
            }

            // Reset the statement to ensure it's ready for execution
            int resetResult = sqlite3_reset(stmt);
            if (resetResult != SQLITE_OK)
            {
                throw DBException("7Y8Z9A0B1C2D: Failed to reset statement: " + std::string(sqlite3_errmsg(db)) +
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
                throw DBException("3E4F5G6H7I8J: Statement is closed");
            }

            // Reset the statement to ensure it's ready for execution
            int resetResult = sqlite3_reset(stmt);
            if (resetResult != SQLITE_OK)
            {
                throw DBException("9K0L1M2N3O4P: Failed to reset statement: " + std::string(sqlite3_errmsg(db)) +
                                  " (result=" + std::to_string(resetResult) + ")");
            }

            // Execute the statement
            int result = sqlite3_step(stmt);
            if (result != SQLITE_DONE)
            {
                throw DBException("5Q6R7S8T9U0V: Failed to execute update: " + std::string(sqlite3_errmsg(db)) +
                                  " (result=" + std::to_string(result) + ")");
            }

            // Get the number of affected rows
            int changes = sqlite3_changes(db);

            // Reset the statement after execution to allow reuse
            resetResult = sqlite3_reset(stmt);
            if (resetResult != SQLITE_OK)
            {
                throw DBException("1W2X3Y4Z5A6B: Failed to reset statement after execution: " + std::string(sqlite3_errmsg(db)) +
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
                throw DBException("7C8D9E0F1G2H: Statement is closed");
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
                throw DBException("3I4J5K6L7M8N: Failed to execute statement: " + std::string(sqlite3_errmsg(db)));
            }
        }

        // SQLiteConnection implementation

        SQLiteConnection::SQLiteConnection(const std::string &database,
                                           const std::map<std::string, std::string> &options)
            : db(nullptr), closed(false), autoCommit(true),
              isolationLevel(TransactionIsolationLevel::TRANSACTION_SERIALIZABLE) // SQLite default
        {
            int result = sqlite3_open_v2(database.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
            if (result != SQLITE_OK)
            {
                std::string error = sqlite3_errmsg(db);
                sqlite3_close_v2(db);
                db = nullptr;
                throw DBException("9O0P1Q2R3S4T: Failed to connect to SQLite database: " + error);
            }

            // Aplicar opciones de configuración
            for (const auto &option : options)
            {
                if (option.first == "foreign_keys" && option.second == "true")
                {
                    executeUpdate("PRAGMA foreign_keys = ON");
                }
                else if (option.first == "journal_mode" && option.second == "WAL")
                {
                    executeUpdate("PRAGMA journal_mode = WAL");
                }
                else if (option.first == "synchronous" && option.second == "FULL")
                {
                    executeUpdate("PRAGMA synchronous = FULL");
                }
                else if (option.first == "synchronous" && option.second == "NORMAL")
                {
                    executeUpdate("PRAGMA synchronous = NORMAL");
                }
                else if (option.first == "synchronous" && option.second == "OFF")
                {
                    executeUpdate("PRAGMA synchronous = OFF");
                }
            }

            // Si no se especificó foreign_keys en las opciones, habilitarlo por defecto
            if (options.find("foreign_keys") == options.end())
            {
                executeUpdate("PRAGMA foreign_keys = ON");
            }

            // Register this connection's weak_ptr in the active connections list
            {
                std::lock_guard<std::mutex> lock(connectionsListMutex);
                try
                {
                    // shared_from_this() will throw if the object wasn't created with make_shared
                    activeConnections.insert(weak_from_this());
                }
                catch (const std::bad_weak_ptr &)
                {
                    // This should not happen if the connection was properly created with make_shared
                    throw DBException("F8A2C7D1E6B5: SQLiteConnection not created with make_shared");
                }
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

            // Ensure this connection's weak_ptr is removed from the active connections list
            // even if close() wasn't called or failed
            {
                std::lock_guard<std::mutex> lock(connectionsListMutex);
                // Find and remove this connection's weak_ptr from the set
                for (auto it = activeConnections.begin(); it != activeConnections.end();)
                {
                    // If the weak_ptr is expired or points to this connection, remove it
                    auto conn_ptr = it->lock();
                    if (!conn_ptr || conn_ptr.get() == this)
                    {
                        it = activeConnections.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
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

                // Remove this connection's weak_ptr from the active connections list
                {
                    std::lock_guard<std::mutex> lock(connectionsListMutex);
                    // Find and remove this connection's weak_ptr from the set
                    for (auto it = activeConnections.begin(); it != activeConnections.end();)
                    {
                        // If the weak_ptr is expired or points to this connection, remove it
                        auto conn_ptr = it->lock();
                        if (!conn_ptr || conn_ptr.get() == this)
                        {
                            it = activeConnections.erase(it);
                        }
                        else
                        {
                            ++it;
                        }
                    }
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
                throw DBException("5U6V7W8X9Y0Z: Connection is closed");
            }

            auto stmt = std::make_shared<SQLitePreparedStatement>(db, sql);
            registerStatement(stmt);
            return stmt;
        }

        std::shared_ptr<ResultSet> SQLiteConnection::executeQuery(const std::string &sql)
        {
            if (closed || !db)
            {
                throw DBException("1A2B3C4D5E6F: Connection is closed");
            }

            sqlite3_stmt *stmt = nullptr;
            int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
            if (result != SQLITE_OK)
            {
                throw DBException("7G8H9I0J1K2L: Failed to prepare query: " + std::string(sqlite3_errmsg(db)));
            }

            return std::make_shared<SQLiteResultSet>(stmt);
        }

        int SQLiteConnection::executeUpdate(const std::string &sql)
        {
            if (closed || !db)
            {
                throw DBException("3M4N5O6P7Q8R: Connection is closed");
            }

            char *errmsg = nullptr;
            int result = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);

            if (result != SQLITE_OK)
            {
                std::string error = errmsg ? errmsg : "Unknown error";
                sqlite3_free(errmsg);
                throw DBException("9S0T1U2V3W4X: Failed to execute update: " + error);
            }

            return sqlite3_changes(db);
        }

        void SQLiteConnection::setAutoCommit(bool autoCommit)
        {
            if (closed || !db)
            {
                throw DBException("5Y6Z7A8B9C0D: Connection is closed");
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
                throw DBException("1E2F3G4H5I6J: Connection is closed");
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
                throw DBException("7K8L9M0N1O2P: Connection is closed");
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
                throw DBException("3Q4R5S6T7U8V: Connection is closed");
            }

            // SQLite only supports SERIALIZABLE isolation level
            if (level != TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
            {
                throw DBException("9W0X1Y2Z3A4B: SQLite only supports SERIALIZABLE isolation level");
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
                                                          const std::string &password,
                                                          const std::map<std::string, std::string> &options)
        {
            std::string database;

            // Check if the URL is in the expected format
            if (acceptsURL(url))
            {
                // URL is in the format cpp_dbc:sqlite:/path/to/database.db or cpp_dbc:sqlite::memory:
                if (!parseURL(url, database))
                {
                    throw DBException("5C6D7E8F9G0H: Invalid SQLite connection URL: " + url);
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
                    throw DBException("1I2J3K4L5M6N: Invalid SQLite connection URL: " + url);
                }
            }

            // It's crucial to use make_shared for shared_from_this() to work properly
            return std::make_shared<SQLiteConnection>(database, options);
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