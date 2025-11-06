// CPPDBC_PostgreSQL.cpp
// Implementation of PostgreSQL classes for cpp_dbc

#include "cpp_dbc/drivers/driver_postgresql.hpp"
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace cpp_dbc
{
    namespace PostgreSQL
    {

        // PostgreSQLResultSet implementation
        PostgreSQLResultSet::PostgreSQLResultSet(PGresult *res) : result(res), rowPosition(0)
        {
            if (result)
            {
                rowCount = PQntuples(result);
                fieldCount = PQnfields(result);

                // Store column names and create column name to index mapping
                for (int i = 0; i < fieldCount; i++)
                {
                    std::string name = PQfname(result, i);
                    columnNames.push_back(name);
                    columnMap[name] = i;
                }
            }
            else
            {
                rowCount = 0;
                fieldCount = 0;
            }
        }

        PostgreSQLResultSet::~PostgreSQLResultSet()
        {
            close();
        }

        bool PostgreSQLResultSet::next()
        {
            if (!result || rowPosition >= rowCount)
            {
                return false;
            }

            rowPosition++;
            return true;
        }

        bool PostgreSQLResultSet::isBeforeFirst()
        {
            return rowPosition == 0;
        }

        bool PostgreSQLResultSet::isAfterLast()
        {
            return result && rowPosition > rowCount;
        }

        int PostgreSQLResultSet::getRow()
        {
            return rowPosition;
        }

        int PostgreSQLResultSet::getInt(int columnIndex)
        {
            if (!result || columnIndex < 1 || columnIndex > fieldCount || rowPosition < 1 || rowPosition > rowCount)
            {
                throw SQLException("Invalid column index or row position");
            }

            // PostgreSQL column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = columnIndex - 1;
            int row = rowPosition - 1;

            if (PQgetisnull(result, row, idx))
            {
                return 0; // Return 0 for NULL values (similar to JDBC)
            }

            const char *value = PQgetvalue(result, row, idx);
            try
            {
                return std::stoi(value);
            }
            catch (const std::exception &e)
            {
                throw SQLException("Failed to convert value to int");
            }
        }

        int PostgreSQLResultSet::getInt(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return getInt(it->second + 1); // +1 because getInt(int) is 1-based
        }

        long PostgreSQLResultSet::getLong(int columnIndex)
        {
            if (!result || columnIndex < 1 || columnIndex > fieldCount || rowPosition < 1 || rowPosition > rowCount)
            {
                throw SQLException("Invalid column index or row position");
            }

            int idx = columnIndex - 1;
            int row = rowPosition - 1;

            if (PQgetisnull(result, row, idx))
            {
                return 0;
            }

            const char *value = PQgetvalue(result, row, idx);
            try
            {
                return std::stol(value);
            }
            catch (const std::exception &e)
            {
                throw SQLException("Failed to convert value to long");
            }
        }

        long PostgreSQLResultSet::getLong(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return getLong(it->second + 1);
        }

        double PostgreSQLResultSet::getDouble(int columnIndex)
        {
            if (!result || columnIndex < 1 || columnIndex > fieldCount || rowPosition < 1 || rowPosition > rowCount)
            {
                throw SQLException("Invalid column index or row position");
            }

            int idx = columnIndex - 1;
            int row = rowPosition - 1;

            if (PQgetisnull(result, row, idx))
            {
                return 0.0;
            }

            const char *value = PQgetvalue(result, row, idx);
            try
            {
                return std::stod(value);
            }
            catch (const std::exception &e)
            {
                throw SQLException("Failed to convert value to double");
            }
        }

        double PostgreSQLResultSet::getDouble(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return getDouble(it->second + 1);
        }

        std::string PostgreSQLResultSet::getString(int columnIndex)
        {
            if (!result || columnIndex < 1 || columnIndex > fieldCount || rowPosition < 1 || rowPosition > rowCount)
            {
                throw SQLException("Invalid column index or row position");
            }

            int idx = columnIndex - 1;
            int row = rowPosition - 1;

            if (PQgetisnull(result, row, idx))
            {
                return "";
            }

            return std::string(PQgetvalue(result, row, idx));
        }

        std::string PostgreSQLResultSet::getString(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return getString(it->second + 1);
        }

        bool PostgreSQLResultSet::getBoolean(int columnIndex)
        {
            std::string value = getString(columnIndex);
            return (value == "t" || value == "true" || value == "1" || value == "TRUE" || value == "True");
        }

        bool PostgreSQLResultSet::getBoolean(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return getBoolean(it->second + 1);
        }

        bool PostgreSQLResultSet::isNull(int columnIndex)
        {
            if (!result || columnIndex < 1 || columnIndex > fieldCount || rowPosition < 1 || rowPosition > rowCount)
            {
                throw SQLException("Invalid column index or row position");
            }

            int idx = columnIndex - 1;
            int row = rowPosition - 1;

            return PQgetisnull(result, row, idx);
        }

        bool PostgreSQLResultSet::isNull(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
            }

            return isNull(it->second + 1);
        }

        std::vector<std::string> PostgreSQLResultSet::getColumnNames()
        {
            return columnNames;
        }

        int PostgreSQLResultSet::getColumnCount()
        {
            return fieldCount;
        }

        void PostgreSQLResultSet::close()
        {
            if (result)
            {
                PQclear(result);
                result = nullptr;
                rowPosition = 0;
                rowCount = 0;
                fieldCount = 0;
            }
        }

        // PostgreSQLPreparedStatement implementation
        PostgreSQLPreparedStatement::PostgreSQLPreparedStatement(PGconn *conn_handle, const std::string &sql_stmt, const std::string &stmt_name)
            : conn(conn_handle), sql(sql_stmt), prepared(false), statementCounter(0), stmtName(stmt_name)
        {
            if (!conn)
            {
                throw SQLException("Invalid PostgreSQL connection");
            }

            // Count parameters (using $1, $2, etc. instead of ?)
            // This is a simplification - in reality, PostgreSQL allows more complex parameter references
            int paramCount = 0;
            size_t pos = 0;
            while ((pos = sql.find("$", pos)) != std::string::npos)
            {
                pos++;
                if (pos < sql.length() && isdigit(sql[pos]))
                {
                    int paramIdx = 0;
                    while (pos < sql.length() && isdigit(sql[pos]))
                    {
                        paramIdx = paramIdx * 10 + (sql[pos] - '0');
                        pos++;
                    }
                    if (paramIdx > paramCount)
                    {
                        paramCount = paramIdx;
                    }
                }
            }

            // Initialize parameter arrays
            paramValues.resize(paramCount);
            paramLengths.resize(paramCount);
            paramFormats.resize(paramCount);
            paramTypes.resize(paramCount);

            // Default to text format for all parameters
            for (int i = 0; i < paramCount; i++)
            {
                paramValues[i] = "";
                paramLengths[i] = 0;
                paramFormats[i] = 0; // 0 = text, 1 = binary
                paramTypes[i] = 0;   // 0 = let server guess
            }
        }

        PostgreSQLPreparedStatement::~PostgreSQLPreparedStatement()
        {
            close();
        }

        void PostgreSQLPreparedStatement::close()
        {
            if (prepared)
            {
                // Deallocate the prepared statement
                std::string deallocateSQL = "DEALLOCATE " + stmtName;
                PGresult *res = PQexec(conn, deallocateSQL.c_str());
                if (res)
                {
                    PQclear(res);
                }
                prepared = false;
            }
        }

        void PostgreSQLPreparedStatement::notifyConnClosing()
        {
            // Connection is closing, invalidate the statement without calling mysql_stmt_close
            // since the connection is already being destroyed
            this->close();
        }

        void PostgreSQLPreparedStatement::setInt(int parameterIndex, int value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(paramValues.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;
            paramValues[idx] = std::to_string(value);
            paramLengths[idx] = paramValues[idx].length();
            paramFormats[idx] = 0; // Text format
            paramTypes[idx] = 23;  // INT4OID
        }

        void PostgreSQLPreparedStatement::setLong(int parameterIndex, long value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(paramValues.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;
            paramValues[idx] = std::to_string(value);
            paramLengths[idx] = paramValues[idx].length();
            paramFormats[idx] = 0; // Text format
            paramTypes[idx] = 20;  // INT8OID
        }

        void PostgreSQLPreparedStatement::setDouble(int parameterIndex, double value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(paramValues.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;
            paramValues[idx] = std::to_string(value);
            paramLengths[idx] = paramValues[idx].length();
            paramFormats[idx] = 0; // Text format
            paramTypes[idx] = 701; // FLOAT8OID
        }

        void PostgreSQLPreparedStatement::setString(int parameterIndex, const std::string &value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(paramValues.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;
            paramValues[idx] = value;
            paramLengths[idx] = paramValues[idx].length();
            paramFormats[idx] = 0; // Text format
            paramTypes[idx] = 25;  // TEXTOID
        }

        void PostgreSQLPreparedStatement::setBoolean(int parameterIndex, bool value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(paramValues.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;
            paramValues[idx] = value ? "t" : "f"; // PostgreSQL uses 't' and 'f' for boolean values
            paramLengths[idx] = 1;
            paramFormats[idx] = 0; // Text format
            paramTypes[idx] = 16;  // BOOLOID
        }

        void PostgreSQLPreparedStatement::setNull(int parameterIndex, Types type)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(paramValues.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            // Set the value to NULL
            paramValues[idx] = "";

            // Set the OID type based on our Types enum
            Oid pgType;
            switch (type)
            {
            case Types::INTEGER:
                pgType = 23; // INT4OID
                break;
            case Types::FLOAT:
                pgType = 700; // FLOAT4OID
                break;
            case Types::DOUBLE:
                pgType = 701; // FLOAT8OID
                break;
            case Types::VARCHAR:
                pgType = 25; // TEXTOID
                break;
            case Types::DATE:
                pgType = 1082; // DATEOID
                break;
            case Types::TIMESTAMP:
                pgType = 1114; // TIMESTAMPOID
                break;
            case Types::BOOLEAN:
                pgType = 16; // BOOLOID
                break;
            case Types::BLOB:
                pgType = 17; // BYTEAOID
                break;
            default:
                pgType = 0; // Let the server guess
            }

            paramTypes[idx] = pgType;
            paramLengths[idx] = 0;
            paramFormats[idx] = 0; // Text format
        }

        void PostgreSQLPreparedStatement::setDate(int parameterIndex, const std::string &value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(paramValues.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;
            paramValues[idx] = value;
            paramLengths[idx] = paramValues[idx].length();
            paramFormats[idx] = 0;  // Text format
            paramTypes[idx] = 1082; // DATEOID
        }

        void PostgreSQLPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(paramValues.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;
            paramValues[idx] = value;
            paramLengths[idx] = paramValues[idx].length();
            paramFormats[idx] = 0;  // Text format
            paramTypes[idx] = 1114; // TIMESTAMPOID
        }

        std::shared_ptr<ResultSet> PostgreSQLPreparedStatement::executeQuery()
        {
            if (!conn)
            {
                throw SQLException("Connection is invalid");
            }

            // Prepare the statement if not already prepared
            if (!prepared)
            {
                PGresult *prepareResult = PQprepare(conn, stmtName.c_str(), sql.c_str(), paramValues.size(), paramTypes.data());
                if (PQresultStatus(prepareResult) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(prepareResult);
                    PQclear(prepareResult);
                    throw SQLException("Failed to prepare statement: " + error);
                }
                PQclear(prepareResult);
                prepared = true;
            }

            // Convert parameter values to C-style array of char pointers
            std::vector<const char *> paramValuePtrs(paramValues.size());
            for (size_t i = 0; i < paramValues.size(); i++)
            {
                paramValuePtrs[i] = paramValues[i].empty() ? nullptr : paramValues[i].c_str();
            }

            // Execute the prepared statement
            PGresult *result = PQexecPrepared(
                conn,
                stmtName.c_str(),
                paramValues.size(),
                paramValuePtrs.data(),
                paramLengths.data(),
                paramFormats.data(),
                0 // Result format (0 = text)
            );

            if (PQresultStatus(result) != PGRES_TUPLES_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw SQLException("Failed to execute query: " + error);
            }

            auto resultSet = std::make_shared<PostgreSQLResultSet>(result);

            // Close the statement after execution (single-use)
            // This is safe because PQexecPrepared() copies all data to the PGresult
            close();

            return resultSet;
        }

        int PostgreSQLPreparedStatement::executeUpdate()
        {
            if (!conn)
            {
                throw SQLException("Connection is invalid");
            }

            // Prepare the statement if not already prepared
            if (!prepared)
            {
                PGresult *prepareResult = PQprepare(conn, stmtName.c_str(), sql.c_str(), paramValues.size(), paramTypes.data());
                if (PQresultStatus(prepareResult) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(prepareResult);
                    PQclear(prepareResult);
                    throw SQLException("Failed to prepare statement: " + error);
                }
                PQclear(prepareResult);
                prepared = true;
            }

            // Convert parameter values to C-style array of char pointers
            std::vector<const char *> paramValuePtrs(paramValues.size());
            for (size_t i = 0; i < paramValues.size(); i++)
            {
                paramValuePtrs[i] = paramValues[i].empty() ? nullptr : paramValues[i].c_str();
            }

            // Execute the prepared statement
            PGresult *result = PQexecPrepared(
                conn,
                stmtName.c_str(),
                paramValues.size(),
                paramValuePtrs.data(),
                paramLengths.data(),
                paramFormats.data(),
                0 // Result format (0 = text)
            );

            if (PQresultStatus(result) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw SQLException("Failed to execute update: " + error);
            }

            // Get the number of affected rows
            char *affectedRows = PQcmdTuples(result);
            int rowCount = 0;
            if (affectedRows && affectedRows[0] != '\0')
            {
                rowCount = std::stoi(affectedRows);
            }

            PQclear(result);

            // Close the statement after execution (single-use)
            close();

            return rowCount;
        }

        bool PostgreSQLPreparedStatement::execute()
        {
            if (!conn)
            {
                throw SQLException("Connection is invalid");
            }

            // Prepare the statement if not already prepared
            if (!prepared)
            {
                PGresult *prepareResult = PQprepare(conn, stmtName.c_str(), sql.c_str(), paramValues.size(), paramTypes.data());
                if (PQresultStatus(prepareResult) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(prepareResult);
                    PQclear(prepareResult);
                    throw SQLException("Failed to prepare statement: " + error);
                }
                PQclear(prepareResult);
                prepared = true;
            }

            // Convert parameter values to C-style array of char pointers
            std::vector<const char *> paramValuePtrs(paramValues.size());
            for (size_t i = 0; i < paramValues.size(); i++)
            {
                paramValuePtrs[i] = paramValues[i].empty() ? nullptr : paramValues[i].c_str();
            }

            // Execute the prepared statement
            PGresult *result = PQexecPrepared(
                conn,
                stmtName.c_str(),
                paramValues.size(),
                paramValuePtrs.data(),
                paramLengths.data(),
                paramFormats.data(),
                0 // Result format (0 = text)
            );

            ExecStatusType status = PQresultStatus(result);
            bool hasResultSet = (status == PGRES_TUPLES_OK);

            // Clean up if there was an error
            if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw SQLException("Failed to execute statement: " + error);
            }

            PQclear(result);
            return hasResultSet;
        }

        // PostgreSQLConnection implementation
        PostgreSQLConnection::PostgreSQLConnection(const std::string &host,
                                                   int port,
                                                   const std::string &database,
                                                   const std::string &user,
                                                   const std::string &password)
            : conn(nullptr), closed(false), autoCommit(true), statementCounter(0),
              isolationLevel(TransactionIsolationLevel::TRANSACTION_READ_COMMITTED) // PostgreSQL default
        {
            // Build connection string
            std::stringstream conninfo;
            conninfo << "host=" << host << " ";
            conninfo << "port=" << port << " ";
            conninfo << "dbname=" << database << " ";
            conninfo << "user=" << user << " ";
            conninfo << "password=" << password;

            // Connect to the database
            conn = PQconnectdb(conninfo.str().c_str());
            if (PQstatus(conn) != CONNECTION_OK)
            {
                std::string error = PQerrorMessage(conn);
                PQfinish(conn);
                conn = nullptr;
                throw SQLException("Failed to connect to PostgreSQL: " + error);
            }

            // Set up a notice processor to suppress NOTICE messages
            PQsetNoticeProcessor(conn, [](void *, const char *)
                                 {
                                     // Do nothing with the notice message
                                 },
                                 nullptr);

            // Set auto-commit mode
            setAutoCommit(true);
        }

        PostgreSQLConnection::~PostgreSQLConnection()
        {
            close();
        }

        void PostgreSQLConnection::close()
        {
            if (!closed && conn)
            {

                // Notify all active statements that connection is closing
                {
                    std::lock_guard<std::mutex> lock(statementsMutex);
                    for (auto &stmt : activeStatements)
                    {
                        // if (auto stmt = weakStmt.lock())
                        if (stmt)
                        {
                            stmt->notifyConnClosing();
                        }
                    }
                    activeStatements.clear();
                }

                PQfinish(conn);
                conn = nullptr;
                closed = true;

                // Sleep for 5ms to avoid problems with concurrency and memory stability
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        bool PostgreSQLConnection::isClosed()
        {
            return closed;
        }

        void PostgreSQLConnection::returnToPool()
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

        bool PostgreSQLConnection::isPooled()
        {
            return false;
        }

        std::shared_ptr<PreparedStatement> PostgreSQLConnection::prepareStatement(const std::string &sql)
        {
            if (closed || !conn)
            {
                throw SQLException("Connection is closed");
            }

            // Generate a unique statement name and pass it to the prepared statement
            std::string stmtName = generateStatementName();
            auto stmt = std::make_shared<PostgreSQLPreparedStatement>(conn, sql, stmtName);

            registerStatement(stmt);

            return stmt;
        }

        std::shared_ptr<ResultSet> PostgreSQLConnection::executeQuery(const std::string &sql)
        {
            if (closed || !conn)
            {
                throw SQLException("Connection is closed");
            }

            PGresult *result = PQexec(conn, sql.c_str());
            if (PQresultStatus(result) != PGRES_TUPLES_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw SQLException("Query failed: " + error);
            }

            return std::make_shared<PostgreSQLResultSet>(result);
        }

        int PostgreSQLConnection::executeUpdate(const std::string &sql)
        {
            if (closed || !conn)
            {
                throw SQLException("Connection is closed");
            }

            PGresult *result = PQexec(conn, sql.c_str());
            if (PQresultStatus(result) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw SQLException("Update failed: " + error);
            }

            // Get the number of affected rows
            char *affectedRows = PQcmdTuples(result);
            int rowCount = 0;
            if (affectedRows && affectedRows[0] != '\0')
            {
                rowCount = std::stoi(affectedRows);
            }

            PQclear(result);
            return rowCount;
        }

        void PostgreSQLConnection::setAutoCommit(bool autoCommit)
        {
            if (closed || !conn)
            {
                throw SQLException("Connection is closed");
            }

            // PostgreSQL: BEGIN starts a transaction, COMMIT ends it
            // If autoCommit is true, we don't need to do anything special
            // If autoCommit is false, we need to start a transaction if we're not already in one
            if (this->autoCommit && !autoCommit)
            {
                // Start a transaction
                std::string beginCmd = "BEGIN";

                // For SERIALIZABLE isolation, we need to ensure the snapshot is acquired immediately
                // by using a READ ONLY DEFERRABLE transaction when possible
                if (isolationLevel == TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
                {
                    beginCmd = "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE";

                    // Execute a dummy SELECT to force snapshot acquisition immediately
                    PGresult *dummyResult = PQexec(conn, beginCmd.c_str());
                    if (PQresultStatus(dummyResult) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(dummyResult);
                        PQclear(dummyResult);
                        throw SQLException("Failed to start SERIALIZABLE transaction: " + error);
                    }
                    PQclear(dummyResult);

                    // Force snapshot acquisition with a dummy query
                    PGresult *snapshotResult = PQexec(conn, "SELECT 1");
                    if (PQresultStatus(snapshotResult) != PGRES_TUPLES_OK)
                    {
                        std::string error = PQresultErrorMessage(snapshotResult);
                        PQclear(snapshotResult);
                        throw SQLException("Failed to acquire snapshot: " + error);
                    }
                    PQclear(snapshotResult);
                    return; // We've already started the transaction and acquired the snapshot
                }
                else
                {
                    // Standard BEGIN for other isolation levels
                    PGresult *result = PQexec(conn, beginCmd.c_str());
                    if (PQresultStatus(result) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(result);
                        PQclear(result);
                        throw SQLException("Failed to start transaction: " + error);
                    }
                    PQclear(result);
                }
            }
            else if (!this->autoCommit && autoCommit)
            {
                // Commit the current transaction
                PGresult *result = PQexec(conn, "COMMIT");
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    throw SQLException("Failed to commit transaction: " + error);
                }
                PQclear(result);
            }

            this->autoCommit = autoCommit;
        }

        bool PostgreSQLConnection::getAutoCommit()
        {
            return autoCommit;
        }

        void PostgreSQLConnection::commit()
        {
            if (closed || !conn)
            {
                throw SQLException("Connection is closed");
            }

            PGresult *result = PQexec(conn, "COMMIT");
            if (PQresultStatus(result) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw SQLException("Commit failed: " + error);
            }
            PQclear(result);

            // Start a new transaction if auto-commit is disabled
            if (!autoCommit)
            {
                result = PQexec(conn, "BEGIN");
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    throw SQLException("Failed to start transaction: " + error);
                }
                PQclear(result);
            }
        }

        void PostgreSQLConnection::rollback()
        {
            if (closed || !conn)
            {
                throw SQLException("Connection is closed");
            }

            PGresult *result = PQexec(conn, "ROLLBACK");
            if (PQresultStatus(result) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw SQLException("Rollback failed: " + error);
            }
            PQclear(result);

            // Start a new transaction if auto-commit is disabled
            if (!autoCommit)
            {
                result = PQexec(conn, "BEGIN");
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    throw SQLException("Failed to start transaction: " + error);
                }
                PQclear(result);
            }
        }

        void PostgreSQLConnection::setTransactionIsolation(TransactionIsolationLevel level)
        {
            if (closed || !conn)
            {
                throw SQLException("Connection is closed");
            }

            std::string query;
            switch (level)
            {
            case TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED:
                // PostgreSQL treats READ UNCOMMITTED the same as READ COMMITTED
                query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
                break;
            case TransactionIsolationLevel::TRANSACTION_READ_COMMITTED:
                query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL READ COMMITTED";
                break;
            case TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ:
                query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL REPEATABLE READ";
                break;
            case TransactionIsolationLevel::TRANSACTION_SERIALIZABLE:
                query = "SET SESSION CHARACTERISTICS AS TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                break;
            default:
                throw SQLException("Unsupported transaction isolation level");
            }

            PGresult *result = PQexec(conn, query.c_str());
            if (PQresultStatus(result) != PGRES_COMMAND_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw SQLException("Failed to set transaction isolation level: " + error);
            }
            PQclear(result);

            this->isolationLevel = level;

            // If we're in a transaction (autoCommit = false), we need to restart it
            // for the new isolation level to take effect
            if (!autoCommit)
            {
                result = PQexec(conn, "COMMIT");
                if (PQresultStatus(result) != PGRES_COMMAND_OK)
                {
                    std::string error = PQresultErrorMessage(result);
                    PQclear(result);
                    throw SQLException("Failed to commit transaction: " + error);
                }
                PQclear(result);

                // For SERIALIZABLE isolation, we need special handling
                if (isolationLevel == TransactionIsolationLevel::TRANSACTION_SERIALIZABLE)
                {
                    std::string beginCmd = "BEGIN TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                    result = PQexec(conn, beginCmd.c_str());
                    if (PQresultStatus(result) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(result);
                        PQclear(result);
                        throw SQLException("Failed to start SERIALIZABLE transaction: " + error);
                    }
                    PQclear(result);

                    // Force snapshot acquisition with a dummy query
                    PGresult *snapshotResult = PQexec(conn, "SELECT 1");
                    if (PQresultStatus(snapshotResult) != PGRES_TUPLES_OK)
                    {
                        std::string error = PQresultErrorMessage(snapshotResult);
                        PQclear(snapshotResult);
                        throw SQLException("Failed to acquire snapshot: " + error);
                    }
                    PQclear(snapshotResult);
                }
                else
                {
                    // Standard BEGIN for other isolation levels
                    result = PQexec(conn, "BEGIN");
                    if (PQresultStatus(result) != PGRES_COMMAND_OK)
                    {
                        std::string error = PQresultErrorMessage(result);
                        PQclear(result);
                        throw SQLException("Failed to start transaction: " + error);
                    }
                    PQclear(result);
                }
            }
        }

        TransactionIsolationLevel PostgreSQLConnection::getTransactionIsolation()
        {
            if (closed || !conn)
            {
                throw SQLException("Connection is closed");
            }

            // Query the current isolation level
            PGresult *result = PQexec(conn, "SHOW transaction_isolation");
            if (PQresultStatus(result) != PGRES_TUPLES_OK)
            {
                std::string error = PQresultErrorMessage(result);
                PQclear(result);
                throw SQLException("Failed to get transaction isolation level: " + error);
            }

            if (PQntuples(result) == 0)
            {
                PQclear(result);
                throw SQLException("Failed to fetch transaction isolation level");
            }

            std::string level = PQgetvalue(result, 0, 0);
            PQclear(result);

            // Convert the string value to the enum - handle both formats
            std::string levelLower = level;
            // Convert to lowercase for case-insensitive comparison
            std::transform(levelLower.begin(), levelLower.end(), levelLower.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });

            if (levelLower == "read uncommitted" || levelLower == "read_uncommitted")
                return TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED;
            else if (levelLower == "read committed" || levelLower == "read_committed")
                return TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;
            else if (levelLower == "repeatable read" || levelLower == "repeatable_read")
                return TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ;
            else if (levelLower == "serializable")
                return TransactionIsolationLevel::TRANSACTION_SERIALIZABLE;
            else
                return TransactionIsolationLevel::TRANSACTION_NONE;
        }

        std::string PostgreSQLConnection::generateStatementName()
        {
            std::stringstream ss;
            ss << "stmt_" << statementCounter++;
            return ss.str();
        }

        void PostgreSQLConnection::registerStatement(std::shared_ptr<PostgreSQLPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(statementsMutex);
            // activeStatements.insert(std::weak_ptr<MySQLPreparedStatement>(stmt));
            activeStatements.insert(stmt);
        }

        void PostgreSQLConnection::unregisterStatement(std::shared_ptr<PostgreSQLPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(statementsMutex);
            // activeStatements.erase(std::weak_ptr<MySQLPreparedStatement>(stmt));
            activeStatements.erase(stmt);
        }

        // PostgreSQLDriver implementation
        PostgreSQLDriver::PostgreSQLDriver()
        {
            // PostgreSQL doesn't require explicit initialization like MySQL
        }

        PostgreSQLDriver::~PostgreSQLDriver()
        {
            // Also call PQfinish with nullptr as a fallback
            PQfinish(nullptr);

            // Sleep a bit more to ensure all resources are properly released
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        std::shared_ptr<Connection> PostgreSQLDriver::connect(const std::string &url,
                                                              const std::string &user,
                                                              const std::string &password)
        {
            std::string host;
            int port;
            std::string database;

            // Check if the URL is in the expected format
            if (acceptsURL(url))
            {
                // URL is in the format cpp_dbc:postgresql://host:port/database
                if (!parseURL(url, host, port, database))
                {
                    throw SQLException("Invalid PostgreSQL connection URL: " + url);
                }
            }
            else
            {
                // Try to extract host, port, and database directly
                size_t hostStart = url.find("://");
                if (hostStart != std::string::npos)
                {
                    std::string temp = url.substr(hostStart + 3);

                    // Find host:port separator
                    size_t hostEnd = temp.find(":");
                    if (hostEnd == std::string::npos)
                    {
                        // Try to find database separator if no port is specified
                        hostEnd = temp.find("/");
                        if (hostEnd == std::string::npos)
                        {
                            throw SQLException("Invalid PostgreSQL connection URL: " + url);
                        }

                        host = temp.substr(0, hostEnd);
                        port = 5432; // Default PostgreSQL port
                    }
                    else
                    {
                        host = temp.substr(0, hostEnd);

                        // Find port/database separator
                        size_t portEnd = temp.find("/", hostEnd + 1);
                        if (portEnd == std::string::npos)
                        {
                            throw SQLException("Invalid PostgreSQL connection URL: " + url);
                        }

                        std::string portStr = temp.substr(hostEnd + 1, portEnd - hostEnd - 1);
                        try
                        {
                            port = std::stoi(portStr);
                        }
                        catch (...)
                        {
                            throw SQLException("Invalid port in URL: " + url);
                        }

                        // Extract database
                        database = temp.substr(portEnd + 1);
                    }
                }
                else
                {
                    throw SQLException("Invalid PostgreSQL connection URL: " + url);
                }
            }

            return std::make_shared<PostgreSQLConnection>(host, port, database, user, password);
        }

        bool PostgreSQLDriver::acceptsURL(const std::string &url)
        {
            return url.substr(0, 21) == "cpp_dbc:postgresql://";
        }

        bool PostgreSQLDriver::parseURL(const std::string &url,
                                        std::string &host,
                                        int &port,
                                        std::string &database)
        {
            // Parse URL of format: cpp_dbc:postgresql://host:port/database
            if (!acceptsURL(url))
            {
                return false;
            }

            // Extract host, port, and database
            std::string temp = url.substr(21); // Remove "cpp_dbc:postgresql://"

            // Find host:port separator
            size_t hostEnd = temp.find(":");
            if (hostEnd == std::string::npos)
            {
                // Try to find database separator if no port is specified
                hostEnd = temp.find("/");
                if (hostEnd == std::string::npos)
                {
                    return false;
                }

                host = temp.substr(0, hostEnd);
                port = 5432; // Default PostgreSQL port
            }
            else
            {
                host = temp.substr(0, hostEnd);

                // Find port/database separator
                size_t portEnd = temp.find("/", hostEnd + 1);
                if (portEnd == std::string::npos)
                {
                    return false;
                }

                std::string portStr = temp.substr(hostEnd + 1, portEnd - hostEnd - 1);
                try
                {
                    port = std::stoi(portStr);
                }
                catch (...)
                {
                    return false;
                }

                temp = temp.substr(portEnd);
            }

            // Extract database name (remove leading '/')
            database = temp.substr(1);

            return true;
        }
    } // namespace PostgreSQL
} // namespace cpp_dbc
