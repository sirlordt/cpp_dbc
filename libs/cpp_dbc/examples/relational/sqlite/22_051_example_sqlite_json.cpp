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
 * @file 22_051_example_sqlite_json.cpp
 * @brief SQLite-specific example demonstrating JSON operations
 *
 * This example demonstrates:
 * - SQLite JSON1 extension operations (json_extract, json_set, json_array, etc.)
 * - Inserting and querying JSON data
 * - Filtering based on JSON values
 * - Modifying JSON documents
 *
 * Note: Requires SQLite 3.9.0+ with JSON1 extension enabled.
 *
 * Usage:
 *   ./22_051_example_sqlite_json [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - SQLite support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <iomanip>
#include <sstream>

#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_SQLITE
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
            std::string value = rs->isNull(column) ? "NULL" : rs->getString(column);
            if (value.length() > 18)
            {
                value = value.substr(0, 15) + "...";
            }
            rowOss << std::setw(20) << std::left << value;
        }
        logData(rowOss.str());
    }
    logMsg("");
}

// Function to demonstrate JSON operations with SQLite
void demonstrateSQLiteJson(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("=== SQLite JSON Operations ===");
    logMsg("");

    try
    {
        // Check if JSON1 extension is available
        logStep("Checking JSON1 extension availability...");
        auto checkRs = conn->executeQuery("SELECT json('{\"test\": 1}')");
        if (checkRs->next())
        {
            logOk("JSON1 extension is available");
        }

        // Create a table with JSON data stored as TEXT
        conn->executeUpdate("DROP TABLE IF EXISTS product_catalog");
        conn->executeUpdate(
            "CREATE TABLE product_catalog ("
            "id INTEGER PRIMARY KEY, "
            "product_data TEXT"
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

        // Example 1: Extract specific JSON fields using json_extract
        logMsg("");
        logStep("Example 1: Extracting specific JSON fields with json_extract");
        auto rs = conn->executeQuery(
            "SELECT id, "
            "json_extract(product_data, '$.name') AS product_name, "
            "json_extract(product_data, '$.price') AS price, "
            "json_extract(product_data, '$.specs.cpu') AS cpu "
            "FROM product_catalog");
        printJsonResults(rs);

        // Example 2: Filter based on JSON values
        logStep("Example 2: Filtering based on JSON values");
        rs = conn->executeQuery(
            "SELECT id, json_extract(product_data, '$.name') AS product_name "
            "FROM product_catalog "
            "WHERE json_extract(product_data, '$.price') > 700");
        printJsonResults(rs);

        // Example 3: Check if JSON array contains a value using json_each
        logStep("Example 3: Finding products with 'Silver' color");
        rs = conn->executeQuery(
            "SELECT DISTINCT p.id, json_extract(p.product_data, '$.name') AS product_name "
            "FROM product_catalog p, json_each(p.product_data, '$.colors') AS colors "
            "WHERE colors.value = 'Silver'");
        printJsonResults(rs);

        // Example 4: Get array elements using json_each
        logStep("Example 4: Listing all colors for each product");
        rs = conn->executeQuery(
            "SELECT p.id, json_extract(p.product_data, '$.name') AS product_name, "
            "colors.value AS color "
            "FROM product_catalog p, json_each(p.product_data, '$.colors') AS colors "
            "ORDER BY p.id, colors.key");
        printJsonResults(rs);

        // Example 5: Modify JSON data using json_set
        logStep("Example 5: Modifying JSON data with json_set");
        conn->executeUpdate(
            "UPDATE product_catalog "
            "SET product_data = json_set(product_data, '$.price', 1199.99, '$.on_sale', 1) "
            "WHERE id = 1");

        rs = conn->executeQuery(
            "SELECT id, "
            "json_extract(product_data, '$.name') AS name, "
            "json_extract(product_data, '$.price') AS price, "
            "json_extract(product_data, '$.on_sale') AS on_sale "
            "FROM product_catalog WHERE id = 1");
        printJsonResults(rs);

        // Example 6: Create JSON array dynamically
        logStep("Example 6: Creating JSON arrays with json_array");
        rs = conn->executeQuery(
            "SELECT json_array(1, 2, 'three', 4.0) AS created_array");
        printJsonResults(rs);

        // Example 7: Create JSON object dynamically
        logStep("Example 7: Creating JSON objects with json_object");
        rs = conn->executeQuery(
            "SELECT json_object('name', 'Test Product', 'price', 99.99) AS created_object");
        printJsonResults(rs);

        // Example 8: Get JSON type
        logStep("Example 8: Getting JSON types");
        rs = conn->executeQuery(
            "SELECT "
            "json_type('{\"a\":1}') AS object_type, "
            "json_type('[1,2,3]') AS array_type, "
            "json_type('123') AS number_type, "
            "json_type('\"hello\"') AS string_type");
        printJsonResults(rs);

        // Clean up
        conn->executeUpdate("DROP TABLE product_catalog");
        logOk("Table dropped successfully");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("SQLite JSON operation error: " + e.what_s());
        throw;
    }
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc SQLite JSON Operations Example");
    logMsg("========================================");
    logMsg("");

#if !USE_SQLITE
    logError("SQLite support is not enabled");
    logInfo("Build with -DUSE_SQLITE=ON to enable SQLite support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,sqlite");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("22_051_example_sqlite_json", "sqlite");
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

    logStep("Getting SQLite database configuration...");
    auto sqliteResult = getDbConfig(configManager, args.dbName.empty() ? "" : args.dbName, "sqlite");

    if (!sqliteResult)
    {
        logError("Failed to get SQLite config: " + sqliteResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!sqliteResult.value())
    {
        logError("SQLite configuration not found");
        return EXIT_ERROR_;
    }

    auto &sqliteConfig = *sqliteResult.value();
    logOk("Using database: " + sqliteConfig.getName() +
          " (" + sqliteConfig.getType() + "://" + sqliteConfig.getDatabase() + ")");

    logStep("Registering SQLite driver...");
    registerDriver("sqlite");
    logOk("Driver registered");

    try
    {
        logStep("Connecting to SQLite...");
        auto sqliteConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(
                sqliteConfig.createConnectionString(),
                sqliteConfig.getUsername(),
                sqliteConfig.getPassword()));

        logOk("Connected to SQLite");

        // Demonstrate SQLite JSON operations
        demonstrateSQLiteJson(sqliteConn);

        logStep("Closing SQLite connection...");
        sqliteConn->close();
        logOk("SQLite connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("SQLite error: " + e.what_s());
        return EXIT_ERROR_;
    }

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return EXIT_OK_;
#endif
}
