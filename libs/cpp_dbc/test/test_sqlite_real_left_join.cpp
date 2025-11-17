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

 @file test_sqlite_real_left_join.cpp
 @brief Tests for SQLite database operations

*/

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <yaml-cpp/yaml.h>
#include <optional>
#include <cpp_dbc/cpp_dbc.hpp>
#if USE_SQLITE
#include <cpp_dbc/drivers/driver_sqlite.hpp>
#endif
#include <string>
#include <memory>
#include <vector>
#include <iostream>

// Helper function to get the path to the test_db_connections.yml file
extern std::string getConfigFilePath();

#if USE_SQLITE
// Test case for SQLite LEFT JOIN operations
TEST_CASE("SQLite LEFT JOIN operations", "[sqlite_real_left_join]")
{
    // Load the YAML configuration
    std::string config_path = getConfigFilePath();
    YAML::Node config = YAML::LoadFile(config_path);

    // Find the test_sqlite configuration
    YAML::Node dbConfig;
    if (config["databases"].IsSequence())
    {
        for (std::size_t i = 0; i < config["databases"].size(); ++i)
        {
            YAML::Node db = config["databases"][i];
            if (db["name"].as<std::string>() == "test_sqlite")
            {
                dbConfig = db;
                break;
            }
        }
    }

    // Check that the database configuration was found
    REQUIRE(dbConfig.IsDefined());

    // Create connection string
    std::string type = dbConfig["type"].as<std::string>();
    std::string database = dbConfig["database"].as<std::string>();

    // Test connection
    std::string connStr = "cpp_dbc:" + type + "://" + database;

    // Register the SQLite driver
    cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDriver>());

    try
    {
        // Attempt to connect to SQLite
        std::cout << "Attempting to connect to SQLite with connection string: " << connStr << std::endl;

        auto conn = cpp_dbc::DriverManager::getConnection(connStr, "", "");

        // Create test tables
        conn->executeUpdate("DROP TABLE IF EXISTS test_orders");
        conn->executeUpdate("DROP TABLE IF EXISTS test_customers");
        conn->executeUpdate("DROP TABLE IF EXISTS test_products");

        // Create test_customers table
        conn->executeUpdate(
            "CREATE TABLE test_customers ("
            "customer_id INTEGER PRIMARY KEY, "
            "name TEXT, "
            "email TEXT, "
            "phone TEXT, "
            "credit_limit REAL, "
            "created_at TEXT"
            ")");

        // Create test_products table
        conn->executeUpdate(
            "CREATE TABLE test_products ("
            "product_id INTEGER PRIMARY KEY, "
            "name TEXT, "
            "description TEXT, "
            "price REAL, "
            "stock_quantity INTEGER, "
            "is_active INTEGER"
            ")");

        // Create test_orders table
        conn->executeUpdate(
            "CREATE TABLE test_orders ("
            "order_id INTEGER PRIMARY KEY, "
            "customer_id INTEGER, "
            "product_id INTEGER, "
            "quantity INTEGER, "
            "total_price REAL, "
            "order_date TEXT, "
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

        SECTION("Basic LEFT JOIN")
        {
            // Test LEFT JOIN between customers and orders
            std::string query =
                "SELECT c.customer_id, c.name, o.order_id, o.total_price "
                "FROM test_customers c "
                "LEFT JOIN test_orders o ON c.customer_id = o.customer_id "
                "ORDER BY c.customer_id, o.order_id";

            auto rs = conn->executeQuery(query);

            // Verify results - all customers should be included, even those without orders
            std::vector<std::tuple<int, std::string, std::optional<int>, std::optional<double>>> expectedResults = {
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

                REQUIRE(rs->getInt("customer_id") == std::get<0>(expectedResults[rowCount]));
                REQUIRE(rs->getString("name") == std::get<1>(expectedResults[rowCount]));

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

        SECTION("Three-table LEFT JOIN")
        {
            // Test LEFT JOIN across all three tables
            std::string query =
                "SELECT c.name as customer_name, p.name as product_name, o.quantity, o.total_price "
                "FROM test_customers c "
                "LEFT JOIN test_orders o ON c.customer_id = o.customer_id "
                "LEFT JOIN test_products p ON o.product_id = p.product_id "
                "ORDER BY c.name, IFNULL(p.name, '')";

            auto rs = conn->executeQuery(query);

            // Verify results - all customers should be included
            std::vector<std::tuple<std::string, std::optional<std::string>, std::optional<int>, std::optional<double>>> expectedResults = {
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

                REQUIRE(rs->getString("customer_name") == std::get<0>(expectedResults[rowCount]));

                if (std::get<1>(expectedResults[rowCount]))
                {
                    REQUIRE_FALSE(rs->isNull("product_name"));
                    REQUIRE(rs->getString("product_name") == std::get<1>(expectedResults[rowCount]).value());
                    REQUIRE(rs->getInt("quantity") == std::get<2>(expectedResults[rowCount]).value());
                    REQUIRE(rs->getDouble("total_price") == Catch::Approx(std::get<3>(expectedResults[rowCount]).value()).margin(0.01));
                }
                else
                {
                    REQUIRE(rs->isNull("product_name"));
                    REQUIRE(rs->isNull("quantity"));
                    REQUIRE(rs->isNull("total_price"));
                }

                rowCount++;
            }

            REQUIRE(rowCount == expectedResults.size());
        }

        SECTION("LEFT JOIN with WHERE clause")
        {
            // Test LEFT JOIN with additional filtering
            std::string query =
                "SELECT c.name as customer_name, p.name as product_name, o.quantity, o.total_price "
                "FROM test_customers c "
                "LEFT JOIN test_orders o ON c.customer_id = o.customer_id "
                "LEFT JOIN test_products p ON o.product_id = p.product_id "
                "WHERE c.credit_limit > 3000 "
                "ORDER BY c.name, IFNULL(p.name, '')";

            auto rs = conn->executeQuery(query);

            // Verify results - only customers with credit_limit > 3000
            std::vector<std::tuple<std::string, std::optional<std::string>, std::optional<int>, std::optional<double>>> expectedResults = {
                {"Alice Brown", "Smartphone", 1, 499.99},
                {"Charlie Davis", "Tablet", 1, 299.99},
                {"Eva Wilson", std::nullopt, std::nullopt, std::nullopt},
                {"Frank Miller", std::nullopt, std::nullopt, std::nullopt}};

            size_t rowCount = 0;
            while (rs->next())
            {
                REQUIRE(rowCount < expectedResults.size());

                REQUIRE(rs->getString("customer_name") == std::get<0>(expectedResults[rowCount]));

                if (std::get<1>(expectedResults[rowCount]))
                {
                    REQUIRE_FALSE(rs->isNull("product_name"));
                    REQUIRE(rs->getString("product_name") == std::get<1>(expectedResults[rowCount]).value());
                    REQUIRE(rs->getInt("quantity") == std::get<2>(expectedResults[rowCount]).value());
                    REQUIRE(rs->getDouble("total_price") == Catch::Approx(std::get<3>(expectedResults[rowCount]).value()).margin(0.01));
                }
                else
                {
                    REQUIRE(rs->isNull("product_name"));
                    REQUIRE(rs->isNull("quantity"));
                    REQUIRE(rs->isNull("total_price"));
                }

                rowCount++;
            }

            REQUIRE(rowCount == expectedResults.size());
        }

        SECTION("LEFT JOIN with NULL check")
        {
            // Test LEFT JOIN with NULL check to find customers without orders
            std::string query =
                "SELECT c.customer_id, c.name "
                "FROM test_customers c "
                "LEFT JOIN test_orders o ON c.customer_id = o.customer_id "
                "WHERE o.order_id IS NULL "
                "ORDER BY c.customer_id";

            auto rs = conn->executeQuery(query);

            // Verify results - only customers without orders
            std::vector<std::pair<int, std::string>> expectedResults = {
                {6, "Eva Wilson"},
                {7, "Frank Miller"}};

            size_t rowCount = 0;
            while (rs->next())
            {
                REQUIRE(rowCount < expectedResults.size());

                REQUIRE(rs->getInt("customer_id") == expectedResults[rowCount].first);
                REQUIRE(rs->getString("name") == expectedResults[rowCount].second);

                rowCount++;
            }

            REQUIRE(rowCount == expectedResults.size());
        }

        SECTION("LEFT JOIN with invalid column")
        {
            // Test LEFT JOIN with an invalid column name
            std::string query =
                "SELECT c.customer_id, c.name, o.order_id, o.non_existent_column "
                "FROM test_customers c "
                "LEFT JOIN test_orders o ON c.customer_id = o.customer_id";

            // This should throw an exception
            REQUIRE_THROWS_AS(conn->executeQuery(query), cpp_dbc::DBException);
        }

        SECTION("LEFT JOIN with type mismatch")
        {
            // Test LEFT JOIN with a type mismatch in the join condition
            std::string query =
                "SELECT c.customer_id, c.name, o.order_id "
                "FROM test_customers c "
                "LEFT JOIN test_orders o ON c.name = o.customer_id";

            // This should execute but return only NULL values for the right side
            auto rs = conn->executeQuery(query);

            size_t rowCount = 0;
            while (rs->next())
            {
                REQUIRE_FALSE(rs->isNull("customer_id"));
                REQUIRE_FALSE(rs->isNull("name"));
                REQUIRE(rs->isNull("order_id"));
                rowCount++;
            }

            REQUIRE(rowCount == customers.size());
        }

        // Clean up
        conn->executeUpdate("DROP TABLE IF EXISTS test_orders");
        conn->executeUpdate("DROP TABLE IF EXISTS test_products");
        conn->executeUpdate("DROP TABLE IF EXISTS test_customers");

        // Close the connection
        conn->close();
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::string errorMsg = e.what_s();
        std::cout << "SQLite real database error: " << errorMsg << std::endl;
        FAIL("SQLite real database test failed: " + std::string(e.what_s()));
    }
}
#endif