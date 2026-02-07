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
 * @file firebird_example.cpp
 * @brief Example demonstrating basic Firebird database operations
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - Firebird CRUD operations
 * - Prepared statements
 * - Firebird-specific features (generators, stored procedures)
 *
 * Usage:
 *   ./firebird_example [--config=<path>] [--db=<name>] [--help]
 *
 * Examples:
 *   ./firebird_example                    # Uses dev_firebird from default config
 *   ./firebird_example --db=test_firebird # Uses test_firebird configuration
 */

#include "../../common/example_common.hpp"
#include <iomanip>
#include <sstream>
#include <any>

#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_FIREBIRD
void printResults(std::shared_ptr<cpp_dbc::RelationalDBResultSet> rs)
{
    auto columnNames = rs->getColumnNames();

    std::ostringstream headerOss;
    for (const auto &column : columnNames)
    {
        headerOss << std::setw(15) << std::left << column;
    }
    logData(headerOss.str());

    std::ostringstream sepOss;
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        sepOss << std::string(15, '-');
    }
    logData(sepOss.str());

    int rowCount = 0;
    while (rs->next())
    {
        std::ostringstream rowOss;
        for (const auto &column : columnNames)
        {
            rowOss << std::setw(15) << std::left << rs->getString(column);
        }
        logData(rowOss.str());
        ++rowCount;
    }
    logOk(std::to_string(rowCount) + " row(s) returned");
}

void demonstrateBasicOperations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Basic CRUD Operations ---");

    // ===== Create Table =====
    logStep("Creating products table...");
    conn->executeUpdate(
        "RECREATE TABLE products ("
        "id INTEGER NOT NULL PRIMARY KEY, "
        "name VARCHAR(100) NOT NULL, "
        "price NUMERIC(10,2) NOT NULL, "
        "description VARCHAR(500), "
        "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
        ")");
    logOk("Table created");

    // ===== Insert Data =====
    log("");
    log("--- Insert Operations ---");

    logStep("Preparing insert statement...");
    auto prepStmt = conn->prepareStatement(
        "INSERT INTO products (id, name, price, description) VALUES (?, ?, ?, ?)");
    logOk("Statement prepared");

    logStep("Inserting products...");

    prepStmt->setInt(1, 1);
    prepStmt->setString(2, "Firebird Database Server");
    prepStmt->setDouble(3, 0.00);
    prepStmt->setString(4, "Open source SQL relational database management system");
    prepStmt->executeUpdate();
    logData("Product 1 inserted");

    prepStmt->setInt(1, 2);
    prepStmt->setString(2, "cpp_dbc Library");
    prepStmt->setDouble(3, 0.00);
    prepStmt->setString(4, "C++ Database Connectivity Library");
    prepStmt->executeUpdate();
    logData("Product 2 inserted");

    prepStmt->setInt(1, 3);
    prepStmt->setString(2, "Custom Database Solution");
    prepStmt->setDouble(3, 999.99);
    prepStmt->setString(4, "Enterprise-grade database solution with support");
    prepStmt->executeUpdate();
    logData("Product 3 inserted");

    prepStmt->close();
    logOk("3 products inserted");

    // ===== Select All =====
    log("");
    log("--- Select All Products ---");

    logStep("Querying all products...");
    auto rs = conn->executeQuery("SELECT * FROM products ORDER BY id");
    printResults(rs);

    // ===== Select with Filter =====
    log("");
    log("--- Select Free Products ---");

    logStep("Querying free products (price = 0)...");
    rs = conn->executeQuery("SELECT id, name, price FROM products WHERE price = 0.00");
    printResults(rs);

    // ===== Update =====
    log("");
    log("--- Update Operation ---");

    logStep("Updating product 3...");
    conn->executeUpdate(
        "UPDATE products SET price = 1299.99, description = 'Premium enterprise-grade database solution with 24/7 support' "
        "WHERE id = 3");
    logOk("Product updated");

    logStep("Verifying update...");
    rs = conn->executeQuery("SELECT * FROM products WHERE id = 3");
    printResults(rs);

    // ===== Delete =====
    log("");
    log("--- Delete Operation ---");

    logStep("Deleting product 2...");
    conn->executeUpdate("DELETE FROM products WHERE id = 2");
    logOk("Product deleted");

    logStep("Verifying deletion...");
    rs = conn->executeQuery("SELECT * FROM products ORDER BY id");
    printResults(rs);

    // ===== Cleanup =====
    log("");
    log("--- Cleanup ---");

    logStep("Dropping table...");
    conn->executeUpdate("DROP TABLE products");
    logOk("Table dropped");
}

