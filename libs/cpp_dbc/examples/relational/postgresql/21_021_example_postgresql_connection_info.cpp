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
 * @file 21_021_example_postgresql_connection_info.cpp
 * @brief PostgreSQL-specific example demonstrating connection URL information
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - PostgreSQL connection URL information
 *
 * Usage:
 *   ./21_021_example_postgresql_connection_info [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - PostgreSQL support not enabled at compile time
 */

#include "../../common/example_common.hpp"

#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif

using namespace cpp_dbc::examples;

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc PostgreSQL Connection Info Example");
    log("========================================");
    log("");

#if !USE_POSTGRESQL
    logError("PostgreSQL support is not enabled");
    logInfo("Build with -DUSE_POSTGRESQL=ON to enable PostgreSQL support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,postgres");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("21_021_example_postgresql_connection_info", "postgresql");
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

    logStep("Registering PostgreSQL driver...");
    registerDriver("postgresql");
    logOk("Driver registered");

    try
    {
        log("");
        log("--- PostgreSQL Connection URL ---");

        logStep("Getting PostgreSQL configuration...");
        auto pgResult = getDbConfig(configManager, args.dbName, "postgresql");

        if (!pgResult)
        {
            logError("Failed to get PostgreSQL config: " + pgResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!pgResult.value())
        {
            logError("PostgreSQL configuration not found");
            return EXIT_ERROR_;
        }

        auto &pgConfig = *pgResult.value();
        logOk("Using: " + pgConfig.getName());

        logStep("Connecting to PostgreSQL...");
        auto pgConn = pgConfig.createDBConnection();
        logOk("Connected");

        logData("PostgreSQL Connection URL: " + pgConn->getURL());

        logStep("Closing connection...");
        pgConn->close();
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
