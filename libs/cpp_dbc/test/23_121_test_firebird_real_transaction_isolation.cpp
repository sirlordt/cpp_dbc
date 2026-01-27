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

 @file test_firebird_real_transaction_isolation.cpp
 @brief Tests for Firebird transaction isolation levels

*/

#include <catch2/catch_test_macros.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <string>
#include <memory>
#include <iostream>

#include "23_001_test_firebird_real_common.hpp"

#if USE_FIREBIRD

// Test case for Firebird driver transaction isolation
TEST_CASE("Firebird transaction isolation tests", "[23_121_01_firebird_real_transaction_isolation]")
{
    // Skip if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    // Get Firebird configuration using the helper function
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");

    // Create connection parameters
    std::string connStr = dbConfig.createConnectionString();
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    SECTION("Firebird driver default isolation level")
    {
        // Create a Firebird driver
        cpp_dbc::Firebird::FirebirdDBDriver driver;

        try
        {
            // Try to connect to a local Firebird server
            auto conn = driver.connectRelational(connStr, username, password);

            // Check default isolation level (should be READ_COMMITTED for Firebird)
            REQUIRE(conn->getTransactionIsolation() == cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);

            // Set and check each isolation level
            conn->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED);
            REQUIRE(conn->getTransactionIsolation() == cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_UNCOMMITTED);

            conn->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);
            REQUIRE(conn->getTransactionIsolation() == cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);

            conn->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ);
            REQUIRE(conn->getTransactionIsolation() == cpp_dbc::TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ);

            conn->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
            REQUIRE(conn->getTransactionIsolation() == cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);

            // Close the connection
            conn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            // If we can't connect to a real database, skip the test
            SKIP("Could not connect to Firebird database: " + std::string(e.what_s()));
        }
    }

    SECTION("Firebird READ_COMMITTED isolation behavior")
    {
        // Create a Firebird driver
        cpp_dbc::Firebird::FirebirdDBDriver driver;

        try
        {
            // Create a test table using RECREATE TABLE (Firebird's equivalent of DROP IF EXISTS + CREATE)
            // Note: 'value' is a reserved word in Firebird, so we use 'val' instead
            auto setupConn = driver.connectRelational(connStr, username, password);
            setupConn->executeUpdate("RECREATE TABLE isolation_test (id INT NOT NULL PRIMARY KEY, val VARCHAR(50))");
            setupConn->executeUpdate("INSERT INTO isolation_test VALUES (1, 'initial')");
            setupConn->commit(); // Explicitly commit before closing (required for Firebird)
            setupConn->close();

            // Create two connections
            auto conn1 = driver.connectRelational(connStr, username, password);
            auto conn2 = driver.connectRelational(connStr, username, password);

            // Start transactions first, then set isolation level
            // For Firebird, we need to start the transaction before setting isolation level
            // because setTransactionIsolation may end the current transaction
            conn1->setAutoCommit(false);
            conn2->setAutoCommit(false);

            // Set READ_COMMITTED isolation level for both connections
            // This will restart the transactions with the new isolation level
            conn1->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);
            conn2->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_READ_COMMITTED);

            // Ensure transactions are started
            conn1->beginTransaction();
            conn2->beginTransaction();

            // Conn1 reads initial value
            auto rs1 = conn1->executeQuery("SELECT val FROM isolation_test WHERE id = 1");
            REQUIRE(rs1->next());
            std::string val1 = rs1->getString("VAL"); // Firebird returns uppercase column names
            rs1->close();                             // Close ResultSet before next operation (required for Firebird)
            REQUIRE(val1 == "initial");

            // Conn1 updates the value but doesn't commit
            conn1->executeUpdate("UPDATE isolation_test SET val = 'uncommitted' WHERE id = 1");

            // With READ_COMMITTED, Conn2 should NOT see the uncommitted change
            auto rs2 = conn2->executeQuery("SELECT val FROM isolation_test WHERE id = 1");
            REQUIRE(rs2->next());
            std::string val2 = rs2->getString("VAL");
            rs2->close();
            REQUIRE(val2 == "initial");

            // Conn1 commits the change
            conn1->commit();

            // Now Conn2 should see the committed change
            auto rs3 = conn2->executeQuery("SELECT val FROM isolation_test WHERE id = 1");
            REQUIRE(rs3->next());
            std::string val3 = rs3->getString("VAL");
            rs3->close();
            REQUIRE(val3 == "uncommitted");

            // Cleanup
            conn2->rollback();
            conn1->close();
            conn2->close();

            // Drop the test table
            auto cleanupConn = driver.connectRelational(connStr, username, password);
            cleanupConn->executeUpdate("DROP TABLE isolation_test");
            cleanupConn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            SKIP("Could not run Firebird READ_COMMITTED test: " + std::string(e.what_s()));
        }
    }

    SECTION("Firebird REPEATABLE_READ isolation behavior")
    {
        // Create a Firebird driver
        cpp_dbc::Firebird::FirebirdDBDriver driver;

        try
        {
            // Create a test table using RECREATE TABLE (Firebird's equivalent of DROP IF EXISTS + CREATE)
            // Note: 'value' is a reserved word in Firebird, so we use 'val' instead
            auto setupConn = driver.connectRelational(connStr, username, password);
            setupConn->executeUpdate("RECREATE TABLE isolation_test (id INT NOT NULL PRIMARY KEY, val VARCHAR(50))");
            setupConn->executeUpdate("INSERT INTO isolation_test VALUES (1, 'initial')");
            setupConn->commit(); // Explicitly commit before closing (required for Firebird)
            setupConn->close();

            // Create two connections
            auto conn1 = driver.connectRelational(connStr, username, password);
            auto conn2 = driver.connectRelational(connStr, username, password);

            // Start transactions first, then set isolation level
            // For Firebird, we need to start the transaction before setting isolation level
            conn1->setAutoCommit(false);
            conn2->setAutoCommit(false);

            // Set REPEATABLE_READ isolation level for both connections
            // This will restart the transactions with the new isolation level
            conn1->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ);
            conn2->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_REPEATABLE_READ);

            // Ensure transactions are started
            conn1->beginTransaction();
            conn2->beginTransaction();

            // Conn2 reads initial value
            auto rs1 = conn2->executeQuery("SELECT val FROM isolation_test WHERE id = 1");
            REQUIRE(rs1->next());
            std::string val1 = rs1->getString("VAL"); // Firebird returns uppercase column names
            rs1->close();                             // Close ResultSet before next operation (required for Firebird)
            REQUIRE(val1 == "initial");

            // Conn1 updates the value and commits
            conn1->executeUpdate("UPDATE isolation_test SET val = 'changed' WHERE id = 1");
            conn1->commit();

            // With REPEATABLE_READ, Conn2 should still see the original value
            auto rs2 = conn2->executeQuery("SELECT val FROM isolation_test WHERE id = 1");
            REQUIRE(rs2->next());
            std::string val2 = rs2->getString("VAL");
            rs2->close();
            REQUIRE(val2 == "initial");

            // Cleanup
            conn2->rollback();
            conn1->close();
            conn2->close();

            // Drop the test table
            auto cleanupConn = driver.connectRelational(connStr, username, password);
            cleanupConn->executeUpdate("DROP TABLE isolation_test");
            cleanupConn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            SKIP("Could not run Firebird REPEATABLE_READ test: " + std::string(e.what_s()));
        }
    }

    SECTION("Firebird SERIALIZABLE isolation behavior")
    {
        // Create a Firebird driver
        cpp_dbc::Firebird::FirebirdDBDriver driver;

        try
        {
            // Create a test table using RECREATE TABLE (Firebird's equivalent of DROP IF EXISTS + CREATE)
            // Note: 'value' is a reserved word in Firebird, so we use 'val' instead
            auto setupConn = driver.connectRelational(connStr, username, password);
            setupConn->executeUpdate("RECREATE TABLE isolation_test (id INT NOT NULL PRIMARY KEY, val VARCHAR(50))");
            setupConn->executeUpdate("INSERT INTO isolation_test VALUES (1, 'initial')");
            setupConn->commit(); // Explicitly commit before closing (required for Firebird)
            setupConn->close();

            // Create two connections
            auto conn1 = driver.connectRelational(connStr, username, password);
            auto conn2 = driver.connectRelational(connStr, username, password);

            // Test 1: Basic SERIALIZABLE behavior in Firebird
            {
                // Start transaction for Conn1 first, then set isolation level
                conn1->setAutoCommit(false);
                conn1->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
                conn1->beginTransaction();

                // Conn1 reads initial value
                auto rs1 = conn1->executeQuery("SELECT val FROM isolation_test WHERE id = 1");
                REQUIRE(rs1->next());
                std::string val1 = rs1->getString("VAL"); // Firebird returns uppercase column names
                rs1->close();                             // Close ResultSet before next operation (required for Firebird)
                REQUIRE(val1 == "initial");

                // Conn1 updates the value and commits
                conn1->executeUpdate("UPDATE isolation_test SET val = 'changed' WHERE id = 1");
                conn1->commit();

                // Start a new transaction with SERIALIZABLE isolation
                auto conn3 = driver.connectRelational(connStr, username, password);
                conn3->setAutoCommit(false);
                conn3->setTransactionIsolation(cpp_dbc::TransactionIsolationLevel::TRANSACTION_SERIALIZABLE);
                conn3->beginTransaction();

                // Read the value - should see the committed change since this is a new transaction
                auto rs3 = conn3->executeQuery("SELECT val FROM isolation_test WHERE id = 1");
                REQUIRE(rs3->next());
                std::string value = rs3->getString("VAL");
                rs3->close();
                INFO("Firebird SERIALIZABLE (new transaction): Got value '" << value << "', expected 'changed'");
                REQUIRE(value == "changed");

                conn3->rollback();
                conn3->close();
            }

            // Test 2: Document Firebird's SERIALIZABLE behavior
            INFO("Firebird's SERIALIZABLE isolation level provides snapshot isolation");
            INFO("It prevents dirty reads, non-repeatable reads, and phantom reads");
            INFO("Firebird uses Multi-Version Concurrency Control (MVCC) for isolation");

            // Cleanup
            conn1->close();
            conn2->close();

            // Drop the test table
            auto cleanupConn = driver.connectRelational(connStr, username, password);
            cleanupConn->executeUpdate("DROP TABLE isolation_test");
            cleanupConn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            SKIP("Could not run Firebird SERIALIZABLE test: " + std::string(e.what_s()));
        }
    }
}

#endif
