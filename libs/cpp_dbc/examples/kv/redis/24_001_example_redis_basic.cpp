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
 * @file redis_example.cpp
 * @brief Example demonstrating Redis key-value database operations
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - String operations (set, get, TTL)
 * - Counter operations (increment, decrement)
 * - List operations (push, pop, range)
 * - Hash operations (set, get, delete)
 * - Set operations (add, remove, members)
 * - Sorted set operations
 * - Key scanning and server info
 *
 * Usage:
 *   ./redis_example [--config=<path>] [--db=<name>] [--help]
 *
 * Examples:
 *   ./redis_example                    # Uses dev_redis from default config
 *   ./redis_example --db=test_redis    # Uses test_redis configuration
 */

#include "../../common/example_common.hpp"
#include <vector>
#include <sstream>

#if USE_REDIS
#include <cpp_dbc/drivers/kv/driver_redis.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_REDIS
void performRedisOperations(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    // ===== String Operations =====
    logMsg("");
    logMsg("--- String Operations ---");

    std::string key = "example_string";
    std::string value = "Hello, Redis!";

    logStep("Setting string value...");
    if (conn->setString(key, value))
    {
        logData("SET " + key + " = '" + value + "'");
        logOk("String set successfully");
    }

    logStep("Getting string value...");
    std::string retrievedValue = conn->getString(key);
    logData("GET " + key + " = '" + retrievedValue + "'");
    logOk("String retrieved");

    logStep("Setting string with expiration (60s)...");
    if (conn->setString(key + "_exp", value, 60))
    {
        int64_t ttl = conn->getTTL(key + "_exp");
        logData("TTL(" + key + "_exp) = " + std::to_string(ttl) + " seconds");
        logOk("String with TTL set");
    }

    // ===== Counter Operations =====
    logMsg("");
    logMsg("--- Counter Operations ---");

    std::string counterKey = "example_counter";

    logStep("Initializing counter to 10...");
    conn->setString(counterKey, "10");
    logOk("Counter initialized");

    logStep("Incrementing counter...");
    int64_t newValue = conn->increment(counterKey);
    logData("INCR " + counterKey + " = " + std::to_string(newValue));

    logStep("Incrementing counter by 5...");
    newValue = conn->increment(counterKey, 5);
    logData("INCRBY " + counterKey + " 5 = " + std::to_string(newValue));

    logStep("Decrementing counter...");
    newValue = conn->decrement(counterKey);
    logData("DECR " + counterKey + " = " + std::to_string(newValue));
    logOk("Counter operations complete");

    // ===== List Operations =====
    logMsg("");
    logMsg("--- List Operations ---");

    std::string listKey = "example_list";

    logStep("Clearing existing list...");
    conn->deleteKey(listKey);

    logStep("Pushing elements to list...");
    conn->listPushRight(listKey, "first");
    conn->listPushRight(listKey, "second");
    conn->listPushLeft(listKey, "zero");
    logData("RPUSH/LPUSH: [zero, first, second]");
    logData("List length: " + std::to_string(conn->listLength(listKey)));

    auto listValues = conn->listRange(listKey, 0, -1);
    std::ostringstream listOss;
    listOss << "LRANGE 0 -1: ";
    for (const auto &item : listValues)
    {
        listOss << "'" << item << "' ";
    }
    logData(listOss.str());

    logStep("Popping from list...");
    std::string popValue = conn->listPopLeft(listKey);
    logData("LPOP = '" + popValue + "'");
    popValue = conn->listPopRight(listKey);
    logData("RPOP = '" + popValue + "'");
    logOk("List operations complete");

    // ===== Hash Operations =====
    logMsg("");
    logMsg("--- Hash Operations ---");

    std::string hashKey = "example_hash";

    logStep("Clearing existing hash...");
    conn->deleteKey(hashKey);

    logStep("Setting hash fields...");
    conn->hashSet(hashKey, "field1", "value1");
    conn->hashSet(hashKey, "field2", "value2");
    conn->hashSet(hashKey, "field3", "value3");
    logOk("3 fields set");

    logStep("Getting hash field...");
    logData("HGET field1 = '" + conn->hashGet(hashKey, "field1") + "'");

    logStep("Getting all hash fields...");
    auto hashValues = conn->hashGetAll(hashKey);
    for (const auto &[field, val] : hashValues)
    {
        logData(field + " = '" + val + "'");
    }

    logStep("Deleting field2...");
    conn->hashDelete(hashKey, "field2");
    logData("Hash length after delete: " + std::to_string(conn->hashLength(hashKey)));
    logOk("Hash operations complete");

    // ===== Set Operations =====
    logMsg("");
    logMsg("--- Set Operations ---");

    std::string setKey = "example_set";

    logStep("Clearing existing set...");
    conn->deleteKey(setKey);

    logStep("Adding members to set...");
    conn->setAdd(setKey, "member1");
    conn->setAdd(setKey, "member2");
    conn->setAdd(setKey, "member3");
    logOk("3 members added");

    logData("SISMEMBER member2 = " + std::string(conn->setIsMember(setKey, "member2") ? "true" : "false"));
    logData("Set size: " + std::to_string(conn->setSize(setKey)));

    auto setMembers = conn->setMembers(setKey);
    std::ostringstream setOss;
    setOss << "SMEMBERS: ";
    for (const auto &member : setMembers)
    {
        setOss << "'" << member << "' ";
    }
    logData(setOss.str());

    logStep("Removing member2...");
    conn->setRemove(setKey, "member2");
    logData("Set size after removal: " + std::to_string(conn->setSize(setKey)));
    logOk("Set operations complete");

    // ===== Sorted Set Operations =====
    logMsg("");
    logMsg("--- Sorted Set Operations ---");

    std::string zsetKey = "example_zset";

    logStep("Clearing existing sorted set...");
    conn->deleteKey(zsetKey);

    logStep("Adding scored members...");
    conn->sortedSetAdd(zsetKey, 1.0, "item1");
    conn->sortedSetAdd(zsetKey, 2.5, "item2");
    conn->sortedSetAdd(zsetKey, 3.7, "item3");
    logOk("3 members added with scores");

    auto score = conn->sortedSetScore(zsetKey, "item2");
    if (score)
    {
        logData("ZSCORE item2 = " + std::to_string(*score));
    }

    auto zsetMembers = conn->sortedSetRange(zsetKey, 0, -1);
    std::ostringstream zsetOss;
    zsetOss << "ZRANGE 0 -1: ";
    for (const auto &member : zsetMembers)
    {
        zsetOss << "'" << member << "' ";
    }
    logData(zsetOss.str());
    logData("Sorted set size: " + std::to_string(conn->sortedSetSize(zsetKey)));
    logOk("Sorted set operations complete");

    // ===== Key Scan =====
    logMsg("");
    logMsg("--- Key Scan ---");

    logStep("Scanning for keys matching 'example_*'...");
    auto keys = conn->scanKeys("example_*");
    std::ostringstream keysOss;
    keysOss << "Found " << keys.size() << " keys: ";
    for (const auto &k : keys)
    {
        keysOss << "'" << k << "' ";
    }
    logData(keysOss.str());
    logOk("Key scan complete");

    // ===== Server Info =====
    logMsg("");
    logMsg("--- Server Info ---");

    logStep("Pinging server...");
    std::string pong = conn->ping();
    logData("PING = '" + pong + "'");
    logOk("Server responded");

    // ===== Cleanup =====
    logMsg("");
    logMsg("--- Cleanup ---");

    logStep("Deleting all example keys...");
    std::vector<std::string> keysToDelete = {
        key, key + "_exp", counterKey, listKey, hashKey, setKey, zsetKey};
    int64_t deleted = conn->deleteKeys(keysToDelete);
    logData("Deleted " + std::to_string(deleted) + " keys");
    logOk("Cleanup complete");
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc Redis Key-Value Example");
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
        printHelp("redis_example", "redis");
        return EXIT_OK_;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    // Check for real error (DBException)
    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return EXIT_ERROR_;
    }

    // Check if file was found
    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return EXIT_ERROR_;
    }
    logOk("Configuration loaded successfully");

    auto &configManager = *configResult.value();

    logStep("Getting database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "redis");

    // Check for real error
    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return EXIT_ERROR_;
    }

    // Check if config was found
    if (!dbResult.value())
    {
        logError("Redis configuration not found");
        return EXIT_ERROR_;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) + ")");

    logStep("Registering Redis driver...");
    registerDriver("redis");
    logOk("Driver registered");

    try
    {
        logStep("Connecting to Redis...");

        // Create Redis driver and connect
        auto driver = std::make_shared<cpp_dbc::Redis::RedisDriver>();
        std::string url = "cpp_dbc:redis://" + dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) + "/" + dbConfig.getDatabase();
        auto conn = driver->connectKV(url, dbConfig.getUsername(), dbConfig.getPassword());

        logOk("Connected to Redis");

        performRedisOperations(conn);

        logMsg("");
        logStep("Closing connection...");
        conn->close();
        logOk("Connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
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
