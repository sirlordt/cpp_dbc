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

 @file cpp_dbc.hpp
 @brief Implementation for the cpp_dbc library

*/

#ifndef CPP_DBC_HPP
#define CPP_DBC_HPP

// Configuration macros for conditional compilation
#ifndef USE_MYSQL
#define USE_MYSQL 1 // Default to enabled
#endif

#ifndef USE_POSTGRESQL
#define USE_POSTGRESQL 1 // Default to enabled
#endif

#ifndef USE_SQLITE
#define USE_SQLITE 0 // Default to disabled
#endif

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <stdexcept>
#include <cstdint>
#include "cpp_dbc/backward.hpp"
#include "cpp_dbc/common/system_utils.hpp"

// Forward declaration of configuration classes
namespace cpp_dbc
{
    namespace config
    {
        class DatabaseConfig;
        class DatabaseConfigManager;
    }
}

namespace cpp_dbc
{

    // Forward declarations
    class Blob;
    class InputStream;
    class OutputStream;
    class ResultSet;
    class PreparedStatement;
    class Connection;
    class Driver;
    class DriverManager;

    // Custom exceptions
    class DBException : public std::runtime_error
    {
    private:
        std::string m_mark;
        mutable std::string m_full_message;
        std::vector<system_utils::StackFrame> m_callstack;

    public:
        explicit DBException(const std::string &mark, const std::string &message,
                             const std::vector<system_utils::StackFrame> &callstack = {})
            : std::runtime_error(message),
              m_mark(mark),
              m_callstack(callstack) {}

        virtual ~DBException() = default;

        [[deprecated("Use what_s() instead. It avoids the unsafe const char* pointer.")]]
        const char *what() const noexcept override
        {
            if (m_mark.empty())
            {
                return std::runtime_error::what();
            }

            m_full_message = m_mark + ": " + std::runtime_error::what();
            return m_full_message.c_str();
        }

        virtual const std::string &what_s() const noexcept
        {
            if (m_mark.empty())
            {
                m_full_message = std::runtime_error::what();
                return m_full_message;
            }

            m_full_message = m_mark + ": " + std::runtime_error::what();
            return m_full_message;
        }

        const std::string &getMark() const
        {
            return m_mark;
        }

        void printCallStack() const
        {
            system_utils::printCallStack(m_callstack);
        }

        const std::vector<system_utils::StackFrame> &getCallStack() const
        {
            return m_callstack;
        }
    };

    // Represents a SQL parameter type
    enum class Types
    {
        INTEGER,
        FLOAT,
        DOUBLE,
        VARCHAR,
        DATE,
        TIMESTAMP,
        BOOLEAN,
        BLOB
    };

    // Transaction isolation levels (following JDBC standard)
    enum class TransactionIsolationLevel
    {
        TRANSACTION_NONE = 0,
        TRANSACTION_READ_UNCOMMITTED = 1,
        TRANSACTION_READ_COMMITTED = 2,
        TRANSACTION_REPEATABLE_READ = 4,
        TRANSACTION_SERIALIZABLE = 8
    };

    // Abstract base class for input streams
    class InputStream
    {
    public:
        virtual ~InputStream() = default;

        // Read up to length bytes into the buffer
        // Returns the number of bytes actually read, or -1 if end of stream
        virtual int read(uint8_t *buffer, size_t length) = 0;

        // Skip n bytes
        virtual void skip(size_t n) = 0;

        // Close the stream
        virtual void close() = 0;
    };

    // Abstract base class for output streams
    class OutputStream
    {
    public:
        virtual ~OutputStream() = default;

        // Write length bytes from the buffer
        virtual void write(const uint8_t *buffer, size_t length) = 0;

        // Flush any buffered data
        virtual void flush() = 0;

        // Close the stream
        virtual void close() = 0;
    };

    // Abstract base class for BLOB objects
    class Blob
    {
    public:
        virtual ~Blob() = default;

        // Get the length of the BLOB
        virtual size_t length() const = 0;

        // Get a portion of the BLOB as a vector of bytes
        virtual std::vector<uint8_t> getBytes(size_t pos, size_t length) const = 0;

        // Get a stream to read from the BLOB
        virtual std::shared_ptr<InputStream> getBinaryStream() const = 0;

        // Get a stream to write to the BLOB starting at position pos
        virtual std::shared_ptr<OutputStream> setBinaryStream(size_t pos) = 0;

        // Write bytes to the BLOB starting at position pos
        virtual void setBytes(size_t pos, const std::vector<uint8_t> &bytes) = 0;

        // Write bytes to the BLOB starting at position pos
        virtual void setBytes(size_t pos, const uint8_t *bytes, size_t length) = 0;

        // Truncate the BLOB to the specified length
        virtual void truncate(size_t len) = 0;

        // Free resources associated with the BLOB
        virtual void free() = 0;
    };

    // Abstract base class for result sets
    class ResultSet
    {
    public:
        virtual ~ResultSet() = default;

        virtual bool next() = 0;
        virtual bool isBeforeFirst() = 0;
        virtual bool isAfterLast() = 0;
        virtual int getRow() = 0;

        virtual int getInt(int columnIndex) = 0;
        virtual int getInt(const std::string &columnName) = 0;

        virtual long getLong(int columnIndex) = 0;
        virtual long getLong(const std::string &columnName) = 0;

        virtual double getDouble(int columnIndex) = 0;
        virtual double getDouble(const std::string &columnName) = 0;

        virtual std::string getString(int columnIndex) = 0;
        virtual std::string getString(const std::string &columnName) = 0;

        virtual bool getBoolean(int columnIndex) = 0;
        virtual bool getBoolean(const std::string &columnName) = 0;

