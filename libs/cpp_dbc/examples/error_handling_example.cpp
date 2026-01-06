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

 @file error_handling_example.cpp
 @brief Example demonstrating database error handling with cpp_dbc

*/

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif
#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>
#include <filesystem>
#include <fstream>

// Include the YAML loader if YAML support is enabled
#ifdef USE_CPP_YAML
#include <cpp_dbc/config/yaml_config_loader.hpp>
#endif

namespace fs = std::filesystem;

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
        std::cout << "\n=== Executing: " << operationName << " ===" << std::endl;
        operation();
        std::cout << "Operation completed successfully." << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error in " << operationName << ": " << e.what_s() << std::endl;
        e.printCallStack();
    }
    catch (const AppException &e)
    {
        std::cerr << "Application error in " << operationName << ": " << e.what() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Standard exception in " << operationName << ": " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "Unknown error in " << operationName << std::endl;
    }
}

// Helper function to get the executable path and name
std::string getExecutablePathAndName()
{
    std::vector<char> buffer(2048);
    ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (len != -1)
    {
        buffer[len] = '\0';
        return std::string(buffer.data());
    }
    return {};
}

// Helper function to get only the executable path
std::string getOnlyExecutablePath()
{
    fs::path p(getExecutablePathAndName());
    return p.parent_path().string() + "/"; // Returns only the directory with trailing slash
}

// Helper function to get the path to the config file
std::string getConfigFilePath()
{
    // The example_config.yml file is expected to be in the same directory as the executable
    return getOnlyExecutablePath() + "example_config.yml";
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
    executeWithErrorHandling("Syntax Error Example", [&conn]()
                             {
        // Intentional syntax error in SQL query
        conn->executeQuery("SELCT * FROM error_test_customers"); });
}

// Function to demonstrate handling constraint violations
void demonstrateConstraintViolations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    // Primary key violation
    executeWithErrorHandling("Primary Key Violation", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "INSERT INTO error_test_customers (customer_id, name, email, credit_limit) "
            "VALUES (?, ?, ?, ?)"
        );
        
        // Try to insert a customer with an existing ID (violates primary key constraint)
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
        
        // Try to insert a customer with an existing email (violates unique constraint)
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
        
        // Try to insert a customer with negative credit limit (violates check constraint)
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
        
        // Try to insert an order with a non-existent customer ID (violates foreign key constraint)
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
        
        // Try to insert a customer with null name (violates not null constraint)
        pstmt->setInt(1, 5); // New ID
        pstmt->setNull(2, cpp_dbc::Types::VARCHAR); // Null name
        pstmt->setString(3, "null@example.com");
        pstmt->setDouble(4, 500.00);
        pstmt->executeUpdate(); });
}

// Function to demonstrate handling data type errors
void demonstrateDataTypeErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    // Type conversion error
    executeWithErrorHandling("Type Conversion Error", [&conn]()
                             {
        // Try to convert a string to an integer
        auto rs = conn->executeQuery("SELECT 'abc' + 123 FROM error_test_customers");
        rs->next(); });

    // Invalid date format
    executeWithErrorHandling("Invalid Date Format", [&conn]()
                             {
        // Try to use an invalid date format
        conn->executeQuery("SELECT * FROM error_test_customers WHERE customer_id = '2023-13-32'"); });

    // Numeric overflow
    executeWithErrorHandling("Numeric Overflow", [&conn]()
                             {
        // Try to perform a calculation that causes numeric overflow
        auto rs = conn->executeQuery("SELECT 9999999999999999999999999999 * 9999999999999999999999999999 FROM error_test_customers");
        rs->next(); });
}

// Function to demonstrate handling transaction errors
void demonstrateTransactionErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    // Transaction rollback example
    executeWithErrorHandling("Transaction Rollback", [&conn]()
                             {
        // Start a transaction
        conn->setAutoCommit(false);
        
        try {
            // First operation succeeds
            auto pstmt1 = conn->prepareStatement(
                "INSERT INTO error_test_customers (customer_id, name, email, credit_limit) "
                "VALUES (?, ?, ?, ?)"
            );
            
            pstmt1->setInt(1, 10);
            pstmt1->setString(2, "Transaction Test");
            pstmt1->setString(3, "transaction@example.com");
            pstmt1->setDouble(4, 1000.00);
            pstmt1->executeUpdate();
            
            std::cout << "First operation in transaction succeeded." << std::endl;
            
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
            
            // This line should not be reached
            conn->commit();
        }
        catch (const cpp_dbc::DBException& e) {
            std::cerr << "Error in transaction: " << e.what_s() << std::endl;
            e.getCallStack();
            std::cerr << "Rolling back transaction..." << std::endl;
            conn->rollback();
            
            // Verify the rollback worked (customer ID 10 should not exist)
            auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM error_test_customers WHERE customer_id = 10");
            rs->next();
            int count = rs->getInt("count");
            std::cout << "After rollback, customer ID 10 count: " << count << std::endl;
            
            if (count > 0) {
                throw AppException("Transaction rollback failed!");
            }
        }
        
        // Restore auto-commit mode
        conn->setAutoCommit(true); });

    // Deadlock simulation (would require multiple connections)
    std::cout << "\n=== Deadlock Simulation ===" << std::endl;
    std::cout << "Note: A true deadlock simulation would require multiple concurrent connections." << std::endl;
    std::cout << "In a real application, you would need to handle deadlock errors by retrying the transaction." << std::endl;
}

