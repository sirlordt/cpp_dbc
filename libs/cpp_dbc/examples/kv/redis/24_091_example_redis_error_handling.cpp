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
 * @file 24_091_example_redis_error_handling.cpp
 * @brief Redis-specific example demonstrating error handling
 *
 * This example demonstrates:
 * - Connection errors (wrong host, port, authentication)
 * - Operation errors (wrong data type, missing keys)
 * - Transaction errors (WATCH conflicts, DISCARD)
 * - Command errors (invalid commands, wrong number of arguments)
 * - Error recovery patterns for Redis operations
 *
 * Usage:
 *   ./24_091_example_redis_error_handling [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - Redis support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <functional>
#include <stdexcept>

#if USE_REDIS
#include <cpp_dbc/drivers/kv/driver_redis.hpp>
#endif

using namespace cpp_dbc::examples;
using cpp_dbc::examples::logMsg; // Disambiguate from std::logMsg

#if USE_REDIS

// Custom exception class for application-specific errors
class AppException : public std::runtime_error
{
public:
    explicit AppException(const std::string &message) : std::runtime_error(message) {}
};

// Helper function to execute an operation and handle errors
void executeWithErrorHandling(const std::string &operationName, std::function<void()> operation)
{
    try
    {
        logMsg("");
        logStep("Executing: " + operationName);
        operation();
        logOk("Operation completed successfully");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error in " + operationName + ": " + e.what_s());
        e.printCallStack();
    }
    catch (const AppException &e)
    {
        logError("Application error in " + operationName + ": " + std::string(e.what()));
    }
    catch (const std::exception &e)
    {
        logError("Standard exception in " + operationName + ": " + std::string(e.what()));
    }
    catch (...)
    {
        logError("Unknown error in " + operationName);
    }
}

void demonstrateConnectionErrors(cpp_dbc::Redis::RedisDriver &driver)
{
    logMsg("");
    logMsg("=== Connection Errors ===");
    logInfo("Demonstrating various connection error scenarios");

    // Wrong host
    executeWithErrorHandling("Connect to non-existent host", [&driver]()
                             {
        logData("Attempting to connect to invalid_host:6379...");
        auto conn = driver.connectKV("redis://invalid_host_that_does_not_exist:6379", "", "");
        // If we get here, connection succeeded (unlikely)
        conn->close(); });

    // Wrong port
    executeWithErrorHandling("Connect to wrong port", [&driver]()
                             {
        logData("Attempting to connect to localhost:12345...");
        auto conn = driver.connectKV("redis://localhost:12345", "", "");
        conn->close(); });
}

void demonstrateWrongTypeErrors(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("=== Wrong Type Errors ===");
    logInfo("Redis returns WRONGTYPE when operating on wrong data structure");

    std::string key = "error_test_key";

    // Clean up first
    conn->deleteKey(key);

    // Set as string
    executeWithErrorHandling("Setup: Set key as string", [&conn, &key]()
                             {
        conn->setString(key, "I am a string value");
        logData("Set '" + key + "' as string"); });

    // Try to use as list (WRONGTYPE error)
    executeWithErrorHandling("Try list operation on string", [&conn, &key]()
                             {
                                 logData("Attempting listPushLeft on a string key...");
                                 conn->listPushLeft(key, "new_item");
                                 // This should fail with WRONGTYPE
                             });

    // Try to use as hash (WRONGTYPE error)
    executeWithErrorHandling("Try hash operation on string", [&conn, &key]()
                             {
                                 logData("Attempting hashSet on a string key...");
                                 conn->hashSet(key, "field", "value");
                                 // This should fail with WRONGTYPE
                             });

    // Try to use as set (WRONGTYPE error)
    executeWithErrorHandling("Try set operation on string", [&conn, &key]()
                             {
                                 logData("Attempting setAdd on a string key...");
                                 conn->setAdd(key, "member");
                                 // This should fail with WRONGTYPE
                             });

    // Cleanup
    conn->deleteKey(key);
}

void demonstrateInvalidCommandErrors(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("=== Invalid Command Errors ===");
    logInfo("Demonstrating errors from invalid Redis commands");

    // Non-existent command
    executeWithErrorHandling("Execute non-existent command", [&conn]()
                             {
        logData("Attempting to execute 'NOTACOMMAND'...");
        conn->executeCommand("NOTACOMMAND"); });

    // Command with wrong number of arguments
    executeWithErrorHandling("SET with no value", [&conn]()
                             {
        logData("Attempting SET with missing value...");
        conn->executeCommand("SET", {"only_key_no_value"}); });

    // Invalid argument type
    executeWithErrorHandling("INCR on non-integer value", [&conn]()
                             {
        std::string key = "error_incr_test";
        conn->setString(key, "not_a_number");
        logData("Set '" + key + "' to 'not_a_number'");
        logData("Attempting INCR on non-integer...");
        conn->increment(key);
        conn->deleteKey(key); });
}

void demonstrateKeyNotFoundBehavior(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("=== Key Not Found Behavior ===");
    logInfo("Redis returns special values for missing keys (not errors)");

    std::string missingKey = "definitely_not_exists_" + std::to_string(time(nullptr));

    executeWithErrorHandling("Get non-existent string key", [&conn, &missingKey]()
                             {
        logData("Getting value for non-existent key...");
        std::string value = conn->getString(missingKey);
        if (value.empty()) {
            logInfo("Returned empty string for missing key (expected behavior)");
        } else {
            logData("Value: '" + value + "'");
        } });

    executeWithErrorHandling("Check key existence", [&conn, &missingKey]()
                             {
        logData("Checking if key exists...");
        bool exists = conn->exists(missingKey);
        logData("exists() returned: " + std::string(exists ? "true" : "false"));
        if (!exists) {
            logInfo("Key does not exist (expected)");
        } });

    executeWithErrorHandling("Get TTL for non-existent key", [&conn, &missingKey]()
                             {
        logData("Getting TTL for non-existent key...");
        int64_t ttl = conn->getTTL(missingKey);
        logData("TTL: " + std::to_string(ttl));
        if (ttl == -2) {
            logInfo("TTL = -2 means key does not exist (expected)");
        } });

    executeWithErrorHandling("Delete non-existent key", [&conn, &missingKey]()
                             {
        logData("Deleting non-existent key...");
        bool deleted = conn->deleteKey(missingKey);
        logData("deleteKey() returned: " + std::string(deleted ? "true" : "false"));
        if (!deleted) {
            logInfo("Key was not deleted because it didn't exist");
        } });
}

void demonstrateNothrowAPI(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("=== Nothrow API Usage ===");
    logInfo("Using std::nothrow API for exception-free error handling");

    std::string key = "nothrow_test_key";

    // First set as string
    conn->setString(key, "string_value");

    // Use nothrow API to attempt list operation on string
    logMsg("");
    logStep("Using nothrow API for safe operations...");

    // Get string (should succeed)
    auto getResult = conn->getString(std::nothrow, key);
    if (getResult)
    {
        logOk("getString succeeded: '" + getResult.value() + "'");
    }
    else
    {
        logError("getString failed: " + getResult.error().what_s());
    }

    // Try list operation on string (should fail)
    auto listResult = conn->listPushLeft(std::nothrow, key, "item");
    if (listResult)
    {
        logData("listPushLeft succeeded with length: " + std::to_string(listResult.value()));
    }
    else
    {
        logInfo("listPushLeft failed (expected): " + listResult.error().what_s());
    }

    // Check if key exists using nothrow
    auto existsResult = conn->exists(std::nothrow, key);
    if (existsResult)
    {
        logData("exists check: " + std::string(existsResult.value() ? "true" : "false"));
    }
    else
    {
        logError("exists check failed: " + existsResult.error().what_s());
    }

    logOk("Nothrow API demonstration completed");

    // Cleanup
    conn->deleteKey(key);
}

void demonstrateTransactionErrors(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("=== Transaction Errors ===");
    logInfo("Demonstrating transaction-related errors");

    // EXEC without MULTI
    executeWithErrorHandling("EXEC without MULTI", [&conn]()
                             {
        logData("Attempting EXEC without starting MULTI...");
        conn->executeCommand("EXEC"); });

    // DISCARD without MULTI
    executeWithErrorHandling("DISCARD without MULTI", [&conn]()
                             {
        logData("Attempting DISCARD without starting MULTI...");
        conn->executeCommand("DISCARD"); });

    // Nested MULTI (not allowed)
    executeWithErrorHandling("Nested MULTI", [&conn]()
                             {
        logData("Starting first MULTI...");
        conn->executeCommand("MULTI");
        logData("Attempting nested MULTI...");
        try {
            conn->executeCommand("MULTI");
        } catch (...) {
            // Cleanup by discarding the transaction
            conn->executeCommand("DISCARD");
            throw;
        }
        conn->executeCommand("DISCARD"); });
}

void demonstrateErrorRecovery(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    logMsg("");
    logMsg("=== Error Recovery Patterns ===");
    logInfo("Demonstrating how to recover from errors");

    std::string key = "recovery_test";

    // Pattern 1: Check before operate
    logMsg("");
    logStep("Pattern 1: Check existence before operating...");
    if (!conn->exists(key))
    {
        logData("Key doesn't exist, initializing...");
        conn->setString(key, "0");
    }
    logData("Key value: " + conn->getString(key));

    // Pattern 2: Use default values
    logMsg("");
    logStep("Pattern 2: Use default values for missing keys...");
    std::string missingKey = "missing_key_test";
    std::string value = conn->getString(missingKey);
    if (value.empty())
    {
        value = "default_value";
        logData("Using default value: " + value);
    }
    else
    {
        logData("Found value: " + value);
    }

    // Pattern 3: Try-catch with specific handling
    logMsg("");
    logStep("Pattern 3: Specific error type handling...");
    try
    {
        // Try an operation that might fail
        conn->setString(key, "test_value");
        conn->increment(key); // Will fail - not a number
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Caught DBException, recovering...");
        // Recovery: reset the key to a valid number
        conn->setString(key, "0");
        int64_t newVal = conn->increment(key);
        logOk("Recovered and incremented to: " + std::to_string(newVal));
    }

    // Cleanup
    conn->deleteKey(key);
    conn->deleteKey(missingKey);
}

#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc Redis Error Handling Example");
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
        printHelp("24_091_example_redis_error_handling", "redis");
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

    auto driver = std::make_shared<cpp_dbc::Redis::RedisDriver>();

    // Demonstrate connection errors (before main connection)
    demonstrateConnectionErrors(*driver);

    try
    {
        logStep("Connecting to Redis...");
        std::string url = "cpp_dbc:redis://" + dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) + "/" + dbConfig.getDatabase();
        auto conn = driver->connectKV(url, dbConfig.getUsername(), dbConfig.getPassword());
        logOk("Connected to Redis");

        demonstrateWrongTypeErrors(conn);
        demonstrateInvalidCommandErrors(conn);
        demonstrateKeyNotFoundBehavior(conn);
        demonstrateNothrowAPI(conn);
        demonstrateTransactionErrors(conn);
        demonstrateErrorRecovery(conn);

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