        virtual bool isNull(int columnIndex) = 0;
        virtual bool isNull(const std::string &columnName) = 0;

        virtual std::vector<std::string> getColumnNames() = 0;
        virtual int getColumnCount() = 0;

        // BLOB support methods
        virtual std::shared_ptr<Blob> getBlob(int columnIndex) = 0;
        virtual std::shared_ptr<Blob> getBlob(const std::string &columnName) = 0;

        virtual std::shared_ptr<InputStream> getBinaryStream(int columnIndex) = 0;
        virtual std::shared_ptr<InputStream> getBinaryStream(const std::string &columnName) = 0;

        virtual std::vector<uint8_t> getBytes(int columnIndex) = 0;
        virtual std::vector<uint8_t> getBytes(const std::string &columnName) = 0;

        // Close the result set and free resources
        virtual void close() = 0;
    };

    // Abstract base class for prepared statements
    class PreparedStatement
    {
    public:
        virtual ~PreparedStatement() = default;

        virtual void setInt(int parameterIndex, int value) = 0;
        virtual void setLong(int parameterIndex, long value) = 0;
        virtual void setDouble(int parameterIndex, double value) = 0;
        virtual void setString(int parameterIndex, const std::string &value) = 0;
        virtual void setBoolean(int parameterIndex, bool value) = 0;
        virtual void setNull(int parameterIndex, Types type) = 0;
        virtual void setDate(int parameterIndex, const std::string &value) = 0;
        virtual void setTimestamp(int parameterIndex, const std::string &value) = 0;

        // BLOB support methods
        virtual void setBlob(int parameterIndex, std::shared_ptr<Blob> x) = 0;
        virtual void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x) = 0;
        virtual void setBinaryStream(int parameterIndex, std::shared_ptr<InputStream> x, size_t length) = 0;
        virtual void setBytes(int parameterIndex, const std::vector<uint8_t> &x) = 0;
        virtual void setBytes(int parameterIndex, const uint8_t *x, size_t length) = 0;

        virtual std::shared_ptr<ResultSet> executeQuery() = 0;
        virtual int executeUpdate() = 0;
        virtual bool execute() = 0;

        // Close the prepared statement and release resources
        virtual void close() = 0;
    };

    // Abstract base class for database connections
    class Connection
    {
    public:
        virtual ~Connection() = default;

        virtual void close() = 0;
        virtual bool isClosed() = 0;
        virtual void returnToPool() = 0;
        virtual bool isPooled() = 0;

        virtual std::shared_ptr<PreparedStatement> prepareStatement(const std::string &sql) = 0;
        virtual std::shared_ptr<ResultSet> executeQuery(const std::string &sql) = 0;
        virtual int executeUpdate(const std::string &sql) = 0;

        virtual void setAutoCommit(bool autoCommit) = 0;
        virtual bool getAutoCommit() = 0;

        virtual void commit() = 0;
        virtual void rollback() = 0;

        // Transaction isolation level methods
        virtual void setTransactionIsolation(TransactionIsolationLevel level) = 0;
        virtual TransactionIsolationLevel getTransactionIsolation() = 0;
    };

    // Abstract driver class
    class Driver
    {
    public:
        virtual ~Driver() = default;

        virtual std::shared_ptr<Connection> connect(const std::string &url,
                                                    const std::string &user,
                                                    const std::string &password,
                                                    const std::map<std::string, std::string> &options = std::map<std::string, std::string>()) = 0;

        virtual bool acceptsURL(const std::string &url) = 0;
    };

    // Manager class to retrieve driver instances
    class DriverManager
    {
    private:
        static std::map<std::string, std::shared_ptr<Driver>> drivers;

    public:
        static void registerDriver(const std::string &name, std::shared_ptr<Driver> driver);

        static std::shared_ptr<Connection> getConnection(const std::string &url,
                                                         const std::string &user,
                                                         const std::string &password,
                                                         const std::map<std::string, std::string> &options = std::map<std::string, std::string>());

        // New methods for integration with configuration classes
        static std::shared_ptr<Connection> getConnection(const config::DatabaseConfig &dbConfig);

        static std::shared_ptr<Connection> getConnection(const config::DatabaseConfigManager &configManager,
                                                         const std::string &configName);

        // Get list of registered driver names
        static std::vector<std::string> getRegisteredDrivers();

        // Clear all registered drivers (useful for testing)
        static void clearDrivers();

        // Unregister a specific driver
        static void unregisterDriver(const std::string &name);
    };

#if USE_MYSQL
    // MySQL specific implementation declarations
    namespace MySQL
    {
        class MySQLDriver;
        class MySQLConnection;
        class MySQLPreparedStatement;
        class MySQLResultSet;
        class MySQLBlob;
        class MySQLInputStream;
        class MySQLOutputStream;
    }
#endif

#if USE_POSTGRESQL
    // PostgreSQL specific implementation declarations
    namespace PostgreSQL
    {
        class PostgreSQLDriver;
        class PostgreSQLConnection;
        class PostgreSQLPreparedStatement;
        class PostgreSQLResultSet;
        class PostgreSQLBlob;
        class PostgreSQLInputStream;
        class PostgreSQLOutputStream;
    }
#endif

#if USE_SQLITE
    // SQLite specific implementation declarations
    namespace SQLite
    {
        class SQLiteDriver;
        class SQLiteConnection;
        class SQLitePreparedStatement;
        class SQLiteResultSet;
        class SQLiteBlob;
        class SQLiteInputStream;
        class SQLiteOutputStream;
    }
#endif

} // namespace cpp_dbc

#endif // CPP_DBC_HPP
