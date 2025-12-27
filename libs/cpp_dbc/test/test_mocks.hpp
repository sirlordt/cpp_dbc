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

 @file test_mocks.hpp
 @brief Implementation for the cpp_dbc library

*/

#ifndef CPP_DBC_TEST_MOCKS_HPP
#define CPP_DBC_TEST_MOCKS_HPP

#include <map>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/blob.hpp>

namespace cpp_dbc_test
{
    // Flag to control behavior for different test contexts
    // Commented out unused variable
    // static bool g_interfaceTestMode;

    // Simple mock implementation of Blob for testing
    class MockBlob : public cpp_dbc::Blob
    {
    private:
        std::vector<uint8_t> data;

    public:
        MockBlob() = default;

        size_t length() const override
        {
            return data.size();
        }

        std::vector<uint8_t> getBytes(size_t /*pos*/, size_t /*length*/) const override
        {
            return std::vector<uint8_t>();
        }

        std::shared_ptr<cpp_dbc::InputStream> getBinaryStream() const override
        {
            return nullptr;
        }

        std::shared_ptr<cpp_dbc::OutputStream> setBinaryStream(size_t /*pos*/) override
        {
            return nullptr;
        }

        void setBytes(size_t /*pos*/, const std::vector<uint8_t> & /*bytes*/) override
        {
        }

        void setBytes(size_t /*pos*/, const uint8_t * /*bytes*/, size_t /*length*/) override
        {
        }

        void truncate(size_t /*len*/) override
        {
        }

        void free() override
        {
            data.clear();
        }
    };

    // Simple mock implementation of InputStream for testing
    class MockInputStream : public cpp_dbc::InputStream
    {
    public:
        int read(uint8_t * /*buffer*/, size_t /*length*/) override
        {
            return -1; // End of stream
        }

        void skip(size_t /*n*/) override
        {
        }

        void close() override
        {
        }
    };

    // Improved mock implementation of ResultSet with in-memory data storage
    class MockResultSet : public cpp_dbc::RelationalDBResultSet
    {
    private:
        // Column definitions
        std::vector<std::string> columnNames;
        std::map<std::string, size_t> columnMap;

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
        std::string getValueByIndex(size_t columnIndex) const
        {
            if (columnIndex < 1 || columnIndex > columnNames.size())
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

        uint64_t getRow() override
        {
            return rowPosition;
        }

        int getInt(size_t columnIndex) override
        {
            return stringToInt(getValueByIndex(columnIndex));
        }

        int getInt(const std::string &columnName) override
        {
            return stringToInt(getValueByName(columnName));
        }

        long getLong(size_t columnIndex) override
        {
            return stringToLong(getValueByIndex(columnIndex));
        }

        long getLong(const std::string &columnName) override
        {
            return stringToLong(getValueByName(columnName));
        }

        double getDouble(size_t columnIndex) override
        {
            return stringToDouble(getValueByIndex(columnIndex));
        }

        double getDouble(const std::string &columnName) override
        {
            return stringToDouble(getValueByName(columnName));
        }

        std::string getString(size_t columnIndex) override
        {
            return getValueByIndex(columnIndex);
        }

        std::string getString(const std::string &columnName) override
        {
            return getValueByName(columnName);
        }

        bool getBoolean(size_t columnIndex) override
        {
            return stringToBoolean(getValueByIndex(columnIndex));
        }

        bool getBoolean(const std::string &columnName) override
        {
            return stringToBoolean(getValueByName(columnName));
        }

        bool isNull(size_t columnIndex) override
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

        size_t getColumnCount() override
        {
            return columnNames.size();
        }

        void close() override
        {
            isClosed = true;
        }

        bool isEmpty() override
        {
            return rows.empty();
        }

        // BLOB support methods
        std::shared_ptr<cpp_dbc::Blob> getBlob(size_t /*columnIndex*/) override
        {
            // Return an empty mock blob
            return std::make_shared<MockBlob>();
        }

        std::shared_ptr<cpp_dbc::Blob> getBlob(const std::string & /*columnName*/) override
        {
            // Return an empty mock blob
            return std::make_shared<MockBlob>();
        }

        std::shared_ptr<cpp_dbc::InputStream> getBinaryStream(size_t /*columnIndex*/) override
        {
            // Return a mock input stream
            return std::make_shared<MockInputStream>();
        }

        std::shared_ptr<cpp_dbc::InputStream> getBinaryStream(const std::string & /*columnName*/) override
        {
            // Return a mock input stream
            return std::make_shared<MockInputStream>();
        }

        std::vector<uint8_t> getBytes(size_t /*columnIndex*/) override
        {
            // Return an empty vector
            return std::vector<uint8_t>();
        }

        std::vector<uint8_t> getBytes(const std::string & /*columnName*/) override
        {
            // Return an empty vector
            return std::vector<uint8_t>();
        }
    };

