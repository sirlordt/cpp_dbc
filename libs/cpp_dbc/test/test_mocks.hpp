#ifndef CPP_DBC_TEST_MOCKS_HPP
#define CPP_DBC_TEST_MOCKS_HPP

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <map>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

namespace cpp_dbc_test
{
    // Flag to control behavior for different test contexts
    static bool g_interfaceTestMode;

    // Improved mock implementation of ResultSet with in-memory data storage
    class MockResultSet : public cpp_dbc::ResultSet
    {
    private:
        // Column definitions
        std::vector<std::string> columnNames;
        std::map<std::string, int> columnMap;

        // Data storage - vector of rows, each row is a map of column name to value
        std::vector<std::map<std::string, std::string>> rows;

        // Current position
        int rowPosition;
        bool isClosed;

        // Helper to convert string to various types
        int stringToInt(const std::string &value) const
        {
            return value.empty() ? 0 : std::stoi(value);
        }

        long stringToLong(const std::string &value) const
        {
            return value.empty() ? 0L : std::stol(value);
        }

        double stringToDouble(const std::string &value) const
        {
            return value.empty() ? 0.0 : std::stod(value);
        }

        bool stringToBoolean(const std::string &value) const
        {
            return (value == "1" || value == "true" || value == "TRUE" || value == "True");
        }

        // Get current row data safely
        const std::map<std::string, std::string> &getCurrentRow() const
        {
            static const std::map<std::string, std::string> emptyRow;
            if (rowPosition <= 0 || rowPosition > static_cast<int>(rows.size()))
            {
                return emptyRow;
            }
            return rows[rowPosition - 1];
        }

        // Get value from current row by column index
        std::string getValueByIndex(int columnIndex) const
        {
            if (columnIndex < 1 || columnIndex > static_cast<int>(columnNames.size()))
            {
                return "";
            }
            const auto &row = getCurrentRow();
            const std::string &columnName = columnNames[columnIndex - 1];
            auto it = row.find(columnName);
            return (it != row.end()) ? it->second : "";
        }

        // Get value from current row by column name
        std::string getValueByName(const std::string &columnName) const
        {
            const auto &row = getCurrentRow();
            auto it = row.find(columnName);
            return (it != row.end()) ? it->second : "";
        }

    public:
        MockResultSet() : rowPosition(0), isClosed(false)
        {
            // Default setup with just the mock column for interface tests
            addColumn("mock");

            // No rows by default for interface tests
            // This ensures rs->next() returns false as expected in test_drivers.cpp
        }

        // Add a column to the result set
        void addColumn(const std::string &name)
        {
            // Only add if it doesn't exist
            if (columnMap.find(name) == columnMap.end())
            {
                columnMap[name] = columnNames.size();
                columnNames.push_back(name);
            }
        }

        // Add a row to the result set
        void addRow(const std::map<std::string, std::string> &rowData)
        {
            // Make sure all columns exist
            for (const auto &pair : rowData)
            {
                addColumn(pair.first);
            }
            rows.push_back(rowData);
        }

        // Clear all data
        void clearData()
        {
            rows.clear();
            rowPosition = 0;
        }

        bool next() override
        {
            if (isClosed || rowPosition >= static_cast<int>(rows.size()))
            {
                return false;
            }
            rowPosition++;
            return true;
        }

        bool isBeforeFirst() override
        {
            return rowPosition == 0;
        }

        bool isAfterLast() override
        {
            return rowPosition > static_cast<int>(rows.size());
        }

        int getRow() override
        {
            return rowPosition;
        }

        int getInt(int columnIndex) override
        {
            return stringToInt(getValueByIndex(columnIndex));
        }

        int getInt(const std::string &columnName) override
        {
            return stringToInt(getValueByName(columnName));
        }

        long getLong(int columnIndex) override
        {
            return stringToLong(getValueByIndex(columnIndex));
        }

        long getLong(const std::string &columnName) override
        {
            return stringToLong(getValueByName(columnName));
        }

        double getDouble(int columnIndex) override
        {
            return stringToDouble(getValueByIndex(columnIndex));
        }

        double getDouble(const std::string &columnName) override
        {
            return stringToDouble(getValueByName(columnName));
        }

        std::string getString(int columnIndex) override
        {
            return getValueByIndex(columnIndex);
        }

        std::string getString(const std::string &columnName) override
        {
            return getValueByName(columnName);
        }

        bool getBoolean(int columnIndex) override
        {
            return stringToBoolean(getValueByIndex(columnIndex));
        }

        bool getBoolean(const std::string &columnName) override
        {
            return stringToBoolean(getValueByName(columnName));
        }

        bool isNull(int columnIndex) override
        {
            return getValueByIndex(columnIndex).empty();
        }

        bool isNull(const std::string &columnName) override
        {
            return getValueByName(columnName).empty();
        }

        std::vector<std::string> getColumnNames() override
        {
            return columnNames;
        }

