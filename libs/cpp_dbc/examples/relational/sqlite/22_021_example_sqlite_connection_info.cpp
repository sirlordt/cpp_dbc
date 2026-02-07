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
 * @file 22_021_example_sqlite_connection_info.cpp
 * @brief SQLite-specific example demonstrating connection URL information
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - SQLite connection URL information
 * - In-memory database connection
 *
 * Usage:
 *   ./22_021_example_sqlite_connection_info [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - SQLite support not enabled at compile time
 */

#include "../../common/example_common.hpp"

#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif

using namespace cpp_dbc::examples;

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc SQLite Connection Info Example");
    log("========================================");
    log("");

#if !USE_SQLITE
    logError("SQLite support is not enabled");
    logInfo("Build with -DUSE_SQLITE=ON to enable SQLite support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,sqlite");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("22_021_example_sqlite_connection_info", "sqlite");
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

    logStep("Registering SQLite driver...");
    registerDriver("sqlite");
    logOk("Driver registered");

    try
    {
        log("");
        log("--- SQLite Connection URL (from config) ---");

        logStep("Getting SQLite configuration...");
        auto sqliteResult = getDbConfig(configManager, args.dbName, "sqlite");

        if (sqliteResult && sqliteResult.value())
        {
            auto &sqliteConfig = *sqliteResult.value();
            logOk("Using: " + sqliteConfig.getName());

            logStep("Connecting to SQLite...");
            auto sqliteConn = sqliteConfig.createDBConnection();
            logOk("Connected");

            logData("SQLite Connection URL: " + sqliteConn->getURL());

            logStep("Closing connection...");
            sqliteConn->close();
            logOk("Connection closed");
        }
        else
        {
            logInfo("SQLite configuration not found in config file");
        }

        log("");
        log("--- SQLite In-Memory Database ---");

        logStep("Connecting to SQLite in-memory database...");
        auto sqliteMemConn = cpp_dbc::DriverManager::getDBConnection(
            "cpp_dbc:sqlite://:memory:",
            "", "");
        logOk("Connected to in-memory database");

        logData("SQLite In-Memory Connection URL: " + sqliteMemConn->getURL());

        logStep("Closing connection...");
        sqliteMemConn->close();
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
