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
 * @file firebird_example.cpp
 * @brief Example demonstrating basic Firebird database operations
 *
 * This example demonstrates how to connect to a Firebird database and perform
 * basic CRUD operations (Create, Read, Update, Delete) using the cpp_dbc library.
 *
 * Build and run:
 *   cd build
 *   cmake .. -DUSE_FIREBIRD=ON -DCPP_DBC_BUILD_EXAMPLES=ON
 *   make firebird_example
 *   ./firebird_example
 */

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif
#include <iostream>
#include <memory>
#include <iomanip>
#include <any>

// Database configuration - update these values based on your setup
const std::string FIREBIRD_HOST = "localhost";
const int FIREBIRD_PORT = 3050;
const std::string FIREBIRD_DATABASE = "/firebird/data/example.fdb";
const std::string FIREBIRD_USER = "SYSDBA";
const std::string FIREBIRD_PASSWORD = "masterkey";

#if USE_FIREBIRD

/**
 * @brief Try to create the database if it doesn't exist
 * @param driver The Firebird driver instance
 * @param url The database URL
 * @return true if database exists or was created successfully
 */
bool tryCreateDatabase(std::shared_ptr<cpp_dbc::DBDriver> driver, const std::string &url)
{
    // First, try to connect to see if database already exists
    try
    {
        auto conn = cpp_dbc::DriverManager::getDBConnection(url, FIREBIRD_USER, FIREBIRD_PASSWORD);
        std::cout << "Database exists and connection successful!" << std::endl;
        conn->close();
        return true;
    }
    catch (const std::exception &e)
    {
        // Database doesn't exist, try to create it
        std::cout << "Database doesn't exist: " << e.what() << std::endl;
        std::cout << "Attempting to create it..." << std::endl;
    }

    try
    {
        // Use the driver's command method to create the database
        std::map<std::string, std::any> params = {
            {"command", std::string("create_database")},
            {"url", url},
            {"user", FIREBIRD_USER},
            {"password", FIREBIRD_PASSWORD},
            {"page_size", std::string("4096")},
            {"charset", std::string("UTF8")}};

        driver->command(params);
        std::cout << "Database created successfully!" << std::endl;
        return true;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Failed to create database: " << e.what_s() << std::endl;
        std::cerr << std::endl;
        std::cerr << "To fix this, you may need to:" << std::endl;
        std::cerr << "1. Ensure the directory exists and is writable by the Firebird server" << std::endl;
        std::cerr << "   sudo mkdir -p /firebird/data" << std::endl;
        std::cerr << "   sudo chown firebird:firebird /firebird/data" << std::endl;
        std::cerr << "2. Configure Firebird to allow database creation in the target directory" << std::endl;
        std::cerr << "   Edit /etc/firebird/3.0/firebird.conf (or similar path)" << std::endl;
        std::cerr << "   Set: DatabaseAccess = Full" << std::endl;
        std::cerr << "3. Restart Firebird: sudo systemctl restart firebird3.0" << std::endl;
        return false;
    }
}