// Function to demonstrate handling connection errors
void demonstrateConnectionErrors()
{
    executeWithErrorHandling("Connection Error", []()
                             {
        // Try to connect with invalid credentials
        auto conn = cpp_dbc::DriverManager::getDBConnection(
            "cpp_dbc:mysql://localhost:3306/nonexistent_db",
            "invalid_user",
            "invalid_password"
        ); });

    executeWithErrorHandling("Invalid Connection URL", []()
                             {
        // Try to connect with an invalid URL format
        auto conn = cpp_dbc::DriverManager::getDBConnection(
            "invalid:url:format",
            "user",
            "password"
        ); });
}

// Function to demonstrate handling prepared statement errors
void demonstratePreparedStatementErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    // Invalid parameter index
    executeWithErrorHandling("Invalid Parameter Index", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "SELECT * FROM error_test_customers WHERE customer_id = ?"
        );
        
        // Try to set a parameter with an invalid index
        pstmt->setInt(2, 1); // Only parameter 1 is valid
        pstmt->executeQuery(); });

    // Parameter type mismatch
    executeWithErrorHandling("Parameter Type Mismatch", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "SELECT * FROM error_test_customers WHERE customer_id = ?"
        );
        
        // Try to use a string for an integer parameter
        pstmt->setString(1, "not_an_integer");
        pstmt->executeQuery(); });

    // Missing parameter
    executeWithErrorHandling("Missing Parameter", [&conn]()
                             {
        auto pstmt = conn->prepareStatement(
            "SELECT * FROM error_test_customers WHERE customer_id = ? AND name = ?"
        );
        
        // Only set the first parameter, leaving the second one unset
        pstmt->setInt(1, 1);
        pstmt->executeQuery(); });
}

// Function to demonstrate handling result set errors
void demonstrateResultSetErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    // Invalid column name
    executeWithErrorHandling("Invalid Column Name", [&conn]()
                             {
        auto rs = conn->executeQuery("SELECT * FROM error_test_customers");
        rs->next();
        
        // Try to access a non-existent column
        std::string value = rs->getString("non_existent_column"); });

    // Type conversion error in result set
    executeWithErrorHandling("Result Set Type Conversion Error", [&conn]()
                             {
                                 auto rs = conn->executeQuery("SELECT name FROM error_test_customers");
                                 rs->next();

                                 // Try to get a string column as an integer
                                 // int value = 
                                 rs->getInt("name"); });

    // Accessing result set after it's closed
    executeWithErrorHandling("Closed Result Set Access", [&conn]()
                             {
        auto rs = conn->executeQuery("SELECT * FROM error_test_customers");
        rs->close();
        
        // Try to access the result set after it's closed
        rs->next(); });
}

// Function to demonstrate proper error recovery
void demonstrateErrorRecovery(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== Error Recovery Example ===" << std::endl;

    try
    {
        // Try an operation that will fail
        std::cout << "Attempting an operation that will fail..." << std::endl;
        conn->executeUpdate("INSERT INTO error_test_customers (customer_id) VALUES (1)"); // Missing required fields
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Expected error occurred: " << e.what_s() << std::endl;

        // Recover by performing a valid operation
        std::cout << "Recovering by performing a valid operation..." << std::endl;

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
                std::cout << "Connection is no longer valid. Reconnecting..." << std::endl;
                // In a real application, you would reconnect here
            }

            // Perform a valid operation
            auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM error_test_customers");
            rs->next();
            int count = rs->getInt("count");
            std::cout << "Recovery successful. Customer count: " << count << std::endl;
        }
        catch (const cpp_dbc::DBException &recoverError)
        {
            std::cerr << "Recovery failed: " << recoverError.what_s() << std::endl;
        }
    }
}

