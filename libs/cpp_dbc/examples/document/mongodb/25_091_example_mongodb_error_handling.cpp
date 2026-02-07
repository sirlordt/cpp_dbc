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
 * @file 25_091_example_mongodb_error_handling.cpp
 * @brief MongoDB-specific example demonstrating error handling
 *
 * This example demonstrates:
 * - Connection errors (wrong host, port, authentication)
 * - Document validation errors
 * - Duplicate key errors
 * - Query errors (invalid operators, syntax)
 * - Error recovery patterns
 *
 * Usage:
 *   ./25_091_example_mongodb_error_handling [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - MongoDB support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <cpp_dbc/core/document/document_db_connection.hpp>
#include <functional>
#include <stdexcept>

#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#endif

using namespace cpp_dbc::examples;
using cpp_dbc::examples::logMsg; // Disambiguate from std::logMsg

#if USE_MONGODB

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

void demonstrateConnectionErrors(cpp_dbc::MongoDB::MongoDBDriver &driver)
{
    logMsg("");
    logMsg("===Connection Errors ===");
    logInfo("Demonstrating various connection error scenarios");

    // Wrong host
    executeWithErrorHandling("Connect to non-existent host", [&driver]()
                             {
        logData("Attempting to connect to invalid_host:27017...");
        auto conn = driver.connectDocument("cpp_dbc:mongodb://invalid_host_that_does_not_exist:27017/test", "", "");
        // Try to use the connection to trigger the error
        conn->ping();
        conn->close(); });

    // Wrong port
    executeWithErrorHandling("Connect to wrong port", [&driver]()
                             {
        logData("Attempting to connect to localhost:12345...");
        auto conn = driver.connectDocument("cpp_dbc:mongodb://localhost:12345/test", "", "");
        conn->ping();
        conn->close(); });
}

void demonstrateDuplicateKeyError(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("===Duplicate Key Errors ===");
    logInfo("Demonstrating unique constraint violations");

    auto collection = conn->getCollection("error_test_users");

    // Clean up and create unique index
    executeWithErrorHandling("Setup unique constraint", [&collection]()
                             {
        collection->deleteMany("{}");
        collection->dropAllIndexes();
        collection->createIndex(R"({"email": 1})", R"({"unique": true})");
        logData("Created unique index on 'email' field"); });

    // Insert first document
    executeWithErrorHandling("Insert first document", [&collection]()
                             {
        collection->insertOne(R"({"email": "user@test.com", "name": "First User"})");
        logData("Inserted user with email: user@test.com"); });

    // Try to insert duplicate
    executeWithErrorHandling("Insert duplicate email (should fail)", [&collection]()
                             {
                                 logData("Attempting to insert another user with same email...");
                                 collection->insertOne(R"({"email": "user@test.com", "name": "Duplicate User"})");
                                 // This should throw a duplicate key error
                             });

    // Clean up
    collection->deleteMany("{}");
    collection->dropAllIndexes();
}

void demonstrateInvalidJsonError(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("===Invalid JSON Errors ===");
    logInfo("Demonstrating JSON parsing errors");

    auto collection = conn->getCollection("error_test_json");

    // Invalid JSON - missing closing brace
    executeWithErrorHandling("Insert invalid JSON (missing brace)", [&conn, &collection]()
                             {
                                 logData("Attempting to insert: {\"name\": \"test\"");
                                 collection->insertOne(R"({"name": "test")"); // Missing closing brace
                             });

    // Invalid JSON - trailing comma
    executeWithErrorHandling("Insert invalid JSON (trailing comma)", [&conn, &collection]()
                             {
                                 logData("Attempting to insert: {\"name\": \"test\",}");
                                 collection->insertOne(R"({"name": "test",})"); // Trailing comma
                             });

    // Invalid JSON in filter
    executeWithErrorHandling("Query with invalid filter", [&collection]()
                             {
                                 logData("Attempting to find with invalid filter...");
                                 collection->find(R"({"$invalid": })"); // Invalid JSON
                             });
}

void demonstrateInvalidOperatorError(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("===Invalid Query Operator Errors ===");
    logInfo("Demonstrating invalid MongoDB query operators");

    auto collection = conn->getCollection("error_test_operators");

    // Setup test data
    collection->deleteMany("{}");
    collection->insertOne(R"({"name": "test", "value": 42})");

    // Invalid operator
    executeWithErrorHandling("Query with invalid operator", [&collection]()
                             {
        logData("Attempting to query with $notAnOperator...");
        auto cursor = collection->find(R"({"value": {"$notAnOperator": 10}})");
        // Iterate to trigger the error
        while (cursor->hasNext()) {
            cursor->nextDocument();
        } });

    // Invalid $regex pattern
    executeWithErrorHandling("Query with invalid regex", [&collection]()
                             {
        logData("Attempting to query with invalid regex pattern...");
        auto cursor = collection->find(R"({"name": {"$regex": "[invalid("}})");
        while (cursor->hasNext()) {
            cursor->nextDocument();
        } });

    // Clean up
    collection->deleteMany("{}");
}

void demonstrateUpdateErrors(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("===Update Operation Errors ===");
    logInfo("Demonstrating update-related errors");

    auto collection = conn->getCollection("error_test_update");

    // Setup test data
    collection->deleteMany("{}");
    collection->insertOne(R"({"name": "test", "value": 42})");

    // Update without operator
    executeWithErrorHandling("Update without operator", [&collection]()
                             {
        logData("Attempting update without $ operator...");
        // This should fail - update document must contain update operators
        collection->updateOne(
            R"({"name": "test"})",
            R"({"value": 100})"  // Missing $set or other operator
        ); });

    // Invalid update operator
    executeWithErrorHandling("Update with invalid operator", [&collection]()
                             {
        logData("Attempting update with $notAnUpdateOp...");
        collection->updateOne(
            R"({"name": "test"})",
            R"({"$notAnUpdateOp": {"value": 100}})"
        ); });

    // $inc on non-numeric field
    executeWithErrorHandling("Increment on string field", [&collection]()
                             {
        logData("Attempting to $inc a string field...");
        collection->updateOne(
            R"({"name": "test"})",
            R"({"$inc": {"name": 1}})"  // Can't increment string
        ); });

    // Clean up
    collection->deleteMany("{}");
}

void demonstrateNothrowAPI(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("===Nothrow API Usage ===");
    logInfo("Using std::nothrow API for exception-free error handling");

    auto collection = conn->getCollection("error_test_nothrow");
    collection->deleteMany("{}");

    // Insert using nothrow API
    logMsg("");
    logStep("Using nothrow API for insertOne...");

    auto insertResult = collection->insertOne(std::nothrow, R"({"name": "test", "value": 42})");
    if (insertResult)
    {
        logOk("insertOne succeeded, ID: " + insertResult.value().insertedId);
    }
    else
    {
        logError("insertOne failed: " + insertResult.error().what_s());
    }

    // Find using nothrow API
    auto findResult = collection->findOne(std::nothrow, R"({"name": "test"})");
    if (findResult && findResult.value())
    {
        logOk("findOne succeeded: " + findResult.value()->toJson());
    }
    else if (!findResult)
    {
        logError("findOne failed: " + findResult.error().what_s());
    }
    else
    {
        logInfo("findOne returned no document");
    }

    // Invalid operation using nothrow API
    logMsg("");
    logStep("Testing invalid operation with nothrow API...");

    auto badResult = collection->insertOne(std::nothrow, R"(invalid json{)");
    if (badResult)
    {
        logData("Unexpected success");
    }
    else
    {
        logInfo("Operation failed safely (expected): " + badResult.error().what_s());
    }

    logOk("Nothrow API demonstration completed");

    // Clean up
    collection->deleteMany("{}");
}

void demonstrateErrorRecovery(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("===Error Recovery Patterns ===");
    logInfo("Demonstrating how to recover from errors");

    auto collection = conn->getCollection("error_test_recovery");
    collection->deleteMany("{}");

    // Pattern 1: Check and create
    logMsg("");
    logStep("Pattern 1: Check before inserting (upsert pattern)...");

    std::string email = "recovery@test.com";
    auto existing = collection->findOne(R"({"email": ")" + email + R"("})");
    if (existing)
    {
        logData("User already exists, updating...");
        collection->updateOne(
            R"({"email": ")" + email + R"("})",
            R"({"$set": {"lastSeen": "now"}})");
    }
    else
    {
        logData("User doesn't exist, creating...");
        collection->insertOne(R"({"email": ")" + email + R"(", "name": "Recovery User"})");
    }
    logOk("Check-and-create pattern completed");

    // Pattern 2: Use upsert option
    logMsg("");
    logStep("Pattern 2: Using upsert option...");

    cpp_dbc::DocumentUpdateOptions upsertOpts;
    upsertOpts.upsert = true;

    auto result = collection->updateOne(
        R"({"email": "upsert@test.com"})",
        R"({"$set": {"name": "Upserted User", "email": "upsert@test.com"}})",
        upsertOpts);

    if (!result.upsertedId.empty())
    {
        logData("Document was inserted (upserted): " + result.upsertedId);
    }
    else
    {
        logData("Existing document was updated");
    }
    logOk("Upsert pattern completed");

    // Pattern 3: Retry with backoff (simulated)
    logMsg("");
    logStep("Pattern 3: Retry pattern...");

    int maxRetries = 3;
    bool success = false;
    for (int attempt = 1; attempt <= maxRetries && !success; ++attempt)
    {
        try
        {
            logData("Attempt " + std::to_string(attempt) + "...");
            collection->insertOne(R"({"attempt": )" + std::to_string(attempt) + "}");
            success = true;
            logOk("Operation succeeded on attempt " + std::to_string(attempt));
        }
        catch (const cpp_dbc::DBException &e)
        {
            logError("Attempt " + std::to_string(attempt) + " failed: " + e.what_s());
            if (attempt < maxRetries)
            {
                logInfo("Retrying...");
            }
        }
    }

    // Clean up
    collection->deleteMany("{}");
}

void cleanup(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("---Cleanup ---");
    logStep("Dropping test collections...");

    std::vector<std::string> collections = {
        "error_test_users",
        "error_test_json",
        "error_test_operators",
        "error_test_update",
        "error_test_nothrow",
        "error_test_recovery"};

    for (const auto &coll : collections)
    {
        try
        {
            conn->dropCollection(coll);
        }
        catch (...)
        {
            // Ignore errors during cleanup
        }
    }
    logOk("Test collections dropped");
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc MongoDB Error Handling Example");
    logMsg("========================================");
    logMsg("");

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
        printHelp("25_091_example_mongodb_error_handling", "mongodb");
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

    logStep("Getting MongoDB database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "mongodb");

    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!dbResult.value())
    {
        logError("MongoDB configuration not found");
        return EXIT_ERROR_;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) +
          "/" + dbConfig.getDatabase() + ")");

    logStep("Registering MongoDB driver...");
    registerDriver("mongodb");
    logOk("Driver registered");

    auto driver = std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>();

    // Demonstrate connection errors (before main connection)
    demonstrateConnectionErrors(*driver);

    try
    {
        logStep("Connecting to MongoDB...");
        auto connBase = dbConfig.createDBConnection();
        auto conn = std::dynamic_pointer_cast<cpp_dbc::DocumentDBConnection>(connBase);

        if (!conn)
        {
            logError("Failed to cast connection to DocumentDBConnection");
            return EXIT_ERROR_;
        }
        logOk("Connected to MongoDB");

        demonstrateDuplicateKeyError(conn);
        demonstrateInvalidJsonError(conn);
        demonstrateInvalidOperatorError(conn);
        demonstrateUpdateErrors(conn);
        demonstrateNothrowAPI(conn);
        demonstrateErrorRecovery(conn);
        cleanup(conn);

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