        int getColumnCount() override
        {
            return columnNames.size();
        }

        void close() override
        {
            isClosed = true;
        }
    };

    // Basic mock implementation of PreparedStatement
    class MockPreparedStatement : public cpp_dbc::PreparedStatement
    {
    private:
        std::map<int, std::string> parameters;

    public:
        MockPreparedStatement() {}

        void setInt(int parameterIndex, int value) override
        {
            parameters[parameterIndex] = std::to_string(value);
        }

        void setLong(int parameterIndex, long value) override
        {
            parameters[parameterIndex] = std::to_string(value);
        }

        void setDouble(int parameterIndex, double value) override
        {
            parameters[parameterIndex] = std::to_string(value);
        }

        void setString(int parameterIndex, const std::string &value) override
        {
            parameters[parameterIndex] = value;
        }

        void setBoolean(int parameterIndex, bool value) override
        {
            parameters[parameterIndex] = value ? "true" : "false";
        }

        void setNull(int parameterIndex, cpp_dbc::Types type) override
        {
            parameters[parameterIndex] = "";
        }

        void setDate(int parameterIndex, const std::string &value) override
        {
            parameters[parameterIndex] = value;
        }

        void setTimestamp(int parameterIndex, const std::string &value) override
        {
            parameters[parameterIndex] = value;
        }

        std::shared_ptr<cpp_dbc::ResultSet> executeQuery() override
        {
            // Create a result set with appropriate data based on the prepared statement
            auto rs = std::make_shared<MockResultSet>();
            rs->clearData();

            // Check if parameter 1 is set to 2 (for the test in test_integration.cpp line 174)
            if (parameters.find(1) != parameters.end() && parameters[1] == "2")
            {
                // For prepared statement with parameter = 2 (test_integration.cpp line 174)
                rs->addColumn("id");
                rs->addColumn("name");
                rs->addColumn("email");
                rs->addRow({{"id", "2"}, {"name", "John"}, {"email", "john@example.com"}});
            }
            else
            {
                // Default case - just return a simple result set with a value column
                rs->addColumn("value");
                rs->addRow({{"value", "1"}});
            }

            return rs;
        }

        int executeUpdate() override { return 1; }
        bool execute() override { return true; }
        void close() override { /* Mock implementation - do nothing */ }

        // Helper method for testing
        std::string getParameter(int index) const
        {
            auto it = parameters.find(index);
            if (it != parameters.end())
            {
                return it->second;
            }
            return "";
        }
    };

    // Basic mock implementation of Connection
    class MockConnection : public cpp_dbc::Connection
    {
    private:
        bool closed = false;
        bool autoCommit = true;
        bool committed = false;
        bool rolledBack = false;
        cpp_dbc::TransactionIsolationLevel isolationLevel = cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;

    public:
        void close() override { closed = true; }
        bool isClosed() override { return closed; }
        void returnToPool() override {};
        bool isPooled() override { return false; };

        std::shared_ptr<cpp_dbc::PreparedStatement> prepareStatement(const std::string &) override
        {
            return std::make_shared<MockPreparedStatement>();
        }

        std::shared_ptr<cpp_dbc::ResultSet> executeQuery(const std::string &sql) override
        {
            auto rs = std::make_shared<MockResultSet>();

            // Clear any default data
            rs->clearData();

            // Simple SQL parser for common test patterns
            if (sql.find("SELECT 1") != std::string::npos)
            {
                // For "SELECT 1", create a result set with a single row and a "value" column with value "1"
                rs->addColumn("value");
                rs->addRow({{"value", "1"}});
            }
            else if (sql.find("SELECT 2") != std::string::npos)
            {
                // For "SELECT 2", create a result set with a single row and a "value" column with value "2"
                rs->addColumn("value");
                rs->addRow({{"value", "2"}});
            }
            else if (sql.find("SELECT") != std::string::npos && sql.find("FROM users") != std::string::npos)
            {
                // For queries selecting from users table, add standard user data
                rs->addColumn("id");
                rs->addColumn("name");
                rs->addColumn("email");
                rs->addRow({{"id", "1"}, {"name", "John"}, {"email", "john@example.com"}});
                rs->addRow({{"id", "2"}, {"name", "Jane"}, {"email", "jane@example.com"}});
                rs->addRow({{"id", "3"}, {"name", "Bob"}, {"email", "bob@example.com"}});
            }
            else
            {
                // For other queries, add default columns but no rows
                rs->addColumn("id");
                rs->addColumn("name");
                rs->addColumn("value");
            }

            return rs;
        }

        int executeUpdate(const std::string &sql) override
        {
            if (sql.find("UPDATE") != std::string::npos)
            {
                return 2; // Return 2 for UPDATE queries
            }
            if (sql.find("DELETE") != std::string::npos)
            {
                return 3; // Return 3 for DELETE queries
            }
            return 1;
        }
        void setAutoCommit(bool ac) override { autoCommit = ac; }
        bool getAutoCommit() override { return autoCommit; }
        void commit() override { committed = true; }
        void rollback() override { rolledBack = true; }

