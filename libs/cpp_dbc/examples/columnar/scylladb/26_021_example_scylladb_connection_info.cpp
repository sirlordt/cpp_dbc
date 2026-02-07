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
 * @file 26_021_example_scylladb_connection_info.cpp
 * @brief ScyllaDB-specific example demonstrating connection URL information
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - ScyllaDB connection URL information
 * - Cluster and keyspace information retrieval
 *
 * Usage:
 *   ./26_021_example_scylladb_connection_info [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - ScyllaDB support not enabled at compile time
 */

#include "../../common/example_common.hpp"

#if USE_SCYLLADB
#include <cpp_dbc/drivers/columnar/driver_scylladb.hpp>
#endif

using namespace cpp_dbc::examples;

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc ScyllaDB Connection Info Example");
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
        printHelp("26_021_example_scylladb_connection_info", "scylladb");
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

    logStep("Registering ScyllaDB driver...");
    registerDriver("scylladb");
    logOk("Driver registered");

    try
    {
        logMsg("");
        logMsg("--- ScyllaDB Connection URL ---");

        logStep("Getting ScyllaDB configuration...");
        auto scyllaResult = getDbConfig(configManager, args.dbName, "scylladb");

        if (!scyllaResult)
        {
            logError("Failed to get ScyllaDB config: " + scyllaResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!scyllaResult.value())
        {
            logError("ScyllaDB configuration not found");
            return EXIT_ERROR_;
        }

        auto &scyllaConfig = *scyllaResult.value();
        logOk("Using: " + scyllaConfig.getName());

        // Display configuration details
        logMsg("");
        logMsg("--- Configuration Details ---");
        logData("Name: " + scyllaConfig.getName());
        logData("Type: " + scyllaConfig.getType());
        logData("Host: " + scyllaConfig.getHost());
        logData("Port: " + std::to_string(scyllaConfig.getPort()));
        logData("Keyspace: " + scyllaConfig.getDatabase());

        logStep("Connecting to ScyllaDB...");
        auto scyllaConn = scyllaConfig.createDBConnection();
        logOk("Connected");

        logData("ScyllaDB Connection URL: " + scyllaConn->getURL());

        // Cast to ColumnarDBConnection for additional queries
        auto colConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(scyllaConn);
        if (colConn)
        {
            // Query cluster information
            logMsg("");
            logMsg("--- Cluster Information ---");
            logStep("Querying cluster information...");

            try
            {
                auto localInfo = colConn->executeQuery("SELECT cluster_name, data_center, rack FROM system.local");
                if (localInfo->next())
                {
                    logData("Cluster name: " + localInfo->getString("cluster_name"));
                    logData("Data center: " + localInfo->getString("data_center"));
                    logData("Rack: " + localInfo->getString("rack"));
                }
                logOk("Local node info retrieved");
            }
            catch (const cpp_dbc::DBException &e)
            {
                logInfo("Could not get local info: " + e.what_s());
            }

            // Get server version
            logStep("Getting server version...");
            try
            {
                auto version = colConn->executeQuery("SELECT release_version FROM system.local");
                if (version->next())
                {
                    logData("Release version: " + version->getString("release_version"));
                }
                logOk("Version retrieved");
            }
            catch (const cpp_dbc::DBException &e)
            {
                logInfo("Could not get version: " + e.what_s());
            }

            // List keyspaces
            logStep("Listing keyspaces...");
            try
            {
                auto keyspaces = colConn->executeQuery("SELECT keyspace_name FROM system_schema.keyspaces");
                logData("Available keyspaces:");
                while (keyspaces->next())
                {
                    logData("  - " + keyspaces->getString("keyspace_name"));
                }
                logOk("Keyspaces listed");
            }
            catch (const cpp_dbc::DBException &e)
            {
                logInfo("Could not list keyspaces: " + e.what_s());
            }

            // List nodes in cluster
            logStep("Listing cluster nodes...");
            try
            {
                auto peers = colConn->executeQuery("SELECT peer, data_center, rack FROM system.peers");
                int peerCount = 0;
                while (peers->next())
                {
                    logData("  Peer: " + peers->getString("peer") +
                            " (DC: " + peers->getString("data_center") +
                            ", Rack: " + peers->getString("rack") + ")");
                    ++peerCount;
                }
                if (peerCount == 0)
                {
                    logData("  (single-node cluster)");
                }
                else
                {
                    logData("  Total peers: " + std::to_string(peerCount));
                }
                logOk("Cluster nodes listed");
            }
            catch (const cpp_dbc::DBException &e)
            {
                logInfo("Could not list peers: " + e.what_s());
            }
        }

        logStep("Closing connection...");
        scyllaConn->close();
        logOk("Connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        e.printCallStack();
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
