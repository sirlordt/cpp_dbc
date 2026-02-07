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

 @file 20_101_test_mysql_real_full_join.cpp
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
// Test case for MySQL FULL JOIN operations (emulated with UNION)
TEST_CASE("MySQL FULL JOIN operations (emulated)", "[20_101_01_mysql_real_full_join]")
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

    // Insert 7 customers (some won't have orders)
    std::vector<std::pair<int, std::string>> customers = {
        {1, "John Doe"},
        {2, "Jane Smith"},
        {3, "Bob Johnson"},
        {4, "Alice Brown"},
        {5, "Charlie Davis"},
        {6, "Eva Wilson"},
        {7, "Frank Miller"}};

    for (const auto &customer : customers)
    {
        customerStmt->setInt(1, customer.first);
        customerStmt->setString(2, customer.second);
        customerStmt->setString(3, customer.second.substr(0, customer.second.find(' ')) + "@example.com");
        customerStmt->setString(4, "555-" + std::to_string(1000 + customer.first));
        customerStmt->setDouble(5, 1000.0 * customer.first);
        customerStmt->setTimestamp(6, "2023-01-" + std::to_string(customer.first + 10) + " 10:00:00");
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

    // Insert 8 orders (some customers and products won't have orders)
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
        orderStmt->setTimestamp(6, "2023-02-" + std::to_string(orderId % 28 + 1) + " 14:30:00");
        orderStmt->executeUpdate();
    }

    SECTION("Basic FULL JOIN (emulated with UNION)")
    {
        // Test FULL JOIN between customers and orders using UNION
        std::string query =
            "SELECT c.customer_id, c.name, o.order_id, o.total_price "
            "FROM test_customers c "
            "LEFT JOIN test_orders o ON c.customer_id = o.customer_id "
            "UNION "
            "SELECT c.customer_id, c.name, o.order_id, o.total_price "
            "FROM test_customers c "
            "RIGHT JOIN test_orders o ON c.customer_id = o.customer_id "
            "WHERE c.customer_id IS NULL "
            "ORDER BY customer_id, order_id";

        auto rs = conn->executeQuery(query);

        // Verify results - all customers should be included, even those without orders
        std::vector<std::tuple<std::optional<int>, std::optional<std::string>, std::optional<int>, std::optional<double>>> expectedResults = {
            {1, "John Doe", 1001, 999.99},
            {1, "John Doe", 1002, 599.98},
            {2, "Jane Smith", 1003, 499.99},
            {3, "Bob Johnson", 1004, 999.99},
            {3, "Bob Johnson", 1005, 299.97},
            {3, "Bob Johnson", 1006, 399.98},
            {4, "Alice Brown", 1007, 499.99},
            {5, "Charlie Davis", 1008, 299.99},
            {6, "Eva Wilson", std::nullopt, std::nullopt},
            {7, "Frank Miller", std::nullopt, std::nullopt}};

        size_t rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            if (std::get<0>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("customer_id"));
                REQUIRE(rs->getInt("customer_id") == std::get<0>(expectedResults[rowCount]).value());
                REQUIRE(rs->getString("name") == std::get<1>(expectedResults[rowCount]).value());
            }
            else
            {
                REQUIRE(rs->isNull("customer_id"));
                REQUIRE(rs->isNull("name"));
            }

            if (std::get<2>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("order_id"));
                REQUIRE(rs->getInt("order_id") == std::get<2>(expectedResults[rowCount]).value());
                REQUIRE(rs->getDouble("total_price") == Catch::Approx(std::get<3>(expectedResults[rowCount]).value()).margin(0.01));
            }
            else
            {
                REQUIRE(rs->isNull("order_id"));
                REQUIRE(rs->isNull("total_price"));
            }

            rowCount++;
        }

        REQUIRE(rowCount == expectedResults.size());
    }

    SECTION("FULL JOIN between products and orders (emulated with UNION)")
    {
        // Test FULL JOIN between products and orders using UNION
        std::string query =
            "SELECT p.product_id, p.name, o.order_id, o.quantity "
            "FROM test_products p "
            "LEFT JOIN test_orders o ON p.product_id = o.product_id "
            "UNION "
            "SELECT p.product_id, p.name, o.order_id, o.quantity "
            "FROM test_products p "
            "RIGHT JOIN test_orders o ON p.product_id = o.product_id "
            "WHERE p.product_id IS NULL "
            "ORDER BY product_id, order_id";

        auto rs = conn->executeQuery(query);

        // Verify results - all products should be included, even those without orders
        std::vector<std::tuple<std::optional<int>, std::optional<std::string>, std::optional<int>, std::optional<int>>> expectedResults = {
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

            if (std::get<0>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("product_id"));
                REQUIRE(rs->getInt("product_id") == std::get<0>(expectedResults[rowCount]).value());
                REQUIRE(rs->getString("name") == std::get<1>(expectedResults[rowCount]).value());
            }
            else
            {
                REQUIRE(rs->isNull("product_id"));
                REQUIRE(rs->isNull("name"));
            }

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

    SECTION("Three-table FULL JOIN (emulated with UNION)")
    {
        // Test FULL JOIN across all three tables using UNION
        std::string query =
            "SELECT c.name as customer_name, p.name as product_name, o.quantity, o.total_price "
            "FROM test_customers c "
            "LEFT JOIN test_orders o ON c.customer_id = o.customer_id "
            "LEFT JOIN test_products p ON o.product_id = p.product_id "
            "UNION "
            "SELECT c.name as customer_name, p.name as product_name, o.quantity, o.total_price "
            "FROM test_customers c "
            "RIGHT JOIN test_orders o ON c.customer_id = o.customer_id "
            "RIGHT JOIN test_products p ON o.product_id = p.product_id "
            "WHERE c.customer_id IS NULL OR o.order_id IS NULL "
            "ORDER BY IFNULL(customer_name, ''), IFNULL(product_name, '')";

        auto rs = conn->executeQuery(query);

        // Verify results - all customers and products should be included
        std::vector<std::tuple<std::optional<std::string>, std::optional<std::string>, std::optional<int>, std::optional<double>>> expectedResults = {
            {std::nullopt, "Keyboard", std::nullopt, std::nullopt},
            {std::nullopt, "Mouse", std::nullopt, std::nullopt},
            {"Alice Brown", "Smartphone", 1, 499.99},
            {"Bob Johnson", "Headphones", 3, 299.97},
            {"Bob Johnson", "Laptop", 1, 999.99},
            {"Bob Johnson", "Monitor", 2, 399.98},
            {"Charlie Davis", "Tablet", 1, 299.99},
            {"Eva Wilson", std::nullopt, std::nullopt, std::nullopt},
            {"Frank Miller", std::nullopt, std::nullopt, std::nullopt},
            {"Jane Smith", "Smartphone", 1, 499.99},
            {"John Doe", "Laptop", 1, 999.99},
            {"John Doe", "Tablet", 2, 599.98}};

        size_t rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            if (std::get<0>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("customer_name"));
                REQUIRE(rs->getString("customer_name") == std::get<0>(expectedResults[rowCount]).value());
            }
            else
            {
                REQUIRE(rs->isNull("customer_name"));
            }

            if (std::get<1>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("product_name"));
                REQUIRE(rs->getString("product_name") == std::get<1>(expectedResults[rowCount]).value());
            }
            else
            {
                REQUIRE(rs->isNull("product_name"));
            }

            if (std::get<2>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("quantity"));
                REQUIRE(rs->getInt("quantity") == std::get<2>(expectedResults[rowCount]).value());
                REQUIRE(rs->getDouble("total_price") == Catch::Approx(std::get<3>(expectedResults[rowCount]).value()).margin(0.01));
            }
            else
            {
                REQUIRE(rs->isNull("quantity"));
                REQUIRE(rs->isNull("total_price"));
            }

            rowCount++;
        }

        REQUIRE(rowCount == expectedResults.size());
    }

    SECTION("FULL JOIN with WHERE clause (emulated with UNION)")
    {
        // Test FULL JOIN with additional filtering
        std::string query =
            "SELECT c.name as customer_name, p.name as product_name, o.quantity, o.total_price "
            "FROM test_customers c "
            "LEFT JOIN test_orders o ON c.customer_id = o.customer_id "
            "LEFT JOIN test_products p ON o.product_id = p.product_id "
            "WHERE c.credit_limit > 3000 OR p.price < 100 "
            "UNION "
            "SELECT c.name as customer_name, p.name as product_name, o.quantity, o.total_price "
            "FROM test_customers c "
            "RIGHT JOIN test_orders o ON c.customer_id = o.customer_id "
            "RIGHT JOIN test_products p ON o.product_id = p.product_id "
            "WHERE (c.customer_id IS NULL OR o.order_id IS NULL) AND (p.price < 100) "
            "ORDER BY IFNULL(customer_name, ''), IFNULL(product_name, '')";

        auto rs = conn->executeQuery(query);

        // Verify results - customers with credit_limit > 3000 or products with price < 100
        // The query filters for customers with credit_limit > 3000 OR products with price < 100
        // Bob Johnson has orders for Headphones (price 99.99 < 100), so he should be included
        std::vector<std::tuple<std::optional<std::string>, std::optional<std::string>, std::optional<int>, std::optional<double>>> expectedResults = {
            {std::nullopt, "Keyboard", std::nullopt, std::nullopt},
            {std::nullopt, "Mouse", std::nullopt, std::nullopt},
            {"Alice Brown", "Smartphone", 1, 499.99},
            {"Bob Johnson", "Headphones", 3, 299.97},
            {"Charlie Davis", "Tablet", 1, 299.99},
            {"Eva Wilson", std::nullopt, std::nullopt, std::nullopt},
            {"Frank Miller", std::nullopt, std::nullopt, std::nullopt}};

        size_t rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            if (std::get<0>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("customer_name"));
                REQUIRE(rs->getString("customer_name") == std::get<0>(expectedResults[rowCount]).value());
            }
            else
            {
                REQUIRE(rs->isNull("customer_name"));
            }

            if (std::get<1>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("product_name"));
                REQUIRE(rs->getString("product_name") == std::get<1>(expectedResults[rowCount]).value());
            }
            else
            {
                REQUIRE(rs->isNull("product_name"));
            }

            if (std::get<2>(expectedResults[rowCount]))
            {
                REQUIRE_FALSE(rs->isNull("quantity"));
                REQUIRE(rs->getInt("quantity") == std::get<2>(expectedResults[rowCount]).value());
                REQUIRE(rs->getDouble("total_price") == Catch::Approx(std::get<3>(expectedResults[rowCount]).value()).margin(0.01));
            }
            else
            {
                REQUIRE(rs->isNull("quantity"));
                REQUIRE(rs->isNull("total_price"));
            }

            rowCount++;
        }

        REQUIRE(rowCount == expectedResults.size());
    }

    SECTION("FULL JOIN with invalid column (emulated with UNION)")
    {
        // Test FULL JOIN with an invalid column name
        std::string query =
            "SELECT c.customer_id, c.name, o.order_id, o.non_existent_column "
            "FROM test_customers c "
            "LEFT JOIN test_orders o ON c.customer_id = o.customer_id "
            "UNION "
            "SELECT c.customer_id, c.name, o.order_id, o.non_existent_column "
            "FROM test_customers c "
            "RIGHT JOIN test_orders o ON c.customer_id = o.customer_id "
            "WHERE c.customer_id IS NULL";

        // This should throw an exception
        REQUIRE_THROWS_AS(conn->executeQuery(query), cpp_dbc::DBException);
    }

    // Clean up
    conn->executeUpdate("DROP TABLE IF EXISTS test_orders");
    conn->executeUpdate("DROP TABLE IF EXISTS test_products");
    conn->executeUpdate("DROP TABLE IF EXISTS test_customers");

    // Close the connection
    conn->close();
}
#endif