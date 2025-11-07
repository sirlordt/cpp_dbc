// CPPDBC_MySQL.cpp
// Implementation of MySQL classes for cpp_dbc

#include "cpp_dbc/drivers/driver_mysql.hpp"
#include <cstring>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <cpp_dbc/common/system_utils.hpp>

namespace cpp_dbc
{
    namespace MySQL
    {

        // MySQLResultSet implementation
        MySQLResultSet::MySQLResultSet(MYSQL_RES *res) : result(res), currentRow(nullptr), rowPosition(0)
        {
            if (result)
            {
                rowCount = mysql_num_rows(result);
                fieldCount = mysql_num_fields(result);

                // Store column names and create column name to index mapping
                MYSQL_FIELD *fields = mysql_fetch_fields(result);
                for (int i = 0; i < fieldCount; i++)
                {
                    std::string name = fields[i].name;
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

        MySQLResultSet::~MySQLResultSet()
        {
            close();
        }

        bool MySQLResultSet::next()
        {
            if (!result || rowPosition >= rowCount)
            {
                return false;
            }

            currentRow = mysql_fetch_row(result);
            if (currentRow)
            {
                rowPosition++;
                return true;
            }

            return false;
        }

        bool MySQLResultSet::isBeforeFirst()
        {
            return rowPosition == 0;
        }

        bool MySQLResultSet::isAfterLast()
        {
            return result && rowPosition > rowCount;
        }

        int MySQLResultSet::getRow()
        {
            return rowPosition;
        }

        int MySQLResultSet::getInt(int columnIndex)
        {
            if (!result || !currentRow || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw DBException("7O8P9Q0R1S2T: Invalid column index");
            }

            // MySQL column indexes are 0-based, but our API is 1-based (like JDBC)
            int idx = columnIndex - 1;
            if (currentRow[idx] == nullptr)
            {
                return 0; // Return 0 for NULL values (similar to JDBC)
            }

            return std::stoi(currentRow[idx]);
        }

        int MySQLResultSet::getInt(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw DBException("3U4V5W6X7Y8Z: Column not found: " + columnName);
            }

            return getInt(it->second + 1); // +1 because getInt(int) is 1-based
        }

        long MySQLResultSet::getLong(int columnIndex)
        {
            if (!result || !currentRow || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw DBException("9A0B1C2D3E4F: Invalid column index");
            }

            int idx = columnIndex - 1;
            if (currentRow[idx] == nullptr)
            {
                return 0;
            }

            return std::stol(currentRow[idx]);
        }

        long MySQLResultSet::getLong(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw DBException("5G6H7I8J9K0L: Column not found: " + columnName);
            }

            return getLong(it->second + 1);
        }

        double MySQLResultSet::getDouble(int columnIndex)
        {
            if (!result || !currentRow || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw DBException("1M2N3O4P5Q6R: Invalid column index");
            }

            int idx = columnIndex - 1;
            if (currentRow[idx] == nullptr)
            {
                return 0.0;
            }

            return std::stod(currentRow[idx]);
        }

        double MySQLResultSet::getDouble(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw DBException("71685784D1EB: Column not found: " + columnName);
            }

            return getDouble(it->second + 1);
        }

        std::string MySQLResultSet::getString(int columnIndex)
        {
            if (!result || !currentRow || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw DBException("089F37F0D90E: Invalid column index");
            }

            int idx = columnIndex - 1;
            if (currentRow[idx] == nullptr)
            {
                return "";
            }

            return std::string(currentRow[idx]);
        }

        std::string MySQLResultSet::getString(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw DBException("45B8E019C425: Column not found: " + columnName);
            }

            return getString(it->second + 1);
        }

        bool MySQLResultSet::getBoolean(int columnIndex)
        {
            std::string value = getString(columnIndex);
            return (value == "1" || value == "true" || value == "TRUE" || value == "True");
        }

        bool MySQLResultSet::getBoolean(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw DBException("94A1D34DC156: Column not found: " + columnName);
            }