    // Basic mock implementation of PreparedStatement
    class MockPreparedStatement : public cpp_dbc::RelationalDBPreparedStatement
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

        void setNull(int parameterIndex, cpp_dbc::Types /*type*/) override
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

        std::shared_ptr<cpp_dbc::RelationalDBResultSet> executeQuery() override
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

        uint64_t executeUpdate() override { return 1; }
        bool execute() override { return true; }
        void close() override { /* Mock implementation - do nothing */ }

        // BLOB support methods
        void setBlob(int parameterIndex, std::shared_ptr<cpp_dbc::Blob> /*x*/) override
        {
            // Store a placeholder in parameters
            parameters[parameterIndex] = "[BLOB]";
        }

        void setBinaryStream(int parameterIndex, std::shared_ptr<cpp_dbc::InputStream> /*x*/) override
        {
            // Store a placeholder in parameters
            parameters[parameterIndex] = "[BINARY_STREAM]";
        }

        void setBinaryStream(int parameterIndex, std::shared_ptr<cpp_dbc::InputStream> /*x*/, size_t length) override
        {
            // Store a placeholder in parameters
            parameters[parameterIndex] = "[BINARY_STREAM:" + std::to_string(length) + "]";
        }

        void setBytes(int parameterIndex, const std::vector<uint8_t> &x) override
        {
            // Store a placeholder in parameters
            parameters[parameterIndex] = "[BYTES:" + std::to_string(x.size()) + "]";
        }

        void setBytes(int parameterIndex, const uint8_t * /*x*/, size_t length) override
        {
            // Store a placeholder in parameters
            parameters[parameterIndex] = "[BYTES:" + std::to_string(length) + "]";
        }

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
    class MockConnection : public cpp_dbc::RelationalDBConnection
    {
    private:
        bool closed = false;
        bool m_autoCommit = true;
        bool committed = false;
        bool rolledBack = false;
        bool m_transactionActive = false;
        cpp_dbc::TransactionIsolationLevel isolationLevel = cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;

    public:
        void close() override { closed = true; }
        bool isClosed() override { return closed; }
        void returnToPool() override
        {
            if (!closed)
            {
                close();
            }
        };
        bool isPooled() override { return true; };

        std::shared_ptr<cpp_dbc::RelationalDBPreparedStatement> prepareStatement(const std::string &) override
        {
            return std::make_shared<MockPreparedStatement>();
        }

        std::shared_ptr<cpp_dbc::RelationalDBResultSet> executeQuery(const std::string &sql) override
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

        uint64_t executeUpdate(const std::string &sql) override
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
        void setAutoCommit(bool ac) override { m_autoCommit = ac; }
        bool getAutoCommit() override { return m_autoCommit; }
        void commit() override
        {
            committed = true;
            m_transactionActive = false;
            m_autoCommit = true;
        }

        void rollback() override
        {
            rolledBack = true;
            m_transactionActive = false;
            m_autoCommit = true;
        }

        // Transaction management methods
        bool beginTransaction() override
        {
            if (m_transactionActive)
            {
                return true; // Transaction already active
            }
            m_transactionActive = true;
            m_autoCommit = false;
            return true;
        }

        bool transactionActive() override
        {
            return m_transactionActive;
        }

        // Transaction isolation level methods
        void setTransactionIsolation(cpp_dbc::TransactionIsolationLevel level) override { isolationLevel = level; }
        cpp_dbc::TransactionIsolationLevel getTransactionIsolation() override { return isolationLevel; }

