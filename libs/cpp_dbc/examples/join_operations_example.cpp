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

 @file join_operations_example.cpp
 @brief Example demonstrating SQL JOIN operations with cpp_dbc

*/

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#if USE_SQLITE
#include <cpp_dbc/drivers/driver_sqlite.hpp>
#endif
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <iomanip>
#include <functional>

// Helper function to print query results
void printResults(std::shared_ptr<cpp_dbc::ResultSet> rs)
{
    // Get column names
    auto columnNames = rs->getColumnNames();

    // Calculate column widths (minimum 15 characters)
    std::vector<size_t> columnWidths(columnNames.size(), 15);

    // Print header
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        std::cout << std::setw(columnWidths[i]) << std::left << columnNames[i] << " | ";
    }
    std::cout << std::endl;

    // Print separator
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        std::cout << std::string(columnWidths[i], '-') << "-|-";
    }
    std::cout << std::endl;

    // Print data
    int rowCount = 0;
    while (rs->next())
    {
        for (const auto &column : columnNames)
        {
            std::string value;
            if (rs->isNull(column))
            {
                value = "NULL";
            }
            else
            {
                value = rs->getString(column);
            }
            std::cout << std::setw(columnWidths[columnNames.size() - 1]) << std::left << value << " | ";
        }
        std::cout << std::endl;
        rowCount++;
    }

    std::cout << rowCount << " row(s) returned" << std::endl
              << std::endl;
}

// Function to set up test database schema and data
void setupDatabase(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "Setting up test database schema and data..." << std::endl;

    // Drop existing tables if they exist
    conn->executeUpdate("DROP TABLE IF EXISTS orders");
    conn->executeUpdate("DROP TABLE IF EXISTS customers");
    conn->executeUpdate("DROP TABLE IF EXISTS products");

    // Create customers table
    conn->executeUpdate(
        "CREATE TABLE customers ("
        "customer_id INT PRIMARY KEY, "
        "name VARCHAR(100), "
        "email VARCHAR(100), "
        "city VARCHAR(50), "
        "country VARCHAR(50), "
        "registration_date DATE"
        ")");

    // Create products table
    conn->executeUpdate(
        "CREATE TABLE products ("
        "product_id INT PRIMARY KEY, "
        "name VARCHAR(100), "
        "category VARCHAR(50), "
        "price DECIMAL(10,2), "
        "stock_quantity INT"
        ")");

    // Create orders table
    conn->executeUpdate(
        "CREATE TABLE orders ("
        "order_id INT PRIMARY KEY, "
        "customer_id INT, "
        "product_id INT, "
        "order_date DATE, "
        "quantity INT, "
        "total_price DECIMAL(10,2)"
        ")");

    // Insert data into customers table
    auto customerStmt = conn->prepareStatement(
        "INSERT INTO customers (customer_id, name, email, city, country, registration_date) "
        "VALUES (?, ?, ?, ?, ?, ?)");

    std::vector<std::tuple<int, std::string, std::string, std::string, std::string, std::string>> customers = {
        {1, "John Smith", "john@example.com", "New York", "USA", "2022-01-15"},
        {2, "Maria Garcia", "maria@example.com", "Madrid", "Spain", "2022-02-20"},
        {3, "Hiroshi Tanaka", "hiroshi@example.com", "Tokyo", "Japan", "2022-03-10"},
        {4, "Sophie Dubois", "sophie@example.com", "Paris", "France", "2022-04-05"},
        {5, "Li Wei", "li@example.com", "Beijing", "China", "2022-05-12"},
        {6, "Ahmed Hassan", "ahmed@example.com", "Cairo", "Egypt", "2022-06-18"}};

    for (const auto &customer : customers)
    {
        customerStmt->setInt(1, std::get<0>(customer));
        customerStmt->setString(2, std::get<1>(customer));
        customerStmt->setString(3, std::get<2>(customer));
        customerStmt->setString(4, std::get<3>(customer));
        customerStmt->setString(5, std::get<4>(customer));
        customerStmt->setString(6, std::get<5>(customer));
        customerStmt->executeUpdate();
    }

    // Insert data into products table
    auto productStmt = conn->prepareStatement(
        "INSERT INTO products (product_id, name, category, price, stock_quantity) "
        "VALUES (?, ?, ?, ?, ?)");

    std::vector<std::tuple<int, std::string, std::string, double, int>> products = {
        {101, "Laptop Pro", "Electronics", 1299.99, 50},
        {102, "Smartphone X", "Electronics", 799.99, 100},
        {103, "Coffee Maker", "Home Appliances", 89.99, 30},
        {104, "Running Shoes", "Sportswear", 129.99, 75},
        {105, "Desk Chair", "Furniture", 199.99, 25},
        {106, "Wireless Headphones", "Electronics", 149.99, 60},
        {107, "Blender", "Home Appliances", 69.99, 40}};

    for (const auto &product : products)
    {
        productStmt->setInt(1, std::get<0>(product));
        productStmt->setString(2, std::get<1>(product));
        productStmt->setString(3, std::get<2>(product));
        productStmt->setDouble(4, std::get<3>(product));
        productStmt->setInt(5, std::get<4>(product));
        productStmt->executeUpdate();
    }

    // Insert data into orders table
    auto orderStmt = conn->prepareStatement(
        "INSERT INTO orders (order_id, customer_id, product_id, order_date, quantity, total_price) "
        "VALUES (?, ?, ?, ?, ?, ?)");

    std::vector<std::tuple<int, int, int, std::string, int, double>> orders = {
        {1001, 1, 101, "2023-01-10", 1, 1299.99},
        {1002, 1, 106, "2023-01-10", 1, 149.99},
        {1003, 2, 102, "2023-01-15", 1, 799.99},
        {1004, 3, 104, "2023-01-20", 2, 259.98},
        {1005, 4, 103, "2023-01-25", 1, 89.99},
        {1006, 4, 107, "2023-01-25", 1, 69.99},
        {1007, 5, 105, "2023-02-05", 1, 199.99},
        {1008, 1, 102, "2023-02-10", 1, 799.99}
        // Note: Customer 6 has no orders (for LEFT JOIN example)
        // Note: Product 103 has no orders (for RIGHT JOIN example)
    };

    for (const auto &order : orders)
    {
        orderStmt->setInt(1, std::get<0>(order));
        orderStmt->setInt(2, std::get<1>(order));
        orderStmt->setInt(3, std::get<2>(order));
        orderStmt->setString(4, std::get<3>(order));
        orderStmt->setInt(5, std::get<4>(order));
        orderStmt->setDouble(6, std::get<5>(order));
        orderStmt->executeUpdate();
    }

    std::cout << "Database setup completed." << std::endl;
}

