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
 * @file 25_001_example_mongodb_basic.cpp
 * @brief Example demonstrating MongoDB database operations
 *
 * This example demonstrates:
 * - Loading configuration from YAML file
 * - MongoDB document database operations
 * - CRUD operations with collections
 * - Advanced queries and features
 *
 * Usage:
 *   ./mongodb_example [--config=<path>] [--db=<name>] [--help]
 *
 * Examples:
 *   ./mongodb_example                    # Uses dev_mongodb from default config
 *   ./mongodb_example --db=test_mongodb  # Uses test_mongodb configuration
 */

#include "../../common/example_common.hpp"
#include <cpp_dbc/core/document/document_db_connection.hpp>
#include <iomanip>

#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_MONGODB
void printDocument(const std::shared_ptr<cpp_dbc::DocumentDBData> &doc)
{
    if (!doc)
    {
        logMsg("null");
        return;
    }
    logMsg(doc->toJsonPretty());
}

void printCursor(std::shared_ptr<cpp_dbc::DocumentDBCursor> cursor)
{
    int count = 0;
    while (cursor->next())
    {
        auto doc = cursor->current();
        logData("Document " + std::to_string(++count) + ":");
        printDocument(doc);
    }

    if (count == 0)
    {
        logInfo("No documents found");
    }
    else
    {
        logOk("Total: " + std::to_string(count) + " document(s)");
    }
}

void demonstrateBasicOperations(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("--- Basic CRUD Operations ---");

    logStep("Getting products collection...");
    auto collection = conn->getCollection("products");
    logOk("Collection ready");

    logStep("Dropping existing collection...");
    collection->drop();
    logOk("Collection dropped");

    // ===== Insert Documents =====
    logMsg("");
    logMsg("--- Insert Documents ---");

    logStep("Creating and inserting documents...");

    // Product 1
    auto product1 = conn->createDocument();
    product1->setString("name", "MongoDB Database");
    product1->setDouble("price", 0.0);
    product1->setString("description", "NoSQL document database");
    product1->setBool("available", true);

    auto specs1 = conn->createDocument();
    specs1->setString("type", "Document Database");
    specs1->setString("license", "SSPL");
    product1->setDocument("specifications", specs1);

    auto insertResult1 = collection->insertOne(product1);
    logData("Product 1 ID: " + insertResult1.insertedId);

    // Product 2
    auto product2 = conn->createDocument();
    product2->setString("name", "cpp_dbc Library");
    product2->setDouble("price", 0.0);
    product2->setString("description", "C++ Database Connectivity Library");
    product2->setBool("available", true);

    auto specs2 = conn->createDocument();
    specs2->setString("type", "C++ Library");
    specs2->setString("license", "GPL v3");
    product2->setDocument("specifications", specs2);

    auto insertResult2 = collection->insertOne(product2);
    logData("Product 2 ID: " + insertResult2.insertedId);

    // Product 3
    auto product3 = conn->createDocument();
    product3->setString("name", "Enterprise Database Solution");
    product3->setDouble("price", 999.99);
    product3->setString("description", "Complete enterprise database solution");
    product3->setBool("available", true);

    auto specs3 = conn->createDocument();
    specs3->setString("type", "Enterprise Solution");
    specs3->setString("support", "24/7");
    product3->setDocument("specifications", specs3);

    auto insertResult3 = collection->insertOne(product3);
    logData("Product 3 ID: " + insertResult3.insertedId);
    logOk("3 products inserted");

    // ===== Query All =====
    logMsg("");
    logMsg("--- Query All Documents ---");

    logStep("Finding all products...");
    auto cursor = collection->find();
    printCursor(cursor);

    // ===== Query with Filter =====
    logMsg("");
    logMsg("--- Query with Filter ---");

    logStep("Finding free products (price = 0)...");
    cursor = collection->find(R"({"price": 0})");
    printCursor(cursor);

    // ===== Find One =====
    logMsg("");
    logMsg("--- Find One Document ---");

    logStep("Finding cpp_dbc Library...");
    auto result = collection->findOne(R"({"name": "cpp_dbc Library"})");
    if (result)
    {
        logData("Found:");
        printDocument(result);
        logOk("Document found");
    }
    else
    {
        logInfo("Document not found");
    }

    // ===== Update =====
    logMsg("");
    logMsg("--- Update Document ---");

    logStep("Updating Enterprise product...");
    auto updateResult = collection->updateOne(
        R"({"name": "Enterprise Database Solution"})",
        R"({"$set": {"price": 1299.99, "description": "Premium enterprise-grade solution"}})");
    logData("Matched: " + std::to_string(updateResult.matchedCount) +
            ", Modified: " + std::to_string(updateResult.modifiedCount));
    logOk("Document updated");

    logStep("Verifying update...");
    result = collection->findOne(R"({"name": "Enterprise Database Solution"})");
    if (result)
    {
        logData("Updated document:");
        printDocument(result);
        logOk("Update verified");
    }

    // ===== Delete =====
    logMsg("");
    logMsg("--- Delete Document ---");

    logStep("Deleting cpp_dbc Library...");
    auto deleteResult = collection->deleteOne(R"({"name": "cpp_dbc Library"})");
    logData("Deleted: " + std::to_string(deleteResult.deletedCount) + " document(s)");
    logOk("Document deleted");

    logStep("Verifying deletion...");
    cursor = collection->find();
    printCursor(cursor);

    // ===== Cleanup =====
    logMsg("");
    logMsg("--- Cleanup ---");

    logStep("Dropping collection...");
    collection->drop();
    logOk("Collection dropped");
}

