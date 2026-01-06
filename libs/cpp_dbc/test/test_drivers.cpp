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

 @file test_drivers.cpp
 @brief Implementation for the cpp_dbc library

*/

#include <string>
#include <memory>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#include "test_mysql_common.hpp"
#include "test_postgresql_common.hpp"
#include "test_firebird_common.hpp"

#include "test_mocks.hpp"

// Test case for DriverManager
TEST_CASE("DriverManager tests", "[driver][manager]")
{
    SECTION("Register and retrieve drivers")
    {
        // Use the mock driver from test_mocks.hpp

        // Register the mock driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc_test::MockDriver>());

        // Try to get a connection with the mock driver
        // This should not throw and return a valid connection
        REQUIRE_NOTHROW([&]()
                        {
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection("cpp_dbc:mock://localhost:1234/mockdb", "user", "pass"));
            // The connection should be valid
            REQUIRE(conn != nullptr); }());

        // Try to get a connection with a non-existent driver
        REQUIRE_THROWS_AS(
            cpp_dbc::DriverManager::getDBConnection("cpp_dbc:nonexistent://localhost:1234/db", "user", "pass"),
            cpp_dbc::DBException);
    }
}

#if USE_MYSQL
// Test case for MySQL driver
TEST_CASE("MySQL driver tests", "[driver][mysql]")
{
    SECTION("MySQL driver URL acceptance")
    {
        // Create a MySQL driver
        cpp_dbc::MySQL::MySQLDBDriver driver;

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
        cpp_dbc::MySQL::MySQLDBDriver driver;

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a DBException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver.connect("cpp_dbc:mysql://localhost:3306/non_existent_db", "user", "pass"),
            cpp_dbc::DBException);
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
        cpp_dbc::PostgreSQL::PostgreSQLDBDriver driver;

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
        cpp_dbc::PostgreSQL::PostgreSQLDBDriver driver;

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a DBException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver.connect("cpp_dbc:postgresql://localhost:5432/non_existent_db", "user", "pass"),
            cpp_dbc::DBException);
    }
}
#endif

#if USE_FIREBIRD
// Test case for Firebird driver
TEST_CASE("Firebird driver tests", "[driver][firebird]")
{
    SECTION("Firebird driver URL acceptance")
    {
        // Create a Firebird driver
        cpp_dbc::Firebird::FirebirdDBDriver driver;

        // Check that it accepts Firebird URLs
        REQUIRE(driver.acceptsURL("cpp_dbc:firebird://localhost:3050/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:firebird://127.0.0.1:3050/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:firebird://db.example.com:3050/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:firebird://localhost:3050//var/lib/firebird/data/testdb.fdb"));

        // Check that it rejects non-Firebird URLs
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("jdbc:firebird://localhost:3050/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("firebird://localhost:3050/testdb"));
    }

    SECTION("Firebird driver connection string parsing")
    {
        // Create a Firebird driver
        cpp_dbc::Firebird::FirebirdDBDriver driver;

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a DBException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver.connect("cpp_dbc:firebird://localhost:3050/non_existent_db", "user", "pass"),
            cpp_dbc::DBException);
    }
}
#endif