void demonstrateFirebirdFeatures(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("--- Firebird-Specific Features ---");

    // ===== Generators/Sequences =====
    log("");
    log("--- Auto-Increment with Generator ---");

    logStep("Creating sequence and table...");
    try { conn->executeUpdate("DROP TABLE auto_increment_test"); } catch (...) {}
    try { conn->executeUpdate("DROP SEQUENCE product_id_seq"); } catch (...) {}

    conn->executeUpdate("CREATE SEQUENCE product_id_seq");

    conn->executeUpdate(
        "CREATE TABLE auto_increment_test ("
        "id INTEGER NOT NULL PRIMARY KEY, "
        "name VARCHAR(100) NOT NULL"
        ")");

    conn->executeUpdate(
        "CREATE TRIGGER auto_increment_test_bi FOR auto_increment_test "
        "ACTIVE BEFORE INSERT POSITION 0 AS "
        "BEGIN "
        "    IF (NEW.ID IS NULL) THEN "
        "        NEW.ID = NEXT VALUE FOR product_id_seq; "
        "END");
    logOk("Sequence and trigger created");

    logStep("Inserting data with auto-increment...");
    conn->executeUpdate("INSERT INTO auto_increment_test (name) VALUES ('Product A')");
    conn->executeUpdate("INSERT INTO auto_increment_test (name) VALUES ('Product B')");
    conn->executeUpdate("INSERT INTO auto_increment_test (name) VALUES ('Product C')");
    logOk("3 products inserted");

    logStep("Querying auto-increment results...");
    auto rs = conn->executeQuery("SELECT * FROM auto_increment_test ORDER BY id");
    printResults(rs);

    // ===== Stored Procedures =====
    log("");
    log("--- Stored Procedures ---");

    logStep("Creating stored procedure...");
    try { conn->executeUpdate("DROP PROCEDURE get_product_by_id"); } catch (...) {}

    conn->executeUpdate(
        "CREATE PROCEDURE get_product_by_id (id_param INTEGER) "
        "RETURNS (id INTEGER, name VARCHAR(100)) AS "
        "BEGIN "
        "    FOR SELECT id, name FROM auto_increment_test WHERE id = :id_param INTO :id, :name DO "
        "    SUSPEND; "
        "END");
    logOk("Procedure created");

    logStep("Calling stored procedure...");
    rs = conn->executeQuery("SELECT * FROM get_product_by_id(2)");
    printResults(rs);

    // ===== Cleanup =====
    log("");
    log("--- Cleanup ---");

    logStep("Dropping objects...");
    conn->executeUpdate("DROP PROCEDURE get_product_by_id");
    conn->executeUpdate("DROP TABLE auto_increment_test");
    conn->executeUpdate("DROP SEQUENCE product_id_seq");
    logOk("Objects dropped");
}
#endif

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc Firebird Example");
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
        printHelp("firebird_example", "firebird");
        return 0;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    // Check for real error (DBException)
    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return 1;
    }

    // Check if file was found
    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return 1;
    }
    logOk("Configuration loaded successfully");

    auto &configManager = *configResult.value();

    logStep("Getting database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "firebird");

    // Check for real error
    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return 1;
    }

    // Check if config was found
    if (!dbResult.value())
    {
        logError("Firebird configuration not found");
        return 1;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) +
          "/" + dbConfig.getDatabase() + ")");

    logStep("Registering Firebird driver...");
    registerDriver("firebird");
    logOk("Driver registered");

    // Try to create database if it doesn't exist
    tryCreateFirebirdDatabase(dbConfig);

    try
    {
        logStep("Connecting to Firebird...");
        auto connBase = dbConfig.createDBConnection();
        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(connBase);

        if (!conn)
        {
            logError("Failed to cast connection to RelationalDBConnection");
            return 1;
        }
        logOk("Connected to Firebird");

        demonstrateBasicOperations(conn);
        demonstrateFirebirdFeatures(conn);

        log("");
        logStep("Closing connection...");
        conn->close();
        logOk("Connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        return 1;
    }
    catch (const std::exception &e)
    {
        logError("Error: " + std::string(e.what()));
        return 1;
    }

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return 0;
#endif
}
