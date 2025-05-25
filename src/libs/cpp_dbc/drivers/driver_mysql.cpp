// CPPDBC_MySQL.cpp
// Implementation of MySQL classes for cpp_dbc

#include "driver_mysql.hpp"
#include <cstring>
#include <sstream>
#include <iostream>

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
            if (result)
            {
                mysql_free_result(result);
                result = nullptr;
            }
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
                throw SQLException("Invalid column index");
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
                throw SQLException("Column not found: " + columnName);
            }

            return getInt(it->second + 1); // +1 because getInt(int) is 1-based
        }

        long MySQLResultSet::getLong(int columnIndex)
        {
            if (!result || !currentRow || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw SQLException("Invalid column index");
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
                throw SQLException("Column not found: " + columnName);
            }

            return getLong(it->second + 1);
        }

        double MySQLResultSet::getDouble(int columnIndex)
        {
            if (!result || !currentRow || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw SQLException("Invalid column index");
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
                throw SQLException("Column not found: " + columnName);
            }

            return getDouble(it->second + 1);
        }

        std::string MySQLResultSet::getString(int columnIndex)
        {
            if (!result || !currentRow || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw SQLException("Invalid column index");
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
                throw SQLException("Column not found: " + columnName);
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
                throw SQLException("Column not found: " + columnName);
            }

            return getBoolean(it->second + 1);
        }

        bool MySQLResultSet::isNull(int columnIndex)
        {
            if (!result || !currentRow || columnIndex < 1 || columnIndex > fieldCount)
            {
                throw SQLException("Invalid column index");
            }

            int idx = columnIndex - 1;
            return currentRow[idx] == nullptr;
        }

        bool MySQLResultSet::isNull(const std::string &columnName)
        {
            auto it = columnMap.find(columnName);
            if (it == columnMap.end())
            {
                throw SQLException("Column not found: " + columnName);
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

        // MySQLPreparedStatement implementation
        MySQLPreparedStatement::MySQLPreparedStatement(MYSQL *mysql_conn, const std::string &sql_stmt)
            : mysql(mysql_conn), sql(sql_stmt), stmt(nullptr)
        {

            if (!mysql)
            {
                throw SQLException("Invalid MySQL connection");
            }

            stmt = mysql_stmt_init(mysql);
            if (!stmt)
            {
                throw SQLException("Failed to initialize statement");
            }

            if (mysql_stmt_prepare(stmt, sql.c_str(), sql.length()) != 0)
            {
                std::string error = mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                throw SQLException("Failed to prepare statement: " + error);
            }

            // Count parameters (question marks) in the SQL
            unsigned long paramCount = mysql_stmt_param_count(stmt);
            binds.resize(paramCount);
            memset(binds.data(), 0, sizeof(MYSQL_BIND) * paramCount);

            // Initialize string values vector
            stringValues.resize(paramCount);
        }

        MySQLPreparedStatement::~MySQLPreparedStatement()
        {
            if (stmt)
            {
                mysql_stmt_close(stmt);
                stmt = nullptr;
            }
        }

        void MySQLPreparedStatement::setInt(int parameterIndex, int value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            binds[idx].buffer_type = MYSQL_TYPE_LONG;
            binds[idx].buffer = new int(value);
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;
        }

        void MySQLPreparedStatement::setLong(int parameterIndex, long value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            binds[idx].buffer_type = MYSQL_TYPE_LONGLONG;
            binds[idx].buffer = new long(value);
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;
        }

        void MySQLPreparedStatement::setDouble(int parameterIndex, double value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            binds[idx].buffer_type = MYSQL_TYPE_DOUBLE;
            binds[idx].buffer = new double(value);
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;
        }

        void MySQLPreparedStatement::setString(int parameterIndex, const std::string &value)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw SQLException("Invalid parameter index");
            }

            int idx = parameterIndex - 1;

            // Store the string in our vector to keep it alive
            stringValues[idx] = value;

            binds[idx].buffer_type = MYSQL_TYPE_STRING;
            binds[idx].buffer = (void *)stringValues[idx].c_str();
            binds[idx].buffer_length = stringValues[idx].length();
            binds[idx].is_null = nullptr;
            binds[idx].length = nullptr;
        }

        void MySQLPreparedStatement::setBoolean(int parameterIndex, bool value)
        {
            setInt(parameterIndex, value ? 1 : 0);
        }

        void MySQLPreparedStatement::setNull(int parameterIndex, Types type)
        {
            if (parameterIndex < 1 || parameterIndex > static_cast<int>(binds.size()))
            {
                throw SQLException("Invalid parameter index");
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

            binds[idx].buffer_type = mysqlType;
            binds[idx].is_null = new bool(true); // Set to NULL
            binds[idx].buffer = nullptr;
            binds[idx].buffer_length = 0;
            binds[idx].length = nullptr;
        }

        std::shared_ptr<ResultSet> MySQLPreparedStatement::executeQuery()
        {
            if (!stmt)
            {
                throw SQLException("Statement is not initialized");
            }

            // Bind parameters
            if (!binds.empty() && mysql_stmt_bind_param(stmt, binds.data()) != 0)
            {
                throw SQLException(std::string("Failed to bind parameters: ") + mysql_stmt_error(stmt));
            }

            // Execute the query
            if (mysql_stmt_execute(stmt) != 0)
            {
                throw SQLException(std::string("Failed to execute query: ") + mysql_stmt_error(stmt));
            }

            // Get the result
            if (mysql_stmt_store_result(stmt) != 0)
            {
                throw SQLException(std::string("Failed to store result: ") + mysql_stmt_error(stmt));
            }

            // We need to convert MYSQL_STMT result to MYSQL_RES for our ResultSet
            // In a real implementation, we'd create a dedicated MySQLPreparedResultSet class
            // For simplicity, we'll execute the query again using the regular interface

            MYSQL_RES *result = mysql_store_result(mysql);
            if (!result)
            {
                throw SQLException(std::string("Failed to get result set: ") + mysql_error(mysql));
            }

            return std::make_shared<MySQLResultSet>(result);
        }

        int MySQLPreparedStatement::executeUpdate()
        {
            if (!stmt)
            {
                throw SQLException("Statement is not initialized");
            }

            // Bind parameters
            if (!binds.empty() && mysql_stmt_bind_param(stmt, binds.data()) != 0)
            {
                throw SQLException(std::string("Failed to bind parameters: ") + mysql_stmt_error(stmt));
            }

            // Execute the query
            if (mysql_stmt_execute(stmt) != 0)
            {
                throw SQLException(std::string("Failed to execute update: ") + mysql_stmt_error(stmt));
            }

            // Return the number of affected rows
            return mysql_stmt_affected_rows(stmt);
        }

        bool MySQLPreparedStatement::execute()
        {
            if (!stmt)
            {
                throw SQLException("Statement is not initialized");
            }

            // Bind parameters
            if (!binds.empty() && mysql_stmt_bind_param(stmt, binds.data()) != 0)
            {
                throw SQLException(std::string("Failed to bind parameters: ") + mysql_stmt_error(stmt));
            }

            // Execute the query
            if (mysql_stmt_execute(stmt) != 0)
            {
                throw SQLException(std::string("Failed to execute statement: ") + mysql_stmt_error(stmt));
            }

            // Return whether there's a result set
            return mysql_stmt_field_count(stmt) > 0;
        }

        // MySQLConnection implementation
        MySQLConnection::MySQLConnection(const std::string &host,
                                         int port,
                                         const std::string &database,
                                         const std::string &user,
                                         const std::string &password)
            : mysql(nullptr), closed(false), autoCommit(true)
        {

            mysql = mysql_init(nullptr);
            if (!mysql)
            {
                throw SQLException("Failed to initialize MySQL connection");
            }

            // Connect to the database
            if (!mysql_real_connect(mysql, host.c_str(), user.c_str(), password.c_str(),
                                    database.c_str(), port, nullptr, 0))
            {
                std::string error = mysql_error(mysql);
                mysql_close(mysql);
                mysql = nullptr;
                throw SQLException("Failed to connect to MySQL: " + error);
            }

            // Disable auto-commit by default to match JDBC behavior
            setAutoCommit(true);
        }

        MySQLConnection::~MySQLConnection()
        {
            close();
        }

        void MySQLConnection::close()
        {
            if (!closed && mysql)
            {
                mysql_close(mysql);
                mysql = nullptr;
                closed = true;
            }
        }

        bool MySQLConnection::isClosed()
        {
            return closed;
        }

        std::shared_ptr<PreparedStatement> MySQLConnection::prepareStatement(const std::string &sql)
        {
            if (closed || !mysql)
            {
                throw SQLException("Connection is closed");
            }

            return std::make_shared<MySQLPreparedStatement>(mysql, sql);
        }

        std::shared_ptr<ResultSet> MySQLConnection::executeQuery(const std::string &sql)
        {
            if (closed || !mysql)
            {
                throw SQLException("Connection is closed");
            }

            if (mysql_query(mysql, sql.c_str()) != 0)
            {
                throw SQLException(std::string("Query failed: ") + mysql_error(mysql));
            }

            MYSQL_RES *result = mysql_store_result(mysql);
            if (!result && mysql_field_count(mysql) > 0)
            {
                throw SQLException(std::string("Failed to get result set: ") + mysql_error(mysql));
            }

            return std::make_shared<MySQLResultSet>(result);
        }

        int MySQLConnection::executeUpdate(const std::string &sql)
        {
            if (closed || !mysql)
            {
                throw SQLException("Connection is closed");
            }

            if (mysql_query(mysql, sql.c_str()) != 0)
            {
                throw SQLException(std::string("Update failed: ") + mysql_error(mysql));
            }

            return mysql_affected_rows(mysql);
        }

        void MySQLConnection::setAutoCommit(bool autoCommit)
        {
            if (closed || !mysql)
            {
                throw SQLException("Connection is closed");
            }

            // MySQL: autocommit=0 disables auto-commit, autocommit=1 enables it
            std::string query = "SET autocommit=" + std::to_string(autoCommit ? 1 : 0);
            if (mysql_query(mysql, query.c_str()) != 0)
            {
                throw SQLException(std::string("Failed to set autocommit mode: ") + mysql_error(mysql));
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
                throw SQLException("Connection is closed");
            }

            if (mysql_query(mysql, "COMMIT") != 0)
            {
                throw SQLException(std::string("Commit failed: ") + mysql_error(mysql));
            }
        }

        void MySQLConnection::rollback()
        {
            if (closed || !mysql)
            {
                throw SQLException("Connection is closed");
            }

            if (mysql_query(mysql, "ROLLBACK") != 0)
            {
                throw SQLException(std::string("Rollback failed: ") + mysql_error(mysql));
            }
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
                                                         const std::string &password)
        {
            std::string host;
            int port;
            std::string database;

            if (!parseURL(url, host, port, database))
            {
                throw SQLException("Invalid MySQL connection URL: " + url);
            }

            return std::make_shared<MySQLConnection>(host, port, database, user, password);
        }

        bool MySQLDriver::acceptsURL(const std::string &url)
        {
            return url.substr(0, 14) == "cppdbc:mysql:";
        }

        bool MySQLDriver::parseURL(const std::string &url,
                                   std::string &host,
                                   int &port,
                                   std::string &database)
        {
            // Parse URL of format: cppdbc:mysql://host:port/database
            if (!acceptsURL(url))
            {
                return false;
            }

            // Extract host, port, and database
            std::string temp = url.substr(16); // Remove "cppdbc:mysql://"

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
                port = 3306; // Default MySQL port
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

    } // namespace MySQL
} // namespace cpp_dbc
