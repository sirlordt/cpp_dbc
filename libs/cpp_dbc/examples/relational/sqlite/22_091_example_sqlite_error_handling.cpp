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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file 22_091_example_sqlite_error_handling.cpp
 * @brief SQLite-specific example demonstrating database error handling
 *
 * This example demonstrates:
 * - Syntax errors, constraint violations, data type errors
 * - Transaction errors and connection errors
 * - Prepared statement errors and result set errors
 * - Error recovery and custom error handling
 * - SQLite-specific error handling (database locked, PRAGMA errors)
 *
 * Usage:
 *   ./22_091_example_sqlite_error_handling [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - SQLite support not enabled at compile time
 */

#include "../example_relational_common.hpp"
#include <functional>
#include <stdexcept>

using namespace cpp_dbc::examples;

#if USE_SQLITE

// Custom exception class for application-specific errors
class AppException : public std::runtime_error
{
public:
    explicit AppException(const std::string &message) : std::runtime_error(message) {}
};

// Helper function to execute a database operation and handle errors
void executeWithErrorHandling(const std::string &operationName, std::function<void()> operation)
{
    try
    {
        log("");
        logStep("Executing: " + operationName);
        operation();
        logOk("Operation completed successfully");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error in " + operationName + ": " + e.what_s());
        e.printCallStack();
    }
    catch (const AppException &e)
    {
        logError("Application error in " + operationName + ": " + std::string(e.what()));
    }
    catch (const std::exception &e)
    {
        logError("Standard exception in " + operationName + ": " + std::string(e.what()));
    }
    catch (...)
    {
        logError("Unknown error in " + operationName);
    }
}

// Function to set up the test database
void setupDatabase(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    executeWithErrorHandling("Setup Database", [&conn]()
                             {
        // Drop existing tables if they exist
        conn->executeUpdate("DROP TABLE IF EXISTS error_test_orders");
        conn->executeUpdate("DROP TABLE IF EXISTS error_test_customers");

        // Create customers table
        conn->executeUpdate(
            "CREATE TABLE error_test_customers ("
            "customer_id INTEGER PRIMARY KEY, "
            "name TEXT NOT NULL, "
            "email TEXT UNIQUE, "
            "credit_limit REAL CHECK (credit_limit >= 0)"
            ")"
        );

        // Create orders table with foreign key constraint
        conn->executeUpdate("PRAGMA foreign_keys = ON");
        conn->executeUpdate(
            "CREATE TABLE error_test_orders ("
            "order_id INTEGER PRIMARY KEY, "
            "customer_id INTEGER NOT NULL, "
            "product_name TEXT NOT NULL, "
            "quantity INTEGER NOT NULL CHECK (quantity > 0), "
            "price REAL NOT NULL, "
            "FOREIGN KEY (customer_id) REFERENCES error_test_customers(customer_id)"
            ")"
        );

        // Insert some valid customer data
        auto pstmt = conn->prepareStatement(
            "INSERT INTO error_test_customers (customer_id, name, email, credit_limit) "
            "VALUES (?, ?, ?, ?)"
        );

        pstmt->setInt(1, 1);
        pstmt->setString(2, "John Doe");
        pstmt->setString(3, "john@example.com");
        pstmt->setDouble(4, 1000.00);
        pstmt->executeUpdate();

        pstmt->setInt(1, 2);
        pstmt->setString(2, "Jane Smith");
        pstmt->setString(3, "jane@example.com");
        pstmt->setDouble(4, 2000.00);
        pstmt->executeUpdate();

        // Insert some valid order data
        auto orderStmt = conn->prepareStatement(
            "INSERT INTO error_test_orders (order_id, customer_id, product_name, quantity, price) "
            "VALUES (?, ?, ?, ?, ?)"
        );

        orderStmt->setInt(1, 101);
        orderStmt->setInt(2, 1);
        orderStmt->setString(3, "Laptop");
        orderStmt->setInt(4, 1);
        orderStmt->setDouble(5, 999.99);
        orderStmt->executeUpdate();

        orderStmt->setInt(1, 102);
        orderStmt->setInt(2, 2);
        orderStmt->setString(3, "Smartphone");
        orderStmt->setInt(4, 2);
        orderStmt->setDouble(5, 599.98);
        orderStmt->executeUpdate(); });
}

// Function to demonstrate handling syntax errors
void demonstrateSyntaxErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Syntax Errors ---");

    executeWithErrorHandling("Syntax Error Example", [&conn]()
                             {
        // Intentional syntax error in SQL query
        conn->executeQuery("SELCT * FROM error_test_customers"); });
}

