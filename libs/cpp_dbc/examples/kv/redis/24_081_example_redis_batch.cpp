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
 * @file 24_081_example_redis_batch.cpp
 * @brief Redis-specific example demonstrating bulk operations
 *
 * This example demonstrates:
 * - Bulk key deletion with deleteKeys()
 * - Multiple key set with MSET command
 * - Multiple key get with MGET command
 * - Batch hash operations with HMSET
 * - Performance comparison: individual vs batch operations
 *
 * Note: Redis batch operations improve performance by reducing round-trips.
 * For atomic batch operations, use MULTI/EXEC (see transaction example).
 *
 * Usage:
 *   ./24_081_example_redis_batch [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - Redis support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <vector>
#include <chrono>

#if USE_REDIS
#include <cpp_dbc/drivers/kv/driver_redis.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_REDIS
void demonstrateBulkDelete(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    log("");
    log("--- Bulk Key Deletion ---");
    logInfo("Using deleteKeys() to delete multiple keys at once");

    // Create test keys
    std::vector<std::string> keys;
    for (int i = 1; i <= 10; ++i)
    {
        std::string key = "batch_del_key_" + std::to_string(i);
        keys.push_back(key);
        conn->setString(key, "value_" + std::to_string(i));
    }
    logData("Created " + std::to_string(keys.size()) + " test keys");

    logStep("Verifying keys exist...");
    int existCount = 0;
    for (const auto &key : keys)
    {
        if (conn->exists(key))
        {
            ++existCount;
        }
    }
    logData("Keys existing before delete: " + std::to_string(existCount));

    logStep("Deleting all keys with single deleteKeys() call...");
    int64_t deleted = conn->deleteKeys(keys);
    logData("Keys deleted: " + std::to_string(deleted));
    logOk("Bulk delete completed");

    logStep("Verifying keys deleted...");
    existCount = 0;
    for (const auto &key : keys)
    {
        if (conn->exists(key))
        {
            ++existCount;
        }
    }
    logData("Keys remaining: " + std::to_string(existCount));
    if (existCount == 0)
    {
        logOk("All keys successfully deleted");
    }
    else
    {
        logError("Some keys were not deleted!");
    }
}

void demonstrateMsetMget(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    log("");
    log("--- MSET/MGET Operations ---");
    logInfo("Using executeCommand for bulk set/get operations");

    // Build MSET arguments: key1 value1 key2 value2 ...
    std::vector<std::string> msetArgs;
    std::vector<std::string> keys;
    for (int i = 1; i <= 5; ++i)
    {
        std::string key = "batch_mset_key_" + std::to_string(i);
        std::string value = "batch_value_" + std::to_string(i);
        keys.push_back(key);
        msetArgs.push_back(key);
        msetArgs.push_back(value);
    }

    logStep("Setting multiple keys with MSET...");
    std::string result = conn->executeCommand("MSET", msetArgs);
    logData("MSET response: " + result);
    logOk("Multiple keys set in single command");

    logStep("Getting multiple keys with MGET...");
    std::string mgetResult = conn->executeCommand("MGET", keys);
    logData("MGET response: " + mgetResult);

    // Verify individually
    logStep("Verifying values...");
    for (size_t i = 0; i < keys.size(); ++i)
    {
        std::string value = conn->getString(keys[i]);
        logData(keys[i] + " = '" + value + "'");
    }
    logOk("All values verified");

    // Cleanup
    conn->deleteKeys(keys);
}

void demonstrateBatchHashOperations(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    log("");
    log("--- Batch Hash Operations ---");
    logInfo("Using HMSET to set multiple hash fields at once");

    std::string hashKey = "batch_user_profile";

    logStep("Deleting existing hash...");
    conn->deleteKey(hashKey);

    logStep("Setting multiple hash fields with HMSET...");
    std::vector<std::string> hmsetArgs = {
        hashKey,
        "name", "John Doe",
        "email", "john@example.com",
        "age", "30",
        "city", "New York",
        "country", "USA",
        "role", "developer"};
    std::string result = conn->executeCommand("HMSET", hmsetArgs);
    logData("HMSET response: " + result);
    logOk("Multiple hash fields set in single command");

    logStep("Getting all hash fields with HGETALL...");
    auto fields = conn->hashGetAll(hashKey);
    for (const auto &[field, value] : fields)
    {
        logData(field + " = '" + value + "'");
    }
    logOk("All fields retrieved");

    logStep("Getting multiple specific fields with HMGET...");
    std::string hmgetResult = conn->executeCommand("HMGET", {hashKey, "name", "email", "role"});
    logData("HMGET response: " + hmgetResult);

    // Cleanup
    conn->deleteKey(hashKey);
}

void demonstratePerformanceComparison(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    log("");
    log("--- Performance Comparison ---");
    logInfo("Comparing individual vs batch operations");

    const int numKeys = 100;
    std::vector<std::string> keys;
    for (int i = 0; i < numKeys; ++i)
    {
        keys.push_back("perf_test_key_" + std::to_string(i));
    }

    // Individual deletions (cleanup any existing keys)
    conn->deleteKeys(keys);

    // Individual SET operations
    logStep("Individual SET operations (" + std::to_string(numKeys) + " keys)...");
    auto startIndividual = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < numKeys; ++i)
    {
        conn->setString(keys[i], "value_" + std::to_string(i));
    }
    auto endIndividual = std::chrono::high_resolution_clock::now();
    auto durationIndividual = std::chrono::duration_cast<std::chrono::microseconds>(
        endIndividual - startIndividual);
    logData("Individual SET time: " + std::to_string(durationIndividual.count()) + " microseconds");

    // Cleanup
    conn->deleteKeys(keys);

    // Batch SET with MSET
    logStep("Batch MSET operation (" + std::to_string(numKeys) + " keys)...");
    std::vector<std::string> msetArgs;
    for (int i = 0; i < numKeys; ++i)
    {
        msetArgs.push_back(keys[i]);
        msetArgs.push_back("value_" + std::to_string(i));
    }

    auto startBatch = std::chrono::high_resolution_clock::now();
    conn->executeCommand("MSET", msetArgs);
    auto endBatch = std::chrono::high_resolution_clock::now();
    auto durationBatch = std::chrono::duration_cast<std::chrono::microseconds>(
        endBatch - startBatch);
    logData("Batch MSET time: " + std::to_string(durationBatch.count()) + " microseconds");

    // Calculate speedup
    if (durationBatch.count() > 0)
    {
        double speedup = static_cast<double>(durationIndividual.count()) / static_cast<double>(durationBatch.count());
        logData("Speedup factor: " + std::to_string(speedup) + "x");
    }
    logOk("Performance comparison completed");

    // Individual DELETE vs bulk DELETE
    logStep("Individual DELETE operations...");
    auto startDelIndividual = std::chrono::high_resolution_clock::now();
    for (const auto &key : keys)
    {
        conn->deleteKey(key);
    }
    auto endDelIndividual = std::chrono::high_resolution_clock::now();
    auto durationDelIndividual = std::chrono::duration_cast<std::chrono::microseconds>(
        endDelIndividual - startDelIndividual);
    logData("Individual DELETE time: " + std::to_string(durationDelIndividual.count()) + " microseconds");

    // Recreate keys for bulk delete test
    conn->executeCommand("MSET", msetArgs);

    logStep("Bulk deleteKeys operation...");
    auto startDelBatch = std::chrono::high_resolution_clock::now();
    conn->deleteKeys(keys);
    auto endDelBatch = std::chrono::high_resolution_clock::now();
    auto durationDelBatch = std::chrono::duration_cast<std::chrono::microseconds>(
        endDelBatch - startDelBatch);
    logData("Bulk DELETE time: " + std::to_string(durationDelBatch.count()) + " microseconds");

    if (durationDelBatch.count() > 0)
    {
        double speedup = static_cast<double>(durationDelIndividual.count()) / static_cast<double>(durationDelBatch.count());
        logData("DELETE speedup factor: " + std::to_string(speedup) + "x");
    }
    logOk("Delete comparison completed");
}

