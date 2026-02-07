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
 * @file 20_071_example_mysql_join.cpp
 * @brief MySQL-specific example demonstrating SQL JOIN operations
 *
 * This example demonstrates:
 * - INNER JOIN, LEFT JOIN, RIGHT JOIN
 * - CROSS JOIN, SELF JOIN
 * - FULL JOIN (simulated with UNION for MySQL)
 * - JOIN with aggregate functions
 * - Multi-table JOIN and JOIN with subquery
 *
 * Usage:
 *   ./20_071_example_mysql_join [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - MySQL support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <iomanip>
#include <sstream>

#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_MYSQL
// Helper function to print query results
void printResults(std::shared_ptr<cpp_dbc::RelationalDBResultSet> rs)
{
    auto columnNames = rs->getColumnNames();

    std::vector<size_t> columnWidths(columnNames.size(), 15);

    std::ostringstream headerStream;
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        headerStream << std::setw(static_cast<int>(columnWidths[i])) << std::left << columnNames[i] << " | ";
    }
    logData(headerStream.str());

    std::ostringstream sepStream;
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        sepStream << std::string(columnWidths[i], '-') << "-|-";
    }
    logData(sepStream.str());

    int rowCount = 0;
    while (rs->next())
    {
        std::ostringstream rowStream;
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
            rowStream << std::setw(static_cast<int>(columnWidths[columnNames.size() - 1])) << std::left << value << " | ";
        }
        logData(rowStream.str());
        rowCount++;
    }

    logData(std::to_string(rowCount) + " row(s) returned");
}

// Function to set up test database schema and data
void setupDatabase(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logStep("Setting up test database schema and data...");

    conn->executeUpdate("DROP TABLE IF EXISTS orders");
    conn->executeUpdate("DROP TABLE IF EXISTS customers");
    conn->executeUpdate("DROP TABLE IF EXISTS products");

    conn->executeUpdate(
        "CREATE TABLE customers ("
        "customer_id INT PRIMARY KEY, "
        "name VARCHAR(100), "
        "email VARCHAR(100), "
        "city VARCHAR(50), "
        "country VARCHAR(50), "
        "registration_date DATE"
        ")");

    conn->executeUpdate(
        "CREATE TABLE products ("
        "product_id INT PRIMARY KEY, "
        "name VARCHAR(100), "
        "category VARCHAR(50), "
        "price DECIMAL(10,2), "
        "stock_quantity INT"
        ")");

    conn->executeUpdate(
        "CREATE TABLE orders ("
        "order_id INT PRIMARY KEY, "
        "customer_id INT, "
        "product_id INT, "
        "order_date DATE, "
        "quantity INT, "
        "total_price DECIMAL(10,2)"
        ")");

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
        {1008, 1, 102, "2023-02-10", 1, 799.99}};

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

    logOk("Database setup completed");
}

void demonstrateInnerJoin(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- INNER JOIN Example ---");
    logInfo("INNER JOIN returns only the rows where there is a match in both tables");
    logStep("Query: Get all customers who have placed orders");

    std::string query =
        "SELECT c.customer_id, c.name, o.order_id, o.order_date, o.total_price "
        "FROM customers c "
        "INNER JOIN orders o ON c.customer_id = o.customer_id "
        "ORDER BY c.customer_id, o.order_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
    logOk("INNER JOIN completed");
}

void demonstrateLeftJoin(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- LEFT JOIN Example ---");
    logInfo("LEFT JOIN returns all rows from the left table and matching rows from the right table");
    logStep("Query: Get all customers and their orders (if any)");

    std::string query =
        "SELECT c.customer_id, c.name, o.order_id, o.order_date, o.total_price "
        "FROM customers c "
        "LEFT JOIN orders o ON c.customer_id = o.customer_id "
        "ORDER BY c.customer_id, o.order_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
    logOk("LEFT JOIN completed");
}

void demonstrateRightJoin(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- RIGHT JOIN Example ---");
    logInfo("RIGHT JOIN returns all rows from the right table and matching rows from the left table");
    logStep("Query: Get all products and their orders (if any)");

    std::string query =
        "SELECT p.product_id, p.name, p.category, o.order_id, o.customer_id, o.quantity "
        "FROM orders o "
        "RIGHT JOIN products p ON o.product_id = p.product_id "
        "ORDER BY p.product_id, o.order_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
    logOk("RIGHT JOIN completed");
}