        // Transaction isolation level methods
        void setTransactionIsolation(cpp_dbc::TransactionIsolationLevel level) override { isolationLevel = level; }
        cpp_dbc::TransactionIsolationLevel getTransactionIsolation() override { return isolationLevel; }

        // Helper methods for testing
        bool isCommitted() const { return committed; }
        bool isRolledBack() const { return rolledBack; }
        void reset()
        {
            committed = false;
            rolledBack = false;
        }
    };

    // Basic mock implementation of Driver
    class MockDriver : public cpp_dbc::Driver
    {
    public:
        std::shared_ptr<cpp_dbc::Connection> connect(const std::string &, const std::string &, const std::string &) override
        {
            return std::make_shared<MockConnection>();
        }

        bool acceptsURL(const std::string &url) override
        {
            return url.find("cpp_dbc:mock") == 0;
        }
    };

    // Mock ConnectionPool that can handle unlimited connections
    class MockConnectionPool : public cpp_dbc::ConnectionPool
    {
    private:
        mutable std::mutex poolMutex;
        std::atomic<int> activeCount{0};

    public:
        MockConnectionPool() : cpp_dbc::ConnectionPool(
                                   "cpp_dbc:mock://localhost:1234/mockdb",
                                   "mockuser",
                                   "mockpass",
                                   0,         // initialSize - set to 0 to avoid creating connections
                                   20,        // maxSize - large enough for concurrent tests
                                   0,         // minIdle - set to 0 to avoid maintenance
                                   1000,      // maxWaitMillis
                                   1000,      // validationTimeoutMillis
                                   10000,     // idleTimeoutMillis
                                   10000,     // maxLifetimeMillis
                                   false,     // testOnBorrow - disable to avoid validation
                                   false,     // testOnReturn
                                   "SELECT 1" // validationQuery
                               )
        {
            // Register the mock driver
            try
            {
                auto mockDriver = std::make_shared<MockDriver>();
                cpp_dbc::DriverManager::registerDriver("mock", mockDriver);
            }
            catch (...)
            {
                // Driver might already be registered, ignore
            }

            // Close the parent pool immediately to avoid any maintenance threads
            cpp_dbc::ConnectionPool::close();
        }

        // Override getConnection to always return a new mock connection
        std::shared_ptr<cpp_dbc::Connection> getConnection() override
        {
            std::lock_guard<std::mutex> lock(poolMutex);
            activeCount++;

            // Create a new mock connection for each request
            auto mockConnection = std::make_shared<MockConnection>();

            // Wrap it in a custom connection that decrements the counter when closed
            return std::make_shared<MockPooledConnection>(mockConnection, this);
        }

        void decrementActiveCount()
        {
            activeCount--;
        }

    private:
        // Custom connection wrapper that tracks when connections are returned
        class MockPooledConnection : public cpp_dbc::Connection
        {
        private:
            std::shared_ptr<cpp_dbc::Connection> underlying;
            MockConnectionPool *pool;
            bool closed = false;

        public:
            MockPooledConnection(std::shared_ptr<cpp_dbc::Connection> conn, MockConnectionPool *p)
                : underlying(conn), pool(p) {}

            ~MockPooledConnection()
            {
                if (!closed)
                {
                    close();
                }
            }

            void close() override
            {
                if (!closed)
                {
                    closed = true;
                    pool->decrementActiveCount();
                    underlying->close();
                }
            }

            void returnToPool() override { this->close(); };
            bool isPooled() override { return false; };

            bool isClosed() override { return closed || underlying->isClosed(); }
            std::shared_ptr<cpp_dbc::PreparedStatement> prepareStatement(const std::string &sql) override
            {
                return underlying->prepareStatement(sql);
            }
            std::shared_ptr<cpp_dbc::ResultSet> executeQuery(const std::string &sql) override
            {
                return underlying->executeQuery(sql);
            }
            int executeUpdate(const std::string &sql) override
            {
                return underlying->executeUpdate(sql);
            }
            void setAutoCommit(bool autoCommit) override
            {
                underlying->setAutoCommit(autoCommit);
            }
            bool getAutoCommit() override
            {
                return underlying->getAutoCommit();
            }
            void commit() override
            {
                underlying->commit();
            }
            void rollback() override
            {
                underlying->rollback();
            }

            // Transaction isolation level methods
            void setTransactionIsolation(cpp_dbc::TransactionIsolationLevel level) override
            {
                underlying->setTransactionIsolation(level);
            }

            cpp_dbc::TransactionIsolationLevel getTransactionIsolation() override
            {
                return underlying->getTransactionIsolation();
            }
        };
    };

} // namespace cpp_dbc_test

#endif // CPP_DBC_TEST_MOCKS_HPP