// Function to demonstrate INNER JOIN
void demonstrateInnerJoin(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== INNER JOIN Example ===\n"
              << std::endl;
    std::cout << "INNER JOIN returns only the rows where there is a match in both tables." << std::endl;
    std::cout << "Query: Get all customers who have placed orders, along with their order details." << std::endl;

    std::string query =
        "SELECT c.customer_id, c.name, o.order_id, o.order_date, o.total_price "
        "FROM customers c "
        "INNER JOIN orders o ON c.customer_id = o.customer_id "
        "ORDER BY c.customer_id, o.order_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
}

// Function to demonstrate LEFT JOIN
void demonstrateLeftJoin(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== LEFT JOIN Example ===\n"
              << std::endl;
    std::cout << "LEFT JOIN returns all rows from the left table and matching rows from the right table." << std::endl;
    std::cout << "If there is no match, NULL values are returned for the right table columns." << std::endl;
    std::cout << "Query: Get all customers and their orders (if any)." << std::endl;

    std::string query =
        "SELECT c.customer_id, c.name, o.order_id, o.order_date, o.total_price "
        "FROM customers c "
        "LEFT JOIN orders o ON c.customer_id = o.customer_id "
        "ORDER BY c.customer_id, o.order_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
}

// Function to demonstrate RIGHT JOIN
void demonstrateRightJoin(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== RIGHT JOIN Example ===\n"
              << std::endl;
    std::cout << "RIGHT JOIN returns all rows from the right table and matching rows from the left table." << std::endl;
    std::cout << "If there is no match, NULL values are returned for the left table columns." << std::endl;
    std::cout << "Query: Get all products and their orders (if any)." << std::endl;

    std::string query =
        "SELECT p.product_id, p.name, p.category, o.order_id, o.customer_id, o.quantity "
        "FROM orders o "
        "RIGHT JOIN products p ON o.product_id = p.product_id "
        "ORDER BY p.product_id, o.order_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
}

