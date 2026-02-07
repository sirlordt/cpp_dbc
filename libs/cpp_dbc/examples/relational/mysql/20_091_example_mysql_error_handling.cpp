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
 * @file 20_091_example_mysql_error_handling.cpp
 * @brief MySQL-specific example demonstrating database error handling
 *
 * This example demonstrates:
 * - Syntax errors, constraint violations, data type errors
 * - Transaction errors and connection errors
 * - Prepared statement errors and result set errors
 * - Error recovery and custom error handling
 * - MySQL-specific error handling (invalid engine, too many connections)
 *
 * Usage:
 *   ./20_091_example_mysql_error_handling [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - MySQL support not enabled at compile time
 */

#include "../example_relational_common.hpp"
#include <functional>
#include <stdexcept>

using namespace cpp_dbc::examples;

#if USE_MYSQL

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
        logMsg("");
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
            "customer_id INT PRIMARY KEY, "
            "name VARCHAR(100) NOT NULL, "
            "email VARCHAR(100) UNIQUE, "
            "credit_limit DECIMAL(10,2) CHECK (credit_limit >= 0)"
            ")"
        );

        // Create orders table with foreign key constraint
        conn->executeUpdate(
            "CREATE TABLE error_test_orders ("
            "order_id INT PRIMARY KEY, "
            "customer_id INT NOT NULL, "
            "product_name VARCHAR(100) NOT NULL, "
            "quantity INT NOT NULL CHECK (quantity > 0), "
            "price DECIMAL(10,2) NOT NULL, "
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
    logMsg("");
    logMsg("--- Syntax Errors ---");

    executeWithErrorHandling("Syntax Error Example", [&conn]()
                             {
        // Intentional syntax error in SQL query
        conn->executeQuery("SELCT * FROM error_test_customers"); });
}

// Function to demonstrate handling constraint violations
void demonstrateConstraintViolations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Constraint Violations ---");

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
    logMsg("");
    logMsg("--- Data Type Errors ---");

    // Type conversion error
    executeWithErrorHandling("Type Conversion Error", [&conn]()
                             {
        auto rs = conn->executeQuery("SELECT 'abc' + 123 FROM error_test_customers");
        rs->next(); });

    // Invalid date format
    executeWithErrorHandling("Invalid Date Format", [&conn]()
                             { conn->executeQuery("SELECT * FROM error_test_customers WHERE customer_id = '2023-13-32'"); });

    // Numeric overflow
    executeWithErrorHandling("Numeric Overflow", [&conn]()
                             {
        auto rs = conn->executeQuery("SELECT 9999999999999999999999999999 * 9999999999999999999999999999 FROM error_test_customers");
        rs->next(); });
}

// Function to demonstrate handling transaction errors
void demonstrateTransactionErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Transaction Errors ---");

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

    logInfo("Deadlock simulation would require multiple concurrent connections");
}

// Function to demonstrate handling connection errors
void demonstrateConnectionErrors()
{
    logMsg("");
    logMsg("--- Connection Errors ---");

    executeWithErrorHandling("Connection Error", []()
                             { auto conn = cpp_dbc::DriverManager::getDBConnection(
                                   "cpp_dbc:mysql://localhost:3306/nonexistent_db",
                                   "invalid_user",
                                   "invalid_password"); });

    executeWithErrorHandling("Invalid Connection URL", []()
                             { auto conn = cpp_dbc::DriverManager::getDBConnection(
                                   "invalid:url:format",
                                   "user",
                                   "password"); });
}

// Function to demonstrate handling prepared statement errors
void demonstratePreparedStatementErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Prepared Statement Errors ---");

    // Invalid parameter index
    executeWithErrorHandling("Invalid Parameter Index", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "SELECT * FROM error_test_customers WHERE customer_id = ?"
        );

        pstmt->setInt(2, 1); // Only parameter 1 is valid
        pstmt->executeQuery(); });

    // Parameter type mismatch
    executeWithErrorHandling("Parameter Type Mismatch", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "SELECT * FROM error_test_customers WHERE customer_id = ?"
        );

        pstmt->setString(1, "not_an_integer");
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
    logMsg("");
    logMsg("--- Result Set Errors ---");

    // Invalid column name
    executeWithErrorHandling("Invalid Column Name", [&conn]()
                             {
        auto rs = conn->executeQuery("SELECT * FROM error_test_customers");
        rs->next();
        rs->getString("non_existent_column"); });

    // Type conversion error in result set
    executeWithErrorHandling("Result Set Type Conversion Error", [&conn]()
                             {
        auto rs = conn->executeQuery("SELECT name FROM error_test_customers");
        rs->next();
        rs->getInt("name"); });

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
    logMsg("");
    logMsg("--- Error Recovery ---");

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
            // Check if the connection is still valid
            bool isValid = true;
            try
            {
                auto rs = conn->executeQuery("SELECT 1");
                rs->next();
                rs->getInt(1);
            }
            catch (const cpp_dbc::DBException &)
            {
                isValid = false;
            }

            if (!isValid)
            {
                logInfo("Connection is no longer valid. Reconnecting...");
            }

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