// Helper function to print result set
void printResults(std::shared_ptr<cpp_dbc::RelationalDBResultSet> rs)
{
    // Get column names
    auto columnNames = rs->getColumnNames();

    // Print header
    for (const auto &column : columnNames)
    {
        std::cout << std::setw(15) << std::left << column;
    }
    std::cout << std::endl;

    // Print separator
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        std::cout << std::string(15, '-');
    }
    std::cout << std::endl;

    // Print data
    while (rs->next())
    {
        for (const auto &column : columnNames)
        {
            std::cout << std::setw(15) << std::left << rs->getString(column);
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

// Demonstrate basic CRUD operations
void demonstrateBasicOperations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== Basic CRUD Operations ===" << std::endl;

    try
    {
        // Create a products table
        std::cout << "Creating products table..." << std::endl;
        conn->executeUpdate("DROP TABLE IF EXISTS products");
        conn->executeUpdate(
            "CREATE TABLE products ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "name VARCHAR(100) NOT NULL, "
            "price NUMERIC(10,2) NOT NULL, "
            "description VARCHAR(500), "
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")");

        // Insert data using a prepared statement
        std::cout << "Inserting data..." << std::endl;
        auto prepStmt = conn->prepareStatement(
            "INSERT INTO products (id, name, price, description) VALUES (?, ?, ?, ?)");

        // Insert product 1
        prepStmt->setInt(1, 1);
        prepStmt->setString(2, "Firebird Database Server");
        prepStmt->setDouble(3, 0.00); // It's free!
        prepStmt->setString(4, "Open source SQL relational database management system");
        prepStmt->executeUpdate();

        // Insert product 2
        prepStmt->setInt(1, 2);
        prepStmt->setString(2, "cpp_dbc Library");
        prepStmt->setDouble(3, 0.00); // Also free!
        prepStmt->setString(4, "C++ Database Connectivity Library");
        prepStmt->executeUpdate();

        // Insert product 3
        prepStmt->setInt(1, 3);
        prepStmt->setString(2, "Custom Database Solution");
        prepStmt->setDouble(3, 999.99);
        prepStmt->setString(4, "Enterprise-grade database solution with support");
        prepStmt->executeUpdate();

        // Close the prepared statement (important for Firebird)
        prepStmt->close();

        // Select all products
        std::cout << "\nQuery 1: Select all products" << std::endl;
        auto rs = conn->executeQuery("SELECT * FROM products ORDER BY id");
        printResults(rs);

        // Select with a WHERE clause
        std::cout << "Query 2: Select free products" << std::endl;
        rs = conn->executeQuery("SELECT id, name, price FROM products WHERE price = 0.00");
        printResults(rs);

        // Update a record
        std::cout << "Updating product with ID 3..." << std::endl;
        conn->executeUpdate(
            "UPDATE products SET price = 1299.99, description = 'Premium enterprise-grade database solution with 24/7 support' "
            "WHERE id = 3");

        // Verify the update
        std::cout << "Query 3: Verify update" << std::endl;
        rs = conn->executeQuery("SELECT * FROM products WHERE id = 3");
        printResults(rs);

        // Delete a record
        std::cout << "Deleting product with ID 2..." << std::endl;
        conn->executeUpdate("DELETE FROM products WHERE id = 2");

        // Verify the delete
        std::cout << "Query 4: Verify delete and show remaining products" << std::endl;
        rs = conn->executeQuery("SELECT * FROM products ORDER BY id");
        printResults(rs);

        // Drop the table when done
        conn->executeUpdate("DROP TABLE products");
        std::cout << "Table dropped successfully." << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Error in basic operations: " << e.what_s() << std::endl;
    }
}

// Demonstrate Firebird-specific features
void demonstrateFirebirdFeatures(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== Firebird-Specific Features ===" << std::endl;

    try
    {
        // 1. Create a table with a generator/sequence
        std::cout << "Creating a table with auto-increment via generator..." << std::endl;

        // Drop existing objects if they exist
        try
        {
            conn->executeUpdate("DROP TABLE auto_increment_test");
        }
        catch (...)
        {
        }
        try
        {
            conn->executeUpdate("DROP SEQUENCE product_id_seq");
        }
        catch (...)
        {
        }

        // Create sequence
        conn->executeUpdate("CREATE SEQUENCE product_id_seq");

        // Create table
        conn->executeUpdate(
            "CREATE TABLE auto_increment_test ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "name VARCHAR(100) NOT NULL"
            ")");

        // Create trigger for auto-increment
        conn->executeUpdate(
            "CREATE TRIGGER auto_increment_test_bi FOR auto_increment_test "
            "ACTIVE BEFORE INSERT POSITION 0 AS "
            "BEGIN "
            "    IF (NEW.ID IS NULL) THEN "
            "        NEW.ID = NEXT VALUE FOR product_id_seq; "
            "END");

        std::cout << "Inserting data with auto-increment..." << std::endl;

        // Insert with auto-increment
        conn->executeUpdate("INSERT INTO auto_increment_test (name) VALUES ('Product A')");
        conn->executeUpdate("INSERT INTO auto_increment_test (name) VALUES ('Product B')");
        conn->executeUpdate("INSERT INTO auto_increment_test (name) VALUES ('Product C')");

        // Select results
        std::cout << "\nQuery: Auto-increment results" << std::endl;
        auto rs = conn->executeQuery("SELECT * FROM auto_increment_test ORDER BY id");
        printResults(rs);

        // 2. Demonstrate stored procedures
        std::cout << "\nCreating a stored procedure..." << std::endl;
        try
        {
            conn->executeUpdate("DROP PROCEDURE get_product_by_id");
        }
        catch (...)
        {
        }

        conn->executeUpdate(
            "CREATE PROCEDURE get_product_by_id (id_param INTEGER) "
            "RETURNS (id INTEGER, name VARCHAR(100)) AS "
            "BEGIN "
            "    FOR SELECT id, name FROM auto_increment_test WHERE id = :id_param INTO :id, :name DO "
            "    SUSPEND; "
            "END");

        std::cout << "Calling stored procedure..." << std::endl;
        rs = conn->executeQuery("SELECT * FROM get_product_by_id(2)");
        printResults(rs);

        // 3. Demonstrate transaction isolation levels
        std::cout << "\nDemonstrating transaction isolation levels..." << std::endl;
        conn->close(); // Close the current connection

        // Connect with READ COMMITTED isolation
        std::string url = "cpp_dbc:firebird://" + FIREBIRD_HOST + ":" +
                          std::to_string(FIREBIRD_PORT) + FIREBIRD_DATABASE;

        std::cout << "Connecting with READ COMMITTED isolation..." << std::endl;
        auto connRC = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(url + "?isolation=read_committed", FIREBIRD_USER, FIREBIRD_PASSWORD));

        std::cout << "Testing READ COMMITTED isolation..." << std::endl;
        rs = connRC->executeQuery("SELECT * FROM auto_increment_test WHERE id = 1");
        printResults(rs);

        // Clean up
        connRC->executeUpdate("DROP PROCEDURE get_product_by_id");
        connRC->executeUpdate("DROP TABLE auto_increment_test");
        connRC->executeUpdate("DROP SEQUENCE product_id_seq");
        std::cout << "Objects dropped successfully." << std::endl;

        connRC->close();
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Error in Firebird features: " << e.what_s() << std::endl;
    }
}