// Function to demonstrate FULL JOIN (not supported in MySQL, simulated with UNION)
void demonstrateFullJoin(std::shared_ptr<cpp_dbc::Connection> conn, bool isMySQL)
{
    std::cout << "\n=== FULL JOIN Example ===\n"
              << std::endl;
    std::cout << "FULL JOIN returns all rows when there is a match in either the left or right table." << std::endl;
    std::cout << "If there is no match, NULL values are returned for the columns of the table without a match." << std::endl;
    std::cout << "Query: Get all customers and all products, showing all possible combinations that exist in orders." << std::endl;

    std::string query;

    if (isMySQL)
    {
        // MySQL doesn't support FULL JOIN directly, so we simulate it with UNION
        std::cout << "(Note: MySQL doesn't support FULL JOIN directly, using LEFT JOIN UNION RIGHT JOIN instead)" << std::endl;
        query =
            "SELECT c.customer_id, c.name, p.product_id, p.name AS product_name, o.order_id, o.quantity "
            "FROM customers c "
            "LEFT JOIN orders o ON c.customer_id = o.customer_id "
            "LEFT JOIN products p ON o.product_id = p.product_id "
            "UNION "
            "SELECT c.customer_id, c.name, p.product_id, p.name AS product_name, o.order_id, o.quantity "
            "FROM products p "
            "LEFT JOIN orders o ON p.product_id = o.product_id "
            "LEFT JOIN customers c ON o.customer_id = c.customer_id "
            "WHERE c.customer_id IS NULL "
            "ORDER BY customer_id, product_id";
    }
    else
    {
        // PostgreSQL and some other databases support FULL JOIN
        query =
            "SELECT c.customer_id, c.name, p.product_id, p.name AS product_name, o.order_id, o.quantity "
            "FROM customers c "
            "FULL JOIN orders o ON c.customer_id = o.customer_id "
            "FULL JOIN products p ON o.product_id = p.product_id "
            "ORDER BY c.customer_id, p.product_id";
    }

    auto rs = conn->executeQuery(query);
    printResults(rs);
}

// Function to demonstrate CROSS JOIN
void demonstrateCrossJoin(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== CROSS JOIN Example ===\n"
              << std::endl;
    std::cout << "CROSS JOIN returns the Cartesian product of the two tables (all possible combinations)." << std::endl;
    std::cout << "Query: Get all possible combinations of customers and product categories." << std::endl;

    // First, get distinct categories to limit the result set size
    auto categoryRs = conn->executeQuery("SELECT DISTINCT category FROM products");
    std::vector<std::string> categories;

    while (categoryRs->next())
    {
        categories.push_back(categoryRs->getString("category"));
    }

    // Now do a CROSS JOIN with a limited set
    std::string query =
        "SELECT c.customer_id, c.name, p.category "
        "FROM customers c "
        "CROSS JOIN (SELECT DISTINCT category FROM products) p "
        "ORDER BY c.customer_id, p.category";

    auto rs = conn->executeQuery(query);
    printResults(rs);
}

// Function to demonstrate SELF JOIN
void demonstrateSelfJoin(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== SELF JOIN Example ===\n"
              << std::endl;
    std::cout << "SELF JOIN is used to join a table to itself, treating it as two separate tables." << std::endl;
    std::cout << "Query: Find customers from the same country." << std::endl;

    std::string query =
        "SELECT c1.customer_id, c1.name, c1.country, c2.customer_id AS other_id, c2.name AS other_name "
        "FROM customers c1 "
        "JOIN customers c2 ON c1.country = c2.country AND c1.customer_id < c2.customer_id "
        "ORDER BY c1.country, c1.customer_id, c2.customer_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
}

// Function to demonstrate JOIN with aggregate functions
void demonstrateJoinWithAggregates(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== JOIN with Aggregate Functions Example ===\n"
              << std::endl;
    std::cout << "This example shows how to use JOIN with aggregate functions like COUNT, SUM, AVG, etc." << std::endl;
    std::cout << "Query: Get the total number of orders and total spending for each customer." << std::endl;

    std::string query =
        "SELECT c.customer_id, c.name, c.country, "
        "COUNT(o.order_id) AS order_count, "
        "SUM(o.total_price) AS total_spent "
        "FROM customers c "
        "LEFT JOIN orders o ON c.customer_id = o.customer_id "
        "GROUP BY c.customer_id, c.name, c.country "
        "ORDER BY total_spent DESC";

    auto rs = conn->executeQuery(query);
    printResults(rs);
}