// Function to demonstrate custom error handling and logging
void demonstrateCustomErrorHandling(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Custom Error Handling ---");

    auto customLogError = [](const std::string &operation, const std::exception &e)
    {
        logError("LOG: [" + operation + "] " + std::string(e.what()));
    };

    auto executeWithLogging = [&customLogError, &conn](const std::string &sql, const std::string &operation)
    {
        try
        {
            logStep("Executing: " + operation);
            conn->executeUpdate(sql);
            logOk("Operation completed successfully");
        }
        catch (const cpp_dbc::DBException &e)
        {
            customLogError(operation, e);

            std::string errorMsg = e.what_s();
            if (errorMsg.find("constraint") != std::string::npos ||
                errorMsg.find("CONSTRAINT") != std::string::npos ||
                errorMsg.find("duplicate") != std::string::npos ||
                errorMsg.find("UNIQUE") != std::string::npos ||
                errorMsg.find("Duplicate") != std::string::npos)
            {
                logData("Error category: Constraint violation");
            }
            else if (errorMsg.find("syntax") != std::string::npos ||
                     errorMsg.find("SYNTAX") != std::string::npos)
            {
                logData("Error category: Syntax error");
            }
            else if (errorMsg.find("connect") != std::string::npos ||
                     errorMsg.find("CONNECTION") != std::string::npos)
            {
                logData("Error category: Connection error");
            }
            else
            {
                logData("Error category: Other database error");
            }
        }
        catch (const std::exception &e)
        {
            customLogError(operation, e);
            logData("Error category: Standard exception");
        }
    };

    executeWithLogging(
        "INSERT INTO error_test_customers (customer_id, name, email, credit_limit) VALUES (1, 'Duplicate', 'dup@example.com', 100)",
        "Primary Key Violation Test");

    executeWithLogging(
        "SELCT * FROM error_test_customers",
        "Syntax Error Test");

    executeWithLogging(
        "INSERT INTO nonexistent_table (id) VALUES (1)",
        "Missing Table Test");
}

// Function to demonstrate MySQL-specific errors
void demonstrateMySQLSpecificErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- MySQL-Specific Error Handling ---");

    // Invalid storage engine
    executeWithErrorHandling("MySQL Invalid Storage Engine", [&conn]()
                             { conn->executeUpdate(
                                   "CREATE TABLE invalid_engine_table (id INT) ENGINE=INVALID_ENGINE"); });

    // Division by zero behavior
    executeWithErrorHandling("MySQL Division by Zero", [&conn]()
                             {
        auto rs = conn->executeQuery("SELECT 1/0 as result");
        rs->next();
        logData("MySQL returns: " + std::to_string(rs->getDouble("result")) + " for division by zero"); });

    // String truncation
    executeWithErrorHandling("MySQL String Truncation", [&conn]()
                             {
        conn->executeUpdate(
            "CREATE TABLE test_truncation (id INT PRIMARY KEY, name VARCHAR(5))"
        );
        conn->executeUpdate(
            "INSERT INTO test_truncation (id, name) VALUES (1, 'This is too long')"
        );
        conn->executeUpdate("DROP TABLE IF EXISTS test_truncation"); });

    logInfo("In production, handle 'Too many connections' (MySQL error 1040) with connection pooling");
    logInfo("Consider using connection pool with proper limits and timeout settings");
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
    demonstrateCustomErrorHandling(conn);
    demonstrateMySQLSpecificErrors(conn);

    logMsg("");
    logStep("Cleaning up tables...");
    conn->executeUpdate("DROP TABLE IF EXISTS error_test_orders");
    conn->executeUpdate("DROP TABLE IF EXISTS error_test_customers");
    logOk("Tables dropped");
}

#endif // USE_MYSQL

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc MySQL Error Handling Example");
    logMsg("========================================");
    logMsg("");

#if !USE_MYSQL
    logError("MySQL support is not enabled");
    logInfo("Build with -DUSE_MYSQL=ON to enable MySQL support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,mysql");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("20_091_example_mysql_error_handling", "mysql");
        return EXIT_OK_;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    // Check for real error (DBException)
    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return 1;
    }

    // Check if file was found
    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return 1;
    }
    logOk("Configuration loaded successfully");

    auto &configManager = *configResult.value();

    logStep("Registering MySQL driver...");
    registerDriver("mysql");
    logOk("Driver registered");

    try
    {
        logStep("Getting MySQL configuration...");
        auto mysqlResult = getDbConfig(configManager, args.dbName.empty() ? "" : args.dbName, "mysql");

        if (!mysqlResult)
        {
            logError("Failed to get MySQL config: " + mysqlResult.error().what_s());
            return 1;
        }

        if (!mysqlResult.value())
        {
            logError("MySQL configuration not found");
            return 1;
        }

        auto &mysqlConfig = *mysqlResult.value();
        logOk("Using database: " + mysqlConfig.getName() +
              " (" + mysqlConfig.getType() + "://" +
              mysqlConfig.getHost() + ":" + std::to_string(mysqlConfig.getPort()) +
              "/" + mysqlConfig.getDatabase() + ")");

        logStep("Connecting to MySQL...");
        auto mysqlConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            mysqlConfig.createDBConnection());
        logOk("Connected to MySQL");

        runAllDemonstrations(mysqlConn);

        // Demonstrate connection errors (these don't require an existing connection)
        demonstrateConnectionErrors();

        logStep("Closing MySQL connection...");
        mysqlConn->close();
        logOk("MySQL connection closed");
    }
    catch (const std::exception &e)
    {
        logError("Unhandled error: " + std::string(e.what()));
        return 1;
    }

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return EXIT_OK_;
#endif
}
