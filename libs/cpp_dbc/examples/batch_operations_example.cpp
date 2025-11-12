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

 @file batch_operations_example.cpp
 @brief Example demonstrating batch operations with prepared statements

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
#include <chrono>
#include <random>
#include <iomanip>

// Helper function to generate random product data
std::vector<std::tuple<int, std::string, std::string, double, int>> generateProductData(int count, int startId = 1)
{
    std::vector<std::tuple<int, std::string, std::string, double, int>> products;

    // Random generators
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> priceDistrib(10.0, 1000.0);
    std::uniform_int_distribution<> stockDistrib(1, 1000);

    // Product categories
    std::vector<std::string> categories = {
        "Electronics", "Clothing", "Home & Kitchen", "Books", "Sports",
        "Toys", "Beauty", "Automotive", "Health", "Garden"};

    // Product name prefixes
    std::vector<std::string> prefixes = {
        "Premium", "Deluxe", "Basic", "Professional", "Ultra",
        "Advanced", "Smart", "Eco", "Compact", "Portable"};

    // Product name suffixes
    std::vector<std::string> suffixes = {
        "Pro", "Plus", "Lite", "Max", "Mini", "XL", "S", "Elite", "Prime", "Ultimate"};

    // Product types
    std::vector<std::string> types = {
        "Laptop", "Phone", "Shirt", "Pants", "Blender", "Chair", "Table", "Novel",
        "Textbook", "Ball", "Toy", "Cream", "Tool", "Vitamin", "Plant"};

    // Generate random products
    for (int i = 0; i < count; i++)
    {
        int id = startId + i;

        // Generate a random product name
        std::string prefix = prefixes[gen() % prefixes.size()];
        std::string type = types[gen() % types.size()];
        std::string suffix = suffixes[gen() % suffixes.size()];
        std::string name = prefix + " " + type + " " + suffix;

        // Generate a random category
        std::string category = categories[gen() % categories.size()];

        // Generate random price and stock
        double price = std::round(priceDistrib(gen) * 100) / 100;
        int stock = stockDistrib(gen);

        products.emplace_back(id, name, category, price, stock);
    }

    return products;
}

// Helper function to generate random order data
std::vector<std::tuple<int, int, int, std::string, int, double>> generateOrderData(
    int count,
    int startId,
    const std::vector<int> &customerIds,
    const std::vector<std::tuple<int, std::string, std::string, double, int>> &products)
{

    std::vector<std::tuple<int, int, int, std::string, int, double>> orders;

    // Random generators
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> customerDistrib(0, customerIds.size() - 1);
    std::uniform_int_distribution<> productDistrib(0, products.size() - 1);
    std::uniform_int_distribution<> quantityDistrib(1, 5);

    // Date range for orders (last 365 days)
    auto now = std::chrono::system_clock::now();
    auto today = std::chrono::floor<std::chrono::days>(now);
    std::uniform_int_distribution<> dayDistrib(0, 365);

    // Generate random orders
    for (int i = 0; i < count; i++)
    {
        int orderId = startId + i;

        // Select a random customer
        int customerIndex = customerDistrib(gen);
        int customerId = customerIds[customerIndex];

        // Select a random product
        int productIndex = productDistrib(gen);
        int productId = std::get<0>(products[productIndex]);
        double productPrice = std::get<3>(products[productIndex]);

        // Generate a random quantity
        int quantity = quantityDistrib(gen);

        // Calculate total price
        double totalPrice = productPrice * quantity;

        // Generate a random date within the last year
        auto orderDate = today - std::chrono::days(dayDistrib(gen));
        auto ymd = std::chrono::year_month_day(orderDate);
        std::string dateStr = std::to_string(static_cast<int>(ymd.year())) + "-" +
                              std::to_string(static_cast<unsigned>(ymd.month())) + "-" +
                              std::to_string(static_cast<unsigned>(ymd.day()));

        orders.emplace_back(orderId, customerId, productId, dateStr, quantity, totalPrice);
    }

    return orders;
}

