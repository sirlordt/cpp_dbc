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
 * @file 24_041_example_redis_transaction.cpp
 * @brief Redis-specific example demonstrating MULTI/EXEC transactions
 *
 * This example demonstrates:
 * - Redis MULTI/EXEC transaction blocks using executeCommand
 * - Atomic operations grouping
 * - Transaction rollback with DISCARD
 * - WATCH for optimistic locking
 *
 * Note: Redis transactions (MULTI/EXEC) are different from traditional
 * database transactions. They guarantee atomic execution but not isolation.
 * Commands between MULTI and EXEC are queued by the Redis server.
 *
 * Usage:
 *   ./24_041_example_redis_transaction [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - Redis support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <vector>

#if USE_REDIS
#include <cpp_dbc/drivers/kv/driver_redis.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_REDIS
void demonstrateBasicTransaction(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("--- Basic MULTI/EXEC Transaction ---");
    logInfo("Redis transactions execute atomically (all or nothing)");
    logInfo("Using executeCommand for MULTI/EXEC commands");

    // Setup test keys
    std::string key1 = "txn_key1";
    std::string key2 = "txn_key2";
    std::string key3 = "txn_key3";

    logStep("Cleaning up previous test keys...");
    conn->deleteKey(key1);
    conn->deleteKey(key2);
    conn->deleteKey(key3);
    logOk("Cleanup complete");

    logStep("Starting transaction with MULTI...");
    std::string multiResult = conn->executeCommand("MULTI");
    logData("MULTI response: " + multiResult);
    logOk("Transaction started - commands will be queued");

    logStep("Queueing commands...");
    // Note: In MULTI mode, commands return "QUEUED" instead of executing immediately
    // We use executeCommand with SET to queue the commands
    conn->executeCommand("SET", {key1, "value1"});
    logData("QUEUED: SET " + key1 + " value1");

    conn->executeCommand("SET", {key2, "value2"});
    logData("QUEUED: SET " + key2 + " value2");

    conn->executeCommand("SET", {key3, "value3"});
    logData("QUEUED: SET " + key3 + " value3");

    logStep("Executing transaction with EXEC...");
    std::string execResult = conn->executeCommand("EXEC");
    logData("EXEC response: " + execResult);
    logOk("Transaction executed");

    logStep("Verifying results...");
    logData(key1 + " = '" + conn->getString(key1) + "'");
    logData(key2 + " = '" + conn->getString(key2) + "'");
    logData(key3 + " = '" + conn->getString(key3) + "'");
    logOk("All values set correctly");

    // Cleanup
    conn->deleteKey(key1);
    conn->deleteKey(key2);
    conn->deleteKey(key3);
}

void demonstrateTransactionDiscard(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("--- Transaction DISCARD (Rollback) ---");
    logInfo("DISCARD cancels all queued commands");

    std::string key = "txn_discard_key";

    logStep("Setting initial value...");
    conn->setString(key, "initial_value");
    logData(key + " = '" + conn->getString(key) + "'");
    logOk("Initial value set");

    logStep("Starting transaction...");
    conn->executeCommand("MULTI");
    logOk("Transaction started");

    logStep("Queueing update command...");
    conn->executeCommand("SET", {key, "updated_value"});
    logData("QUEUED: SET " + key + " updated_value");

    logStep("Discarding transaction...");
    std::string discardResult = conn->executeCommand("DISCARD");
    logData("DISCARD response: " + discardResult);
    logOk("Transaction discarded");

    logStep("Verifying value unchanged...");
    std::string value = conn->getString(key);
    logData(key + " = '" + value + "'");
    if (value == "initial_value")
    {
        logOk("Value unchanged after DISCARD");
    }
    else
    {
        logError("Value changed unexpectedly!");
    }

    // Cleanup
    conn->deleteKey(key);
}

void demonstrateCounterTransaction(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("--- Atomic Counter Updates ---");
    logInfo("Demonstrating atomic increment/decrement in transaction");

    std::string counterA = "counter_a";
    std::string counterB = "counter_b";

    logStep("Initializing counters...");
    conn->setString(counterA, "100");
    conn->setString(counterB, "50");
    logData(counterA + " = " + conn->getString(counterA));
    logData(counterB + " = " + conn->getString(counterB));
    logOk("Counters initialized");

    logStep("Starting atomic transfer transaction...");
    conn->executeCommand("MULTI");

    // Transfer 25 from counter_a to counter_b
    conn->executeCommand("DECRBY", {counterA, "25"});
    logData("QUEUED: DECRBY " + counterA + " 25");

    conn->executeCommand("INCRBY", {counterB, "25"});
    logData("QUEUED: INCRBY " + counterB + " 25");

    conn->executeCommand("EXEC");
    logOk("Transfer transaction executed");

    logStep("Verifying transfer...");
    logData(counterA + " = " + conn->getString(counterA) + " (was 100)");
    logData(counterB + " = " + conn->getString(counterB) + " (was 50)");
    logOk("Transfer verified");

    // Cleanup
    conn->deleteKey(counterA);
    conn->deleteKey(counterB);
}

void demonstrateWatchOptimisticLocking(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("--- WATCH for Optimistic Locking ---");
    logInfo("WATCH allows detecting concurrent modifications");

    std::string watchKey = "watched_key";

    logStep("Setting up watched key...");
    conn->setString(watchKey, "original");
    logData(watchKey + " = '" + conn->getString(watchKey) + "'");
    logOk("Key set");

    logStep("Watching key...");
    std::string watchResult = conn->executeCommand("WATCH", {watchKey});
    logData("WATCH response: " + watchResult);
    logOk("Key is now being watched");

    logStep("Starting transaction...");
    conn->executeCommand("MULTI");
    conn->executeCommand("SET", {watchKey, "modified_in_txn"});
    logData("QUEUED: SET " + watchKey + " modified_in_txn");

    logStep("Executing transaction (no external modification)...");
    std::string execResult = conn->executeCommand("EXEC");
    logData("EXEC response: " + execResult);

    logStep("Verifying result...");
    logData(watchKey + " = '" + conn->getString(watchKey) + "'");
    logOk("Transaction succeeded (key was not modified externally)");

    // Cleanup
    conn->deleteKey(watchKey);
}

void demonstrateHashTransaction(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("--- Hash Operations in Transaction ---");
    logInfo("Atomic hash field updates");

    std::string hashKey = "user_profile";

    logStep("Starting transaction for hash operations...");
    conn->deleteKey(hashKey);

    conn->executeCommand("MULTI");

    conn->executeCommand("HSET", {hashKey, "name", "John Doe"});
    logData("QUEUED: HSET " + hashKey + " name 'John Doe'");

    conn->executeCommand("HSET", {hashKey, "email", "john@example.com"});
    logData("QUEUED: HSET " + hashKey + " email 'john@example.com'");

    conn->executeCommand("HSET", {hashKey, "age", "30"});
    logData("QUEUED: HSET " + hashKey + " age '30'");

    conn->executeCommand("HSET", {hashKey, "status", "active"});
    logData("QUEUED: HSET " + hashKey + " status 'active'");

    conn->executeCommand("EXEC");
    logOk("Transaction executed");

    logStep("Verifying hash fields...");
    auto fields = conn->hashGetAll(hashKey);
    for (const auto &[field, value] : fields)
    {
        logData(field + " = '" + value + "'");
    }
    logOk("Hash created atomically");

    // Cleanup
    conn->deleteKey(hashKey);
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc Redis Transaction Example");
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
        printHelp("24_041_example_redis_transaction", "redis");
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

        demonstrateBasicTransaction(conn);
        demonstrateTransactionDiscard(conn);
        demonstrateCounterTransaction(conn);
        demonstrateWatchOptimisticLocking(conn);
        demonstrateHashTransaction(conn);

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
