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
 * @file 25_021_example_mongodb_connection_info.cpp
 * @brief MongoDB-specific example demonstrating connection URL information
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - MongoDB connection URL information
 * - Server ping and database listing
 *
 * Usage:
 *   ./25_021_example_mongodb_connection_info [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - MongoDB support not enabled at compile time
 */

#include "../../common/example_common.hpp"

#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#endif

using namespace cpp_dbc::examples;

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc MongoDB Connection Info Example");
    log("========================================");
    log("");

#if !USE_MONGODB
    logError("MongoDB support is not enabled");
    logInfo("Build with -DUSE_MONGODB=ON to enable MongoDB support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,mongodb");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("25_021_example_mongodb_connection_info", "mongodb");
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

    logStep("Registering MongoDB driver...");
    registerDriver("mongodb");
    logOk("Driver registered");

    try
    {
        log("");
        log("--- MongoDB Connection URL ---");

        logStep("Getting MongoDB configuration...");
        auto mongoResult = getDbConfig(configManager, args.dbName, "mongodb");

        if (!mongoResult)
        {
            logError("Failed to get MongoDB config: " + mongoResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!mongoResult.value())
        {
            logError("MongoDB configuration not found");
            return EXIT_ERROR_;
        }

        auto &mongoConfig = *mongoResult.value();
        logOk("Using: " + mongoConfig.getName());

        // Display configuration details
        log("");
        log("--- Configuration Details ---");
        logData("Name: " + mongoConfig.getName());
        logData("Type: " + mongoConfig.getType());
        logData("Host: " + mongoConfig.getHost());
        logData("Port: " + std::to_string(mongoConfig.getPort()));
        logData("Database: " + mongoConfig.getDatabase());

        logStep("Connecting to MongoDB...");
        auto mongoConn = mongoConfig.createDBConnection();
        logOk("Connected");

        logData("MongoDB Connection URL: " + mongoConn->getURL());

        // Cast to DocumentDBConnection for additional info
        auto docConn = std::dynamic_pointer_cast<cpp_dbc::DocumentDBConnection>(mongoConn);
        if (docConn)
        {
            // Ping server
            log("");
            log("--- Server Connectivity ---");
            logStep("Pinging server...");
            bool pingResult = docConn->ping();
            logData("PING response: " + std::string(pingResult ? "OK" : "FAILED"));
            if (pingResult)
            {
                logOk("Server is responding");
            }

            // Get database name
            log("");
            log("--- Database Information ---");
            logStep("Getting current database info...");
            logData("Current database: " + docConn->getDatabaseName());

            // List databases (if permissions allow)
            logStep("Listing databases...");
            try
            {
                auto databases = docConn->listDatabases();
                logData("Available databases:");
                for (const auto &db : databases)
                {
                    logData("  - " + db);
                }
                logOk("Databases listed");
            }
            catch (const cpp_dbc::DBException &e)
            {
                logInfo("Could not list databases (permissions may be restricted)");
            }

            // List collections in current database
            logStep("Listing collections in current database...");
            try
            {
                auto collections = docConn->listCollections();
                logData("Collections in " + docConn->getDatabaseName() + ":");
                if (collections.empty())
                {
                    logData("  (no collections)");
                }
                else
                {
                    for (const auto &coll : collections)
                    {
                        logData("  - " + coll);
                    }
                }
                logOk("Collections listed");
            }
            catch (const cpp_dbc::DBException &e)
            {
                logInfo("Could not list collections: " + e.what_s());
            }
        }

        logStep("Closing connection...");
        mongoConn->close();
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

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return EXIT_OK_;
#endif
}