// Function to demonstrate basic batch insert
void demonstrateBasicBatchInsert(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== Basic Batch Insert Example ===\n"
              << std::endl;

    try
    {
        // Create a test table
        conn->executeUpdate("DROP TABLE IF EXISTS batch_products");
        conn->executeUpdate(
            "CREATE TABLE batch_products ("
            "product_id INT PRIMARY KEY, "
            "name VARCHAR(100), "
            "category VARCHAR(50), "
            "price DECIMAL(10,2), "
            "stock_quantity INT"
            ")");

        std::cout << "Table created successfully." << std::endl;

        // Generate sample product data (100 products)
        auto products = generateProductData(100);
        std::cout << "Generated " << products.size() << " product records." << std::endl;

        // Prepare a statement for batch insert
        auto pstmt = conn->prepareStatement(
            "INSERT INTO batch_products (product_id, name, category, price, stock_quantity) "
            "VALUES (?, ?, ?, ?, ?)");

        // Start timing
        auto startTime = std::chrono::high_resolution_clock::now();

        // Execute multiple statements in a loop (simulating batch processing)
        int totalRowsAffected = 0;
        for (const auto &product : products)
        {
            pstmt->setInt(1, std::get<0>(product));
            pstmt->setString(2, std::get<1>(product));
            pstmt->setString(3, std::get<2>(product));
            pstmt->setDouble(4, std::get<3>(product));
            pstmt->setInt(5, std::get<4>(product));
            totalRowsAffected += pstmt->executeUpdate();
        }

        // End timing
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << "Batch insert completed: " << totalRowsAffected << " rows affected." << std::endl;
        std::cout << "Execution time: " << duration << " ms" << std::endl;

        // Verify the data was inserted
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_products");
        if (rs->next())
        {
            std::cout << "Verified row count: " << rs->getInt("count") << std::endl;
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what() << std::endl;
    }
}

// Function to demonstrate batch insert with transactions
void demonstrateBatchWithTransaction(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== Batch Insert with Transaction Example ===\n"
              << std::endl;

    try
    {
        // Create a test table
        conn->executeUpdate("DROP TABLE IF EXISTS batch_orders");
        conn->executeUpdate(
            "CREATE TABLE batch_orders ("
            "order_id INT PRIMARY KEY, "
            "customer_id INT, "
            "product_id INT, "
            "order_date DATE, "
            "quantity INT, "
            "total_price DECIMAL(10,2)"
            ")");

        std::cout << "Table created successfully." << std::endl;

        // Sample customer IDs
        std::vector<int> customerIds = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

        // Generate sample product data (for reference in orders)
        auto products = generateProductData(20);

        // Generate sample order data (1000 orders)
        auto orders = generateOrderData(1000, 1, customerIds, products);
        std::cout << "Generated " << orders.size() << " order records." << std::endl;

        // Prepare a statement for batch insert
        auto pstmt = conn->prepareStatement(
            "INSERT INTO batch_orders (order_id, customer_id, product_id, order_date, quantity, total_price) "
            "VALUES (?, ?, ?, ?, ?, ?)");

        // Disable auto-commit to start a transaction
        conn->setAutoCommit(false);
        std::cout << "Started transaction (auto-commit disabled)." << std::endl;

        // Start timing
        auto startTime = std::chrono::high_resolution_clock::now();

        // Execute multiple statements in a loop (simulating batch processing)
        int totalRowsAffected = 0;
        for (const auto &order : orders)
        {
            pstmt->setInt(1, std::get<0>(order));
            pstmt->setInt(2, std::get<1>(order));
            pstmt->setInt(3, std::get<2>(order));
            pstmt->setString(4, std::get<3>(order));
            pstmt->setInt(5, std::get<4>(order));
            pstmt->setDouble(6, std::get<5>(order));
            totalRowsAffected += pstmt->executeUpdate();
        }

        // Commit the transaction
        conn->commit();
        std::cout << "Transaction committed." << std::endl;

        // End timing
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << "Batch insert completed: " << totalRowsAffected << " rows affected." << std::endl;
        std::cout << "Execution time: " << duration << " ms" << std::endl;

        // Restore auto-commit mode
        conn->setAutoCommit(true);

        // Verify the data was inserted
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_orders");
        if (rs->next())
        {
            std::cout << "Verified row count: " << rs->getInt("count") << std::endl;
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        // If an error occurs, roll back the transaction
        try
        {
            conn->rollback();
            std::cout << "Transaction rolled back due to error." << std::endl;
        }
        catch (const cpp_dbc::DBException &rollbackError)
        {
            std::cerr << "Rollback error: " << rollbackError.what() << std::endl;
        }

        // Restore auto-commit mode
        conn->setAutoCommit(true);

        std::cerr << "Database error: " << e.what() << std::endl;
    }
}

// Function to demonstrate batch update
void demonstrateBatchUpdate(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== Batch Update Example ===\n"
              << std::endl;

    try
    {
        // First, make sure we have data to update
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_products");
        if (!rs->next() || rs->getInt("count") == 0)
        {
            std::cout << "No products to update. Please run the batch insert example first." << std::endl;
            return;
        }

        // Get the product IDs to update
        std::vector<int> productIds;
        rs = conn->executeQuery("SELECT product_id FROM batch_products ORDER BY product_id LIMIT 50");
        while (rs->next())
        {
            productIds.push_back(rs->getInt("product_id"));
        }

        std::cout << "Found " << productIds.size() << " products to update." << std::endl;

        // Prepare a statement for batch update
        auto pstmt = conn->prepareStatement(
            "UPDATE batch_products SET price = price * 1.1, stock_quantity = ? WHERE product_id = ?");

        // Random generator for new stock quantities
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> stockDistrib(10, 500);

        // Start timing
        auto startTime = std::chrono::high_resolution_clock::now();

        // Execute multiple statements in a loop (simulating batch processing)
        int totalRowsAffected = 0;
        for (int productId : productIds)
        {
            int newStock = stockDistrib(gen);
            pstmt->setInt(1, newStock);
            pstmt->setInt(2, productId);
            totalRowsAffected += pstmt->executeUpdate();
        }

        // End timing
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << "Batch update completed: " << totalRowsAffected << " rows affected." << std::endl;
        std::cout << "Execution time: " << duration << " ms" << std::endl;

        // Verify the updates
        rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_products WHERE price > 100");
        if (rs->next())
        {
            std::cout << "Products with price > 100: " << rs->getInt("count") << std::endl;
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what() << std::endl;
    }
}

// Function to demonstrate batch delete
void demonstrateBatchDelete(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== Batch Delete Example ===\n"
              << std::endl;

    try
    {
        // First, make sure we have data to delete
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_orders");
        if (!rs->next() || rs->getInt("count") == 0)
        {
            std::cout << "No orders to delete. Please run the batch with transaction example first." << std::endl;
            return;
        }

        // Get some order IDs to delete (orders with small quantities)
        std::vector<int> orderIds;
        rs = conn->executeQuery("SELECT order_id FROM batch_orders WHERE quantity = 1 LIMIT 200");
        while (rs->next())
        {
            orderIds.push_back(rs->getInt("order_id"));
        }

        std::cout << "Found " << orderIds.size() << " orders to delete." << std::endl;

        // Prepare a statement for batch delete
        auto pstmt = conn->prepareStatement("DELETE FROM batch_orders WHERE order_id = ?");

        // Start timing
        auto startTime = std::chrono::high_resolution_clock::now();

        // Execute multiple statements in a loop (simulating batch processing)
        int totalRowsAffected = 0;
        for (int orderId : orderIds)
        {
            pstmt->setInt(1, orderId);
            totalRowsAffected += pstmt->executeUpdate();
        }

        // End timing
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << "Batch delete completed: " << totalRowsAffected << " rows affected." << std::endl;
        std::cout << "Execution time: " << duration << " ms" << std::endl;

        // Verify the deletes
        rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_orders");
        if (rs->next())
        {
            std::cout << "Remaining orders: " << rs->getInt("count") << std::endl;
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what() << std::endl;
    }
}

// Function to demonstrate batch performance comparison
void demonstrateBatchPerformanceComparison(std::shared_ptr<cpp_dbc::Connection> conn)
{
    std::cout << "\n=== Batch Performance Comparison ===\n"
              << std::endl;

    try
    {
        // Create a test table
        conn->executeUpdate("DROP TABLE IF EXISTS performance_test");
        conn->executeUpdate(
            "CREATE TABLE performance_test ("
            "id INT PRIMARY KEY, "
            "name VARCHAR(100), "
            "value DOUBLE, "
            "created_at TIMESTAMP"
            ")");

        std::cout << "Table created successfully." << std::endl;

        // Number of records to insert
        const int recordCount = 1000;

        // Generate test data
        std::vector<std::tuple<int, std::string, double>> testData;
        for (int i = 1; i <= recordCount; i++)
        {
            std::string name = "Test Item " + std::to_string(i);
            double value = i * 1.5;
            testData.emplace_back(i, name, value);
        }

        // Method 1: Individual inserts
        std::cout << "\nMethod 1: Individual inserts" << std::endl;

        auto startTime1 = std::chrono::high_resolution_clock::now();

        auto pstmt1 = conn->prepareStatement(
            "INSERT INTO performance_test (id, name, value, created_at) "
            "VALUES (?, ?, ?, CURRENT_TIMESTAMP)");

        int rowsAffected1 = 0;
        for (const auto &data : testData)
        {
            pstmt1->setInt(1, std::get<0>(data));
            pstmt1->setString(2, std::get<1>(data));
            pstmt1->setDouble(3, std::get<2>(data));
            rowsAffected1 += pstmt1->executeUpdate();
        }

        auto endTime1 = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime1 - startTime1).count();

        std::cout << "Individual inserts completed: " << rowsAffected1 << " rows affected." << std::endl;
        std::cout << "Execution time: " << duration1 << " ms" << std::endl;

        // Clean up for next test
        conn->executeUpdate("DELETE FROM performance_test");

        // Method 2: Simulated batch insert (still individual inserts but in a single loop)
        std::cout << "\nMethod 2: Simulated batch insert" << std::endl;

        auto startTime2 = std::chrono::high_resolution_clock::now();

        auto pstmt2 = conn->prepareStatement(
            "INSERT INTO performance_test (id, name, value, created_at) "
            "VALUES (?, ?, ?, CURRENT_TIMESTAMP)");

        int rowsAffected2 = 0;
        for (const auto &data : testData)
        {
            pstmt2->setInt(1, std::get<0>(data));
            pstmt2->setString(2, std::get<1>(data));
            pstmt2->setDouble(3, std::get<2>(data));
            rowsAffected2 += pstmt2->executeUpdate();
        }

        auto endTime2 = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime2 - startTime2).count();

        std::cout << "Simulated batch insert completed: " << rowsAffected2 << " rows affected." << std::endl;
        std::cout << "Execution time: " << duration2 << " ms" << std::endl;

        // Method 3: Transaction with individual inserts
        std::cout << "\nMethod 3: Transaction with individual inserts" << std::endl;

        // Clean up for next test
        conn->executeUpdate("DELETE FROM performance_test");

        auto startTime3 = std::chrono::high_resolution_clock::now();

        conn->setAutoCommit(false);

        auto pstmt3 = conn->prepareStatement(
            "INSERT INTO performance_test (id, name, value, created_at) "
            "VALUES (?, ?, ?, CURRENT_TIMESTAMP)");

        int rowsAffected3 = 0;
        for (const auto &data : testData)
        {
            pstmt3->setInt(1, std::get<0>(data));
            pstmt3->setString(2, std::get<1>(data));
            pstmt3->setDouble(3, std::get<2>(data));
            rowsAffected3 += pstmt3->executeUpdate();
        }

        conn->commit();
        conn->setAutoCommit(true);

        auto endTime3 = std::chrono::high_resolution_clock::now();
        auto duration3 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime3 - startTime3).count();

        std::cout << "Transaction with individual inserts completed: " << rowsAffected3 << " rows affected." << std::endl;
        std::cout << "Execution time: " << duration3 << " ms" << std::endl;

        // Method 4: Transaction with simulated batch insert
        std::cout << "\nMethod 4: Transaction with simulated batch insert" << std::endl;

        // Clean up for next test
        conn->executeUpdate("DELETE FROM performance_test");

        auto startTime4 = std::chrono::high_resolution_clock::now();

        conn->setAutoCommit(false);

        auto pstmt4 = conn->prepareStatement(
            "INSERT INTO performance_test (id, name, value, created_at) "
            "VALUES (?, ?, ?, CURRENT_TIMESTAMP)");

        int rowsAffected4 = 0;
        for (const auto &data : testData)
        {
            pstmt4->setInt(1, std::get<0>(data));
            pstmt4->setString(2, std::get<1>(data));
            pstmt4->setDouble(3, std::get<2>(data));
            rowsAffected4 += pstmt4->executeUpdate();
        }

        conn->commit();
        conn->setAutoCommit(true);

        auto endTime4 = std::chrono::high_resolution_clock::now();
        auto duration4 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime4 - startTime4).count();

        std::cout << "Transaction with simulated batch insert completed: " << rowsAffected4 << " rows affected." << std::endl;
        std::cout << "Execution time: " << duration4 << " ms" << std::endl;

        // Summary
        std::cout << "\nPerformance Summary:" << std::endl;
        std::cout << "Method 1 (Individual inserts): " << duration1 << " ms" << std::endl;
        std::cout << "Method 2 (Simulated batch insert): " << duration2 << " ms" << std::endl;
        std::cout << "Method 3 (Transaction with individual inserts): " << duration3 << " ms" << std::endl;
        std::cout << "Method 4 (Transaction with simulated batch insert): " << duration4 << " ms" << std::endl;

        // Calculate speedup
        double speedup1to2 = static_cast<double>(duration1) / duration2;
        double speedup1to3 = static_cast<double>(duration1) / duration3;
        double speedup1to4 = static_cast<double>(duration1) / duration4;

        std::cout << "\nSpeedup Factors:" << std::endl;
        std::cout << "Simulated Batch vs Individual: " << std::fixed << std::setprecision(2) << speedup1to2 << "x" << std::endl;
        std::cout << "Transaction vs Individual: " << std::fixed << std::setprecision(2) << speedup1to3 << "x" << std::endl;
        std::cout << "Transaction+Simulated Batch vs Individual: " << std::fixed << std::setprecision(2) << speedup1to4 << "x" << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        // If an error occurs, roll back any active transaction
        try
        {
            conn->rollback();
        }
        catch (...)
        {
            // Ignore rollback errors
        }

        // Restore auto-commit mode
        conn->setAutoCommit(true);

        std::cerr << "Database error: " << e.what() << std::endl;
    }
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

        // Demonstrate batch operations
        demonstrateBasicBatchInsert(mysqlConn);
        demonstrateBatchWithTransaction(mysqlConn);
        demonstrateBatchUpdate(mysqlConn);
        demonstrateBatchDelete(mysqlConn);
        demonstrateBatchPerformanceComparison(mysqlConn);

        // Clean up
        mysqlConn->executeUpdate("DROP TABLE IF EXISTS batch_products");
        mysqlConn->executeUpdate("DROP TABLE IF EXISTS batch_orders");
        mysqlConn->executeUpdate("DROP TABLE IF EXISTS performance_test");

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

        // Demonstrate batch operations
        demonstrateBasicBatchInsert(pgConn);
        demonstrateBatchWithTransaction(pgConn);
        demonstrateBatchUpdate(pgConn);
        demonstrateBatchDelete(pgConn);
        demonstrateBatchPerformanceComparison(pgConn);

        // Clean up
        pgConn->executeUpdate("DROP TABLE IF EXISTS batch_products");
        pgConn->executeUpdate("DROP TABLE IF EXISTS batch_orders");
        pgConn->executeUpdate("DROP TABLE IF EXISTS performance_test");

        // Close PostgreSQL connection
        pgConn->close();
#else
        std::cout << "PostgreSQL support is not enabled." << std::endl;
#endif
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}