// Function to demonstrate handling constraint violations
void demonstrateConstraintViolations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Constraint Violations ---");

    // Primary key violation
    executeWithErrorHandling("Primary Key Violation", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "INSERT INTO error_test_customers (customer_id, name, email, credit_limit) "
            "VALUES (?, ?, ?, ?)"
        );

        pstmt->setInt(1, 1); // ID 1 already exists
        pstmt->setString(2, "Bob Johnson");
        pstmt->setString(3, "bob@example.com");
        pstmt->setDouble(4, 500.00);
        pstmt->executeUpdate(); });

    // Unique constraint violation
    executeWithErrorHandling("Unique Constraint Violation", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "INSERT INTO error_test_customers (customer_id, name, email, credit_limit) "
            "VALUES (?, ?, ?, ?)"
        );

        pstmt->setInt(1, 3); // New ID
        pstmt->setString(2, "Alice Brown");
        pstmt->setString(3, "john@example.com"); // Email already exists
        pstmt->setDouble(4, 1500.00);
        pstmt->executeUpdate(); });

    // Check constraint violation
    executeWithErrorHandling("Check Constraint Violation", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "INSERT INTO error_test_customers (customer_id, name, email, credit_limit) "
            "VALUES (?, ?, ?, ?)"
        );

        pstmt->setInt(1, 4); // New ID
        pstmt->setString(2, "Charlie Davis");
        pstmt->setString(3, "charlie@example.com");
        pstmt->setDouble(4, -100.00); // Negative credit limit
        pstmt->executeUpdate(); });

    // Foreign key constraint violation
    executeWithErrorHandling("Foreign Key Constraint Violation", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "INSERT INTO error_test_orders (order_id, customer_id, product_name, quantity, price) "
            "VALUES (?, ?, ?, ?, ?)"
        );

        pstmt->setInt(1, 103); // New order ID
        pstmt->setInt(2, 999); // Non-existent customer ID
        pstmt->setString(3, "Headphones");
        pstmt->setInt(4, 1);
        pstmt->setDouble(5, 99.99);
        pstmt->executeUpdate(); });

    // Not null constraint violation
    executeWithErrorHandling("Not Null Constraint Violation", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "INSERT INTO error_test_customers (customer_id, name, email, credit_limit) "
            "VALUES (?, ?, ?, ?)"
        );

        pstmt->setInt(1, 5); // New ID
        pstmt->setNull(2, cpp_dbc::Types::VARCHAR); // Null name
        pstmt->setString(3, "null@example.com");
        pstmt->setDouble(4, 500.00);
        pstmt->executeUpdate(); });
}

// Function to demonstrate handling data type errors
void demonstrateDataTypeErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Data Type Errors ---");
    logInfo("Note: SQLite has dynamic typing, so fewer type errors occur");

    // Invalid SQL operation
    executeWithErrorHandling("Invalid SQL Function", [&conn]()
                             {
        conn->executeQuery("SELECT INVALID_FUNCTION(name) FROM error_test_customers"); });

    // Type affinity behavior (SQLite specific)
    executeWithErrorHandling("Type Affinity Demo", [&conn]()
                             {
        // SQLite will store this as text due to type affinity
        conn->executeUpdate("INSERT INTO error_test_customers (customer_id, name, email, credit_limit) VALUES (10, 'Test', 'test@test.com', 'not_a_number')");
        logInfo("SQLite stored 'not_a_number' in REAL column due to type affinity");
        conn->executeUpdate("DELETE FROM error_test_customers WHERE customer_id = 10"); });
}