            return getBoolean(it->second + 1);
        }

        bool MySQLResultSet::isNull(int columnIndex)
        {
            if (!result || !currentRow || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw DBException("9BB5941B830C: Invalid column index");
            }

            int idx = columnIndex - 1;
            return currentRow[idx] == nullptr;
        }

        bool MySQLResultSet::isNull(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw DBException("DA3E45676022: Column not found: " + columnName);
            }

            return isNull(it->second + 1);
        }

        std::vector<std::string> MySQLResultSet::getColumnNames()
        {
            return columnNames;
        }

        int MySQLResultSet::getColumnCount()
        {
            return fieldCount;
        }

        void MySQLResultSet::close()
        {
            if (result)
            {
                mysql_free_result(result);
                result = nullptr;
                currentRow = nullptr;
                rowPosition = 0;
                rowCount = 0;
                fieldCount = 0;
            }
        }

        // MySQLPreparedStatement implementation
        MySQLPreparedStatement::MySQLPreparedStatement(MYSQL *mysql_conn, const std::string &sql_stmt)
            : mysql(mysql_conn), sql(sql_stmt), stmt(nullptr)
        {

            if (!mysql)
            {
                throw DBException("7S8T9U0V1W2X: Invalid MySQL connection");
            }

            stmt = mysql_stmt_init(mysql);
            if (!stmt)
            {
                throw DBException("3Y4Z5A6B7C8D: Failed to initialize statement");
            }

            if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length()) != 0)
            {
                std::string error = mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                stmt = nullptr;
                throw DBException("9E0F1G2H3I4J: Failed to prepare statement: " + error);
            }

            // Count parameters (question marks) in the SQL
            unsigned long paramCount = mysql_stmt_param_count(stmt);
            binds.resize(paramCount);
            memset(binds.data(), 0, sizeof(MYSQL_BIND) * paramCount);

            // Initialize string values vector
            stringValues.resize(paramCount);

            // Initialize parameter values vector for query reconstruction
            parameterValues.resize(paramCount);

            // Initialize numeric value vectors
            intValues.resize(paramCount);
            longValues.resize(paramCount);
            doubleValues.resize(paramCount);
            nullFlags.resize(paramCount);
        }

        MySQLPreparedStatement::~MySQLPreparedStatement()
        {
            close();
        }

        void MySQLPreparedStatement::close()
        {
            if (stmt)
            {
                mysql_stmt_close(stmt);
                stmt = nullptr;
            }
        }

        void MySQLPreparedStatement::notifyConnClosing()
        {
            // Connection is closing, invalidate the statement without calling mysql_stmt_close
            // since the connection is already being destroyed
            this->close();
        }

        void MySQLPreparedStatement::setInt(int parameterIndex, int value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw DBException("5K6L7M8N9O0P: Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            // Store the value in our vector
            intValues[idx] = value;

            binds[idx].buffer_type = MYSQL_TYPE_LONG;
            binds[idx].buffer = &intValues[idx];
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            parameterValues[idx] = std::to_string(value);
        }

        void MySQLPreparedStatement::setLong(int parameterIndex, long value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw DBException("1Q2R3S4T5U6V: Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            // Store the value in our vector
            longValues[idx] = value;

            binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
            binds[idx].buffer = &longValues[idx];
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            parameterValues[idx] = std::to_string(value);
        }

        void MySQLPreparedStatement::setDouble(int parameterIndex, double value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw DBException("7W8X9Y0Z1A2B: Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            // Store the value in our vector
            doubleValues[idx] = value;

            binds[idx].buffer_type = MYSQL_TYPE_DOUBLE;
            binds[idx].buffer = &doubleValues[idx];
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            parameterValues[idx] = std::to_string(value);
        }

        void MySQLPreparedStatement::setString(int parameterIndex, const std::string &value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw DBException("3C4D5E6F7G8H: Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            // Store the string in our vector to keep it alive
            stringValues[idx] = value;

            binds[idx].buffer_type = MYSQL_TYPE_STRING;
            binds[idx].buffer = (void *)stringValues[idx].c_str();
            binds[idx].buffer_length = stringValues[idx].length();
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;

            // Store parameter value for query reconstruction (with proper escaping)
            std::string escapedValue = value;
            // Simple escape for single quotes
            size_t pos = 0;
            while ((pos = escapedValue.find("'", pos)) != std::string::npos)
            {
                escapedValue.replace(pos, 1, "''");
                pos += 2;
            }
            parameterValues[idx] = "'" + escapedValue + "'";
        }

        void MySQLPreparedStatement::setBoolean(int parameterIndex, bool value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw DBException("9I0J1K2L3M4N: Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            // Store as int value
            intValues[idx] = value ? 1 : 0;

            binds[idx].buffer_type = MYSQL_TYPE_LONG;
            binds[idx].buffer = &intValues[idx];
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;

            // Store parameter value for query reconstruction
            parameterValues[idx] = std::to_string(intValues[idx]);
        }

        void MySQLPreparedStatement::setNull(int parameterIndex, Types type)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw DBException("5O6P7Q8R9S0T: Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            // Setup MySQL type based on our Types enum
            enum_field_types mysqlType;
            switch (type)
            {
            case Types::INTEGER:
                mysqlType = MYSQL_TYPE_LONG;
                break;
            case Types::FLOAT:
                mysqlType = MYSQL_TYPE_FLOAT;
                break;
            case Types::DOUBLE:
                mysqlType = MYSQL_TYPE_DOUBLE;
                break;
            case Types::VARCHAR:
                mysqlType = MYSQL_TYPE_STRING;
                break;
            case Types::DATE:
                mysqlType = MYSQL_TYPE_DATE;
                break;
            case Types::TIMESTAMP:
                mysqlType = MYSQL_TYPE_TIMESTAMP;
                break;
            case Types::BOOLEAN:
                mysqlType = MYSQL_TYPE_TINY;
                break;
            case Types::BLOB:
                mysqlType = MYSQL_TYPE_BLOB;
                break;
            default:
                mysqlType = MYSQL_TYPE_NULL;
            }

            // Store the null flag in our vector (1 for true, 0 for false)
            nullFlags[idx] = 1;

            binds[idx].buffer_type = mysqlType;
            binds[idx].is_null = reinterpret_cast<bool *>(&nullFlags[idx]);
            binds[idx].buffer = nullptr;
            binds[idx].buffer_length = 0;
            binds[idx].length = nullptr;
        }

        void MySQLPreparedStatement::setDate(int parameterIndex, const std::string &value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw DBException("1U2V3W4X5Y6Z: Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            // Store the date string in our vector to keep it alive
            stringValues[idx] = value;

            binds[idx].buffer_type = MYSQL_TYPE_DATE;
            binds[idx].buffer = (void *)stringValues[idx].c_str();
            binds[idx].buffer_length = stringValues[idx].length();
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;
        }

        void MySQLPreparedStatement::setTimestamp(int parameterIndex, const std::string &value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw DBException("7A8B9C0D1E2F: Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            // Store the timestamp string in our vector to keep it alive
            stringValues[idx] = value;

            binds[idx].buffer_type = MYSQL_TYPE_TIMESTAMP;
            binds[idx].buffer = (void *)stringValues[idx].c_str();
            binds[idx].buffer_length = stringValues[idx].length();
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;
        }

        std::shared_ptr<ResultSet> MySQLPreparedStatement::executeQuery()
        {
            if (!stmt)
            {
                throw DBException("3G4H5I6J7K8L: Statement is applied");
            }

            // Reconstruct the query with bound parameters to avoid "Commands out of sync" issue
            std::string finalQuery = sql;

            // Replace each '?' with the corresponding parameter value
            for (size_t i = 0; i < parameterValues.size(); i++)
            {
                size_t pos = finalQuery.find('?');
                if (pos != std::string::npos)
                {
                    finalQuery.replace(pos, 1, parameterValues[i]);
                }
            }

            // Execute the reconstructed query using the regular connection interface
            if (mysql_query(mysql, finalQuery.c_str()) != 0)
            {
                throw DBException(std::string("9M0N1O2P3Q4R: Query failed: ") + mysql_error(mysql));
            }

            MYSQL_RES *result = mysql_store_result(mysql);
            if (!result && mysql_field_count(mysql) > 0)
            {
                throw DBException(std::string("Failed to get result set: ") + mysql_error(mysql));
            }

            auto resultSet = std::make_shared<MySQLResultSet>(result);

            // Close the statement after execution (single-use)
            // This is safe because mysql_store_result() copies all data to client memory
            // close();

            return resultSet;
        }

        int MySQLPreparedStatement::executeUpdate()
        {
            if (!stmt)
            {
                throw DBException("255F5A0C6008: Statement is applied");
            }

            // Bind parameters
            if (!binds.empty() && mysql_stmt_bind_param(stmt, binds.data()) != 0)
            {
                throw DBException(std::string("9B7E537EB656: Failed to bind parameters: ") + mysql_stmt_error(stmt));
            }

            // Execute the query
            if (mysql_stmt_execute(stmt) != 0)
            {
                throw DBException(std::string("547F7466347C: Failed to execute update: ") + mysql_stmt_error(stmt));
            }

            // auto result = mysql_stmt_affected_rows(stmt);

            // this->close();

            // Return the number of affected rows
            return mysql_stmt_affected_rows(stmt);
        }

        bool MySQLPreparedStatement::execute()
        {
            if (!stmt)
            {
                throw DBException("5S6T7U8V9W0X: Statement is not initialized");
            }

            // Bind parameters
            if (!binds.empty() && mysql_stmt_bind_param(stmt, binds.data()) != 0)
            {
                throw DBException(std::string("1Y2Z3A4B5C6D: Failed to bind parameters: ") + mysql_stmt_error(stmt));
            }

            // Execute the query
            if (mysql_stmt_execute(stmt) != 0)
            {
                throw DBException(std::string("7E8F9G0H1I2J: Failed to execute statement: ") + mysql_stmt_error(stmt));
            }

            // Return whether there's a result set
            return mysql_stmt_field_count(stmt) > 0;
        }

        // MySQLConnection implementation
        MySQLConnection::MySQLConnection(const std::string &host,
                                         int port,
                                         const std::string &database,
                                         const std::string &user,
                                         const std::string &password,
                                         const std::map<std::string, std::string> &options)
            : mysql(nullptr), closed(false), autoCommit(true),
              isolationLevel(TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ) // MySQL default
        {
            mysql = mysql_init(nullptr);
            if (!mysql)
            {
                throw DBException("3K4L5M6N7O8P: Failed to initialize MySQL connection");
            }

            // Force TCP/IP connection
            unsigned int protocol = MYSQL_PROTOCOL_TCP;
            mysql_options(mysql, MYSQL_OPT_PROTOCOL, &protocol);

            // Aplicar opciones de conexión desde el mapa
            for (const auto &option : options)
            {
                if (option.first == "connect_timeout")
                {
                    unsigned int timeout = std::stoi(option.second);
                    mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
                }
                else if (option.first == "read_timeout")
                {
                    unsigned int timeout = std::stoi(option.second);
                    mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, &timeout);
                }
                else if (option.first == "write_timeout")
                {
                    unsigned int timeout = std::stoi(option.second);
                    mysql_options(mysql, MYSQL_OPT_WRITE_TIMEOUT, &timeout);
                }
                else if (option.first == "charset")
                {
                    mysql_options(mysql, MYSQL_SET_CHARSET_NAME, option.second.c_str());
                }
                else if (option.first == "auto_reconnect" && option.second == "true")
                {
                    bool reconnect = true;
                    mysql_options(mysql, MYSQL_OPT_RECONNECT, &reconnect);
                }
            }

            // Connect to the database
            if (!mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(),
                                    nullptr, port, nullptr, 0))
            {
                std::string error = mysql_error(mysql);
                mysql_close(mysql);
                mysql = nullptr;
                throw DBException("9Q0R1S2T3U4V: Failed to connect to MySQL: " + error);
            }

            // Select the database if provided
            if (!database.empty())
            {
                if (mysql_select_db(mysql, database.c_str()) != 0)
                {
                    std::string error = mysql_error(mysql);
                    mysql_close(mysql);
                    mysql = nullptr;
                    throw DBException("5W6X7Y8Z9A0B: Failed to select database: " + error);
                }
            }

            // Disable auto-commit by default to match JDBC behavior
            setAutoCommit(true);
        }

        MySQLConnection::~MySQLConnection()
        {
            // cpp_dbc::system_utils::safePrint(cpp_dbc::system_utils::currentTimeMillis(), "(1) MySQLConnection::~MySQLConnection()");

            close();
        }

        void MySQLConnection::close()
        {
            if (!closed && mysql)
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

                // Sleep for 10ms to avoid problems with corrency
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                // cpp_dbc::system_utils::safePrint(cpp_dbc::system_utils::currentTimeMillis(), "(1) MySQLConnection::close()");
                mysql_close(mysql);
                mysql = nullptr;
                closed = true;
            }
        }

        bool MySQLConnection::isClosed()
        {
            return closed;
        }

        void MySQLConnection::returnToPool()
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

        bool MySQLConnection::isPooled()
        {
            return false;
        }

        std::shared_ptr<PreparedStatement> MySQLConnection::prepareStatement(const std::string &sql)
        {
            if (closed || !mysql)
            {
                throw DBException("1C2D3E4F5G6H: Connection is closed");
            }

            auto stmt = std::make_shared<MySQLPreparedStatement>(mysql, sql);

            // Register the statement in our registry
            registerStatement(stmt);

            return stmt;
        }

        std::shared_ptr<ResultSet> MySQLConnection::executeQuery(const std::string &sql)
        {
            if (closed || !mysql)
            {
                throw DBException("7I8J9K0L1M2N: Connection is closed");
            }

            if (mysql_query(mysql, sql.c_str()) != 0)
            {
                throw DBException(std::string("3O4P5Q6R7S8T: Query failed: ") + mysql_error(mysql));
            }

            MYSQL_RES *result = mysql_store_result(mysql);
            if (!result && mysql_field_count(mysql) > 0)
            {
                throw DBException(std::string("9U0V1W2X3Y4Z: Failed to get result set: ") + mysql_error(mysql));
            }

            return std::make_shared<MySQLResultSet>(result);
        }

        int MySQLConnection::executeUpdate(const std::string &sql)
        {
            if (closed || !mysql)
            {
                throw DBException("5A6B7C8D9E0F: Connection is closed");
            }

            if (mysql_query(mysql, sql.c_str()) != 0)
            {
                throw DBException(std::string("1G2H3I4J5K6L: Update failed: ") + mysql_error(mysql));
            }

            return mysql_affected_rows(mysql);
        }

        void MySQLConnection::setAutoCommit(bool autoCommit)
        {
            if (closed || !mysql)
            {
                throw DBException("7M8N9O0P1Q2R: Connection is closed");
            }

            // MySQL: autocommit=0 disables auto-commit, autocommit=1 enables it
            std::string query = "SET autocommit=" + std::to_string(autoCommit ? 1 : 0);
            if (mysql_query(mysql, query.c_str()) != 0)
            {
                throw DBException(std::string("Failed to set autocommit mode: ") + mysql_error(mysql));
            }

            this->autoCommit = autoCommit;
        }

        bool MySQLConnection::getAutoCommit()
        {
            return autoCommit;
        }

        void MySQLConnection::commit()
        {
            if (closed || !mysql)
            {
                throw DBException("3S4T5U6V7W8X: Connection is closed");
            }

            if (mysql_query(mysql, "COMMIT") != 0)
            {
                throw DBException(std::string("9Y0Z1A2B3C4D: Commit failed: ") + mysql_error(mysql));
            }
        }

        void MySQLConnection::rollback()
        {
            if (closed || !mysql)
            {
                throw DBException("5E6F7G8H9I0J: Connection is closed");
            }

            if (mysql_query(mysql, "ROLLBACK") != 0)
            {
                throw DBException(std::string("1K2L3M4N5O6P: Rollback failed: ") + mysql_error(mysql));
            }
        }

        void MySQLConnection::setTransactionIsolation(TransactionIsolationLevel level)
        {
            if (closed || !mysql)
            {
                throw DBException("47FCEE77D4F3: Connection is closed");
            }

            std::string query;
            switch (level)
            {
            case TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL READ UNCOMMITTED";
                break;
            case TransactionIsolationLevel::TRANSACTION_READ_COMMITTED:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL READ COMMITTED";
                break;
            case TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ";
                break;
            case TransactionIsolationLevel::TRANSACTION_SERIALIZABLE:
                query = "SET SESSION TRANSACTION ISOLATION LEVEL SERIALIZABLE";
                break;
            default:
                throw DBException("7Q8R9S0T1U2V: Unsupported transaction isolation level");
            }

            if (mysql_query(mysql, query.c_str()) != 0)
            {
                throw DBException(std::string("3W4X5Y6Z7A8B: Failed to set transaction isolation level: ") + mysql_error(mysql));
            }

            // Verify that the isolation level was actually set
            TransactionIsolationLevel actualLevel = getTransactionIsolation();
            if (actualLevel != level)
            {
                // Some MySQL configurations might not allow certain isolation levels
                // In this case, we'll update our internal state to match what MySQL actually set
                this->isolationLevel = actualLevel;
            }
            else
            {
                this->isolationLevel = level;
            }
        }

        TransactionIsolationLevel MySQLConnection::getTransactionIsolation()
        {
            if (closed || !mysql)
            {
                throw DBException("9C0D1E2F3G4H: Connection is closed");
            }

            // If we're being called from setTransactionIsolation, return the cached value
            // to avoid potential infinite recursion
            static bool inGetTransactionIsolation = false;
            if (inGetTransactionIsolation)
            {
                return this->isolationLevel;
            }

            inGetTransactionIsolation = true;

            try
            {
                // Query the current isolation level
                if (mysql_query(mysql, "SELECT @@transaction_isolation") != 0)
                {
                    // Fall back to older MySQL versions that use tx_isolation
                    if (mysql_query(mysql, "SELECT @@tx_isolation") != 0)
                    {
                        inGetTransactionIsolation = false;
                        throw DBException(std::string("5I6J7K8L9M0N: Failed to get transaction isolation level: ") + mysql_error(mysql));
                    }
                }

                MYSQL_RES *result = mysql_store_result(mysql);
                if (!result)
                {
                    inGetTransactionIsolation = false;
                    throw DBException(std::string("1O2P3Q4R5S6T: Failed to get result set: ") + mysql_error(mysql));
                }

                MYSQL_ROW row = mysql_fetch_row(result);
                if (!row)
                {
                    mysql_free_result(result);
                    inGetTransactionIsolation = false;
                    throw DBException("7U8V9W0X1Y2Z: Failed to fetch transaction isolation level");
                }

                std::string level = row[0];
                mysql_free_result(result);

                // Convert the string value to the enum
                TransactionIsolationLevel isolationResult;
                if (level == "READ-UNCOMMITTED" || level == "READ_UNCOMMITTED")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED;
                else if (level == "READ-COMMITTED" || level == "READ_COMMITTED")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;
                else if (level == "REPEATABLE-READ" || level == "REPEATABLE_READ")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ;
                else if (level == "SERIALIZABLE")
                    isolationResult = TransactionIsolationLevel::TRANSACTION_SERIALIZABLE;
                else
                    isolationResult = TransactionIsolationLevel::TRANSACTION_NONE;

                // Update our cached value
                this->isolationLevel = isolationResult;
                inGetTransactionIsolation = false;
                return isolationResult;
            }
            catch (...)
            {
                inGetTransactionIsolation = false;
                throw;
            }
        }

        void MySQLConnection::registerStatement(std::shared_ptr<MySQLPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(statementsMutex);
            // activeStatements.insert(std::weak_ptr<MySQLPreparedStatement>(stmt));
            activeStatements.insert(stmt);
        }

        void MySQLConnection::unregisterStatement(std::shared_ptr<MySQLPreparedStatement> stmt)
        {
            std::lock_guard<std::mutex> lock(statementsMutex);
            // activeStatements.erase(std::weak_ptr<MySQLPreparedStatement>(stmt));
            activeStatements.erase(stmt);
        }

        // MySQLDriver implementation
        MySQLDriver::MySQLDriver()
        {
            // Initialize MySQL library
            mysql_library_init(0, nullptr, nullptr);
        }

        MySQLDriver::~MySQLDriver()
        {
            // Cleanup MySQL library
            mysql_library_end();
        }

        std::shared_ptr<Connection> MySQLDriver::connect(const std::string &url,
                                                         const std::string &user,
                                                         const std::string &password,
                                                         const std::map<std::string, std::string> &options)
        {
            std::string host;
            int port;
            std::string database = ""; // Default to empty database

            // Simple parsing for common URL formats
            if (url.substr(0, 16) == "cpp_dbc:mysql://")
            {
                std::string temp = url.substr(16); // Remove "cpp_dbc:mysql://"

                // Check if there's a port specified
                size_t colonPos = temp.find(":");
                if (colonPos != std::string::npos)
                {
                    // Host with port
                    host = temp.substr(0, colonPos);

                    // Find if there's a database specified
                    size_t slashPos = temp.find("/", colonPos);
                    if (slashPos != std::string::npos)
                    {
                        // Extract port
                        std::string portStr = temp.substr(colonPos + 1, slashPos - colonPos - 1);
                        try
                        {
                            port = std::stoi(portStr);
                        }
                        catch (...)
                        {
                            throw DBException("3A4B5C6D7E8F: Invalid port in URL: " + url);
                        }

                        // Extract database (if any)
                        if (slashPos + 1 < temp.length())
                        {
                            database = temp.substr(slashPos + 1);
                        }
                    }
                    else
                    {
                        // No database specified, just port
                        std::string portStr = temp.substr(colonPos + 1);
                        try
                        {
                            port = std::stoi(portStr);
                        }
                        catch (...)
                        {
                            throw DBException("9G0H1I2J3K4L: Invalid port in URL: " + url);
                        }
                    }
                }
                else
                {
                    // No port specified
                    size_t slashPos = temp.find("/");
                    if (slashPos != std::string::npos)
                    {
                        // Host with database
                        host = temp.substr(0, slashPos);

                        // Extract database (if any)
                        if (slashPos + 1 < temp.length())
                        {
                            database = temp.substr(slashPos + 1);
                        }

                        port = 3306; // Default MySQL port
                    }
                    else
                    {
                        // Just host
                        host = temp;
                        port = 3306; // Default MySQL port
                    }
                }
            }
            else
            {
                throw DBException("5M6N7O8P9Q0R: Invalid MySQL connection URL: " + url);
            }

            return std::make_shared<MySQLConnection>(host, port, database, user, password, options);
        }

        bool MySQLDriver::acceptsURL(const std::string &url)
        {
            return url.substr(0, 16) == "cpp_dbc:mysql://";
        }

        bool MySQLDriver::parseURL(const std::string &url,
                                   std::string &host,
                                   int &port,
                                   std::string &database)
        {
            // Parse URL of format: cpp_dbc:mysql://host:port/database
            if (!acceptsURL(url))
            {
                return false;
            }

            // Extract host, port, and database
            std::string temp = url.substr(16); // Remove "cpp_dbc:mysql://"

            // Find host:port separator
            size_t hostEnd = temp.find(":");
            if (hostEnd == std::string::npos)
            {
                // Try to find database separator if no port is specified
                hostEnd = temp.find("/");
                if (hostEnd == std::string::npos)
                {
                    // No port and no database specified
                    host = temp;
                    port = 3306;   // Default MySQL port
                    database = ""; // No database
                    return true;
                }

                host = temp.substr(0, hostEnd);
                port = 3306; // Default MySQL port
            }
            else
            {
                host = temp.substr(0, hostEnd);

                // Find port/database separator
                size_t portEnd = temp.find("/", hostEnd + 1);
                if (portEnd == std::string::npos)
                {
                    // No database specified, just port
                    std::string portStr = temp.substr(hostEnd + 1);
                    try
                    {
                        port = std::stoi(portStr);
                    }
                    catch (...)
                    {
                        return false;
                    }

                    // No database part
                    temp = "";
                }
                else
                {
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
            }

            // Extract database name (remove leading '/')
            if (temp.length() > 0)
            {
                database = temp.substr(1);
            }
            else
            {
                // No database specified
                database = "";
            }

            return true;
        }

    } // namespace MySQL
} // namespace cpp_dbc