        std::string getURL() const override
        {
            // Initialize URL string once
            std::stringstream urlBuilder;
            urlBuilder << "cpp_dbc:mock://127.0.0.1:1000/MockDB";
            return urlBuilder.str();
        }

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
    class MockDriver : public cpp_dbc::RelationalDBDriver
    {
    public:
        std::shared_ptr<cpp_dbc::RelationalDBConnection> connectRelational(const std::string &, const std::string &, const std::string &,
                                                                           const std::map<std::string, std::string> & = std::map<std::string, std::string>()) override
        {
            return std::make_shared<MockConnection>();
        }

        bool acceptsURL(const std::string &url) override
        {
            return url.find("cpp_dbc:mock") == 0;
        }
    };

    // Mock ConnectionPool that can handle unlimited connections
    class MockConnectionPool : public cpp_dbc::RelationalDBConnectionPool
    {
    private:
        mutable std::mutex poolMutex;
        std::atomic<int> activeCount{0};
        cpp_dbc::TransactionIsolationLevel transactionIsolation = cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED;

    public:
        MockConnectionPool() : cpp_dbc::RelationalDBConnectionPool(
                                   "cpp_dbc:mock://localhost:1234/mockdb",
                                   "mockuser",
                                   "mockpass",
                                   std::map<std::string, std::string>(), // options
                                   0,                                    // initialSize - set to 0 to avoid creating connections
                                   20,                                   // maxSize - large enough for concurrent tests
                                   0,                                    // minIdle - set to 0 to avoid maintenance
                                   1000,                                 // maxWaitMillis
                                   1000,                                 // validationTimeoutMillis
                                   10000,                                // idleTimeoutMillis
                                   10000,                                // maxLifetimeMillis
                                   false,                                // testOnBorrow - disable to avoid validation
                                   false,                                // testOnReturn
                                   "SELECT 1"                            // validationQuery
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
            cpp_dbc::RelationalDBConnectionPool::close();
        }

        // Override getDBConnection to always return a new mock connection
        std::shared_ptr<cpp_dbc::RelationalDBConnection> getDBConnection() override
        {
            std::lock_guard<std::mutex> lock(poolMutex);
            activeCount++;

            // Create a new mock connection for each request
            auto mockConnection = std::make_shared<MockConnection>();

            // Set the transaction isolation level from the pool
            mockConnection->setTransactionIsolation(transactionIsolation);

            // Wrap it in a custom connection that decrements the counter when closed
            return std::make_shared<MockPooledConnection>(mockConnection, this);
        }

        void decrementActiveCount()
        {
            activeCount--;
        }

        // Set transaction isolation level for the pool
        void setTransactionIsolation(cpp_dbc::TransactionIsolationLevel level)
        {
            transactionIsolation = level;
        }

    private:
        // Custom connection wrapper that tracks when connections are returned
        class MockPooledConnection : public cpp_dbc::RelationalDBConnection
        {
        private:
            std::shared_ptr<cpp_dbc::RelationalDBConnection> underlying;
            MockConnectionPool *pool;
            bool closed = false;

        public:
            MockPooledConnection(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn, MockConnectionPool *p)
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

            void returnToPool() override
            {
                if (!closed)
                {
                    this->close();
                }
            };
            bool isPooled() override { return true; };

            bool isClosed() override { return closed || underlying->isClosed(); }
            std::shared_ptr<cpp_dbc::RelationalDBPreparedStatement> prepareStatement(const std::string &sql) override
            {
                return underlying->prepareStatement(sql);
            }
            std::shared_ptr<cpp_dbc::RelationalDBResultSet> executeQuery(const std::string &sql) override
            {
                return underlying->executeQuery(sql);
            }
            uint64_t executeUpdate(const std::string &sql) override
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

            // Transaction management methods
            bool beginTransaction() override
            {
                return underlying->beginTransaction();
            }

            bool transactionActive() override
            {
                return underlying->transactionActive();
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

            std::string getURL() const override
            {
                return underlying->getURL();
            }
        };
    };

} // namespace cpp_dbc_test

#endif // CPP_DBC_TEST_MOCKS_HPP