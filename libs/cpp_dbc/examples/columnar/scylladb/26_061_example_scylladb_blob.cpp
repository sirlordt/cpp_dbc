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
 * @file 26_061_example_scylladb_blob.cpp
 * @brief Example demonstrating ScyllaDB BLOB operations
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - Creating tables with BLOB columns
 * - Inserting and retrieving binary data
 * - Verifying data integrity
 *
 * Usage:
 *   ./scylla_blob_example [--config=<path>] [--db=<name>] [--help]
 *
 * Examples:
 *   ./scylla_blob_example                    # Uses dev_scylla from default config
 *   ./scylla_blob_example --db=test_scylla   # Uses test_scylla configuration
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
std::string formatHex(const std::vector<uint8_t> &data)
{
    std::ostringstream oss;
    for (size_t i = 0; i < data.size(); ++i)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
        if ((i + 1) % 16 == 0 && i + 1 < data.size())
            oss << "\n";
    }
    return oss.str();
}

void performBlobOperations(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    std::string keyspace = "test_keyspace";
    std::string table = keyspace + ".blob_example";

    // ===== Table Setup =====
    logMsg("");
    logMsg("--- Table Setup ---");

    logStep("Creating keyspace if not exists...");
    conn->executeUpdate("CREATE KEYSPACE IF NOT EXISTS " + keyspace +
                        " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    logOk("Keyspace ready");

    logStep("Dropping existing table if exists...");
    conn->executeUpdate("DROP TABLE IF EXISTS " + table);
    logOk("Old table dropped");

    logStep("Creating table with BLOB column...");
    conn->executeUpdate(
        "CREATE TABLE " + table + " ("
                                  "id int PRIMARY KEY, "
                                  "description text, "
                                  "data blob"
                                  ")");
    logOk("Table created");

    // ===== BLOB Insert =====
    logMsg("");
    logMsg("--- BLOB Insert ---");

    logStep("Creating binary test data...");
    std::vector<uint8_t> binaryData = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    for (int i = 0; i < 10; ++i)
    {
        binaryData.push_back(static_cast<uint8_t>(i));
    }
    logOk("Created " + std::to_string(binaryData.size()) + " bytes of test data");

    logData("Original data (hex):");
    logData(formatHex(binaryData));

    logStep("Inserting BLOB...");
    auto pstmt = conn->prepareStatement("INSERT INTO " + table + " (id, description, data) VALUES (?, ?, ?)");
    pstmt->setInt(1, 1);
    pstmt->setString(2, "Test Blob 1");
    pstmt->setBytes(3, binaryData);
    pstmt->executeUpdate();
    logOk("BLOB inserted");

    // ===== BLOB Retrieve =====
    logMsg("");
    logMsg("--- BLOB Retrieve ---");

    logStep("Retrieving BLOB...");
    auto rs = conn->executeQuery("SELECT * FROM " + table + " WHERE id = 1");
    if (rs->next())
    {
        logData("Description: " + rs->getString("description"));
        std::vector<uint8_t> retrievedData = rs->getBytes("data");
        logData("Retrieved " + std::to_string(retrievedData.size()) + " bytes");

        logData("Retrieved data (hex):");
        logData(formatHex(retrievedData));

        // ===== Verification =====
        logStep("Verifying data integrity...");
        if (binaryData == retrievedData)
        {
            logOk("Data integrity verified - retrieved data matches original");
        }
        else
        {
            logError("Data mismatch - integrity check failed!");
        }
    }
    else
    {
        logError("Row not found");
    }

    // ===== Cleanup =====
    logMsg("");
    logMsg("--- Cleanup ---");

    logStep("Dropping table...");
    conn->executeUpdate("DROP TABLE " + table);
    logOk("Table dropped");
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc ScyllaDB BLOB Example");
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
        printHelp("scylla_blob_example", "scylladb");
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

        performBlobOperations(conn);

        logMsg("");
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

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return 0;
#endif
}