void demonstrateMongoDBFeatures(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    logMsg("");
    logMsg("--- MongoDB Advanced Features ---");

    logStep("Getting users collection...");
    auto collection = conn->getCollection("users");
    collection->drop();
    logOk("Collection ready");

    // ===== Insert Many =====
    logMsg("");
    logMsg("--- Insert Many ---");

    logStep("Preparing user documents...");
    std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> users;

    auto user1 = conn->createDocument();
    user1->setString("username", "john_doe");
    user1->setString("email", "john@example.com");
    user1->setInt("age", 30);
    auto addr1 = conn->createDocument();
    addr1->setString("city", "New York");
    user1->setDocument("address", addr1);
    users.push_back(user1);

    auto user2 = conn->createDocument();
    user2->setString("username", "jane_doe");
    user2->setString("email", "jane@example.com");
    user2->setInt("age", 28);
    auto addr2 = conn->createDocument();
    addr2->setString("city", "San Francisco");
    user2->setDocument("address", addr2);
    users.push_back(user2);

    auto user3 = conn->createDocument();
    user3->setString("username", "alex_smith");
    user3->setString("email", "alex@example.com");
    user3->setInt("age", 35);
    auto addr3 = conn->createDocument();
    addr3->setString("city", "Chicago");
    user3->setDocument("address", addr3);
    users.push_back(user3);

    auto insertResult = collection->insertMany(users);
    logOk("Inserted " + std::to_string(insertResult.insertedCount) + " users");

    // ===== Complex Query =====
    logMsg("");
    logMsg("--- Complex Query ---");

    logStep("Finding users older than 30...");
    auto cursor = collection->find(R"({"age": {"$gt": 30}})");
    printCursor(cursor);

    // ===== Projection =====
    logMsg("");
    logMsg("--- Projection ---");

    logStep("Selecting only username and email fields...");
    cursor = collection->find("", R"({"username": 1, "email": 1, "_id": 0})");
    printCursor(cursor);

    // ===== Sort =====
    logMsg("");
    logMsg("--- Sorting ---");

    logStep("Sorting by age descending...");
    cursor = collection->find();
    cursor->sort(R"({"age": -1})");
    printCursor(cursor);

    // ===== Pagination =====
    logMsg("");
    logMsg("--- Pagination ---");

    logStep("First user (limit 1)...");
    cursor = collection->find();
    cursor->limit(1);
    printCursor(cursor);

    logStep("Second user (skip 1, limit 1)...");
    cursor = collection->find();
    cursor->skip(1).limit(1);
    printCursor(cursor);

    // ===== Update with Operators =====
    logMsg("");
    logMsg("--- Update with Operators ---");

    logStep("Incrementing age for john_doe...");
    auto updateResult = collection->updateOne(
        R"({"username": "john_doe"})",
        R"({"$inc": {"age": 1}})");
    logData("Modified: " + std::to_string(updateResult.modifiedCount) + " document(s)");
    logOk("Age incremented");

    logStep("Verifying update...");
    auto result = collection->findOne(R"({"username": "john_doe"})");
    if (result)
    {
        logData("Updated user:");
        printDocument(result);
    }

    // ===== Cleanup =====
    logMsg("");
    logMsg("--- Cleanup ---");

    logStep("Dropping collection...");
    collection->drop();
    logOk("Collection dropped");
}
#endif

int main(int argc, char *argv[])
{
    logMsg("========================================");
    logMsg("cpp_dbc MongoDB Document Example");
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
        printHelp("mongodb_example", "mongodb");
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
    auto dbResult = getDbConfig(configManager, args.dbName, "mongodb");

    // Check for real error
    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return EXIT_ERROR_;
    }

    // Check if config was found
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

        demonstrateBasicOperations(conn);
        demonstrateMongoDBFeatures(conn);

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
