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
 * @file 20_051_example_mysql_json.cpp
 * @brief MySQL-specific example demonstrating JSON operations
 *
 * This example demonstrates:
 * - MySQL JSON operations (JSON_EXTRACT, JSON_SET, JSON_CONTAINS, etc.)
 * - Inserting and querying JSON data
 * - Filtering based on JSON values
 * - Modifying JSON documents
 *
 * Usage:
 *   ./20_051_example_mysql_json [--config=<path>] [--db=<name>] [--help]
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
// Helper function to print JSON query results
void printJsonResults(std::shared_ptr<cpp_dbc::RelationalDBResultSet> rs)
{
    auto columnNames = rs->getColumnNames();

    // Print header
    std::ostringstream headerOss;
    for (const auto &column : columnNames)
    {
        headerOss << std::setw(20) << std::left << column;
    }
    logData(headerOss.str());

    // Print separator
    std::ostringstream sepOss;
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        sepOss << std::string(20, '-');
    }
    logData(sepOss.str());

    // Print data
    while (rs->next())
    {
        std::ostringstream rowOss;
        for (const auto &column : columnNames)
        {
            rowOss << std::setw(20) << std::left << rs->getString(column);
        }
        logData(rowOss.str());
    }
    logMsg("");
}

// Function to demonstrate JSON operations with MySQL
void demonstrateMySQLJson(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("=== MySQL JSON Operations ===");
    logMsg("");

    try
    {
        // Create a table with a JSON column
        conn->executeUpdate("DROP TABLE IF EXISTS product_catalog");
        conn->executeUpdate(
            "CREATE TABLE product_catalog ("
            "id INT PRIMARY KEY, "
            "product_data JSON"
            ")");

        logOk("Table created successfully");

        // Insert JSON data using prepared statements
        auto pstmt = conn->prepareStatement(
            "INSERT INTO product_catalog (id, product_data) VALUES (?, ?)");

        // Simple JSON object
        pstmt->setInt(1, 1);
        pstmt->setString(2, R"({
            "name": "Laptop",
            "price": 1299.99,
            "specs": {
                "cpu": "Intel i7",
                "ram": "16GB",
                "storage": "512GB SSD"
            },
            "colors": ["Silver", "Space Gray", "Black"]
        })");
        pstmt->executeUpdate();

        // Another JSON object
        pstmt->setInt(1, 2);
        pstmt->setString(2, R"({
            "name": "Smartphone",
            "price": 799.99,
            "specs": {
                "cpu": "Snapdragon 8",
                "ram": "8GB",
                "storage": "256GB"
            },
            "colors": ["Black", "White", "Blue", "Red"]
        })");
        pstmt->executeUpdate();

        // One more JSON object
        pstmt->setInt(1, 3);
        pstmt->setString(2, R"({
            "name": "Tablet",
            "price": 499.99,
            "specs": {
                "cpu": "A14 Bionic",
                "ram": "4GB",
                "storage": "128GB"
            },
            "colors": ["Silver", "Gold"]
        })");
        pstmt->executeUpdate();

        logOk("Data inserted successfully");

        // Example 1: Extract specific JSON fields
        logMsg("");
        logStep("Example 1: Extracting specific JSON fields");
        auto rs = conn->executeQuery(
            "SELECT id, "
            "JSON_EXTRACT(product_data, '$.name') AS product_name, "
            "JSON_EXTRACT(product_data, '$.price') AS price, "
            "JSON_EXTRACT(product_data, '$.specs.cpu') AS cpu "
            "FROM product_catalog");
        printJsonResults(rs);

        // Example 2: Filter based on JSON values
        logStep("Example 2: Filtering based on JSON values");
        rs = conn->executeQuery(
            "SELECT id, JSON_EXTRACT(product_data, '$.name') AS product_name "
            "FROM product_catalog "
            "WHERE JSON_EXTRACT(product_data, '$.price') > 700");
        printJsonResults(rs);

        // Example 3: Check if JSON array contains a value
        logStep("Example 3: Checking if JSON array contains a value");
        rs = conn->executeQuery(
            "SELECT id, JSON_EXTRACT(product_data, '$.name') AS product_name, "
            "JSON_EXTRACT(product_data, '$.colors') AS colors, "
            "JSON_CONTAINS(JSON_EXTRACT(product_data, '$.colors'), '\"Silver\"') AS has_silver "
            "FROM product_catalog");
        printJsonResults(rs);

        // Example 4: Modify JSON data
        logStep("Example 4: Modifying JSON data");
        conn->executeUpdate(
            "UPDATE product_catalog "
            "SET product_data = JSON_SET(product_data, '$.price', 1199.99, '$.on_sale', true) "
            "WHERE id = 1");

        rs = conn->executeQuery("SELECT id, product_data FROM product_catalog WHERE id = 1");
        printJsonResults(rs);

        // Example 5: Add elements to JSON array
        logStep("Example 5: Adding elements to JSON array");
        conn->executeUpdate(
            "UPDATE product_catalog "
            "SET product_data = JSON_ARRAY_APPEND(product_data, '$.colors', '\"Green\"') "
            "WHERE id = 2");

        rs = conn->executeQuery(
            "SELECT id, JSON_EXTRACT(product_data, '$.name') AS product_name, "
            "JSON_EXTRACT(product_data, '$.colors') AS colors "
            "FROM product_catalog WHERE id = 2");
        printJsonResults(rs);

        // Clean up
        conn->executeUpdate("DROP TABLE product_catalog");
        logOk("Table dropped successfully");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("MySQL JSON operation error: " + e.what_s());
        throw;
    }
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc MySQL JSON Operations Example");
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
        printHelp("20_051_example_mysql_json", "mysql");
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

    logStep("Getting MySQL database configuration...");
    auto mysqlResult = getDbConfig(configManager, args.dbName, "mysql");

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
    logOk("Using database: " + mysqlConfig.getName() +
          " (" + mysqlConfig.getType() + "://" +
          mysqlConfig.getHost() + ":" + std::to_string(mysqlConfig.getPort()) +
          "/" + mysqlConfig.getDatabase() + ")");

    logStep("Registering MySQL driver...");
    registerDriver("mysql");
    logOk("Driver registered");

    try
    {
        logStep("Connecting to MySQL...");
        auto mysqlConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(
                mysqlConfig.createConnectionString(),
                mysqlConfig.getUsername(),
                mysqlConfig.getPassword()));

        logOk("Connected to MySQL");

        // Demonstrate MySQL JSON operations
        demonstrateMySQLJson(mysqlConn);

        logStep("Closing MySQL connection...");
        mysqlConn->close();
        logOk("MySQL connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("MySQL error: " + e.what_s());
        return EXIT_ERROR_;
    }

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return EXIT_OK_;
#endif
}
