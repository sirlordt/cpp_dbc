// CPPDBC_PostgreSQL.hpp
// PostgreSQL implementation for cpp_dbc
//
// This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
// See the LICENSE.md file in the project root for more information.
//

#ifndef CPP_DBC_DRIVER_POSTGRESQL_HPP
#define CPP_DBC_DRIVER_POSTGRESQL_HPP

#include "../cpp_dbc.hpp"
#include "postgresql_blob.hpp"

#if USE_POSTGRESQL
#include <libpq-fe.h>
#include <map>
#include <memory>
#include <set>
#include <mutex>

namespace cpp_dbc
{
    namespace PostgreSQL
    {

        class PostgreSQLResultSet : public ResultSet
        {
        private:
            PGresult *result;
            int rowPosition;
            int rowCount;
            int fieldCount;
            std::vector<std::string> columnNames;
            std::map<std::string, int> columnMap;

        public:
            PostgreSQLResultSet(PGresult *res);
            ~PostgreSQLResultSet() override;

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

        class PostgreSQLPreparedStatement : public PreparedStatement
        {
            friend class PostgreSQLConnection;

        private:
            PGconn *conn;
            std::string sql;
            std::string stmtName;
            std::vector<std::string> paramValues;
            std::vector<int> paramLengths;
            std::vector<int> paramFormats;
            std::vector<Oid> paramTypes;
            bool prepared;
            int statementCounter;
            std::vector<std::vector<uint8_t>> blobValues;            // To keep blob values alive
            std::vector<std::shared_ptr<Blob>> blobObjects;          // To keep blob objects alive
            std::vector<std::shared_ptr<InputStream>> streamObjects; // To keep stream objects alive

            // Internal method called by connection when closing
            void notifyConnClosing();

            // Helper method to process SQL and count parameters
            int processSQL(std::string &sqlQuery);

        public:
            PostgreSQLPreparedStatement(PGconn *conn, const std::string &sql, const std::string &stmt_name);
            ~PostgreSQLPreparedStatement() override;

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

        class PostgreSQLConnection : public Connection
        {
        private:
            PGconn *conn;
            bool closed;
            bool autoCommit;
            int statementCounter;
            TransactionIsolationLevel isolationLevel;

            std::set<std::shared_ptr<PostgreSQLPreparedStatement>> activeStatements;
            std::mutex statementsMutex;

            // Internal methods for statement registry
            void registerStatement(std::shared_ptr<PostgreSQLPreparedStatement> stmt);
            void unregisterStatement(std::shared_ptr<PostgreSQLPreparedStatement> stmt);

        public:
            PostgreSQLConnection(const std::string &host,
                                 int port,
                                 const std::string &database,
                                 const std::string &user,
                                 const std::string &password,
                                 const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
            ~PostgreSQLConnection() override;

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

            // Helper to generate unique statement names
            std::string generateStatementName();
        };

        class PostgreSQLDriver : public Driver
        {
        public:
            PostgreSQLDriver();
            ~PostgreSQLDriver() override;

            std::shared_ptr<Connection> connect(const std::string &url,
                                                const std::string &user,
                                                const std::string &password,
                                                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

            bool acceptsURL(const std::string &url) override;

            // Parses a JDBC-like URL: jdbc:postgresql://host:port/database
            bool parseURL(const std::string &url,
                          std::string &host,
                          int &port,
                          std::string &database);
        };

    } // namespace PostgreSQL
} // namespace cpp_dbc

#else // USE_POSTGRESQL

// Stub implementations when PostgreSQL is disabled
namespace cpp_dbc
{
    namespace PostgreSQL
    {
        // Forward declarations only
        class PostgreSQLDriver : public Driver
        {
        public:
            PostgreSQLDriver()
            {
                throw SQLException("PostgreSQL support is not enabled in this build");
            }
            ~PostgreSQLDriver() override = default;

            std::shared_ptr<Connection> connect(const std::string &url,
                                                const std::string &user,
                                                const std::string &password,
                                                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
            {
                throw SQLException("PostgreSQL support is not enabled in this build");
            }

            bool acceptsURL(const std::string &url) override
            {
                return false;
            }
        };
    } // namespace PostgreSQL
} // namespace cpp_dbc

#endif // USE_POSTGRESQL

#endif // CPP_DBC_DRIVER_POSTGRESQL_HPP