#endif // USE_FIREBIRD

int main()
{
#if USE_FIREBIRD
    try
    {
        std::cout << "=== Firebird Database Example ===" << std::endl;
        std::cout << "This example demonstrates basic operations with Firebird." << std::endl;

        // Create and register Firebird driver
        auto firebirdDriver = std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>();
        cpp_dbc::DriverManager::registerDriver("firebird", firebirdDriver);

        // Build connection URL
        std::string url = "cpp_dbc:firebird://" + FIREBIRD_HOST + ":" +
                          std::to_string(FIREBIRD_PORT) + FIREBIRD_DATABASE;

        std::cout << "\nConnecting to Firebird..." << std::endl;
        std::cout << "URL: " << url << std::endl;
        std::cout << "User: " << FIREBIRD_USER << std::endl;

        // Try to create the database if it doesn't exist
        if (!tryCreateDatabase(firebirdDriver, url))
        {
            std::cerr << "Failed to create or connect to database." << std::endl;
            return 1;
        }

        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(url, FIREBIRD_USER, FIREBIRD_PASSWORD));

        std::cout << "Connected successfully!" << std::endl;

        // Run the demonstrations
        demonstrateBasicOperations(conn);
        demonstrateFirebirdFeatures(conn);

        // Close connection
        conn->close();
        std::cout << "\n=== Example completed successfully ===" << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what_s() << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cerr << "Firebird support is not enabled. Build with -DUSE_FIREBIRD=ON" << std::endl;
    return 1;
#endif

    return 0;
}