// Function to demonstrate handling transaction errors
void demonstrateTransactionErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Transaction Errors ---");

    // Transaction rollback example
    executeWithErrorHandling("Transaction Rollback", [&conn]()
                             {
        conn->setAutoCommit(false);

        try {
            auto pstmt1 = conn->prepareStatement(
                "INSERT INTO error_test_customers (customer_id, name, email, credit_limit) "
                "VALUES (?, ?, ?, ?)"
            );

            pstmt1->setInt(1, 10);
            pstmt1->setString(2, "Transaction Test");
            pstmt1->setString(3, "transaction@example.com");
            pstmt1->setDouble(4, 1000.00);
            pstmt1->executeUpdate();

            logData("First operation in transaction succeeded");

            // Second operation fails (primary key violation)
            auto pstmt2 = conn->prepareStatement(
                "INSERT INTO error_test_customers (customer_id, name, email, credit_limit) "
                "VALUES (?, ?, ?, ?)"
            );

            pstmt2->setInt(1, 1); // ID 1 already exists
            pstmt2->setString(2, "Will Fail");
            pstmt2->setString(3, "will.fail@example.com");
            pstmt2->setDouble(4, 500.00);
            pstmt2->executeUpdate();

            conn->commit();
        }
        catch (const cpp_dbc::DBException& e) {
            logError("Error in transaction: " + e.what_s());
            logStep("Rolling back transaction...");
            conn->rollback();

            // Verify the rollback worked
            auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM error_test_customers WHERE customer_id = 10");
            rs->next();
            int count = rs->getInt("count");
            logData("After rollback, customer ID 10 count: " + std::to_string(count));

            if (count > 0) {
                throw AppException("Transaction rollback failed!");
            }
            logOk("Rollback verified");
        }

        conn->setAutoCommit(true); });
}

// Function to demonstrate handling connection errors
void demonstrateConnectionErrors()
{
    log("");
    log("--- Connection Errors ---");

    executeWithErrorHandling("Connection Error - Invalid Path", []()
                             {
        auto conn = cpp_dbc::DriverManager::getDBConnection(
            "cpp_dbc:sqlite:///nonexistent/path/to/database.db",
            "",
            ""
        ); });

    executeWithErrorHandling("Invalid Connection URL", []()
                             {
        auto conn = cpp_dbc::DriverManager::getDBConnection(
            "invalid:url:format",
            "user",
            "password"
        ); });
}

// Function to demonstrate handling prepared statement errors
void demonstratePreparedStatementErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Prepared Statement Errors ---");

    // Invalid parameter index
    executeWithErrorHandling("Invalid Parameter Index", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "SELECT * FROM error_test_customers WHERE customer_id = ?"
        );

        pstmt->setInt(2, 1); // Only parameter 1 is valid
        pstmt->executeQuery(); });

    // Missing parameter
    executeWithErrorHandling("Missing Parameter", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "SELECT * FROM error_test_customers WHERE customer_id = ? AND name = ?"
        );

        pstmt->setInt(1, 1);
        pstmt->executeQuery(); });
}

// Function to demonstrate handling result set errors
void demonstrateResultSetErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Result Set Errors ---");

    // Invalid column name
    executeWithErrorHandling("Invalid Column Name", [&conn]()
                             {
        auto rs = conn->executeQuery("SELECT * FROM error_test_customers");
        rs->next();
        rs->getString("non_existent_column"); });

    // Accessing result set after it's closed
    executeWithErrorHandling("Closed Result Set Access", [&conn]()
                             {
        auto rs = conn->executeQuery("SELECT * FROM error_test_customers");
        rs->close();
        rs->next(); });
}

// Function to demonstrate proper error recovery
void demonstrateErrorRecovery(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Error Recovery ---");

    try
    {
        logStep("Attempting an operation that will fail...");
        conn->executeUpdate("INSERT INTO error_test_customers (customer_id) VALUES (1)");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logData("Expected error occurred: " + e.what_s());
        logStep("Recovering by performing a valid operation...");

        try
        {
            // Perform a valid operation
            auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM error_test_customers");
            rs->next();
            int count = rs->getInt("count");
            logOk("Recovery successful. Customer count: " + std::to_string(count));
        }
        catch (const cpp_dbc::DBException &recoverError)
        {
            logError("Recovery failed: " + recoverError.what_s());
        }
    }
}

