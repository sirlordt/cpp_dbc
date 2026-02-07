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
 * @file 23_021_example_firebird_connection_info.cpp
 * @brief Firebird-specific example demonstrating connection URL information
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - Firebird connection URL information
 *
 * Usage:
 *   ./23_021_example_firebird_connection_info [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - Firebird support not enabled at compile time
 */

#include "../../common/example_common.hpp"

#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif

using namespace cpp_dbc::examples;

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc Firebird Connection Info Example");
    logMsg("========================================");
    logMsg("");

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
        printHelp("23_021_example_firebird_connection_info", "firebird");
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

    logStep("Registering Firebird driver...");
    registerDriver("firebird");
    logOk("Driver registered");

    try
    {
        logMsg("");
        logMsg("--- Firebird Connection URL ---");

        logStep("Getting Firebird configuration...");
        auto firebirdResult = getDbConfig(configManager, args.dbName, "firebird");

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
        logOk("Using: " + firebirdConfig.getName());

        // Try to create database if it doesn't exist
        tryCreateFirebirdDatabase(firebirdConfig);

        logStep("Connecting to Firebird...");
        auto firebirdConn = firebirdConfig.createDBConnection();
        logOk("Connected");

        logData("Firebird Connection URL: " + firebirdConn->getURL());

        // Get server information
        logMsg("");
        logMsg("--- Server Information ---");
        logStep("Querying server information...");

        auto relConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(firebirdConn);
        if (relConn)
        {
            auto rs = relConn->executeQuery(
                "SELECT RDB$GET_CONTEXT('SYSTEM', 'ENGINE_VERSION') AS VERSION, "
                "CURRENT_USER AS CURRENT_USER_NAME, "
                "CURRENT_CONNECTION AS CONNECTION_ID "
                "FROM RDB$DATABASE");

            if (rs->next())
            {
                logData("Server version: " + rs->getString("VERSION"));
                logData("Current user: " + rs->getString("CURRENT_USER_NAME"));
                logData("Connection ID: " + rs->getString("CONNECTION_ID"));
            }
        }

        logStep("Closing connection...");
        firebirdConn->close();
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
