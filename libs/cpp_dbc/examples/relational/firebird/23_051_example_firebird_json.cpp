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
 * @file 23_051_example_firebird_json.cpp
 * @brief Firebird-specific example demonstrating JSON operations
 *
 * This example demonstrates:
 * - Storing JSON data in VARCHAR/BLOB columns
 * - Using UDF or stored procedures for JSON parsing (if available)
 * - Alternative approaches for JSON handling in Firebird
 *
 * Note: Firebird has limited native JSON support compared to MySQL/PostgreSQL.
 * JSON is typically stored as VARCHAR or BLOB and parsed application-side.
 * Firebird 4.0+ has some JSON support through built-in functions.
 *
 * Usage:
 *   ./23_051_example_firebird_json [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - Firebird support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <iomanip>
#include <sstream>

#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_FIREBIRD
// Helper function to print query results
void printResults(std::shared_ptr<cpp_dbc::RelationalDBResultSet> rs)
{
    auto columnNames = rs->getColumnNames();

    std::ostringstream headerOss;
    for (const auto &column : columnNames)
    {
        headerOss << std::setw(20) << std::left << column;
    }
    logData(headerOss.str());

    std::ostringstream sepOss;
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        sepOss << std::string(20, '-');
    }
    logData(sepOss.str());

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
    log("");
}

/**
 * @brief Demonstrates JSON storage and retrieval in Firebird
 *
 * IMPORTANT FIREBIRD-SPECIFIC NOTES:
 * ===================================
 * Firebird requires EXPLICIT closing of ResultSet and PreparedStatement objects
 * before performing DDL operations (DROP TABLE, DROP PROCEDURE, etc.).
 *
 * WHY IS THIS NECESSARY?
 * ----------------------
 * 1. METADATA LOCKS: Firebird maintains metadata locks on tables while ResultSet
 *    or PreparedStatement objects are active (even if they finished reading data).
 *
 * 2. TRANSACTION ISOLATION: Unlike some databases, Firebird doesn't automatically
 *    release locks when you reassign rs = conn->executeQuery(...) to a new result.
 *    The old ResultSet's destructor may not run immediately due to shared_ptr.
 *
 * 3. DDL OPERATIONS: When you try to DROP TABLE while locks are active, Firebird
 *    will throw: "SQLCODE -607: object TABLE is in use"
 *
 * SOLUTION:
 * ---------
 * Always call rs->close() after using each ResultSet and pstmt->close() after
 * using each PreparedStatement, ESPECIALLY before DROP TABLE/PROCEDURE operations.
 *
 * Example pattern:
 *   auto pstmt = conn->prepareStatement(...);
 *   pstmt->setInt(1, value);
 *   pstmt->executeUpdate();
 *   pstmt->close();  // <-- CRITICAL for Firebird
 *
 *   auto rs = conn->executeQuery(...);
 *   processResults(rs);
 *   rs->close();  // <-- CRITICAL for Firebird
 *
 * This is REQUIRED in Firebird, whereas other databases (MySQL, PostgreSQL) may
 * handle this automatically via RAII destructors.
 */
