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
 * @file 26_091_example_scylladb_error_handling.cpp
 * @brief ScyllaDB-specific example demonstrating error handling
 *
 * This example demonstrates:
 * - Connection errors (wrong host, port)
 * - CQL syntax errors
 * - Keyspace and table errors
 * - Data type errors
 * - Timeout errors
 * - Error recovery patterns
 *
 * Usage:
 *   ./26_091_example_scylladb_error_handling [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - ScyllaDB support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#include <functional>
#include <stdexcept>

#if USE_SCYLLADB
#include <cpp_dbc/drivers/columnar/driver_scylladb.hpp>
#endif

using namespace cpp_dbc::examples;
using cpp_dbc::examples::logMsg; // Disambiguate from std::logMsg

#if USE_SCYLLADB

std::string g_keyspace = "error_test_ks";
std::string g_table = "error_test_ks.error_test_table";

// Custom exception class for application-specific errors
class AppException : public std::runtime_error
{
public:
    explicit AppException(const std::string &message) : std::runtime_error(message) {}
};

// Helper function to execute an operation and handle errors
void executeWithErrorHandling(const std::string &operationName, std::function<void()> operation)
{
    try
    {
        cpp_dbc::examples::logMsg("");
        logStep("Executing: " + operationName);
        operation();
        logOk("Operation completed successfully");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error in " + operationName + ": " + e.what_s());
        e.printCallStack();
    }
    catch (const AppException &e)
    {
        logError("Application error in " + operationName + ": " + std::string(e.what()));
    }
    catch (const std::exception &e)
    {
        logError("Standard exception in " + operationName + ": " + std::string(e.what()));
    }
    catch (...)
    {
        logError("Unknown error in " + operationName);
    }
}

void demonstrateConnectionErrors(cpp_dbc::ScyllaDB::ScyllaDBDriver &driver)
{
    logMsg("");
    logMsg("=== Connection Errors ===");
    logInfo("Demonstrating various connection error scenarios");

    // Wrong host
    executeWithErrorHandling("Connect to non-existent host", [&driver]()
                             {
        logData("Attempting to connect to invalid_host:9042...");
        auto conn = driver.connectColumnar("cpp_dbc:scylladb://invalid_host_that_does_not_exist:9042", "", "");
        // Try to use the connection to trigger the error
        conn->executeQuery("SELECT * FROM system.local");
        conn->close(); });

    // Wrong port
    executeWithErrorHandling("Connect to wrong port", [&driver]()
                             {
        logData("Attempting to connect to localhost:12345...");
        auto conn = driver.connectColumnar("cpp_dbc:scylladb://localhost:12345", "", "");
        conn->executeQuery("SELECT * FROM system.local");
        conn->close(); });
}

void setupSchema(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    // Create keyspace
    conn->executeUpdate(
        "CREATE KEYSPACE IF NOT EXISTS " + g_keyspace +
        " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");

    // Create table
    conn->executeUpdate("DROP TABLE IF EXISTS " + g_table);
    conn->executeUpdate(
        "CREATE TABLE " + g_table + " ("
                                    "id int PRIMARY KEY, "
                                    "name text, "
                                    "value double"
                                    ")");
}

void demonstrateCQLSyntaxErrors(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("=== CQL Syntax Errors ===");
    logInfo("Demonstrating CQL parsing errors");

    // Missing FROM clause
    executeWithErrorHandling("Query without FROM clause", [&conn]()
                             {
        logData("Attempting: SELECT * WHERE id = 1");
        conn->executeQuery("SELECT * WHERE id = 1"); });

    // Invalid keyword
    executeWithErrorHandling("Query with invalid keyword", [&conn]()
                             {
        logData("Attempting: SELEKT * FROM system.local");
        conn->executeQuery("SELEKT * FROM system.local"); });

    // Unclosed string
    executeWithErrorHandling("Query with unclosed string", [&conn]()
                             {
        logData("Attempting: SELECT * FROM " + g_table + " WHERE name = 'unclosed");
        conn->executeQuery("SELECT * FROM " + g_table + " WHERE name = 'unclosed"); });

    // Missing semicolon with multiple statements (CQL doesn't support this)
    executeWithErrorHandling("Multiple statements", [&conn]()
                             {
        logData("Attempting multiple statements in one call...");
        conn->executeUpdate("INSERT INTO " + g_table + " (id, name) VALUES (1, 'a'); INSERT INTO " + g_table + " (id, name) VALUES (2, 'b')"); });
}

void demonstrateKeyspaceAndTableErrors(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("=== Keyspace and Table Errors ===");
    logInfo("Demonstrating keyspace/table-related errors");

    // Non-existent keyspace
    executeWithErrorHandling("Query non-existent keyspace", [&conn]()
                             {
        logData("Attempting to query non_existent_keyspace.some_table...");
        conn->executeQuery("SELECT * FROM non_existent_keyspace.some_table"); });

    // Non-existent table
    executeWithErrorHandling("Query non-existent table", [&conn]()
                             {
        logData("Attempting to query " + g_keyspace + ".non_existent_table...");
        conn->executeQuery("SELECT * FROM " + g_keyspace + ".non_existent_table"); });

    // Create duplicate keyspace without IF NOT EXISTS
    executeWithErrorHandling("Create duplicate keyspace", [&conn]()
                             {
        logData("Attempting to create existing keyspace without IF NOT EXISTS...");
        conn->executeUpdate(
            "CREATE KEYSPACE " + g_keyspace +
            " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}"); });

    // Drop non-existent table without IF EXISTS
    executeWithErrorHandling("Drop non-existent table", [&conn]()
                             {
        logData("Attempting to drop non-existent table without IF EXISTS...");
        conn->executeUpdate("DROP TABLE " + g_keyspace + ".definitely_not_exists"); });
}

void demonstrateDataTypeErrors(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("=== Data Type Errors ===");
    logInfo("Demonstrating type mismatch errors");

    // Insert string into int column
    executeWithErrorHandling("Insert string into int column", [&conn]()
                             {
        logData("Attempting to insert 'not_an_int' into id column...");
        conn->executeUpdate("INSERT INTO " + g_table + " (id, name) VALUES ('not_an_int', 'test')"); });

    // Insert invalid UUID
    executeWithErrorHandling("Query with invalid type comparison", [&conn]()
                             {
        logData("Attempting to compare int column with string...");
        conn->executeQuery("SELECT * FROM " + g_table + " WHERE id = 'string_value'"); });

    // Prepared statement type mismatch
    executeWithErrorHandling("Prepared statement type mismatch", [&conn]()
                             {
        logData("Preparing statement and setting wrong type...");
        auto stmt = conn->prepareStatement("INSERT INTO " + g_table + " (id, name, value) VALUES (?, ?, ?)");
        stmt->setString(1, "should_be_int");  // Wrong type for int column
        stmt->setString(2, "test");
        stmt->setDouble(3, 42.0);
        stmt->executeUpdate();
        stmt->close(); });
}

void demonstratePrimaryKeyErrors(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("=== Primary Key Errors ===");
    logInfo("Demonstrating primary key constraint errors");

    // Insert without primary key
    executeWithErrorHandling("Insert without primary key", [&conn]()
                             {
        logData("Attempting insert without primary key value...");
        conn->executeUpdate("INSERT INTO " + g_table + " (name, value) VALUES ('test', 1.0)"); });

    // Query without partition key in WHERE (warning, not error in ScyllaDB)
    logMsg("");
    logStep("Note: ScyllaDB allows full table scans but warns about them");
    logInfo("This query would succeed but may be inefficient:");
    logData("SELECT * FROM " + g_table + " WHERE name = 'test'");
}

void demonstrateNothrowAPI(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("=== Nothrow API Usage ===");
    logInfo("Using std::nothrow API for exception-free error handling");

    // Insert using nothrow API
    logMsg("");
    logStep("Using nothrow API for executeUpdate...");

    auto insertResult = conn->executeUpdate(std::nothrow,
                                            "INSERT INTO " + g_table + " (id, name, value) VALUES (999, 'nothrow_test', 42.0)");
    if (insertResult)
    {
        logOk("executeUpdate succeeded");
    }
    else
    {
        logError("executeUpdate failed: " + insertResult.error().what_s());
    }

    // Query using nothrow API
    auto queryResult = conn->executeQuery(std::nothrow,
                                          "SELECT * FROM " + g_table + " WHERE id = 999");
    if (queryResult)
    {
        auto rs = queryResult.value();
        if (rs->next())
        {
            logOk("executeQuery succeeded: name='" + rs->getString("name") + "'");
        }
    }
    else
    {
        logError("executeQuery failed: " + queryResult.error().what_s());
    }

    // Invalid operation using nothrow API
    logMsg("");
    logStep("Testing invalid operation with nothrow API...");

    auto badResult = conn->executeQuery(std::nothrow, "INVALID CQL QUERY HERE");
    if (badResult)
    {
        logData("Unexpected success");
    }
    else
    {
        logInfo("Operation failed safely (expected): " + badResult.error().what_s());
    }

    logOk("Nothrow API demonstration completed");
}

void demonstrateErrorRecovery(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("=== Error Recovery Patterns ===");
    logInfo("Demonstrating how to recover from errors");

    // Pattern 1: Create if not exists
    logMsg("");
    logStep("Pattern 1: IF NOT EXISTS for idempotent operations...");

    try
    {
        conn->executeUpdate("CREATE TABLE " + g_table + " (id int PRIMARY KEY, name text, value double)");
        logData("Table created (first time)");
    }
    catch (const cpp_dbc::DBException &)
    {
        logData("Table already exists, continuing...");
    }

    // Using IF NOT EXISTS (better approach)
    conn->executeUpdate(
        "CREATE TABLE IF NOT EXISTS " + g_keyspace + ".recovery_test ("
                                                     "id int PRIMARY KEY, name text)");
    logOk("IF NOT EXISTS pattern works idempotently");

    // Pattern 2: IF EXISTS for safe deletes
    logMsg("");
    logStep("Pattern 2: IF EXISTS for safe operations...");

    conn->executeUpdate("DROP TABLE IF EXISTS " + g_keyspace + ".maybe_exists");
    logOk("DROP IF EXISTS completes without error even if table doesn't exist");

    // Pattern 3: Retry pattern
    logMsg("");
    logStep("Pattern 3: Retry pattern for transient errors...");

    int maxRetries = 3;
    bool success = false;
    for (int attempt = 1; attempt <= maxRetries && !success; ++attempt)
    {
        try
        {
            logData("Attempt " + std::to_string(attempt) + "...");
            conn->executeUpdate("INSERT INTO " + g_table + " (id, name, value) VALUES (" +
                                std::to_string(1000 + attempt) + ", 'retry_test', 1.0)");
            success = true;
            logOk("Operation succeeded on attempt " + std::to_string(attempt));
        }
        catch (const cpp_dbc::DBException &e)
        {
            logError("Attempt " + std::to_string(attempt) + " failed: " + e.what_s());
            if (attempt < maxRetries)
            {
                logInfo("Retrying...");
            }
        }
    }

    // Pattern 4: Lightweight transactions for conditional updates
    logMsg("");
    logStep("Pattern 4: Lightweight transactions (IF conditions)...");

    // Insert only if not exists
    try
    {
        conn->executeUpdate("INSERT INTO " + g_table + " (id, name, value) VALUES (2000, 'lwt_test', 1.0) IF NOT EXISTS");
        logOk("Lightweight transaction completed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logInfo("LWT operation result: " + e.what_s());
    }

    // Cleanup
    conn->executeUpdate("DROP TABLE IF EXISTS " + g_keyspace + ".recovery_test");
}

void cleanup(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    logMsg("");
    logMsg("--- Cleanup ---");
    logStep("Dropping test table and keyspace...");
    try
    {
        conn->executeUpdate("DROP TABLE IF EXISTS " + g_table);
        conn->executeUpdate("DROP KEYSPACE IF EXISTS " + g_keyspace);
        logOk("Cleanup completed");
    }
    catch (...)
    {
        logInfo("Cleanup completed (some items may not have existed)");
    }
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc ScyllaDB Error Handling Example");
    logMsg("========================================");
    logMsg("");

#if !USE_SCYLLADB
    logError("ScyllaDB support is not enabled");
    logInfo("Build with -DUSE_SCYLLADB=ON to enable ScyllaDB support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,scylladb");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("26_091_example_scylladb_error_handling", "scylladb");
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

    logStep("Getting ScyllaDB database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "scylladb");

    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!dbResult.value())
    {
        logError("ScyllaDB configuration not found");
        return EXIT_ERROR_;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) + ")");

    logStep("Registering ScyllaDB driver...");
    registerDriver("scylladb");
    logOk("Driver registered");

    auto driver = std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>();

    // Demonstrate connection errors (before main connection)
    demonstrateConnectionErrors(*driver);

    try
    {
        logStep("Connecting to ScyllaDB...");
        auto connBase = dbConfig.createDBConnection();
        auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(connBase);

        if (!conn)
        {
            logError("Failed to cast connection to ColumnarDBConnection");
            return EXIT_ERROR_;
        }
        logOk("Connected to ScyllaDB");

        setupSchema(conn);
        demonstrateCQLSyntaxErrors(conn);
        demonstrateKeyspaceAndTableErrors(conn);
        demonstrateDataTypeErrors(conn);
        demonstratePrimaryKeyErrors(conn);
        demonstrateNothrowAPI(conn);
        demonstrateErrorRecovery(conn);
        cleanup(conn);

        cpp_dbc::examples::logMsg("");
        logStep("Closing connection...");
        conn->close();
        logOk("Connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        return EXIT_ERROR_;
    }
    catch (const std::exception &e)
    {
        logError("Error: " + std::string(e.what()));
        return EXIT_ERROR_;
    }

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return EXIT_OK_;
#endif
}
