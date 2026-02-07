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
 * @file scylla_json_example.cpp
 * @brief Example demonstrating ScyllaDB JSON operations (storing JSON as text)
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - Storing JSON data as text in ScyllaDB
 * - Inserting and retrieving JSON documents
 *
 * Usage:
 *   ./scylla_json_example [--config=<path>] [--db=<name>] [--help]
 *
 * Examples:
 *   ./scylla_json_example                    # Uses dev_scylla from default config
 *   ./scylla_json_example --db=test_scylla   # Uses test_scylla configuration
 */

#include "../../common/example_common.hpp"
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>

#if USE_SCYLLADB
#include <cpp_dbc/drivers/columnar/driver_scylladb.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_SCYLLADB
void performJsonOperations(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    std::string keyspace = "test_keyspace";
    std::string table = keyspace + ".json_example";

    // ===== Table Setup =====
    log("");
    log("--- Table Setup ---");

    logStep("Creating keyspace if not exists...");
    conn->executeUpdate("CREATE KEYSPACE IF NOT EXISTS " + keyspace +
                        " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    logOk("Keyspace ready");

    logStep("Dropping existing table if exists...");
    conn->executeUpdate("DROP TABLE IF EXISTS " + table);
    logOk("Old table dropped");

    logStep("Creating table with JSON text column...");
    conn->executeUpdate(
        "CREATE TABLE " + table + " ("
                                  "id int PRIMARY KEY, "
                                  "json_data text"
                                  ")");
    logOk("Table created");

    // ===== Insert JSON Data =====
    log("");
    log("--- Insert JSON Data ---");

    logStep("Preparing insert statement...");
    auto pstmt = conn->prepareStatement("INSERT INTO " + table + " (id, json_data) VALUES (?, ?)");
    logOk("Statement prepared");

    // Simple JSON object
    logStep("Inserting simple JSON object...");
    std::string json1 = R"({"name": "John", "age": 30, "city": "New York"})";
    pstmt->setInt(1, 1);
    pstmt->setString(2, json1);
    pstmt->executeUpdate();
    logData("JSON 1: " + json1);
    logOk("Inserted");

    // JSON array
    logStep("Inserting JSON array...");
    std::string json2 = "[1, 2, 3, 4, 5]";
    pstmt->setInt(1, 2);
    pstmt->setString(2, json2);
    pstmt->executeUpdate();
    logData("JSON 2: " + json2);
    logOk("Inserted");

    // Nested JSON
    logStep("Inserting nested JSON...");
    std::string json3 = R"({"person": {"name": "Alice", "address": {"city": "Wonderland"}}})";
    pstmt->setInt(1, 3);
    pstmt->setString(2, json3);
    pstmt->executeUpdate();
    logData("JSON 3: " + json3);
    logOk("Inserted");

    // ===== Retrieve Data =====
    log("");
    log("--- Retrieve JSON Data ---");

    logStep("Querying all JSON data...");
    auto rs = conn->executeQuery("SELECT * FROM " + table);
    int rowCount = 0;
    while (rs->next())
    {
        ++rowCount;
        logData("Row " + std::to_string(rowCount) + ": id=" +
                std::to_string(rs->getInt("id")) + ", json=" + rs->getString("json_data"));
    }
    logOk("Retrieved " + std::to_string(rowCount) + " row(s)");

    // ===== Query Specific Row =====
    log("");
    log("--- Query Specific Row ---");

    logStep("Querying row with id=1...");
    auto selectStmt = conn->prepareStatement("SELECT * FROM " + table + " WHERE id = ?");
    selectStmt->setInt(1, 1);
    rs = selectStmt->executeQuery();
    if (rs->next())
    {
        logData("Found: " + rs->getString("json_data"));
        logOk("Row found");
    }
    else
    {
        logInfo("Row not found");
    }

    // ===== Cleanup =====
    log("");
    log("--- Cleanup ---");

    logStep("Dropping table...");
    conn->executeUpdate("DROP TABLE " + table);
    logOk("Table dropped");
}
#endif

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc ScyllaDB JSON Example");
    log("========================================");
    log("");

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
        printHelp("scylla_json_example", "scylladb");
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
    auto dbResult = getDbConfig(configManager, args.dbName, "scylladb");

    // Check for real error
    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return 1;
    }

    // Check if config was found
    if (!dbResult.value())
    {
        logError("ScyllaDB configuration not found");
        return 1;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) +
          "/" + dbConfig.getDatabase() + ")");

    logStep("Registering ScyllaDB driver...");
    registerDriver("scylladb");
    logOk("Driver registered");

    try
    {
        logStep("Connecting to ScyllaDB...");
        auto connBase = dbConfig.createDBConnection();
        auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(connBase);

        if (!conn)
        {
            logError("Failed to cast connection to ColumnarDBConnection");
            return 1;
        }
        logOk("Connected to ScyllaDB");

        performJsonOperations(conn);

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
