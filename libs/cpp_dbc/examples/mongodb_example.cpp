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
 * @file mongodb_example.cpp
 * @brief Example demonstrating MongoDB database operations
 *
 * This example demonstrates how to connect to a MongoDB database and perform
 * various document database operations using the cpp_dbc library.
 *
 * Build and run:
 *   cd build
 *   cmake .. -DUSE_MONGODB=ON -DCPP_DBC_BUILD_EXAMPLES=ON
 *   make mongodb_example
 *   ./mongodb_example
 */

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MONGODB
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#endif
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <iomanip>

// Database configuration - update these values based on your setup
const std::string MONGODB_HOST = "localhost";
const int MONGODB_PORT = 27017;
const std::string MONGODB_DATABASE = "cpp_dbc_example";
const std::string MONGODB_CONNECTION_STRING = "mongodb://" + MONGODB_HOST + ":" + std::to_string(MONGODB_PORT);

#if USE_MONGODB

// Helper function to print a document nicely
void printDocument(const std::shared_ptr<cpp_dbc::DocumentDBData> &doc)
{
    if (!doc)
    {
        std::cout << "null" << std::endl;
        return;
    }

    // Print document as JSON
    std::cout << doc->toJsonPretty() << std::endl;
}

// Helper function to print all documents in a cursor
void printCursor(std::shared_ptr<cpp_dbc::DocumentDBCursor> cursor)
{
    int count = 0;
    while (cursor->hasNext())
    {
        std::cout << "Document " << ++count << ":" << std::endl;
        auto doc = cursor->nextDocument();
        printDocument(doc);
        std::cout << std::endl;
    }

    if (count == 0)
    {
        std::cout << "No documents found." << std::endl;
    }
    else
    {
        std::cout << "Total: " << count << " document(s)" << std::endl;
    }
}

// Demonstrate basic CRUD operations
void demonstrateBasicOperations(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    std::cout << "\n=== Basic CRUD Operations ===" << std::endl;

    try
    {
        // Get or create products collection
        auto collection = conn->getCollection("products");

        // Drop collection if it exists to start fresh
        std::cout << "Dropping collection if it exists..." << std::endl;
        collection->drop();

        // Create documents (products)
        std::cout << "Creating and inserting documents..." << std::endl;

        // Product 1
        auto product1 = conn->createDocument();
        product1->setString("name", "MongoDB Database");
        product1->setDouble("price", 0.0);
        product1->setString("description", "NoSQL document database");
        product1->setBool("available", true);

        // Add nested document for specifications
        auto specs1 = conn->createDocument();
        specs1->setString("type", "Document Database");
        specs1->setString("license", "SSPL");
        product1->setDocument("specifications", specs1);

        // Add array of tags using a JSON string for simplicity
        product1->fromJson(product1->toJson() + R"(, "tags": ["NoSQL", "Document", "JSON"]})");

        // Insert first product
        auto insertResult1 = collection->insertOne(product1);
        std::cout << "Inserted product with ID: " << insertResult1.insertedId << " (count: " << insertResult1.insertedCount << ")" << std::endl;

        // Product 2
        auto product2 = conn->createDocument();
        product2->setString("name", "cpp_dbc Library");
        product2->setDouble("price", 0.0);
        product2->setString("description", "C++ Database Connectivity Library");
        product2->setBool("available", true);

        // Add nested document for specifications
        auto specs2 = conn->createDocument();
        specs2->setString("type", "C++ Library");
        specs2->setString("license", "GPL v3");
        product2->setDocument("specifications", specs2);

        // Add array of tags using JSON
        product2->fromJson(product2->toJson() + R"(, "tags": ["C++", "Database", "Library"]})");

        // Insert second product
        auto insertResult2 = collection->insertOne(product2);
        std::cout << "Inserted product with ID: " << insertResult2.insertedId << " (count: " << insertResult2.insertedCount << ")" << std::endl;

        // Product 3
        auto product3 = conn->createDocument();
        product3->setString("name", "Enterprise Database Solution");
        product3->setDouble("price", 999.99);
        product3->setString("description", "Complete enterprise database solution with support");
        product3->setBool("available", true);

        // Add nested document for specifications
        auto specs3 = conn->createDocument();
        specs3->setString("type", "Enterprise Solution");
        specs3->setString("support", "24/7");
        specs3->setInt("warranty_days", 365);
        product3->setDocument("specifications", specs3);

        // Add array of tags using JSON
        product3->fromJson(product3->toJson() + R"(, "tags": ["Enterprise", "Support", "Premium"]})");

        // Insert third product
        auto insertResult3 = collection->insertOne(product3);
        std::cout << "Inserted product with ID: " << insertResult3.insertedId << " (count: " << insertResult3.insertedCount << ")" << std::endl;

        // Find all products
        std::cout << "\nQuery 1: Find all products" << std::endl;
        auto cursor = collection->find();
        printCursor(cursor);

        // Find products by criteria - using JSON filter
        std::cout << "\nQuery 2: Find free products (price = 0)" << std::endl;
        cursor = collection->find(R"({"price": 0})");
        printCursor(cursor);

        // Find one product by name
        std::cout << "\nQuery 3: Find one product by name" << std::endl;
        auto result = collection->findOne(R"({"name": "cpp_dbc Library"})");
        if (result)
        {
            std::cout << "Found document:" << std::endl;
            printDocument(result);
        }
        else
        {
            std::cout << "Document not found." << std::endl;
        }

        // Update a document
        std::cout << "\nUpdating 'Enterprise Database Solution' product..." << std::endl;
        auto updateResult = collection->updateOne(
            R"({"name": "Enterprise Database Solution"})",
            R"({
                "$set": {
                    "price": 1299.99,
                    "description": "Premium enterprise-grade database solution with 24/7 support"
                }
            })");
        std::cout << "Updated document(s) - matched: " << updateResult.matchedCount << ", modified: " << updateResult.modifiedCount << std::endl;

        // Verify the update
        std::cout << "\nQuery 4: Verify update" << std::endl;
        result = collection->findOne(R"({"name": "Enterprise Database Solution"})");
        if (result)
        {
            std::cout << "Updated document:" << std::endl;
            printDocument(result);
        }

        // Delete a document
        std::cout << "\nDeleting 'cpp_dbc Library' product..." << std::endl;
        auto deleteResult = collection->deleteOne(R"({"name": "cpp_dbc Library"})");
        std::cout << "Deleted " << deleteResult.deletedCount << " document(s)" << std::endl;

        // Verify the deletion and show remaining documents
        std::cout << "\nQuery 5: Verify deletion and show remaining products" << std::endl;
        cursor = collection->find();
        printCursor(cursor);

        // Drop the collection when done
        collection->drop();
        std::cout << "\nCollection dropped successfully." << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Error in basic operations: " << e.what_s() << std::endl;
    }
}