void demonstrateBatchListOperations(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    log("");
    log("--- Batch List Operations ---");
    logInfo("Using LPUSH/RPUSH with multiple values");

    std::string listKey = "batch_queue";

    logStep("Deleting existing list...");
    conn->deleteKey(listKey);

    logStep("Pushing multiple values with RPUSH...");
    std::vector<std::string> values = {"item1", "item2", "item3", "item4", "item5"};
    std::vector<std::string> rpushArgs = {listKey};
    rpushArgs.insert(rpushArgs.end(), values.begin(), values.end());

    std::string result = conn->executeCommand("RPUSH", rpushArgs);
    logData("RPUSH response (list length): " + result);
    logOk("Multiple values pushed in single command");

    logStep("Getting list range...");
    auto items = conn->listRange(listKey, 0, -1);
    for (size_t i = 0; i < items.size(); ++i)
    {
        logData("[" + std::to_string(i) + "] = '" + items[i] + "'");
    }

    // Cleanup
    conn->deleteKey(listKey);
}

void demonstrateBatchSetOperations(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    log("");
    log("--- Batch Set Operations ---");
    logInfo("Using SADD with multiple members");

    std::string setKey = "batch_tags";

    logStep("Deleting existing set...");
    conn->deleteKey(setKey);

    logStep("Adding multiple members with SADD...");
    std::vector<std::string> saddArgs = {setKey, "tag1", "tag2", "tag3", "tag4", "tag5"};
    std::string result = conn->executeCommand("SADD", saddArgs);
    logData("SADD response (members added): " + result);
    logOk("Multiple members added in single command");

    logStep("Getting all set members...");
    auto members = conn->setMembers(setKey);
    for (const auto &member : members)
    {
        logData("Member: '" + member + "'");
    }

    logStep("Removing multiple members with SREM...");
    std::string sremResult = conn->executeCommand("SREM", {setKey, "tag1", "tag3", "tag5"});
    logData("SREM response (members removed): " + sremResult);

    logStep("Remaining members...");
    members = conn->setMembers(setKey);
    for (const auto &member : members)
    {
        logData("Member: '" + member + "'");
    }

    // Cleanup
    conn->deleteKey(setKey);
}
#endif

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc Redis Batch Operations Example");
    log("========================================");
    log("");

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
        printHelp("24_081_example_redis_batch", "redis");
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

    logStep("Getting Redis database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "redis");

    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return EXIT_ERROR_;
    }

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

        auto driver = std::make_shared<cpp_dbc::Redis::RedisDriver>();
        std::string url = "cpp_dbc:redis://" + dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) + "/" + dbConfig.getDatabase();
        auto conn = driver->connectKV(url, dbConfig.getUsername(), dbConfig.getPassword());

        logOk("Connected to Redis");

        demonstrateBulkDelete(conn);
        demonstrateMsetMget(conn);
        demonstrateBatchHashOperations(conn);
        demonstrateBatchListOperations(conn);
        demonstrateBatchSetOperations(conn);
        demonstratePerformanceComparison(conn);

        log("");
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

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return EXIT_OK_;
#endif
}