void demonstrateFirebirdJson(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("=== Firebird JSON Operations ===");
    log("");

    try
    {
        // Create a table to store JSON data
        logStep("Creating table for JSON storage...");
        conn->executeUpdate(
            "RECREATE TABLE product_catalog ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "product_data VARCHAR(4000)"
            ")");
        logOk("Table created successfully");

        // Insert JSON data as text
        logStep("Inserting JSON data...");
        auto pstmt = conn->prepareStatement(
            "INSERT INTO product_catalog (id, product_data) VALUES (?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setString(2, R"({"name":"Laptop","price":1299.99,"specs":{"cpu":"Intel i7","ram":"16GB"},"colors":["Silver","Black"]})");
        pstmt->executeUpdate();

        pstmt->setInt(1, 2);
        pstmt->setString(2, R"({"name":"Smartphone","price":799.99,"specs":{"cpu":"Snapdragon 8","ram":"8GB"},"colors":["Black","White","Blue"]})");
        pstmt->executeUpdate();

        pstmt->setInt(1, 3);
        pstmt->setString(2, R"({"name":"Tablet","price":499.99,"specs":{"cpu":"A14 Bionic","ram":"4GB"},"colors":["Silver","Gold"]})");
        pstmt->executeUpdate();

        logOk("Data inserted successfully");

        // Close prepared statement before queries
        pstmt->close();

        // Query all JSON data
        log("");
        logStep("Example 1: Retrieving all JSON data");
        auto rs = conn->executeQuery("SELECT id, product_data FROM product_catalog ORDER BY id");
        printResults(rs);
        rs->close();  // Close result set explicitly

        // Using LIKE for simple JSON searching (limited but works)
        log("");
        logStep("Example 2: Searching JSON using LIKE (finding 'Laptop')");
        rs = conn->executeQuery(
            "SELECT id, product_data FROM product_catalog "
            "WHERE product_data LIKE '%\"name\":\"Laptop\"%'");
        printResults(rs);
        rs->close();  // Close result set explicitly

        // Search for products with specific color
        log("");
        logStep("Example 3: Searching for products with 'Silver' color");
        rs = conn->executeQuery(
            "SELECT id, product_data FROM product_catalog "
            "WHERE product_data LIKE '%Silver%'");
        printResults(rs);
        rs->close();  // Close result set explicitly

        // Search for products in price range (requires parsing application-side)
        log("");
        logStep("Example 4: Filtering by price pattern");
        logInfo("Note: Complex JSON queries require application-side parsing");
        rs = conn->executeQuery(
            "SELECT id, product_data FROM product_catalog "
            "WHERE product_data LIKE '%\"price\":7%' OR product_data LIKE '%\"price\":4%'");
        printResults(rs);
        rs->close();  // Close result set explicitly

        // Update JSON data
        log("");
        logStep("Example 5: Updating JSON data");
        conn->executeUpdate(
            "UPDATE product_catalog SET "
            "product_data = '{\"name\":\"Laptop Pro\",\"price\":1199.99,\"specs\":{\"cpu\":\"Intel i9\",\"ram\":\"32GB\"},\"colors\":[\"Silver\",\"Black\",\"White\"],\"on_sale\":true}' "
            "WHERE id = 1");
        logOk("JSON data updated");

        rs = conn->executeQuery("SELECT id, product_data FROM product_catalog WHERE id = 1");
        printResults(rs);
        rs->close();  // Close result set explicitly

        /* COMMENTED OUT - Stored procedure causes issues with Firebird cleanup
        // Create stored procedure for JSON-like extraction (simulated)
        log("");
        logStep("Example 6: Creating helper function for JSON name extraction");

        try
        {
            conn->executeUpdate("DROP PROCEDURE extract_json_name");
        }
        catch (...)
        {
        }

        conn->executeUpdate(
            "CREATE PROCEDURE extract_json_name (json_text VARCHAR(4000)) "
            "RETURNS (name_value VARCHAR(100)) AS "
            "DECLARE VARIABLE start_pos INTEGER; "
            "DECLARE VARIABLE end_pos INTEGER; "
            "BEGIN "
            "    start_pos = POSITION('\"name\":\"' IN json_text); "
            "    IF (start_pos > 0) THEN BEGIN "
            "        start_pos = start_pos + 8; "
            "        end_pos = POSITION('\"' IN SUBSTRING(json_text FROM start_pos)); "
            "        name_value = SUBSTRING(json_text FROM start_pos FOR end_pos - 1); "
            "    END "
            "    ELSE BEGIN "
            "        name_value = NULL; "
            "    END "
            "    SUSPEND; "
            "END");
        logOk("Stored procedure created");
        conn->commit();  // Commit procedure creation before using it

        logStep("Using stored procedure to extract product names:");
        rs = conn->executeQuery(
            "SELECT p.id, e.name_value "
            "FROM product_catalog p "
            "CROSS JOIN extract_json_name(p.product_data) e "
            "ORDER BY p.id");
        printResults(rs);
        rs->close();  // Close ResultSet before dropping objects
        conn->commit();  // Commit to release all locks
        */

        // Clean up
        log("");
        logStep("Cleaning up...");
        conn->executeUpdate("DROP TABLE product_catalog");
        logOk("Cleanup completed");

        log("");
        logInfo("Note: For full JSON support, consider:");
        logInfo("  - Firebird 4.0+ with JSON functions");
        logInfo("  - External UDF libraries");
        logInfo("  - Application-side JSON parsing with libraries like nlohmann/json");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Firebird JSON operation error: " + e.what_s());
        throw;
    }
}
#endif

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc Firebird JSON Operations Example");
    log("========================================");
    log("");

#if !USE_FIREBIRD
    logError("Firebird support is not enabled");
    logInfo("Build with -DUSE_FIREBIRD=ON to enable Firebird support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,firebird");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("23_051_example_firebird_json", "firebird");
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

    logStep("Getting Firebird database configuration...");
    auto firebirdResult = getDbConfig(configManager, args.dbName.empty() ? "" : args.dbName, "firebird");

    if (!firebirdResult)
    {
        logError("Failed to get Firebird config: " + firebirdResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!firebirdResult.value())
    {
        logError("Firebird configuration not found");
        return EXIT_ERROR_;
    }

    auto &firebirdConfig = *firebirdResult.value();
    logOk("Using database: " + firebirdConfig.getName() +
          " (" + firebirdConfig.getType() + "://" +
          firebirdConfig.getHost() + ":" + std::to_string(firebirdConfig.getPort()) +
          "/" + firebirdConfig.getDatabase() + ")");

    logStep("Registering Firebird driver...");
    registerDriver("firebird");
    logOk("Driver registered");

    // Try to create database if it doesn't exist
    tryCreateFirebirdDatabase(firebirdConfig);

    try
    {
        logStep("Connecting to Firebird...");
        auto firebirdConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            firebirdConfig.createDBConnection());

        logOk("Connected to Firebird");

        // Demonstrate Firebird JSON operations
        demonstrateFirebirdJson(firebirdConn);

        logStep("Closing Firebird connection...");
        firebirdConn->close();
        logOk("Firebird connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Firebird error: " + e.what_s());
        return EXIT_ERROR_;
    }

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return EXIT_OK_;
#endif
}
