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
 * @file test_scylla_real_inner_join.cpp
 * @brief Tests for ScyllaDB operations that emulate INNER JOIN functionality
 */

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <map>
#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "test_scylla_common.hpp"

#if USE_SCYLLA
// Test case for ScyllaDB operations that emulate INNER JOIN
TEST_CASE("ScyllaDB INNER JOIN emulation", "[scylla_real_inner_join]")
{
    // Skip these tests if we can't connect to ScyllaDB
    if (!scylla_test_helpers::canConnectToScylla())
    {
        SKIP("Cannot connect to ScyllaDB database");
        return;
    }

    // Get ScyllaDB configuration
    auto dbConfig = scylla_test_helpers::getScyllaConfig("dev_scylla");
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string host = dbConfig.getHost();
    int port = dbConfig.getPort();
    std::string keyspace = dbConfig.getDatabase();
    std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;

    // Register the ScyllaDB driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Scylla::ScyllaDBDriver>());

    // Get a connection
    auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
        cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
    REQUIRE(conn != nullptr);

    // Create test tables
    // In ScyllaDB, we'll create separate tables for customers, products, and orders
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_customers");
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_products");
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_orders");

    // Create test_customers table
    conn->executeUpdate(
        "CREATE TABLE " + keyspace + ".test_customers ("
                                     "customer_id int PRIMARY KEY, "
                                     "name text, "
                                     "email text, "
                                     "phone text, "
                                     "credit_limit double, "
                                     "created_at timestamp"
                                     ")");

    // Create test_products table
    conn->executeUpdate(
        "CREATE TABLE " + keyspace + ".test_products ("
                                     "product_id int PRIMARY KEY, "
                                     "name text, "
                                     "description text, "
                                     "price double, "
                                     "stock_quantity int, "
                                     "is_active boolean"
                                     ")");

    // Create test_orders table
    // In ScyllaDB, we can't use foreign keys, but we can store the references
    conn->executeUpdate(
        "CREATE TABLE " + keyspace + ".test_orders ("
                                     "order_id int, "
                                     "customer_id int, "
                                     "product_id int, "
                                     "quantity int, "
                                     "total_price double, "
                                     "order_date timestamp, "
                                     "PRIMARY KEY (order_id)"
                                     ")");

    // Insert data into test_customers
    auto customerStmt = conn->prepareStatement(
        "INSERT INTO " + keyspace + ".test_customers (customer_id, name, email, phone, credit_limit, created_at) "
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
        customerStmt->setTimestamp(6, "2023-01-" + std::to_string(customer.first + 10) + " 10:00:00");
        customerStmt->executeUpdate();
    }

    // Insert data into test_products
    auto productStmt = conn->prepareStatement(
        "INSERT INTO " + keyspace + ".test_products (product_id, name, description, price, stock_quantity, is_active) "
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
        productStmt->setBoolean(6, id % 2 == 1); // Odd IDs are active
        productStmt->executeUpdate();
    }

    // Insert data into test_orders
    auto orderStmt = conn->prepareStatement(
        "INSERT INTO " + keyspace + ".test_orders (order_id, customer_id, product_id, quantity, total_price, order_date) "
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

    SECTION("Basic INNER JOIN emulation")
    {
        // In ScyllaDB, we need to emulate INNER JOIN by performing multiple queries
        // First, get all orders
        auto rsOrders = conn->executeQuery("SELECT * FROM " + keyspace + ".test_orders");

        // Prepare a map to store the joined results
        std::vector<std::tuple<int, std::string, int, double>> joinResults;

        // For each order, get the customer information
        while (rsOrders->next())
        {
            int orderId = rsOrders->getInt("order_id");
            int customerId = rsOrders->getInt("customer_id");
            double totalPrice = rsOrders->getDouble("total_price");

            // Get customer information
            auto customerQuery = conn->prepareStatement("SELECT * FROM " + keyspace + ".test_customers WHERE customer_id = ?");
            customerQuery->setInt(1, customerId);
            auto rsCustomer = customerQuery->executeQuery();

            if (rsCustomer->next())
            {
                std::string customerName = rsCustomer->getString("name");

                // Add to results
                joinResults.push_back(std::make_tuple(customerId, customerName, orderId, totalPrice));
            }
        }

        // Sort results by customer_id and order_id to match the expected order
        std::sort(joinResults.begin(), joinResults.end(),
                  [](const auto &a, const auto &b)
                  {
                      if (std::get<0>(a) != std::get<0>(b))
                      {
                          return std::get<0>(a) < std::get<0>(b);
                      }
                      return std::get<2>(a) < std::get<2>(b);
                  });

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

        REQUIRE(joinResults.size() == expectedResults.size());

        for (size_t i = 0; i < joinResults.size(); i++)
        {
            REQUIRE(std::get<0>(joinResults[i]) == std::get<0>(expectedResults[i]));
            REQUIRE(std::get<1>(joinResults[i]) == std::get<1>(expectedResults[i]));
            REQUIRE(std::get<2>(joinResults[i]) == std::get<2>(expectedResults[i]));
            REQUIRE(std::abs(std::get<3>(joinResults[i]) - std::get<3>(expectedResults[i])) < 0.01);
        }
    }

    SECTION("Three-table INNER JOIN emulation")
    {
        // In ScyllaDB, we need to emulate a three-table INNER JOIN
        // First, get all orders
        auto rsOrders = conn->executeQuery("SELECT * FROM " + keyspace + ".test_orders");

        // Prepare a map to store the joined results
        std::vector<std::tuple<std::string, std::string, int, double>> joinResults;

        // For each order, get the customer and product information
        while (rsOrders->next())
        {
            // We don't need orderId for this join
            int customerId = rsOrders->getInt("customer_id");
            int productId = rsOrders->getInt("product_id");
            int quantity = rsOrders->getInt("quantity");
            double totalPrice = rsOrders->getDouble("total_price");

            // Get customer information
            auto customerQuery = conn->prepareStatement("SELECT * FROM " + keyspace + ".test_customers WHERE customer_id = ?");
            customerQuery->setInt(1, customerId);
            auto rsCustomer = customerQuery->executeQuery();

            // Get product information
            auto productQuery = conn->prepareStatement("SELECT * FROM " + keyspace + ".test_products WHERE product_id = ?");
            productQuery->setInt(1, productId);
            auto rsProduct = productQuery->executeQuery();

            if (rsCustomer->next() && rsProduct->next())
            {
                std::string customerName = rsCustomer->getString("name");
                std::string productName = rsProduct->getString("name");

                // Add to results
                joinResults.push_back(std::make_tuple(customerName, productName, quantity, totalPrice));
            }
        }

        // Sort results by customer_name and product_name to match the expected order
        std::sort(joinResults.begin(), joinResults.end());

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

        REQUIRE(joinResults.size() == expectedResults.size());

        for (size_t i = 0; i < joinResults.size(); i++)
        {
            REQUIRE(std::get<0>(joinResults[i]) == std::get<0>(expectedResults[i]));
            REQUIRE(std::get<1>(joinResults[i]) == std::get<1>(expectedResults[i]));
            REQUIRE(std::get<2>(joinResults[i]) == std::get<2>(expectedResults[i]));
            REQUIRE(std::abs(std::get<3>(joinResults[i]) - std::get<3>(expectedResults[i])) < 0.01);
        }
    }

    SECTION("INNER JOIN with filtering emulation")
    {
        // In ScyllaDB, we need to emulate a filtered INNER JOIN
        // First, get all orders
        auto rsOrders = conn->executeQuery("SELECT * FROM " + keyspace + ".test_orders");

        // Prepare a map to store the joined results
        std::vector<std::tuple<std::string, std::string, int, double>> joinResults;

        // For each order, get the customer and product information with filtering
        while (rsOrders->next())
        {
            int customerId = rsOrders->getInt("customer_id");
            int productId = rsOrders->getInt("product_id");
            int quantity = rsOrders->getInt("quantity");
            double totalPrice = rsOrders->getDouble("total_price");

            // Get customer information
            auto customerQuery = conn->prepareStatement("SELECT * FROM " + keyspace + ".test_customers WHERE customer_id = ?");
            customerQuery->setInt(1, customerId);
            auto rsCustomer = customerQuery->executeQuery();

            // Get product information
            auto productQuery = conn->prepareStatement("SELECT * FROM " + keyspace + ".test_products WHERE product_id = ?");
            productQuery->setInt(1, productId);
            auto rsProduct = productQuery->executeQuery();

            if (rsCustomer->next() && rsProduct->next())
            {
                std::string customerName = rsCustomer->getString("name");
                std::string productName = rsProduct->getString("name");
                double creditLimit = rsCustomer->getDouble("credit_limit");
                double productPrice = rsProduct->getDouble("price");

                // Apply filtering: product price > 300 AND customer credit_limit > 2000
                if (productPrice > 300 && creditLimit > 2000)
                {
                    // Add to results
                    joinResults.push_back(std::make_tuple(customerName, productName, quantity, totalPrice));
                }
            }
        }

        // Sort results by total_price in descending order
        std::sort(joinResults.begin(), joinResults.end(),
                  [](const auto &a, const auto &b)
                  {
                      return std::get<3>(a) > std::get<3>(b);
                  });

        // Verify results
        std::vector<std::tuple<std::string, std::string, int, double>> expectedResults = {
            {"Bob Johnson", "Laptop", 1, 999.99},
            {"Alice Brown", "Smartphone", 1, 499.99}};

        REQUIRE(joinResults.size() == expectedResults.size());

        for (size_t i = 0; i < joinResults.size(); i++)
        {
            REQUIRE(std::get<0>(joinResults[i]) == std::get<0>(expectedResults[i]));
            REQUIRE(std::get<1>(joinResults[i]) == std::get<1>(expectedResults[i]));
            REQUIRE(std::get<2>(joinResults[i]) == std::get<2>(expectedResults[i]));
            REQUIRE(std::abs(std::get<3>(joinResults[i]) - std::get<3>(expectedResults[i])) < 0.01);
        }
    }

    // Clean up
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_orders");
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_products");
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_customers");

    // Close the connection
    conn->close();
}
#else
TEST_CASE("ScyllaDB INNER JOIN emulation (skipped)", "[scylla_real_inner_join]")
{
    SKIP("ScyllaDB support is not enabled");
}
#endif