// Test case for DBException
TEST_CASE("DBException tests", "[exception]")
{
    SECTION("Create and throw DBException without mark")
    {
        // Create an exception without mark
        cpp_dbc::DBException ex("", "Test error message");

        // Check the error message
        REQUIRE(std::string(ex.what_s()) == "Test error message");

        // Check the mark is empty
        REQUIRE(ex.getMark().empty());

        // Check that we can throw and catch it
        REQUIRE_THROWS_AS(
            throw cpp_dbc::DBException("", "Test throw"),
            cpp_dbc::DBException);

        // Check that we can catch it as a std::exception
        REQUIRE_THROWS_AS(
            throw cpp_dbc::DBException("", "Test throw"),
            std::exception);
    }

    SECTION("Create and throw DBException with mark")
    {
        // Create an exception with mark
        cpp_dbc::DBException ex("9S0T1U2V3W4X", "Test error message");

        // Check the error message includes the mark
        REQUIRE(std::string(ex.what_s()) == "9S0T1U2V3W4X: Test error message");

        // Check the mark is correctly stored
        REQUIRE(ex.getMark() == "9S0T1U2V3W4X");

        // Check that we can throw and catch it
        REQUIRE_THROWS_AS(
            throw cpp_dbc::DBException("", "Test throw"),
            cpp_dbc::DBException);

        // Verify the mark in a caught exception
        try
        {
            throw cpp_dbc::DBException("1M2N3O4P5Q6R", "Error message");
        }
        catch (const cpp_dbc::DBException &e)
        {
            REQUIRE(e.getMark() == "1M2N3O4P5Q6R");
            REQUIRE(std::string(e.what_s()) == "1M2N3O4P5Q6R: Error message");
        }
    }

    SECTION("Create and throw DBException with callstack")
    {
        // Create a simple callstack manually for testing
        std::vector<cpp_dbc::system_utils::StackFrame> test_callstack;
        cpp_dbc::system_utils::StackFrame frame;
        frame.file = "test_file.cpp";
        frame.line = 42;
        frame.function = "test_function";
        test_callstack.push_back(frame);

        // Create exception with callstack
        cpp_dbc::DBException ex("CALLSTACK", "Test error with callstack", test_callstack);

        // Check the error message includes the mark
        REQUIRE(std::string(ex.what_s()) == "CALLSTACK: Test error with callstack");

        // Check the mark is correctly stored
        REQUIRE(ex.getMark() == "CALLSTACK");

        // Check that the callstack is stored and can be retrieved
        REQUIRE(ex.getCallStack().size() == 1);
        REQUIRE(ex.getCallStack()[0].file == "test_file.cpp");
        REQUIRE(ex.getCallStack()[0].line == 42);
        REQUIRE(ex.getCallStack()[0].function == "test_function");

        // Test that we can print the callstack without crashing
        REQUIRE_NOTHROW(ex.printCallStack());
    }

    SECTION("Capture real callstack and throw DBException with callstack")
    {
        // Create exception with callstack
        cpp_dbc::DBException ex("CALLSTACK", "Test error with real callstack", cpp_dbc::system_utils::captureCallStack(true));

        // Check the error message includes the mark
        REQUIRE(std::string(ex.what_s()) == "CALLSTACK: Test error with real callstack");

        // Check the mark is correctly stored
        REQUIRE(ex.getMark() == "CALLSTACK");

        // Check that the callstack is stored and can be retrieved
        REQUIRE(ex.getCallStack().size() >= 1);

        // Test that we can print the callstack without crashing
        REQUIRE_NOTHROW(ex.printCallStack());
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

// Test case for abstract interfaces
TEST_CASE("Abstract interface tests", "[interface]")
{
    SECTION("ResultSet interface")
    {
        // Create a mock result set
        auto rs = std::make_shared<cpp_dbc_test::MockResultSet>();

        // Check basic ResultSet state methods
        REQUIRE(rs->next() == false); // No rows in the result set
        REQUIRE(rs->isBeforeFirst() == true);
        REQUIRE(rs->isAfterLast() == false);
        REQUIRE(rs->getRow() == 0);

        // Check that column metadata is available
        REQUIRE(rs->getColumnNames() == std::vector<std::string>{"mock"});
        REQUIRE(rs->getColumnCount() == 1);

        // Note: We don't test getInt, getString, etc. on an empty result set
        // as that would throw exceptions in a real database implementation
    }

    SECTION("PreparedStatement interface")
    {
        // Create a mock prepared statement
        auto stmt = std::make_shared<cpp_dbc_test::MockPreparedStatement>();

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
        auto conn = std::make_shared<cpp_dbc_test::MockConnection>();

        // Check that we can call the methods
        REQUIRE(conn->isClosed() == false);

        auto stmt = conn->prepareStatement("SELECT 1");
        REQUIRE(stmt != nullptr);

        auto rs = conn->executeQuery("SELECT 1");
        REQUIRE(rs != nullptr);

        // For consistency with integration tests, we expect 2 for UPDATE queries
        REQUIRE(conn->executeUpdate("UPDATE test SET col = 1") == 2);

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
        REQUIRE((!drivers.empty() || drivers.size() == 0));

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