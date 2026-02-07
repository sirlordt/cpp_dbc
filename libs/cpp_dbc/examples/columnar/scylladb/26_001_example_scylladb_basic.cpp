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
 * @file scylla_example.cpp
 * @brief Example demonstrating basic ScyllaDB columnar database operations
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - Keyspace creation
 * - Table creation and management
 * - CRUD operations with prepared statements
 * - Data querying and verification
 *
 * Usage:
 *   ./scylla_example [--config=<path>] [--db=<name>] [--help]
 *
 * Examples:
 *   ./scylla_example                    # Uses dev_scylla from default config
 *   ./scylla_example --db=test_scylla   # Uses test_scylla configuration
 */

#include "../../common/example_common.hpp"
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#include <iomanip>
#include <sstream>

#if USE_SCYLLADB
#include <cpp_dbc/drivers/columnar/driver_scylladb.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_SCYLLADB
void performScyllaDBOperations(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    std::string keyspace = "test_keyspace";
    std::string table = keyspace + ".example_table";

    // ===== Keyspace Setup =====
    log("");
    log("--- Keyspace Setup ---");

    logStep("Creating keyspace if not exists...");
    conn->executeUpdate(
        "CREATE KEYSPACE IF NOT EXISTS " + keyspace +
        " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    logOk("Keyspace '" + keyspace + "' ready");

    // ===== Table Setup =====
    log("");
    log("--- Table Setup ---");

    logStep("Dropping existing table if exists...");
    conn->executeUpdate("DROP TABLE IF EXISTS " + table);
    logOk("Old table dropped");

    logStep("Creating table...");
    conn->executeUpdate(
        "CREATE TABLE " + table + " ("
                                  "id int PRIMARY KEY, "
                                  "name text, "
                                  "value double"
                                  ")");
    logOk("Table '" + table + "' created");

    // ===== Insert Operations =====
    log("");
    log("--- Insert Operations ---");

    logStep("Preparing insert statement...");
    auto pstmt = conn->prepareStatement(
        "INSERT INTO " + table + " (id, name, value) VALUES (?, ?, ?)");
    logOk("Statement prepared");

    logStep("Inserting 5 rows...");
    for (int i = 1; i <= 5; ++i)
    {
        pstmt->setInt(1, i);
        pstmt->setString(2, "Item " + std::to_string(i));
        pstmt->setDouble(3, i * 1.5);
        pstmt->executeUpdate();

        std::ostringstream oss;
        oss << "Inserted: id=" << i
            << ", name='Item " << i << "'"
            << ", value=" << std::fixed << std::setprecision(1) << (i * 1.5);
        logData(oss.str());
    }
    logOk("5 rows inserted");

    // ===== Select Single Row =====
    log("");
    log("--- Select Single Row ---");

    logStep("Selecting row with id=3...");
    auto selectStmt = conn->prepareStatement("SELECT * FROM " + table + " WHERE id = ?");
    selectStmt->setInt(1, 3);
    auto rs = selectStmt->executeQuery();

    if (rs->next())
    {
        std::ostringstream oss;
        oss << "Found: id=" << rs->getInt("id")
            << ", name='" << rs->getString("name") << "'"
            << ", value=" << std::fixed << std::setprecision(1) << rs->getDouble("value");
        logData(oss.str());
        logOk("Row found");
    }
    else
    {
        logInfo("Row not found");
    }

    // ===== Update Operation =====
    log("");
    log("--- Update Operation ---");

    logStep("Updating name for id=3...");
    auto updateStmt = conn->prepareStatement("UPDATE " + table + " SET name = ? WHERE id = ?");
    updateStmt->setString(1, "Updated Item 3");
    updateStmt->setInt(2, 3);
    updateStmt->executeUpdate();
    logOk("Row updated");

    logStep("Verifying update...");
    selectStmt->setInt(1, 3);
    rs = selectStmt->executeQuery();
    if (rs->next())
    {
        std::ostringstream oss;
        oss << "Verified: id=" << rs->getInt("id")
            << ", name='" << rs->getString("name") << "'";
        logData(oss.str());
        logOk("Update verified");
    }

    // ===== Select All Rows =====
    log("");
    log("--- Select All Rows ---");

    logStep("Querying all rows...");
    rs = conn->executeQuery("SELECT * FROM " + table);
    int rowCount = 0;
    while (rs->next())
    {
        std::ostringstream oss;
        oss << "Row: id=" << rs->getInt("id")
            << ", name='" << rs->getString("name") << "'"
            << ", value=" << std::fixed << std::setprecision(1) << rs->getDouble("value");
        logData(oss.str());
        ++rowCount;
    }
    logOk("Query returned " + std::to_string(rowCount) + " row(s)");

    // ===== Delete Operation =====
    log("");
    log("--- Delete Operation ---");

    logStep("Deleting row with id=5...");
    conn->executeUpdate("DELETE FROM " + table + " WHERE id = 5");
    logOk("Row deleted");

    logStep("Verifying deletion (count)...");
    rs = conn->executeQuery("SELECT COUNT(*) as count FROM " + table);
    if (rs->next())
    {
        logData("Remaining rows: " + std::to_string(rs->getLong("count")));
        logOk("Deletion verified");
    }

    // ===== Cleanup =====
    log("");
    log("--- Cleanup ---");

    logStep("Dropping table...");
    conn->executeUpdate("DROP TABLE " + table);
    logOk("Table dropped successfully");
}
#endif

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc ScyllaDB Columnar Example");
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
        printHelp("scylla_example", "scylladb");
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

        performScyllaDBOperations(conn);

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