// Demonstrate MongoDB-specific features
void demonstrateMongoDBFeatures(std::shared_ptr<cpp_dbc::DocumentDBConnection> conn)
{
    std::cout << "\n=== MongoDB-Specific Features ===" << std::endl;

    try
    {
        // Get or create users collection
        auto collection = conn->getCollection("users");

        // Drop collection if it exists to start fresh
        collection->drop();

        // Insert multiple documents
        std::cout << "Inserting multiple users..." << std::endl;

        // Prepare documents to insert
        std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> users;

        // User 1
        auto user1 = conn->createDocument();
        user1->setString("username", "john_doe");
        user1->setString("email", "john@example.com");
        user1->setInt("age", 30);

        auto address1 = conn->createDocument();
        address1->setString("city", "New York");
        address1->setString("state", "NY");
        address1->setString("country", "USA");
        user1->setDocument("address", address1);

        // Add interests array using JSON
        user1->fromJson(user1->toJson() + R"(, "interests": ["programming", "hiking", "photography"]})");
        users.push_back(user1);

        // User 2
        auto user2 = conn->createDocument();
        user2->setString("username", "jane_doe");
        user2->setString("email", "jane@example.com");
        user2->setInt("age", 28);

        auto address2 = conn->createDocument();
        address2->setString("city", "San Francisco");
        address2->setString("state", "CA");
        address2->setString("country", "USA");
        user2->setDocument("address", address2);

        // Add interests array using JSON
        user2->fromJson(user2->toJson() + R"(, "interests": ["design", "travel", "cooking"]})");
        users.push_back(user2);

        // User 3
        auto user3 = conn->createDocument();
        user3->setString("username", "alex_smith");
        user3->setString("email", "alex@example.com");
        user3->setInt("age", 35);

        auto address3 = conn->createDocument();
        address3->setString("city", "Chicago");
        address3->setString("state", "IL");
        address3->setString("country", "USA");
        user3->setDocument("address", address3);

        // Add interests array using JSON
        user3->fromJson(user3->toJson() + R"(, "interests": ["music", "movies", "technology"]})");
        users.push_back(user3);

        // Insert many documents
        auto insertResult = collection->insertMany(users);
        std::cout << "Inserted " << insertResult.insertedCount << " users successfully" << std::endl;

        // Feature 1: Query using complex filters
        std::cout << "\nFeature 1: Complex query filters" << std::endl;

        // Find users over 30 using JSON query
        std::cout << "Users older than 30:" << std::endl;
        auto cursor = collection->find(R"({"age": {"$gt": 30}})");
        printCursor(cursor);

        // Feature 2: Projection (selecting only specific fields)
        std::cout << "\nFeature 2: Projection (selecting only specific fields)" << std::endl;

        // Use find with projection
        std::cout << "Users with only username and email fields:" << std::endl;
        cursor = collection->find("", R"({"username": 1, "email": 1, "_id": 0})");
        printCursor(cursor);

        // Feature 3: Sort
        std::cout << "\nFeature 3: Sorting results" << std::endl;

        // Use a cursor with sort
        cursor = collection->find();
        cursor->sort(R"({"age": -1})"); // -1 for descending order
        std::cout << "Users sorted by age (descending):" << std::endl;
        printCursor(cursor);

        // Feature 4: Limit and skip
        std::cout << "\nFeature 4: Limit and skip (pagination)" << std::endl;

        std::cout << "First user (limit 1):" << std::endl;
        cursor = collection->find();
        cursor->limit(1);
        printCursor(cursor);

        std::cout << "Skip first user, show second (skip 1, limit 1):" << std::endl;
        cursor = collection->find();
        cursor->skip(1).limit(1);
        printCursor(cursor);

        // Feature 5: Update with operators
        std::cout << "\nFeature 5: Update with operators" << std::endl;

        // Use $inc to increment age
        auto updateResult = collection->updateOne(
            R"({"username": "john_doe"})",
            R"({"$inc": {"age": 1}})");
        std::cout << "Incremented age for " << updateResult.modifiedCount << " user(s)" << std::endl;

        // Use $push to add to an array
        updateResult = collection->updateOne(
            R"({"username": "john_doe"})",
            R"({"$push": {"interests": "reading"}})");
        std::cout << "Added new interest for " << updateResult.modifiedCount << " user(s)" << std::endl;

        // Check the updated document
        std::cout << "Updated user:" << std::endl;
        auto result = collection->findOne(R"({"username": "john_doe"})");
        printDocument(result);

        // Feature 6: Text search
        std::cout << "\nFeature 6: Text search" << std::endl;

        // This might fail depending on MongoDB version and cpp_dbc implementation
        try
        {
            // Note: In a full implementation, you'd first create a text index with:
            // db.users.createIndex({ username: "text", email: "text" })
            std::cout << "Note: Text search requires a text index to be created first" << std::endl;

            // Simple search by username - not using $text operator since we might not have an index
            cursor = collection->find(R"({"username": {"$regex": "john", "$options": "i"}})");
            std::cout << "Search results for 'john':" << std::endl;
            printCursor(cursor);
        }
        catch (const std::exception &e)
        {
            std::cout << "Text search feature not fully implemented: " << e.what() << std::endl;
        }

        // Clean up
        collection->drop();
        std::cout << "\nCollection dropped successfully." << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Error in MongoDB features: " << e.what_s() << std::endl;
    }
}

