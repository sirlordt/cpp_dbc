// CPPDBC_MySQL.hpp
// MySQL implementation for cpp_dbc

#ifndef CPP_DBC_DRIVER_MYSQL_HPP
#define CPP_DBC_DRIVER_MYSQL_HPP

#include "../cpp_dbc.hpp"

#if USE_MYSQL
#include <mysql/mysql.h>
#include <map>
#include <memory>

namespace cpp_dbc
{
    namespace MySQL
    {

        class MySQLResultSet : public ResultSet
        {
        private:
            MYSQL_RES *result;
            MYSQL_ROW currentRow;
            int rowPosition;
            int rowCount;
            int fieldCount;
            std::vector<std::string> columnNames;
            std::map<std::string, int> columnMap;

        public:
            MySQLResultSet(MYSQL_RES *res);
            ~MySQLResultSet() override;

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
        };

        class MySQLPreparedStatement : public PreparedStatement
        {
        private:
            MYSQL *mysql;
            std::string sql;
            MYSQL_STMT *stmt;
            std::vector<MYSQL_BIND> binds;
            std::vector<std::string> stringValues; // To keep string values alive

        public:
            MySQLPreparedStatement(MYSQL *mysql, const std::string &sql);
            ~MySQLPreparedStatement() override;

            void setInt(int parameterIndex, int value) override;
            void setLong(int parameterIndex, long value) override;
            void setDouble(int parameterIndex, double value) override;
            void setString(int parameterIndex, const std::string &value) override;
            void setBoolean(int parameterIndex, bool value) override;
            void setNull(int parameterIndex, Types type) override;

            std::shared_ptr<ResultSet> executeQuery() override;
            int executeUpdate() override;
            bool execute() override;
        };

        class MySQLConnection : public Connection
        {
        private:
            MYSQL *mysql;
            bool closed;
            bool autoCommit;

        public:
            MySQLConnection(const std::string &host,
                            int port,
                            const std::string &database,
                            const std::string &user,
                            const std::string &password);
            ~MySQLConnection() override;

            void close() override;
            bool isClosed() override;

            std::shared_ptr<PreparedStatement> prepareStatement(const std::string &sql) override;
            std::shared_ptr<ResultSet> executeQuery(const std::string &sql) override;
            int executeUpdate(const std::string &sql) override;

            void setAutoCommit(bool autoCommit) override;
            bool getAutoCommit() override;

            void commit() override;
            void rollback() override;
        };

        class MySQLDriver : public Driver
        {
        public:
            MySQLDriver();
            ~MySQLDriver() override;

            std::shared_ptr<Connection> connect(const std::string &url,
                                                const std::string &user,
                                                const std::string &password) override;

            bool acceptsURL(const std::string &url) override;

            // Parses a JDBC-like URL: jdbc:mysql://host:port/database
            bool parseURL(const std::string &url,
                          std::string &host,
                          int &port,
                          std::string &database);
        };

    } // namespace MySQL
} // namespace cpp_dbc

#else // USE_MYSQL

// Stub implementations when MySQL is disabled
namespace cpp_dbc
{
    namespace MySQL
    {
        // Forward declarations only
        class MySQLDriver : public Driver
        {
        public:
            MySQLDriver()
            {
                throw SQLException("MySQL support is not enabled in this build");
            }
            ~MySQLDriver() override = default;

            std::shared_ptr<Connection> connect(const std::string &url,
                                                const std::string &user,
                                                const std::string &password) override
            {
                throw SQLException("MySQL support is not enabled in this build");
            }

            bool acceptsURL(const std::string &url) override
            {
                return false;
            }
        };
    } // namespace MySQL
} // namespace cpp_dbc

#endif // USE_MYSQL

#endif // CPP_DBC_DRIVER_MYSQL_HPP
