// cpp_dbc.hpp
// A C++ Database Connectivity Library inspired by JDBC
// Provides unified access to MySQL and PostgreSQL databases

#ifndef CPP_DBC_HPP
#define CPP_DBC_HPP

// Configuration macros for conditional compilation
#ifndef USE_MYSQL
#define USE_MYSQL 1 // Default to enabled
#endif

#ifndef USE_POSTGRESQL
#define USE_POSTGRESQL 1 // Default to enabled
#endif

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <stdexcept>

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
    class ResultSet;
    class PreparedStatement;
    class Connection;
    class Driver;
    class DriverManager;

    // Custom exceptions
    class SQLException : public std::runtime_error
    {
    public:
        explicit SQLException(const std::string &message) : std::runtime_error(message) {}
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

        virtual std::shared_ptr<ResultSet> executeQuery() = 0;
        virtual int executeUpdate() = 0;
        virtual bool execute() = 0;
    };

    // Abstract base class for database connections
    class Connection
    {
    public:
        virtual ~Connection() = default;

        virtual void close() = 0;
        virtual bool isClosed() = 0;

        virtual std::shared_ptr<PreparedStatement> prepareStatement(const std::string &sql) = 0;
        virtual std::shared_ptr<ResultSet> executeQuery(const std::string &sql) = 0;
        virtual int executeUpdate(const std::string &sql) = 0;

        virtual void setAutoCommit(bool autoCommit) = 0;
        virtual bool getAutoCommit() = 0;

        virtual void commit() = 0;
        virtual void rollback() = 0;
    };

    // Abstract driver class
    class Driver
    {
    public:
        virtual ~Driver() = default;

        virtual std::shared_ptr<Connection> connect(const std::string &url,
                                                    const std::string &user,
                                                    const std::string &password) = 0;

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
                                                         const std::string &password);

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
    }
#endif

} // namespace cpp_dbc

#endif // CPP_DBC_HPP
