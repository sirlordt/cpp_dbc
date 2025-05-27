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
    // Basic mock implementation of ResultSet
    class MockResultSet : public cpp_dbc::ResultSet
    {
    private:
        std::vector<std::string> columnNames;
        std::map<std::string, int> columnMap;
        int rowPosition;

    public:
        MockResultSet() : rowPosition(0)
        {
            columnNames.push_back("mock");
            columnMap["mock"] = 0;
        }

        bool next() override { return false; }
        bool isBeforeFirst() override { return true; }
        bool isAfterLast() override { return false; }
        int getRow() override { return 0; }
        int getInt(int) override { return 1; }
        int getInt(const std::string &) override { return 1; }
        long getLong(int) override { return 1L; }
        long getLong(const std::string &) override { return 1L; }
        double getDouble(int) override { return 1.0; }
        double getDouble(const std::string &) override { return 1.0; }
        std::string getString(int) override { return "mock"; }
        std::string getString(const std::string &) override { return "mock"; }
        bool getBoolean(int) override { return true; }
        bool getBoolean(const std::string &) override { return true; }
        bool isNull(int) override { return false; }
        bool isNull(const std::string &) override { return false; }
        std::vector<std::string> getColumnNames() override { return columnNames; }
        int getColumnCount() override { return 1; }
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
            return std::make_shared<MockResultSet>();
        }

        int executeUpdate() override { return 1; }
        bool execute() override { return true; }

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

    public:
        void close() override { closed = true; }
        bool isClosed() override { return closed; }

        std::shared_ptr<cpp_dbc::PreparedStatement> prepareStatement(const std::string &) override
        {
            return std::make_shared<MockPreparedStatement>();
        }

        std::shared_ptr<cpp_dbc::ResultSet> executeQuery(const std::string &) override
        {
            return std::make_shared<MockResultSet>();
        }

        int executeUpdate(const std::string &) override { return 1; }
        void setAutoCommit(bool ac) override { autoCommit = ac; }
        bool getAutoCommit() override { return autoCommit; }
        void commit() override { committed = true; }
        void rollback() override { rolledBack = true; }

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
        };
    };

} // namespace cpp_dbc_test

#endif // CPP_DBC_TEST_MOCKS_HPP