// Function to demonstrate multi-table JOIN
void demonstrateMultiTableJoin(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== Multi-Table JOIN Example ===\n"
              << std::endl;
    std::cout << "This example shows how to join more than two tables together." << std::endl;
    std::cout << "Query: Get detailed order information including customer and product details." << std::endl;

    std::string query =
        "SELECT o.order_id, o.order_date, "
        "c.customer_id, c.name AS customer_name, c.country, "
        "p.product_id, p.name AS product_name, p.category, "
        "o.quantity, o.total_price "
        "FROM orders o "
        "JOIN customers c ON o.customer_id = c.customer_id "
        "JOIN products p ON o.product_id = p.product_id "
        "ORDER BY o.order_date, o.order_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
}

// Function to demonstrate JOIN with subquery
void demonstrateJoinWithSubquery(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== JOIN with Subquery Example ===\n"
              << std::endl;
    std::cout << "This example shows how to use JOIN with a subquery." << std::endl;
    std::cout << "Query: Find customers who have ordered products in the 'Electronics' category." << std::endl;

    std::string query =
        "SELECT DISTINCT c.customer_id, c.name, c.email "
        "FROM customers c "
        "JOIN orders o ON c.customer_id = o.customer_id "
        "JOIN (SELECT product_id, name FROM products WHERE category = 'Electronics') p "
        "ON o.product_id = p.product_id "
        "ORDER BY c.customer_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
}

int main()
{
    try
    {
        // Register database drivers
#if USE_MYSQL
        cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDriver>());

        // Connect to MySQL
        std::cout << "Connecting to MySQL..." << std::endl;
        auto mysqlConn = cpp_dbc::DriverManager::getConnection(
            "cpp_dbc:mysql://localhost:3306/testdb",
            "username",
            "password");

        // Set up database schema and data
        setupDatabase(mysqlConn);

        // Demonstrate different types of JOINs
        demonstrateInnerJoin(mysqlConn);
        demonstrateLeftJoin(mysqlConn);
        demonstrateRightJoin(mysqlConn);
        demonstrateFullJoin(mysqlConn, true); // true indicates MySQL
        demonstrateCrossJoin(mysqlConn);
        demonstrateSelfJoin(mysqlConn);
        demonstrateJoinWithAggregates(mysqlConn);
        demonstrateMultiTableJoin(mysqlConn);
        demonstrateJoinWithSubquery(mysqlConn);

        // Clean up
        mysqlConn->executeUpdate("DROP TABLE IF EXISTS orders");
        mysqlConn->executeUpdate("DROP TABLE IF EXISTS customers");
        mysqlConn->executeUpdate("DROP TABLE IF EXISTS products");

        // Close MySQL connection
        mysqlConn->close();
#else
        std::cout << "MySQL support is not enabled." << std::endl;
#endif

#if USE_POSTGRESQL
        cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

        // Connect to PostgreSQL
        std::cout << "\nConnecting to PostgreSQL..." << std::endl;
        auto pgConn = cpp_dbc::DriverManager::getConnection(
            "cpp_dbc:postgresql://localhost:5432/testdb",
            "username",
            "password");

        // Set up database schema and data
        setupDatabase(pgConn);

        // Demonstrate different types of JOINs
        demonstrateInnerJoin(pgConn);
        demonstrateLeftJoin(pgConn);
        demonstrateRightJoin(pgConn);
        demonstrateFullJoin(pgConn, false); // false indicates PostgreSQL
        demonstrateCrossJoin(pgConn);
        demonstrateSelfJoin(pgConn);
        demonstrateJoinWithAggregates(pgConn);
        demonstrateMultiTableJoin(pgConn);
        demonstrateJoinWithSubquery(pgConn);

        // Clean up
        pgConn->executeUpdate("DROP TABLE IF EXISTS orders");
        pgConn->executeUpdate("DROP TABLE IF EXISTS customers");
        pgConn->executeUpdate("DROP TABLE IF EXISTS products");

        // Close PostgreSQL connection
        pgConn->close();
#else
        std::cout << "PostgreSQL support is not enabled." << std::endl;
#endif
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

    return 0;
}