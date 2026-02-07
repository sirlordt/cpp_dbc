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
 * @file 20_021_example_mysql_connection_info.cpp
 * @brief MySQL-specific example demonstrating connection URL information
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - MySQL connection URL information
 *
 * Usage:
 *   ./20_021_example_mysql_connection_info [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - MySQL support not enabled at compile time
 */

#include "../../common/example_common.hpp"

#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif

using namespace cpp_dbc::examples;

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc MySQL Connection Info Example");
    logMsg("========================================");
    logMsg("");

#if !USE_MYSQL
    logError("MySQL support is not enabled");
    logInfo("Build with -DUSE_MYSQL=ON to enable MySQL support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,mysql");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("20_021_example_mysql_connection_info", "mysql");
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

    logStep("Registering MySQL driver...");
    registerDriver("mysql");
    logOk("Driver registered");

    try
    {
        logMsg("");
        logMsg("--- MySQL Connection URL ---");

        logStep("Getting MySQL configuration...");
        auto mysqlResult = getDbConfig(configManager, args.dbName, "mysql");

        if (!mysqlResult)
        {
            logError("Failed to get MySQL config: " + mysqlResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!mysqlResult.value())
        {
            logError("MySQL configuration not found");
            return EXIT_ERROR_;
        }

        auto &mysqlConfig = *mysqlResult.value();
        logOk("Using: " + mysqlConfig.getName());

        logStep("Connecting to MySQL...");
        auto mysqlConn = mysqlConfig.createDBConnection();
        logOk("Connected");

        logData("MySQL Connection URL: " + mysqlConn->getURL());

        logStep("Closing connection...");
        mysqlConn->close();
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
