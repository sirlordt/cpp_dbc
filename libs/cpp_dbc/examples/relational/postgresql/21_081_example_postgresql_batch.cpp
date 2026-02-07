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
 * @file 21_081_example_postgresql_batch.cpp
 * @brief PostgreSQL-specific example demonstrating batch operations
 *
 * This example demonstrates:
 * - Batch insert, update, and delete operations
 * - Transaction management for batch operations
 * - Performance comparison of different batch strategies
 *
 * Usage:
 *   ./21_081_example_postgresql_batch [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - PostgreSQL support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <chrono>
#include <random>
#include <iomanip>

#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_POSTGRESQL
// Helper function to generate random product data
std::vector<std::tuple<int, std::string, std::string, double, int>> generateProductData(int count, int startId = 1)
{
    std::vector<std::tuple<int, std::string, std::string, double, int>> products;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> priceDistrib(10.0, 1000.0);
    std::uniform_int_distribution<> stockDistrib(1, 1000);

    std::vector<std::string> categories = {"Electronics", "Clothing", "Home & Kitchen", "Books", "Sports"};
    std::vector<std::string> prefixes = {"Premium", "Deluxe", "Basic", "Professional", "Ultra"};
    std::vector<std::string> suffixes = {"Pro", "Plus", "Lite", "Max", "Mini"};
    std::vector<std::string> types = {"Laptop", "Phone", "Shirt", "Blender", "Chair"};

    for (int i = 0; i < count; i++)
    {
        int id = startId + i;
        std::string prefix = prefixes[gen() % prefixes.size()];
        std::string type = types[gen() % types.size()];
        std::string suffix = suffixes[gen() % suffixes.size()];
        std::string name = prefix + " " + type + " " + suffix;
        std::string category = categories[gen() % categories.size()];
        double price = std::round(priceDistrib(gen) * 100) / 100;
        int stock = stockDistrib(gen);

        products.emplace_back(id, name, category, price, stock);
    }

    return products;
}

void demonstrateBasicBatchInsert(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Basic Batch Insert ---");

    try
    {
        logStep("Creating batch_products table...");
        conn->executeUpdate("DROP TABLE IF EXISTS batch_products");
        conn->executeUpdate(
            "CREATE TABLE batch_products ("
            "product_id INT PRIMARY KEY, "
            "name VARCHAR(100), "
            "category VARCHAR(50), "
            "price DECIMAL(10,2), "
            "stock_quantity INT"
            ")");
        logOk("Table created");

        logStep("Generating 100 product records...");
        auto products = generateProductData(100);
        logOk("Generated " + std::to_string(products.size()) + " products");

        logStep("Performing batch insert...");
        auto pstmt = conn->prepareStatement(
            "INSERT INTO batch_products (product_id, name, category, price, stock_quantity) "
            "VALUES (?, ?, ?, ?, ?)");

        auto startTime = std::chrono::high_resolution_clock::now();

        uint64_t totalRowsAffected = 0;
        for (const auto &product : products)
        {
            pstmt->setInt(1, std::get<0>(product));
            pstmt->setString(2, std::get<1>(product));
            pstmt->setString(3, std::get<2>(product));
            pstmt->setDouble(4, std::get<3>(product));
            pstmt->setInt(5, std::get<4>(product));
            totalRowsAffected += pstmt->executeUpdate();
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        logOk("Batch insert completed");
        logData("Rows affected: " + std::to_string(totalRowsAffected));
        logData("Execution time: " + std::to_string(duration) + " ms");

        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_products");
        if (rs->next())
        {
            logData("Verified row count: " + std::to_string(rs->getInt("count")));
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
    }
}

void demonstrateBatchWithTransaction(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Batch Insert with Transaction ---");

    try
    {
        logStep("Creating batch_orders table...");
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
        logOk("Table created");

        logStep("Generating 1000 order records...");

        std::random_device rd;
        std::mt19937 gen(rd());

        auto pstmt = conn->prepareStatement(
            "INSERT INTO batch_orders (order_id, customer_id, product_id, order_date, quantity, total_price) "
            "VALUES (?, ?, ?, ?, ?, ?)");

        logStep("Starting transaction...");
        conn->setAutoCommit(false);
        logOk("Transaction started");

        auto startTime = std::chrono::high_resolution_clock::now();

        uint64_t totalRowsAffected = 0;
        for (int i = 1; i <= 1000; ++i)
        {
            pstmt->setInt(1, i);
            pstmt->setInt(2, static_cast<int>((gen() % 10) + 1));
            pstmt->setInt(3, static_cast<int>((gen() % 100) + 1));
            pstmt->setDate(4, "2023-01-15");
            pstmt->setInt(5, static_cast<int>((gen() % 5) + 1));
            pstmt->setDouble(6, static_cast<double>(gen() % 10000) / 100.0);
            totalRowsAffected += pstmt->executeUpdate();
        }

        conn->commit();
        logOk("Transaction committed");

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        conn->setAutoCommit(true);

        logData("Rows affected: " + std::to_string(totalRowsAffected));
        logData("Execution time: " + std::to_string(duration) + " ms");

        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_orders");
        if (rs->next())
        {
            logData("Verified row count: " + std::to_string(rs->getInt("count")));
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        try
        {
            conn->rollback();
        }
        catch (...)
        {
        }
        conn->setAutoCommit(true);
        logError("Database error: " + e.what_s());
    }
}

void demonstrateBatchUpdate(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Batch Update ---");

    try
    {
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_products");
        if (!rs->next() || rs->getInt("count") == 0)
        {
            logInfo("No products to update. Run batch insert first.");
            return;
        }

        logStep("Finding products to update...");
        std::vector<int> productIds;
        rs = conn->executeQuery("SELECT product_id FROM batch_products ORDER BY product_id LIMIT 50");
        while (rs->next())
        {
            productIds.push_back(rs->getInt("product_id"));
        }
        logOk("Found " + std::to_string(productIds.size()) + " products");

        auto pstmt = conn->prepareStatement(
            "UPDATE batch_products SET price = price * 1.1, stock_quantity = ? WHERE product_id = ?");

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> stockDistrib(10, 500);

        logStep("Performing batch update...");
        auto startTime = std::chrono::high_resolution_clock::now();

        uint64_t totalRowsAffected = 0;
        for (int productId : productIds)
        {
            int newStock = stockDistrib(gen);
            pstmt->setInt(1, newStock);
            pstmt->setInt(2, productId);
            totalRowsAffected += pstmt->executeUpdate();
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        logOk("Batch update completed");
        logData("Rows affected: " + std::to_string(totalRowsAffected));
        logData("Execution time: " + std::to_string(duration) + " ms");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
    }
}

void demonstrateBatchDelete(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Batch Delete ---");

    try
    {
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_orders");
        if (!rs->next() || rs->getInt("count") == 0)
        {
            logInfo("No orders to delete. Run batch with transaction first.");
            return;
        }

        logStep("Finding orders to delete...");
        std::vector<int> orderIds;
        rs = conn->executeQuery("SELECT order_id FROM batch_orders WHERE quantity = 1 LIMIT 200");
        while (rs->next())
        {
            orderIds.push_back(rs->getInt("order_id"));
        }
        logOk("Found " + std::to_string(orderIds.size()) + " orders");

        auto pstmt = conn->prepareStatement("DELETE FROM batch_orders WHERE order_id = ?");

        logStep("Performing batch delete...");
        auto startTime = std::chrono::high_resolution_clock::now();

        uint64_t totalRowsAffected = 0;
        for (int orderId : orderIds)
        {
            pstmt->setInt(1, orderId);
            totalRowsAffected += pstmt->executeUpdate();
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        logOk("Batch delete completed");
        logData("Rows affected: " + std::to_string(totalRowsAffected));
        logData("Execution time: " + std::to_string(duration) + " ms");

        rs = conn->executeQuery("SELECT COUNT(*) as count FROM batch_orders");
        if (rs->next())
        {
            logData("Remaining orders: " + std::to_string(rs->getInt("count")));
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
    }
}

void demonstrateBatchPerformanceComparison(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Batch Performance Comparison ---");

    try
    {
        logStep("Creating performance_test table...");
        conn->executeUpdate("DROP TABLE IF EXISTS performance_test");
        conn->executeUpdate(
            "CREATE TABLE performance_test ("
            "id INT PRIMARY KEY, "
            "name VARCHAR(100), "
            "value DOUBLE PRECISION, "
            "created_at TIMESTAMP"
            ")");
        logOk("Table created");

        const int recordCount = 1000;

        std::vector<std::tuple<int, std::string, double>> testData;
        for (int i = 1; i <= recordCount; i++)
        {
            std::string name = "Test Item " + std::to_string(i);
            double value = i * 1.5;
            testData.emplace_back(i, name, value);
        }

        // Method 1: Individual inserts
        logMsg("");
        logStep("Method 1: Individual inserts...");

        auto startTime1 = std::chrono::high_resolution_clock::now();
        auto pstmt1 = conn->prepareStatement(
            "INSERT INTO performance_test (id, name, value, created_at) "
            "VALUES (?, ?, ?, CURRENT_TIMESTAMP)");

        uint64_t rowsAffected1 = 0;
        for (const auto &data : testData)
        {
            pstmt1->setInt(1, std::get<0>(data));
            pstmt1->setString(2, std::get<1>(data));
            pstmt1->setDouble(3, std::get<2>(data));
            rowsAffected1 += pstmt1->executeUpdate();
        }

        auto endTime1 = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime1 - startTime1).count();
        logData("Time: " + std::to_string(duration1) + " ms, Rows: " + std::to_string(rowsAffected1));

        conn->executeUpdate("DELETE FROM performance_test");

        // Method 2: Transaction with inserts
        logStep("Method 2: Transaction with inserts...");

        auto startTime2 = std::chrono::high_resolution_clock::now();
        conn->setAutoCommit(false);

        auto pstmt2 = conn->prepareStatement(
            "INSERT INTO performance_test (id, name, value, created_at) "
            "VALUES (?, ?, ?, CURRENT_TIMESTAMP)");

        uint64_t rowsAffected2 = 0;
        for (const auto &data : testData)
        {
            pstmt2->setInt(1, std::get<0>(data));
            pstmt2->setString(2, std::get<1>(data));
            pstmt2->setDouble(3, std::get<2>(data));
            rowsAffected2 += pstmt2->executeUpdate();
        }

        conn->commit();
        conn->setAutoCommit(true);

        auto endTime2 = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(endTime2 - startTime2).count();
        logData("Time: " + std::to_string(duration2) + " ms, Rows: " + std::to_string(rowsAffected2));

        // Summary
        logMsg("");
        logOk("Performance Summary:");
        logData("Individual inserts: " + std::to_string(duration1) + " ms");
        logData("Transaction batch: " + std::to_string(duration2) + " ms");

        if (duration2 > 0)
        {
            double speedup = static_cast<double>(duration1) / static_cast<double>(duration2);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << speedup;
            logData("Speedup: " + oss.str() + "x");
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        try
        {
            conn->rollback();
        }
        catch (...)
        {
        }
        conn->setAutoCommit(true);
        logError("Database error: " + e.what_s());
    }
}

void runAllDemonstrations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    demonstrateBasicBatchInsert(conn);
    demonstrateBatchWithTransaction(conn);
    demonstrateBatchUpdate(conn);
    demonstrateBatchDelete(conn);
    demonstrateBatchPerformanceComparison(conn);

    logMsg("");
    logStep("Cleaning up tables...");
    conn->executeUpdate("DROP TABLE IF EXISTS batch_products");
    conn->executeUpdate("DROP TABLE IF EXISTS batch_orders");
    conn->executeUpdate("DROP TABLE IF EXISTS performance_test");
    logOk("Tables dropped");
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc PostgreSQL Batch Operations Example");
    logMsg("========================================");
    logMsg("");

#if !USE_POSTGRESQL
    logError("PostgreSQL support is not enabled");
    logInfo("Build with -DUSE_POSTGRESQL=ON to enable PostgreSQL support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,postgres");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("21_081_example_postgresql_batch", "postgresql");
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

    logStep("Registering PostgreSQL driver...");
    registerDriver("postgresql");
    logOk("Driver registered");

    try
    {
        logStep("Getting PostgreSQL configuration...");
        auto pgResult = getDbConfig(configManager, args.dbName.empty() ? "" : args.dbName, "postgresql");

        if (!pgResult)
        {
            logError("Failed to get PostgreSQL config: " + pgResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!pgResult.value())
        {
            logError("PostgreSQL configuration not found");
            return EXIT_ERROR_;
        }

        auto &pgConfig = *pgResult.value();
        logOk("Using: " + pgConfig.getName());

        logStep("Connecting to PostgreSQL...");
        auto pgConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            pgConfig.createDBConnection());
        logOk("Connected to PostgreSQL");

        runAllDemonstrations(pgConn);

        logStep("Closing PostgreSQL connection...");
        pgConn->close();
        logOk("PostgreSQL connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        return EXIT_ERROR_;
    }

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return EXIT_OK_;
#endif
}