#endif // USE_MONGODB

int main()
{
#if USE_MONGODB
    try
    {
        std::cout << "=== MongoDB Database Example ===" << std::endl;
        std::cout << "This example demonstrates operations with MongoDB." << std::endl;

        // Create and register MongoDB driver
        auto mongodbDriver = std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>();
        cpp_dbc::DriverManager::registerDriver(mongodbDriver);

        // Build connection URL
        std::string url = "cpp_dbc:mongodb://" + MONGODB_HOST + ":" +
                          std::to_string(MONGODB_PORT) + "/" + MONGODB_DATABASE;

        std::cout << "\nConnecting to MongoDB..." << std::endl;
        std::cout << "URL: " << url << std::endl;

        // Connect to MongoDB
        auto connBase = cpp_dbc::DriverManager::getDBConnection(url, "", "");
        auto conn = std::dynamic_pointer_cast<cpp_dbc::DocumentDBConnection>(connBase);

        if (!conn)
        {
            throw cpp_dbc::DBException("7D36A5F12E09",
                                       "Failed to cast MongoDB connection to DocumentDBConnection");
        }

        std::cout << "Connected successfully!" << std::endl;

        // Run the demonstrations
        demonstrateBasicOperations(conn);
        demonstrateMongoDBFeatures(conn);

        // Close connection
        conn->close();
        std::cout << "\n=== Example completed successfully ===" << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what_s() << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
#else
    std::cerr << "MongoDB support is not enabled. Build with -DUSE_MONGODB=ON" << std::endl;
    return 1;
#endif

    return 0;
}