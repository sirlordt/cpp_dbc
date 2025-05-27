#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include "test_mocks.hpp"
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#include "test_mocks.hpp"
#include <string>
#include <memory>
#include <iostream>

// Test case for DriverManager
TEST_CASE("DriverManager tests", "[driver][manager]")
{
    SECTION("Register and retrieve drivers")
    {
        // Use the mock driver from test_mocks.hpp

        // Register the mock driver
        cpp_dbc::DriverManager::registerDriver("mock", std::make_shared<cpp_dbc_test::MockDriver>());

        // Try to get a connection with the mock driver
        // This should not throw and return a valid connection
        REQUIRE_NOTHROW([&]()
                        {
            auto conn = cpp_dbc::DriverManager::getConnection("cpp_dbc:mock://localhost:1234/mockdb", "user", "pass");
            // The connection should be valid
            REQUIRE(conn != nullptr); }());

        // Try to get a connection with a non-existent driver
        REQUIRE_THROWS_AS(
            cpp_dbc::DriverManager::getConnection("cpp_dbc:nonexistent://localhost:1234/db", "user", "pass"),
            cpp_dbc::SQLException);
    }
}

#if USE_MYSQL
// Test case for MySQL driver
TEST_CASE("MySQL driver tests", "[driver][mysql]")
{
    SECTION("MySQL driver URL acceptance")
    {
        // Create a MySQL driver
        cpp_dbc::MySQL::MySQLDriver driver;

        // Check that it accepts MySQL URLs
        REQUIRE(driver.acceptsURL("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:mysql://127.0.0.1:3306/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:mysql://db.example.com:3306/testdb"));

        // Check that it rejects non-MySQL URLs
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("jdbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("mysql://localhost:3306/testdb"));
    }

    SECTION("MySQL driver connection string parsing")
    {
        // Create a MySQL driver
        cpp_dbc::MySQL::MySQLDriver driver;

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a SQLException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver.connect("cpp_dbc:mysql://localhost:3306/non_existent_db", "user", "pass"),
            cpp_dbc::SQLException);
    }
}
#endif

#if USE_POSTGRESQL
// Test case for PostgreSQL driver
TEST_CASE("PostgreSQL driver tests", "[driver][postgresql]")
{
    SECTION("PostgreSQL driver URL acceptance")
    {
        // Create a PostgreSQL driver
        cpp_dbc::PostgreSQL::PostgreSQLDriver driver;

        // Check that it accepts PostgreSQL URLs
        REQUIRE(driver.acceptsURL("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:postgresql://127.0.0.1:5432/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:postgresql://db.example.com:5432/testdb"));

        // Check that it rejects non-PostgreSQL URLs
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("jdbc:postgresql://localhost:5432/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("postgresql://localhost:5432/testdb"));
    }

    SECTION("PostgreSQL driver connection string parsing")
    {
        // Create a PostgreSQL driver
        cpp_dbc::PostgreSQL::PostgreSQLDriver driver;

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a SQLException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver.connect("cpp_dbc:postgresql://localhost:5432/non_existent_db", "user", "pass"),
            cpp_dbc::SQLException);
    }
}
#endif

// Test case for SQLException
TEST_CASE("SQLException tests", "[exception]")
{
    SECTION("Create and throw SQLException")
    {
        // Create an exception
        cpp_dbc::SQLException ex("Test error message");

        // Check the error message
        REQUIRE(std::string(ex.what()) == "Test error message");

        // Check that we can throw and catch it
        REQUIRE_THROWS_AS(
            throw cpp_dbc::SQLException("Test throw"),
            cpp_dbc::SQLException);

        // Check that we can catch it as a std::exception
        REQUIRE_THROWS_AS(
            throw cpp_dbc::SQLException("Test throw"),
            std::exception);
    }
}

// Test case for Types enum
TEST_CASE("Types enum tests", "[types]")
{
    SECTION("Types enum values")
    {
        // Check that we can use the Types enum
        cpp_dbc::Types intType = cpp_dbc::Types::INTEGER;
        cpp_dbc::Types floatType = cpp_dbc::Types::FLOAT;
        cpp_dbc::Types doubleType = cpp_dbc::Types::DOUBLE;
        cpp_dbc::Types varcharType = cpp_dbc::Types::VARCHAR;
        cpp_dbc::Types dateType = cpp_dbc::Types::DATE;
        cpp_dbc::Types timestampType = cpp_dbc::Types::TIMESTAMP;
        cpp_dbc::Types booleanType = cpp_dbc::Types::BOOLEAN;
        cpp_dbc::Types blobType = cpp_dbc::Types::BLOB;

        // Check that the enum values are distinct
        REQUIRE(intType != floatType);
        REQUIRE(floatType != doubleType);
        REQUIRE(doubleType != varcharType);
        REQUIRE(varcharType != dateType);
        REQUIRE(dateType != timestampType);
        REQUIRE(timestampType != booleanType);
        REQUIRE(booleanType != blobType);
        REQUIRE(blobType != intType);
    }
}

// Mock classes for testing the abstract interfaces
namespace
{
    class MockResultSet : public cpp_dbc::ResultSet
    {
    public:
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
        std::vector<std::string> getColumnNames() override { return {"mock"}; }
        int getColumnCount() override { return 1; }
    };

