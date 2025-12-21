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

 @file test_firebird_real_inner_join.cpp
 @brief Tests for Firebird INNER JOIN database operations

*/

#include <string>
#include <memory>
#include <vector>
#include <iostream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "test_firebird_common.hpp"

// Helper function to get the path to the test_db_connections.yml file
// Using common_test_helpers namespace for helper functions

#if USE_FIREBIRD
// Test case for Firebird INNER JOIN operations
TEST_CASE("Firebird INNER JOIN operations", "[firebird_real_inner_join]")
{
    // Skip these tests if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    // Get Firebird configuration using the centralized function
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");

    // Extract connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Register the Firebird driver
    cpp_dbc::DriverManager::registerDriver("firebird", std::make_shared<cpp_dbc::Firebird::FirebirdDriver>());

    // Get a connection
    auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);
    REQUIRE(conn != nullptr);

    // Create test tables - Firebird uses different syntax
    // Drop tables in reverse order of dependencies
    try
    {
        conn->executeUpdate("DROP TABLE test_orders");
    }
    catch (...)
    {
    }
    try
    {
        conn->executeUpdate("DROP TABLE test_customers");
    }
    catch (...)
    {
    }
    try
    {
        conn->executeUpdate("DROP TABLE test_products");
    }
    catch (...)
    {
    }

    // Create test_customers table
    conn->executeUpdate(
        "CREATE TABLE test_customers ("
        "customer_id INTEGER NOT NULL PRIMARY KEY, "
        "name VARCHAR(100), "
        "email VARCHAR(100), "
        "phone VARCHAR(20), "
        "credit_limit DECIMAL(10,2), "
        "created_at TIMESTAMP"
        ")");

    // Create test_products table
    conn->executeUpdate(
        "CREATE TABLE test_products ("
        "product_id INTEGER NOT NULL PRIMARY KEY, "
        "name VARCHAR(100), "
        "description BLOB SUB_TYPE TEXT, "
        "price DECIMAL(10,2), "
        "stock_quantity INTEGER, "
        "is_active SMALLINT"
        ")");

    // Create test_orders table
    conn->executeUpdate(
        "CREATE TABLE test_orders ("
        "order_id INTEGER NOT NULL PRIMARY KEY, "
        "customer_id INTEGER, "
        "product_id INTEGER, "
        "quantity INTEGER, "
        "total_price DECIMAL(10,2), "
        "order_date TIMESTAMP, "
        "FOREIGN KEY (customer_id) REFERENCES test_customers(customer_id), "
        "FOREIGN KEY (product_id) REFERENCES test_products(product_id)"
        ")");

    // Insert data into test_customers
    auto customerStmt = conn->prepareStatement(
        "INSERT INTO test_customers (customer_id, name, email, phone, credit_limit, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?)");

    // Insert 5 customers
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

    // Insert 5 products
    std::vector<std::tuple<int, std::string, double>> products = {
        {101, "Laptop", 999.99},
        {102, "Smartphone", 499.99},
        {103, "Tablet", 299.99},
        {104, "Headphones", 99.99},
        {105, "Monitor", 199.99}};

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
        productStmt->setInt(6, id % 2 == 1 ? 1 : 0); // Odd IDs are active
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
        orderStmt->setString(6, "2023-02-" + std::to_string(orderId % 28 + 1) + " 14:30:00");
        orderStmt->executeUpdate();
    }

    SECTION("Basic INNER JOIN")
    {
        // Test INNER JOIN between customers and orders
        std::string query =
            "SELECT c.customer_id, c.name, o.order_id, o.total_price "
            "FROM test_customers c "
            "INNER JOIN test_orders o ON c.customer_id = o.customer_id "
            "ORDER BY c.customer_id, o.order_id";

        auto rs = conn->executeQuery(query);

        // Verify results
        std::vector<std::tuple<int, std::string, int, double>> expectedResults = {
            {1, "John Doe", 1001, 999.99},
            {1, "John Doe", 1002, 599.98},
            {2, "Jane Smith", 1003, 499.99},
            {3, "Bob Johnson", 1004, 999.99},
            {3, "Bob Johnson", 1005, 299.97},
            {3, "Bob Johnson", 1006, 399.98},
            {4, "Alice Brown", 1007, 499.99},
            {5, "Charlie Davis", 1008, 299.99}};

        size_t rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            REQUIRE(rs->getInt("CUSTOMER_ID") == std::get<0>(expectedResults[rowCount]));
            REQUIRE(rs->getString("NAME") == std::get<1>(expectedResults[rowCount]));
            REQUIRE(rs->getInt("ORDER_ID") == std::get<2>(expectedResults[rowCount]));
            REQUIRE(rs->getDouble("TOTAL_PRICE") == Catch::Approx(std::get<3>(expectedResults[rowCount])).margin(0.01));

            rowCount++;
        }

        REQUIRE(rowCount == expectedResults.size());
    }

    SECTION("Three-table INNER JOIN")
    {
        // Test INNER JOIN across all three tables
        std::string query =
            "SELECT c.name as customer_name, p.name as product_name, o.quantity, o.total_price "
            "FROM test_customers c "
            "INNER JOIN test_orders o ON c.customer_id = o.customer_id "
            "INNER JOIN test_products p ON o.product_id = p.product_id "
            "ORDER BY c.name, p.name";

        auto rs = conn->executeQuery(query);

        // Verify results
        std::vector<std::tuple<std::string, std::string, int, double>> expectedResults = {
            {"Alice Brown", "Smartphone", 1, 499.99},
            {"Bob Johnson", "Headphones", 3, 299.97},
            {"Bob Johnson", "Laptop", 1, 999.99},
            {"Bob Johnson", "Monitor", 2, 399.98},
            {"Charlie Davis", "Tablet", 1, 299.99},
            {"Jane Smith", "Smartphone", 1, 499.99},
            {"John Doe", "Laptop", 1, 999.99},
            {"John Doe", "Tablet", 2, 599.98}};

        size_t rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            REQUIRE(rs->getString("CUSTOMER_NAME") == std::get<0>(expectedResults[rowCount]));
            REQUIRE(rs->getString("PRODUCT_NAME") == std::get<1>(expectedResults[rowCount]));
            REQUIRE(rs->getInt("QUANTITY") == std::get<2>(expectedResults[rowCount]));
            REQUIRE(rs->getDouble("TOTAL_PRICE") == Catch::Approx(std::get<3>(expectedResults[rowCount])).margin(0.01));

            rowCount++;
        }

        REQUIRE(rowCount == expectedResults.size());
    }

    SECTION("INNER JOIN with WHERE clause")
    {
        // Test INNER JOIN with additional filtering
        std::string query =
            "SELECT c.name as customer_name, p.name as product_name, o.quantity, o.total_price "
            "FROM test_customers c "
            "INNER JOIN test_orders o ON c.customer_id = o.customer_id "
            "INNER JOIN test_products p ON o.product_id = p.product_id "
            "WHERE p.price > 300 AND c.credit_limit > 2000 "
            "ORDER BY o.total_price DESC";

        auto rs = conn->executeQuery(query);

        // Verify results - only customers with credit_limit > 2000 and products with price > 300
        // John Doe has customer_id 1, so credit_limit = 1000 < 2000, should NOT be included
        // Bob Johnson has customer_id 3, so credit_limit = 3000 > 2000
        // Alice Brown has customer_id 4, so credit_limit = 4000 > 2000
        std::vector<std::tuple<std::string, std::string, int, double>> expectedResults = {
            {"Bob Johnson", "Laptop", 1, 999.99},
            {"Alice Brown", "Smartphone", 1, 499.99}};

        size_t rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            REQUIRE(rs->getString("CUSTOMER_NAME") == std::get<0>(expectedResults[rowCount]));
            REQUIRE(rs->getString("PRODUCT_NAME") == std::get<1>(expectedResults[rowCount]));
            REQUIRE(rs->getInt("QUANTITY") == std::get<2>(expectedResults[rowCount]));
            REQUIRE(rs->getDouble("TOTAL_PRICE") == Catch::Approx(std::get<3>(expectedResults[rowCount])).margin(0.01));

            rowCount++;
        }

        REQUIRE(rowCount == expectedResults.size());
    }

    SECTION("INNER JOIN with invalid column")
    {
        // Test INNER JOIN with an invalid column name
        std::string query =
            "SELECT c.customer_id, c.name, o.order_id, o.non_existent_column "
            "FROM test_customers c "
            "INNER JOIN test_orders o ON c.customer_id = o.customer_id";

        // This should throw an exception
        REQUIRE_THROWS_AS(conn->executeQuery(query), cpp_dbc::DBException);
    }

    SECTION("INNER JOIN with type mismatch")
    {
        // Test INNER JOIN with a type mismatch in the join condition
        std::string query =
            "SELECT c.customer_id, c.name, o.order_id "
            "FROM test_customers c "
            "INNER JOIN test_orders o ON c.name = o.customer_id";

        // Firebird is strict about type safety and throws a conversion error
        // when trying to compare VARCHAR with INTEGER. The error may occur
        // during executeQuery or during rs->next() depending on when Firebird
        // evaluates the join condition.
        auto rs = conn->executeQuery(query);
        // The conversion error happens when fetching rows
        REQUIRE_THROWS_AS(rs->next(), cpp_dbc::DBException);
    }

    // Clean up
    try
    {
        conn->executeUpdate("DROP TABLE test_orders");
    }
    catch (...)
    {
    }
    try
    {
        conn->executeUpdate("DROP TABLE test_products");
    }
    catch (...)
    {
    }
    try
    {
        conn->executeUpdate("DROP TABLE test_customers");
    }
    catch (...)
    {
    }

    // Close the connection
    conn->close();
}
#endif