void demonstrateFullJoin(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- FULL JOIN Example (MySQL simulation) ---");
    logInfo("MySQL doesn't support FULL JOIN directly, using LEFT JOIN UNION RIGHT JOIN");
    logStep("Query: Get all customers and all products");

    std::string query =
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

    auto rs = conn->executeQuery(query);
    printResults(rs);
    logOk("FULL JOIN completed");
}

void demonstrateCrossJoin(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- CROSS JOIN Example ---");
    logInfo("CROSS JOIN returns the Cartesian product of the two tables");
    logStep("Query: Get all possible combinations of customers and product categories");

    std::string query =
        "SELECT c.customer_id, c.name, p.category "
        "FROM customers c "
        "CROSS JOIN (SELECT DISTINCT category FROM products) p "
        "ORDER BY c.customer_id, p.category";

    auto rs = conn->executeQuery(query);
    printResults(rs);
    logOk("CROSS JOIN completed");
}

void demonstrateSelfJoin(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- SELF JOIN Example ---");
    logInfo("SELF JOIN is used to join a table to itself");
    logStep("Query: Find customers from the same country");

    std::string query =
        "SELECT c1.customer_id, c1.name, c1.country, c2.customer_id AS other_id, c2.name AS other_name "
        "FROM customers c1 "
        "JOIN customers c2 ON c1.country = c2.country AND c1.customer_id < c2.customer_id "
        "ORDER BY c1.country, c1.customer_id, c2.customer_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
    logOk("SELF JOIN completed");
}

void demonstrateJoinWithAggregates(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- JOIN with Aggregate Functions Example ---");
    logInfo("Shows how to use JOIN with aggregate functions like COUNT, SUM, AVG");
    logStep("Query: Get the total number of orders and total spending for each customer");

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
    logOk("JOIN with aggregates completed");
}

void demonstrateMultiTableJoin(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Multi-Table JOIN Example ---");
    logInfo("Shows how to join more than two tables together");
    logStep("Query: Get detailed order information including customer and product details");

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
    logOk("Multi-table JOIN completed");
}

void demonstrateJoinWithSubquery(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- JOIN with Subquery Example ---");
    logInfo("Shows how to use JOIN with a subquery");
    logStep("Query: Find customers who have ordered products in the 'Electronics' category");

    std::string query =
        "SELECT DISTINCT c.customer_id, c.name, c.email "
        "FROM customers c "
        "JOIN orders o ON c.customer_id = o.customer_id "
        "JOIN (SELECT product_id, name FROM products WHERE category = 'Electronics') p "
        "ON o.product_id = p.product_id "
        "ORDER BY c.customer_id";

    auto rs = conn->executeQuery(query);
    printResults(rs);
    logOk("JOIN with subquery completed");
}

void runAllDemonstrations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    setupDatabase(conn);
    demonstrateInnerJoin(conn);
    demonstrateLeftJoin(conn);
    demonstrateRightJoin(conn);
    demonstrateFullJoin(conn);
    demonstrateCrossJoin(conn);
    demonstrateSelfJoin(conn);
    demonstrateJoinWithAggregates(conn);
    demonstrateMultiTableJoin(conn);
    demonstrateJoinWithSubquery(conn);

    logMsg("");
    logStep("Cleaning up tables...");
    conn->executeUpdate("DROP TABLE IF EXISTS orders");
    conn->executeUpdate("DROP TABLE IF EXISTS customers");
    conn->executeUpdate("DROP TABLE IF EXISTS products");
    logOk("Tables dropped");
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc MySQL JOIN Operations Example");
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
        printHelp("20_071_example_mysql_join", "mysql");
        return EXIT_OK_;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return EXIT_ERROR_;
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
            return EXIT_ERROR_;
        }

        if (!mysqlResult.value())
        {
            logError("MySQL configuration not found");
            return EXIT_ERROR_;
        }

        auto &mysqlConfig = *mysqlResult.value();
        logOk("Using: " + mysqlConfig.getName());

        logStep("Connecting to MySQL...");
        auto mysqlConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            mysqlConfig.createDBConnection());
        logOk("Connected to MySQL");

        runAllDemonstrations(mysqlConn);

        logStep("Closing MySQL connection...");
        mysqlConn->close();
        logOk("MySQL connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        e.printCallStack();
        return EXIT_ERROR_;
    }
    catch (const std::exception &e)
    {
        logError("Error: " + std::string(e.what()));
        return EXIT_ERROR_;
    }

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return EXIT_OK_;
#endif
}
