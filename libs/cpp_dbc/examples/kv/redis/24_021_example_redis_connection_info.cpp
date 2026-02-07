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
 * @file 24_021_example_redis_connection_info.cpp
 * @brief Redis-specific example demonstrating connection URL information
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - Redis connection URL information
 * - Server ping and basic connectivity test
 *
 * Usage:
 *   ./24_021_example_redis_connection_info [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - Redis support not enabled at compile time
 */

#include "../../common/example_common.hpp"

#if USE_REDIS
#include <cpp_dbc/drivers/kv/driver_redis.hpp>
#endif

using namespace cpp_dbc::examples;

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc Redis Connection Info Example");
    logMsg("========================================");
    logMsg("");

#if !USE_REDIS
    logError("Redis support is not enabled");
    logInfo("Build with -DUSE_REDIS=ON to enable Redis support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,redis");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("24_021_example_redis_connection_info", "redis");
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

    logStep("Registering Redis driver...");
    registerDriver("redis");
    logOk("Driver registered");

    try
    {
        logMsg("");
        logMsg("--- Redis Connection URL ---");

        logStep("Getting Redis configuration...");
        auto redisResult = getDbConfig(configManager, args.dbName, "redis");

        if (!redisResult)
        {
            logError("Failed to get Redis config: " + redisResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!redisResult.value())
        {
            logError("Redis configuration not found");
            return EXIT_ERROR_;
        }

        auto &redisConfig = *redisResult.value();
        logOk("Using: " + redisConfig.getName());

        // Display configuration details
        logMsg("");
        logMsg("--- Configuration Details ---");
        logData("Name: " + redisConfig.getName());
        logData("Type: " + redisConfig.getType());
        logData("Host: " + redisConfig.getHost());
        logData("Port: " + std::to_string(redisConfig.getPort()));

        logStep("Connecting to Redis...");
        auto redisConn = redisConfig.createDBConnection();
        logOk("Connected");

        logData("Redis Connection URL: " + redisConn->getURL());

        // Cast to KVDBConnection to access ping
        auto kvConn = std::dynamic_pointer_cast<cpp_dbc::KVDBConnection>(redisConn);
        if (kvConn)
        {
            logMsg("");
            logMsg("--- Server Connectivity ---");
            logStep("Pinging server...");
            std::string pong = kvConn->ping();
            logData("PING response: " + pong);
            logOk("Server is responding");

            logData("Connected to: " + redisConfig.getHost() + ":" + std::to_string(redisConfig.getPort()));
        }

        logStep("Closing connection...");
        redisConn->close();
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
