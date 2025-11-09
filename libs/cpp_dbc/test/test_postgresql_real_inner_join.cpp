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

 @file test_postgresql_real_inner_join.cpp
 @brief Tests for PostgreSQL database operations

*/

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/config/yaml_config_loader.hpp>
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include "test_postgresql_common.hpp"

// Helper function to get the path to the test_db_connections.yml file
extern std::string getConfigFilePath();

#if USE_POSTGRESQL
// Test case for PostgreSQL INNER JOIN operations
TEST_CASE("PostgreSQL INNER JOIN operations", "[postgresql_real_inner_join]")
{
    // Skip these tests if we can't connect to PostgreSQL
    if (!postgresql_test_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

    // Load the YAML configuration
    std::string config_path = getConfigFilePath();
    YAML::Node config = YAML::LoadFile(config_path);

    // Find the dev_postgresql configuration
    YAML::Node dbConfig;
    for (size_t i = 0; i < config["databases"].size(); i++)
    {
        YAML::Node db = config["databases"][i];
        if (db["name"].as<std::string>() == "dev_postgresql")
        {
            dbConfig = YAML::Node(db);
            break;
        }
    }

    // Create connection parameters
    std::string type = dbConfig["type"].as<std::string>();
    std::string host = dbConfig["host"].as<std::string>();
    int port = dbConfig["port"].as<int>();
    std::string database = dbConfig["database"].as<std::string>();
    std::string username = dbConfig["username"].as<std::string>();
    std::string password = dbConfig["password"].as<std::string>();

    std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

    // Register the PostgreSQL driver
    cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

    // Get a connection
    auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);
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
        "created_at TIMESTAMP"
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
        "order_date TIMESTAMP, "
        "FOREIGN KEY (customer_id) REFERENCES test_customers(customer_id), "
        "FOREIGN KEY (product_id) REFERENCES test_products(product_id)"
        ")");

    // Insert data into test_customers
    auto customerStmt = conn->prepareStatement(
        "INSERT INTO test_customers (customer_id, name, email, phone, credit_limit, created_at) "
        "VALUES ($1, $2, $3, $4, $5, $6)");

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
        customerStmt->setTimestamp(6, "2023-01-" + std::to_string(customer.first + 10) + " 10:00:00");
        customerStmt->executeUpdate();
    }

    // Insert data into test_products
    auto productStmt = conn->prepareStatement(
        "INSERT INTO test_products (product_id, name, description, price, stock_quantity, is_active) "
        "VALUES ($1, $2, $3, $4, $5, $6)");

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
        productStmt->setBoolean(6, id % 2 == 1); // Odd IDs are active
        productStmt->executeUpdate();
    }

    // Insert data into test_orders
    auto orderStmt = conn->prepareStatement(
        "INSERT INTO test_orders (order_id, customer_id, product_id, quantity, total_price, order_date) "
        "VALUES ($1, $2, $3, $4, $5, $6)");

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

        int rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            REQUIRE(rs->getInt("customer_id") == std::get<0>(expectedResults[rowCount]));
            REQUIRE(rs->getString("name") == std::get<1>(expectedResults[rowCount]));
            REQUIRE(rs->getInt("order_id") == std::get<2>(expectedResults[rowCount]));
            REQUIRE(rs->getDouble("total_price") == Catch::Approx(std::get<3>(expectedResults[rowCount])).margin(0.01));

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

        int rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            REQUIRE(rs->getString("customer_name") == std::get<0>(expectedResults[rowCount]));
            REQUIRE(rs->getString("product_name") == std::get<1>(expectedResults[rowCount]));
            REQUIRE(rs->getInt("quantity") == std::get<2>(expectedResults[rowCount]));
            REQUIRE(rs->getDouble("total_price") == Catch::Approx(std::get<3>(expectedResults[rowCount])).margin(0.01));

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
            "ORDER BY o.total_price DESC, c.name, p.name";

        auto rs = conn->executeQuery(query);

        // Verify results - only customers with credit_limit > 2000 and products with price > 300
        std::vector<std::tuple<std::string, std::string, int, double>> expectedResults = {
            {"Bob Johnson", "Laptop", 1, 999.99},
            {"Alice Brown", "Smartphone", 1, 499.99}};

        int rowCount = 0;
        while (rs->next())
        {
            REQUIRE(rowCount < expectedResults.size());

            REQUIRE(rs->getString("customer_name") == std::get<0>(expectedResults[rowCount]));
            REQUIRE(rs->getString("product_name") == std::get<1>(expectedResults[rowCount]));
            REQUIRE(rs->getInt("quantity") == std::get<2>(expectedResults[rowCount]));
            REQUIRE(rs->getDouble("total_price") == Catch::Approx(std::get<3>(expectedResults[rowCount])).margin(0.01));

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
            "INNER JOIN test_orders o ON c.name = o.customer_id::text";

        // This should execute but return no results due to type mismatch
        auto rs = conn->executeQuery(query);
        REQUIRE_FALSE(rs->next());
    }

    // Clean up
    conn->executeUpdate("DROP TABLE IF EXISTS test_orders");
    conn->executeUpdate("DROP TABLE IF EXISTS test_products");
    conn->executeUpdate("DROP TABLE IF EXISTS test_customers");

    // Close the connection
    conn->close();
}
#endif