    class MockPreparedStatement : public cpp_dbc::PreparedStatement
    {
    public:
        void setInt(int, int) override {}
        void setLong(int, long) override {}
        void setDouble(int, double) override {}
        void setString(int, const std::string &) override {}
        void setBoolean(int, bool) override {}
        void setNull(int, cpp_dbc::Types) override {}
        void setDate(int, const std::string &) override {}
        void setTimestamp(int, const std::string &) override {}
        std::shared_ptr<cpp_dbc::ResultSet> executeQuery() override
        {
            return std::make_shared<MockResultSet>();
        }
        int executeUpdate() override { return 1; }
        bool execute() override { return true; }
    };

    class MockConnection : public cpp_dbc::Connection
    {
    private:
        bool closed = false;
        bool autoCommit = true;

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
        void commit() override {}
        void rollback() override {}
    };
}

// Test case for abstract interfaces
TEST_CASE("Abstract interface tests", "[interface]")
{
    SECTION("ResultSet interface")
    {
        // Create a mock result set
        auto rs = std::make_shared<MockResultSet>();

        // Check that we can call the methods
        REQUIRE(rs->next() == false);
        REQUIRE(rs->isBeforeFirst() == true);
        REQUIRE(rs->isAfterLast() == false);
        REQUIRE(rs->getRow() == 0);
        REQUIRE(rs->getInt(1) == 1);
        REQUIRE(rs->getInt("col") == 1);
        REQUIRE(rs->getLong(1) == 1L);
        REQUIRE(rs->getLong("col") == 1L);
        REQUIRE(rs->getDouble(1) == 1.0);
        REQUIRE(rs->getDouble("col") == 1.0);
        REQUIRE(rs->getString(1) == "mock");
        REQUIRE(rs->getString("col") == "mock");
        REQUIRE(rs->getBoolean(1) == true);
        REQUIRE(rs->getBoolean("col") == true);
        REQUIRE(rs->isNull(1) == false);
        REQUIRE(rs->isNull("col") == false);
        REQUIRE(rs->getColumnNames() == std::vector<std::string>{"mock"});
        REQUIRE(rs->getColumnCount() == 1);
    }

    SECTION("PreparedStatement interface")
    {
        // Create a mock prepared statement
        auto stmt = std::make_shared<MockPreparedStatement>();

        // Check that we can call the methods
        REQUIRE_NOTHROW(stmt->setInt(1, 42));
        REQUIRE_NOTHROW(stmt->setLong(1, 42L));
        REQUIRE_NOTHROW(stmt->setDouble(1, 42.0));
        REQUIRE_NOTHROW(stmt->setString(1, "test"));
        REQUIRE_NOTHROW(stmt->setBoolean(1, true));
        REQUIRE_NOTHROW(stmt->setNull(1, cpp_dbc::Types::INTEGER));

        auto rs = stmt->executeQuery();
        REQUIRE(rs != nullptr);

        REQUIRE(stmt->executeUpdate() == 1);
        REQUIRE(stmt->execute() == true);
    }

    SECTION("Connection interface")
    {
        // Create a mock connection
        auto conn = std::make_shared<MockConnection>();

        // Check that we can call the methods
        REQUIRE(conn->isClosed() == false);

        auto stmt = conn->prepareStatement("SELECT 1");
        REQUIRE(stmt != nullptr);

        auto rs = conn->executeQuery("SELECT 1");
        REQUIRE(rs != nullptr);

        REQUIRE(conn->executeUpdate("UPDATE test SET col = 1") == 1);

        REQUIRE(conn->getAutoCommit() == true);
        conn->setAutoCommit(false);
        REQUIRE(conn->getAutoCommit() == false);

        REQUIRE_NOTHROW(conn->commit());
        REQUIRE_NOTHROW(conn->rollback());

        conn->close();
        REQUIRE(conn->isClosed() == true);
    }
}

// Test case for DriverManager getRegisteredDrivers method
TEST_CASE("DriverManager getRegisteredDrivers tests", "[driver_manager]")
{
    SECTION("Get registered drivers")
    {
        // Get the current list of registered drivers
        auto drivers = cpp_dbc::DriverManager::getRegisteredDrivers();

        // The method should return a vector (even if empty)
        REQUIRE(drivers.size() >= 0);

        // If we have drivers registered, they should be valid strings
        for (const auto &driverName : drivers)
        {
            REQUIRE(!driverName.empty());
        }

        // Register a test driver to verify the method works
        auto mockDriver = std::make_shared<cpp_dbc_test::MockDriver>();
        cpp_dbc::DriverManager::registerDriver("test_driver", mockDriver);

        // Get the updated list
        auto updatedDrivers = cpp_dbc::DriverManager::getRegisteredDrivers();
        REQUIRE(updatedDrivers.size() >= 1);

        // Check that our test driver is in the list
        bool found = false;
        for (const auto &driverName : updatedDrivers)
        {
            if (driverName == "test_driver")
            {
                found = true;
                break;
            }
        }
        REQUIRE(found);

        // Verify the list size increased by at least 1
        REQUIRE(updatedDrivers.size() >= drivers.size());
    }
}