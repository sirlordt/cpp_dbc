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

 @file driver_mysql.hpp
 @brief Tests for MySQL database operations

*/

#ifndef CPP_DBC_DRIVER_MYSQL_HPP
#define CPP_DBC_DRIVER_MYSQL_HPP

#include "../cpp_dbc.hpp"
#include "mysql_blob.hpp"

#if USE_MYSQL
#include <mysql/mysql.h>
#include <map>
#include <memory>
#include <set>
#include <mutex>

namespace cpp_dbc
{
    namespace MySQL
    {

        class MySQLResultSet : public ResultSet
        {
        private:
            MYSQL_RES *m_result;
            MYSQL_ROW m_currentRow;
            uint64_t m_rowPosition;
            uint64_t m_rowCount;
            int m_fieldCount;
            std::vector<std::string> m_columnNames;
            std::map<std::string, int> m_columnMap;

        public:
            MySQLResultSet(MYSQL_RES *res);
            ~MySQLResultSet() override;

            bool next() override;
            bool isBeforeFirst() override;
            bool isAfterLast() override;
            uint64_t getRow() override;

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
            size_t getColumnCount() override;
            void close() override;

            // BLOB support methods
            std::shared_ptr<Blob> getBlob(int columnIndex) override;
            std::shared_ptr<Blob> getBlob(const std::string &columnName) override;

            std::shared_ptr<InputStream> getBinaryStream(int columnIndex) override;
            std::shared_ptr<InputStream> getBinaryStream(const std::string &columnName) override;

            std::vector<uint8_t> getBytes(int columnIndex) override;
            std::vector<uint8_t> getBytes(const std::string &columnName) override;
        };

        class MySQLPreparedStatement : public PreparedStatement
        {
            friend class MySQLConnection;

        private:
            MYSQL *m_mysql;
            std::string m_sql;
            MYSQL_STMT *m_stmt;
            std::vector<MYSQL_BIND> m_binds;
            std::vector<std::string> m_stringValues;                   // To keep string values alive
            std::vector<std::string> m_parameterValues;                // To store parameter values for query reconstruction
            std::vector<int> m_intValues;                              // To keep int values alive
            std::vector<long> m_longValues;                            // To keep long values alive
            std::vector<double> m_doubleValues;                        // To keep double values alive
            std::vector<char> m_nullFlags;                             // To keep null flags alive (char instead of bool for pointer access)
            std::vector<std::vector<uint8_t>> m_blobValues;            // To keep blob values alive
            std::vector<std::shared_ptr<Blob>> m_blobObjects;          // To keep blob objects alive
            std::vector<std::shared_ptr<InputStream>> m_streamObjects; // To keep stream objects alive

            // Internal method called by connection when closing
            void notifyConnClosing();

        public:
            MySQLPreparedStatement(MYSQL *mysql, const std::string &sql);
            ~MySQLPreparedStatement() override;

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
            uint64_t executeUpdate() override;
            bool execute() override;
            void close() override;
        };

        class MySQLConnection : public Connection
        {
        private:
            MYSQL *m_mysql;
            bool m_closed;
            bool m_autoCommit;
            TransactionIsolationLevel m_isolationLevel;

            // Registry of active prepared statements
            // std::set<std::weak_ptr<MySQLPreparedStatement>, std::owner_less<std::weak_ptr<MySQLPreparedStatement>>> m_activeStatements;
            std::set<std::shared_ptr<MySQLPreparedStatement>> m_activeStatements;
            std::mutex m_statementsMutex;

            // Internal methods for statement registry
            void registerStatement(std::shared_ptr<MySQLPreparedStatement> stmt);
            void unregisterStatement(std::shared_ptr<MySQLPreparedStatement> stmt);

        public:
            MySQLConnection(const std::string &host,
                            int port,
                            const std::string &database,
                            const std::string &user,
                            const std::string &password,
                            const std::map<std::string, std::string> &options = std::map<std::string, std::string>());
            ~MySQLConnection() override;

            void close() override;
            bool isClosed() override;
            void returnToPool() override;
            bool isPooled() override;

            std::shared_ptr<PreparedStatement> prepareStatement(const std::string &sql) override;
            std::shared_ptr<ResultSet> executeQuery(const std::string &sql) override;
            uint64_t executeUpdate(const std::string &sql) override;

            void setAutoCommit(bool autoCommit) override;
            bool getAutoCommit() override;

            void commit() override;
            void rollback() override;

            // Transaction isolation level methods
            void setTransactionIsolation(TransactionIsolationLevel level) override;
            TransactionIsolationLevel getTransactionIsolation() override;
        };

        class MySQLDriver : public Driver
        {
        public:
            MySQLDriver();
            ~MySQLDriver() override;

            std::shared_ptr<Connection> connect(const std::string &url,
                                                const std::string &user,
                                                const std::string &password,
                                                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override;

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
                                                const std::string &password,
                                                const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) override
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