// Function to demonstrate SQLite-specific errors
void demonstrateSQLiteSpecificErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- SQLite-Specific Error Handling ---");

    // Invalid PRAGMA
    executeWithErrorHandling("SQLite Invalid Table Name", [&conn]()
                             {
        conn->executeQuery("SELECT * FROM table_that_does_not_exist"); });

    // Demonstrate SQLITE_BUSY scenario info
    logInfo("Common SQLite errors to handle in production:");
    logInfo("- SQLITE_BUSY: Database is locked - use exponential backoff retry");
    logInfo("- SQLITE_LOCKED: Table is locked - another connection has a lock");
    logInfo("- SQLITE_FULL: Database or disk is full");
    logInfo("- SQLITE_IOERR: Disk I/O error occurred");

    // Show how to check journal mode
    executeWithErrorHandling("Check Journal Mode", [&conn]()
                             {
        auto rs = conn->executeQuery("PRAGMA journal_mode");
        rs->next();
        logData("Current journal mode: " + rs->getString(1));
        logInfo("Consider using WAL mode for better concurrency: PRAGMA journal_mode=WAL"); });
}

// Function to run all error demonstrations
void runAllDemonstrations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    setupDatabase(conn);
    demonstrateSyntaxErrors(conn);
    demonstrateConstraintViolations(conn);
    demonstrateDataTypeErrors(conn);
    demonstrateTransactionErrors(conn);
    demonstratePreparedStatementErrors(conn);
    demonstrateResultSetErrors(conn);
    demonstrateErrorRecovery(conn);
    demonstrateSQLiteSpecificErrors(conn);

    log("");
    logStep("Cleaning up tables...");
    conn->executeUpdate("DROP TABLE IF EXISTS error_test_orders");
    conn->executeUpdate("DROP TABLE IF EXISTS error_test_customers");
    logOk("Tables dropped");
}

#endif // USE_SQLITE

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc SQLite Error Handling Example");
    log("========================================");
    log("");

#if !USE_SQLITE
    logError("SQLite support is not enabled");
    logInfo("Build with -DUSE_SQLITE=ON to enable SQLite support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,sqlite");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("22_091_example_sqlite_error_handling", "sqlite");
        return EXIT_OK_;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return 1;
    }

    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return 1;
    }
    logOk("Configuration loaded successfully");

    auto &configManager = *configResult.value();

    logStep("Registering SQLite driver...");
    registerDriver("sqlite");
    logOk("Driver registered");

    try
    {
        logStep("Getting SQLite configuration...");
        auto sqliteResult = getDbConfig(configManager, args.dbName.empty() ? "" : args.dbName, "sqlite");

        if (!sqliteResult)
        {
            logError("Failed to get SQLite config: " + sqliteResult.error().what_s());
            return 1;
        }

        if (!sqliteResult.value())
        {
            logError("SQLite configuration not found");
            return 1;
        }

        auto &sqliteConfig = *sqliteResult.value();
        logOk("Using database: " + sqliteConfig.getName() +
              " (" + sqliteConfig.getType() + "://" + sqliteConfig.getDatabase() + ")");

        logStep("Connecting to SQLite...");
        auto sqliteConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            sqliteConfig.createDBConnection());
        logOk("Connected to SQLite");

        // Enable foreign keys for this connection
        sqliteConn->executeUpdate("PRAGMA foreign_keys = ON");

        runAllDemonstrations(sqliteConn);

        // Demonstrate connection errors (these don't require an existing connection)
        demonstrateConnectionErrors();

        logStep("Closing SQLite connection...");
        sqliteConn->close();
        logOk("SQLite connection closed");
    }
    catch (const std::exception &e)
    {
        logError("Unhandled error: " + std::string(e.what()));
        return 1;
    }

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return EXIT_OK_;
#endif
}
