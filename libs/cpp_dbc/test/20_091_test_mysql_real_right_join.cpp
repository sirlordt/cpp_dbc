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

 @file test_mysql_real_right_join.cpp
 @brief Tests for MySQL database operations

*/

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <optional>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "20_001_test_mysql_real_common.hpp"

// Helper function to get the path to the test_db_connections.yml file
// Using common_test_helpers namespace for helper functions

#if USE_MYSQL
// Test case for MySQL RIGHT JOIN operations
TEST_CASE("MySQL RIGHT JOIN operations", "[20_091_01_mysql_real_right_join]")
{
    // Skip these tests if we can't connect to MySQL
    if (!mysql_test_helpers::canConnectToMySQL())
    {
        SKIP("Cannot connect to MySQL database");
        return;
    }

    // Get MySQL configuration using the centralized function
    auto dbConfig = mysql_test_helpers::getMySQLConfig("dev_mysql");

    // Extract connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Register the MySQL driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

    // Get a connection
    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
    REQUIRE(conn != nullptr);

    // Create test tables
    conn->executeUpdate("DROP TABLE IF EXISTS test_orders");
    conn->executeUpdate("DROP TABLE IF EXISTS test_customers");
    conn->executeUpdate("DROP TABLE IF EXISTS test_products");

    // Create test_customers table
    conn->executeUpdate(
        "CREATE TABLE test_customers ("
        "customer_id INT PRIMARY KEY, "
        "name VARCHAR(100), "
        "email VARCHAR(100), "
        "phone VARCHAR(20), "
        "credit_limit DECIMAL(10,2), "
        "created_at DATETIME"
        ")");

    // Create test_products table
    conn->executeUpdate(
        "CREATE TABLE test_products ("
        "product_id INT PRIMARY KEY, "
        "name VARCHAR(100), "
        "description TEXT, "
        "price DECIMAL(10,2), "
        "stock_quantity INT, "
        "is_active BOOLEAN"
        ")");

    // Create test_orders table
    conn->executeUpdate(
        "CREATE TABLE test_orders ("
        "order_id INT PRIMARY KEY, "
        "customer_id INT, "
        "product_id INT, "
        "quantity INT, "
        "total_price DECIMAL(10,2), "
        "order_date DATETIME, "
        "FOREIGN KEY (customer_id) REFERENCES test_customers(customer_id), "
        "FOREIGN KEY (product_id) REFERENCES test_products(product_id)"
        ")");

    // Insert data into test_customers
    auto customerStmt = conn->prepareStatement(
        "INSERT INTO test_customers (customer_id, name, email, phone, credit_limit, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?)");

    // Insert 5 customers (some products won't be ordered)
    std::vector<std::pair<int, std::string>> customers = {
        {1, "John Doe"},
        {2, "Jane Smith"},
        {3, "Bob Johnson"},
        {4, "Alice Brown"},
        {5, "Charlie Davis"}};

    for (const auto &customer : customers)
    {
        customerStmt->setInt(1, customer.first);
        customerStmt->setString(2, customer.second);
        customerStmt->setString(3, customer.second.substr(0, customer.second.find(' ')) + "@example.com");
        customerStmt->setString(4, "555-" + std::to_string(1000 + customer.first));
        customerStmt->setDouble(5, 1000.0 * customer.first);
        customerStmt->setString(6, "2023-01-" + std::to_string(customer.first + 10) + " 10:00:00");
        customerStmt->executeUpdate();
    }

    // Insert data into test_products
    auto productStmt = conn->prepareStatement(
        "INSERT INTO test_products (product_id, name, description, price, stock_quantity, is_active) "
        "VALUES (?, ?, ?, ?, ?, ?)");

    // Insert 7 products (some won't be ordered)
    std::vector<std::tuple<int, std::string, double>> products = {
        {101, "Laptop", 999.99},
        {102, "Smartphone", 499.99},
        {103, "Tablet", 299.99},
        {104, "Headphones", 99.99},
        {105, "Monitor", 199.99},
        {106, "Keyboard", 49.99},
        {107, "Mouse", 29.99}};

    for (const auto &product : products)
    {
        int id = std::get<0>(product);
        std::string name = std::get<1>(product);
        double price = std::get<2>(product);

        productStmt->setInt(1, id);
        productStmt->setString(2, name);
        productStmt->setString(3, "Description for " + name);
        productStmt->setDouble(4, price);
        productStmt->setInt(5, 100 + (id % 10) * 5);
        productStmt->setBoolean(6, id % 2 == 1); // Odd IDs are active
        productStmt->executeUpdate();
    }

    // Insert data into test_orders
    auto orderStmt = conn->prepareStatement(
        "INSERT INTO test_orders (order_id, customer_id, product_id, quantity, total_price, order_date) "
        "VALUES (?, ?, ?, ?, ?, ?)");

    // Insert 8 orders (some products won't be ordered)
    std::vector<std::tuple<int, int, int, int>> orders = {
        {1001, 1, 101, 1},
        {1002, 1, 103, 2},
        {1003, 2, 102, 1},
        {1004, 3, 101, 1},
        {1005, 3, 104, 3},
        {1006, 3, 105, 2},
        {1007, 4, 102, 1},
        {1008, 5, 103, 1}};

    for (const auto &order : orders)
    {
        int orderId = std::get<0>(order);
        int customerId = std::get<1>(order);
        int productId = std::get<2>(order);
        int quantity = std::get<3>(order);

        // Find the product price
        double price = 0.0;
        for (const auto &product : products)
        {
            if (std::get<0>(product) == productId)
            {
                price = std::get<2>(product);
                break;
            }
        }

        double totalPrice = price * quantity;

        orderStmt->setInt(1, orderId);
        orderStmt->setInt(2, customerId);
        orderStmt->setInt(3, productId);
        orderStmt->setInt(4, quantity);
        orderStmt->setDouble(5, totalPrice);
        orderStmt->setString(6, "2023-02-" + std::to_string(orderId % 28 + 1) + " 14:30:00");
        orderStmt->executeUpdate();
    }

    SECTION("Basic RIGHT JOIN")
    {
        // Test RIGHT JOIN between orders and products
        std::string query =
            "SELECT p.product_id, p.name, o.order_id, o.quantity "
            "FROM test_orders o "
            "RIGHT JOIN test_products p ON o.product_id = p.product_id "
            "ORDER BY p.product_id, o.order_id";

        auto rs = conn->executeQuery(query);

        // Verify results - all products should be included, even those without orders
        std::vector<std::tuple<int, std::string, std::optional<int>, std::optional<int>>> expectedResults = {
            {101, "Laptop", 1001, 1},
            {101, "Laptop", 1004, 1},
            {102, "Smartphone", 1003, 1},
            {102, "Smartphone", 1007, 1},
            {103, "Tablet", 1002, 2},
            {103, "Tablet", 1008, 1},
            {104, "Headphones", 1005, 3},
            {105, "Monitor", 1006, 2},
            {106, "Keyboard", std::nullopt, std::nullopt},
            {107, "Mouse", std::nullopt, std::nullopt}};

        size_t rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            REQUIRE(rs->getInt("product_id") == std::get<0>(expectedResults[rowCount]));
            REQUIRE(rs->getString("name") == std::get<1>(expectedResults[rowCount]));

            if (std::get<2>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("order_id"));
                REQUIRE(rs->getInt("order_id") == std::get<2>(expectedResults[rowCount]).value());
                REQUIRE(rs->getInt("quantity") == std::get<3>(expectedResults[rowCount]).value());
            }
            else
            {
                REQUIRE(rs->isNull("order_id"));
                REQUIRE(rs->isNull("quantity"));
            }

            rowCount++;
        }

        REQUIRE(rowCount == expectedResults.size());
    }

    SECTION("Three-table RIGHT JOIN")
    {
        // Test RIGHT JOIN across all three tables
        std::string query =
            "SELECT c.name as customer_name, p.name as product_name, o.quantity, o.total_price "
            "FROM test_customers c "
            "RIGHT JOIN test_orders o ON c.customer_id = o.customer_id "
            "RIGHT JOIN test_products p ON o.product_id = p.product_id "
            "ORDER BY p.name, IFNULL(c.name, '')";

        auto rs = conn->executeQuery(query);

        // Verify results - all products should be included
        // The query orders by p.name, IFNULL(c.name, '')
        std::vector<std::tuple<std::optional<std::string>, std::string, std::optional<int>, std::optional<double>>> expectedResults = {
            {"Bob Johnson", "Headphones", 3, 299.97},
            {std::nullopt, "Keyboard", std::nullopt, std::nullopt},
            {"Bob Johnson", "Laptop", 1, 999.99},
            {"John Doe", "Laptop", 1, 999.99},
            {"Bob Johnson", "Monitor", 2, 399.98},
            {std::nullopt, "Mouse", std::nullopt, std::nullopt},
            {"Alice Brown", "Smartphone", 1, 499.99},
            {"Jane Smith", "Smartphone", 1, 499.99},
            {"Charlie Davis", "Tablet", 1, 299.99},
            {"John Doe", "Tablet", 2, 599.98}};

        size_t rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            REQUIRE_FALSE(rs->isNull("product_name"));
            REQUIRE(rs->getString("product_name") == std::get<1>(expectedResults[rowCount]));

            if (std::get<0>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("customer_name"));
                REQUIRE(rs->getString("customer_name") == std::get<0>(expectedResults[rowCount]).value());
                REQUIRE(rs->getInt("quantity") == std::get<2>(expectedResults[rowCount]).value());
                REQUIRE(rs->getDouble("total_price") == Catch::Approx(std::get<3>(expectedResults[rowCount]).value()).margin(0.01));
            }
            else
            {
                REQUIRE(rs->isNull("customer_name"));
                REQUIRE(rs->isNull("quantity"));
                REQUIRE(rs->isNull("total_price"));
            }

            rowCount++;
        }

        REQUIRE(rowCount == expectedResults.size());
    }

    SECTION("RIGHT JOIN with WHERE clause")
    {
        // Test RIGHT JOIN with additional filtering
        std::string query =
            "SELECT c.name as customer_name, p.name as product_name, o.quantity, o.total_price "
            "FROM test_customers c "
            "RIGHT JOIN test_orders o ON c.customer_id = o.customer_id "
            "RIGHT JOIN test_products p ON o.product_id = p.product_id "
            "WHERE p.price < 300 "
            "ORDER BY p.name, IFNULL(c.name, '')";

        auto rs = conn->executeQuery(query);

        // Verify results - products with price < 300 and Tablet (299.99)
        // Products with price < 300: Headphones (99.99), Keyboard (49.99), Mouse (29.99), Tablet (299.99)
        // The query orders by p.name, IFNULL(c.name, '')
        std::vector<std::tuple<std::optional<std::string>, std::string, std::optional<int>, std::optional<double>>> expectedResults = {
            {"Bob Johnson", "Headphones", 3, 299.97},
            {std::nullopt, "Keyboard", std::nullopt, std::nullopt},
            {"Bob Johnson", "Monitor", 2, 399.98},
            {std::nullopt, "Mouse", std::nullopt, std::nullopt},
            {"Charlie Davis", "Tablet", 1, 299.99},
            {"John Doe", "Tablet", 2, 599.98}};
        size_t rowCount = 0;
        while (rs->next())
        {
            // Verify each row matches the expected results
            if (rowCount < expectedResults.size())
            {
                REQUIRE_FALSE(rs->isNull("product_name"));
                REQUIRE(rs->getString("product_name") == std::get<1>(expectedResults[rowCount]));

                if (std::get<0>(expectedResults[rowCount]))
                {
                    REQUIRE_FALSE(rs->isNull("customer_name"));
                    REQUIRE(rs->getString("customer_name") == std::get<0>(expectedResults[rowCount]).value());
                    REQUIRE(rs->getInt("quantity") == std::get<2>(expectedResults[rowCount]).value());
                    REQUIRE(rs->getDouble("total_price") == Catch::Approx(std::get<3>(expectedResults[rowCount]).value()).margin(0.01));
                }
                else
                {
                    REQUIRE(rs->isNull("customer_name"));
                    REQUIRE(rs->isNull("quantity"));
                    REQUIRE(rs->isNull("total_price"));
                }
            }

            rowCount++;
        }

        // Verify we got exactly the expected number of rows
        REQUIRE(rowCount == expectedResults.size());
    }

    SECTION("RIGHT JOIN with NULL check")
    {
        // Test RIGHT JOIN with NULL check to find products without orders
        std::string query =
            "SELECT p.product_id, p.name "
            "FROM test_orders o "
            "RIGHT JOIN test_products p ON o.product_id = p.product_id "
            "WHERE o.order_id IS NULL "
            "ORDER BY p.product_id";

        auto rs = conn->executeQuery(query);

        // Verify results - only products without orders
        std::vector<std::pair<int, std::string>> expectedResults = {
            {106, "Keyboard"},
            {107, "Mouse"}};

        size_t rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            REQUIRE(rs->getInt("product_id") == expectedResults[rowCount].first);
            REQUIRE(rs->getString("name") == expectedResults[rowCount].second);

            rowCount++;
        }

        REQUIRE(rowCount == expectedResults.size());
    }

    SECTION("RIGHT JOIN with invalid column")
    {
        // Test RIGHT JOIN with an invalid column name
        std::string query =
            "SELECT p.product_id, p.name, o.order_id, o.non_existent_column "
            "FROM test_orders o "
            "RIGHT JOIN test_products p ON o.product_id = p.product_id";

        // This should throw an exception
        REQUIRE_THROWS_AS(conn->executeQuery(query), cpp_dbc::DBException);
    }

    SECTION("RIGHT JOIN with type mismatch")
    {
        // Test RIGHT JOIN with a type mismatch in the join condition
        std::string query =
            "SELECT p.product_id, p.name, o.order_id "
            "FROM test_orders o "
            "RIGHT JOIN test_products p ON o.product_id = p.name";

        // This should execute but return only NULL values for the left side
        auto rs = conn->executeQuery(query);

        size_t rowCount = 0;
        while (rs->next())
        {
            REQUIRE_FALSE(rs->isNull("product_id"));
            REQUIRE_FALSE(rs->isNull("name"));
            REQUIRE(rs->isNull("order_id"));
            rowCount++;
        }

        REQUIRE(rowCount == products.size());
    }

    // Clean up
    conn->executeUpdate("DROP TABLE IF EXISTS test_orders");
    conn->executeUpdate("DROP TABLE IF EXISTS test_products");
    conn->executeUpdate("DROP TABLE IF EXISTS test_customers");

    // Close the connection
    conn->close();
}
#endif