// Function to demonstrate custom error handling and logging
void demonstrateCustomErrorHandling(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== Custom Error Handling Example ===" << std::endl;

    // Define a custom error handler function
    auto logError = [](const std::string &operation, const std::exception &e)
    {
        std::cerr << "ERROR LOG: [" << operation << "] " << e.what() << std::endl;
        // In a real application, you would log to a file or logging service
    };

    // Define a function that uses the custom error handler
    auto executeWithLogging = [&logError, &conn](const std::string &sql, const std::string &operation)
    {
        try
        {
            std::cout << "Executing: " << operation << std::endl;
            conn->executeUpdate(sql);
            std::cout << "Operation completed successfully." << std::endl;
        }
        catch (const cpp_dbc::DBException &e)
        {
            logError(operation, e);

            // Analyze error message to categorize the error
            std::string errorMsg = e.what_s();
            if (errorMsg.find("constraint") != std::string::npos ||
                errorMsg.find("CONSTRAINT") != std::string::npos ||
                errorMsg.find("duplicate") != std::string::npos ||
                errorMsg.find("UNIQUE") != std::string::npos)
            {
                std::cerr << "Constraint violation detected." << std::endl;
            }
            else if (errorMsg.find("syntax") != std::string::npos ||
                     errorMsg.find("SYNTAX") != std::string::npos)
            {
                std::cerr << "Syntax error or access rule violation detected." << std::endl;
            }
            else if (errorMsg.find("connect") != std::string::npos ||
                     errorMsg.find("CONNECTION") != std::string::npos)
            {
                std::cerr << "Connection error detected." << std::endl;
            }
            else
            {
                std::cerr << "Other database error detected." << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            logError(operation, e);
            std::cerr << "Standard exception detected." << std::endl;
        }
    };

    // Test the custom error handler with different types of errors
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

// Function to demonstrate handling database-specific errors
void demonstrateDatabaseSpecificErrors(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn, const std::string &dbType)
{
    std::cout << "\n=== Database-Specific Error Handling for " << dbType << " ===" << std::endl;

    if (dbType == "MySQL")
    {
        // MySQL-specific error handling
        executeWithErrorHandling("MySQL-Specific Error", [&conn]()
                                 {
            // Try to create a table with an invalid engine
            conn->executeUpdate(
                "CREATE TABLE invalid_engine_table (id INT) ENGINE=INVALID_ENGINE"
            ); });

        executeWithErrorHandling("MySQL Max Connections Error Simulation", []()
                                 {
            std::cout << "Note: In a real application, you would handle 'Too many connections' errors (MySQL error 1040)" << std::endl;
            std::cout << "by implementing connection pooling and retry logic." << std::endl; });
    }
    else if (dbType == "PostgreSQL")
    {
        // PostgreSQL-specific error handling
        executeWithErrorHandling("PostgreSQL-Specific Error", [&conn]()
                                 {
            // Try to use a PostgreSQL-specific feature
            conn->executeUpdate(
                "CREATE TABLE invalid_table WITH (fillfactor=invalid_value) AS SELECT 1"
            ); });

        executeWithErrorHandling("PostgreSQL Advisory Lock Simulation", []()
                                 {
            std::cout << "Note: In a real application, you would handle advisory lock conflicts" << std::endl;
            std::cout << "by implementing retry logic or alternative locking strategies." << std::endl; });
    }
    else if (dbType == "SQLite")
    {
        // SQLite-specific error handling
        executeWithErrorHandling("SQLite-Specific Error", [&conn]()
                                 {
            // Try to use a SQLite-specific feature incorrectly
            conn->executeUpdate(
                "PRAGMA invalid_pragma=1"
            ); });

        executeWithErrorHandling("SQLite Busy Error Simulation", []()
                                 {
            std::cout << "Note: In a real application, you would handle 'database is locked' errors" << std::endl;
            std::cout << "by implementing retry logic with exponential backoff." << std::endl; });
    }
}

int main()
{
    try
    {
        // Register database drivers
#if USE_MYSQL
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
#endif
#if USE_POSTGRESQL
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());
#endif
#if USE_SQLITE
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());
#endif

        // Load configuration from YAML file
        cpp_dbc::config::DatabaseConfigManager configManager;
        std::string configFile = getConfigFilePath();

        // std::cout << "Config file path: " << configFile << std::endl;

        std::cout << "Loading configuration from: " << configFile << std::endl;

#ifdef USE_CPP_YAML
        try
        {
            configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(configFile);
            std::cout << "Configuration loaded successfully." << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error loading configuration: " << e.what() << std::endl;
            std::cerr << "Will use hardcoded connection parameters if needed." << std::endl;
        }
#else
        std::cout << "YAML support is not enabled. Will use hardcoded connection parameters." << std::endl;
#endif

#if USE_MYSQL
        try
        {
            std::string connectionString;
            std::string username;
            std::string password;

            // Try to get MySQL configuration from YAML
            auto mysqlConfigOpt = configManager.getDatabaseByName("dev_mysql");
            if (mysqlConfigOpt)
            {
                std::cout << "Using MySQL configuration from YAML file." << std::endl;
                const auto &mysqlConfig = mysqlConfigOpt->get();
                connectionString = mysqlConfig.createConnectionString();
                username = mysqlConfig.getUsername();
                password = mysqlConfig.getPassword();
            }
            else
            {
                // Fallback to hardcoded values
                std::cout << "MySQL configuration not found. Using hardcoded values." << std::endl;
                connectionString = "cpp_dbc:mysql://localhost:3306/testdb";
                username = "username";
                password = "password";
            }

            // Connect to MySQL
            std::cout << "Connecting to MySQL..." << std::endl;
            std::cout << "Connection String: " << connectionString << std::endl;

            auto mysqlConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(
                connectionString,
                username,
                password));

            // Set up the test database
            setupDatabase(mysqlConn);

            // Demonstrate different types of errors and error handling
            demonstrateSyntaxErrors(mysqlConn);
            demonstrateConstraintViolations(mysqlConn);
            demonstrateDataTypeErrors(mysqlConn);
            demonstrateTransactionErrors(mysqlConn);
            demonstratePreparedStatementErrors(mysqlConn);
            demonstrateResultSetErrors(mysqlConn);
            demonstrateErrorRecovery(mysqlConn);
            demonstrateCustomErrorHandling(mysqlConn);
            demonstrateDatabaseSpecificErrors(mysqlConn, "MySQL");

            // Clean up
            mysqlConn->executeUpdate("DROP TABLE IF EXISTS error_test_orders");
            mysqlConn->executeUpdate("DROP TABLE IF EXISTS error_test_customers");

            // Close MySQL connection
            mysqlConn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::cerr << e.what_s() << std::endl;
            e.printCallStack();
        }
#else
        std::cout << "MySQL support is not enabled." << std::endl;
#endif

#if USE_POSTGRESQL
        try
        {
            std::string connectionString;
            std::string username;
            std::string password;

            // Try to get PostgreSQL configuration from YAML
            auto pgConfigOpt = configManager.getDatabaseByName("dev_postgresql");
            if (pgConfigOpt)
            {
                std::cout << "Using PostgreSQL configuration from YAML file." << std::endl;
                const auto &pgConfig = pgConfigOpt->get();
                connectionString = pgConfig.createConnectionString();
                username = pgConfig.getUsername();
                password = pgConfig.getPassword();
            }
            else
            {
                // Fallback to hardcoded values
                std::cout << "PostgreSQL configuration not found. Using hardcoded values." << std::endl;
                connectionString = "cpp_dbc:postgresql://localhost:5432/testdb";
                username = "username";
                password = "password";
            }

            // Connect to PostgreSQL
            std::cout << "\nConnecting to PostgreSQL..." << std::endl;
            std::cout << "Connection String: " << connectionString << std::endl;

            auto pgConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(
                connectionString,
                username,
                password));

            // Set up the test database
            setupDatabase(pgConn);

            // Demonstrate different types of errors and error handling
            demonstrateSyntaxErrors(pgConn);
            demonstrateConstraintViolations(pgConn);
            demonstrateDataTypeErrors(pgConn);
            demonstrateTransactionErrors(pgConn);
            demonstratePreparedStatementErrors(pgConn);
            demonstrateResultSetErrors(pgConn);
            demonstrateErrorRecovery(pgConn);
            demonstrateCustomErrorHandling(pgConn);
            demonstrateDatabaseSpecificErrors(pgConn, "PostgreSQL");

            // Clean up
            pgConn->executeUpdate("DROP TABLE IF EXISTS error_test_orders");
            pgConn->executeUpdate("DROP TABLE IF EXISTS error_test_customers");

            // Close PostgreSQL connection
            pgConn->close();
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::cerr << e.what_s() << std::endl;
            e.printCallStack();
        }
#else
        std::cout << "PostgreSQL support is not enabled." << std::endl;
#endif

        // Demonstrate connection errors (these don't require an existing connection)
        demonstrateConnectionErrors();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Unhandled error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}