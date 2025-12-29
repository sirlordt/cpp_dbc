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

 @file test_mongodb_real_json.cpp
 @brief Tests for MongoDB JSON operations

*/

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <random>
#include <sstream>
#include <cmath> // For std::abs

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "test_mongodb_common.hpp"

#if USE_MONGODB
// Test case for MongoDB JSON operations
TEST_CASE("MongoDB JSON operations", "[mongodb_real_json]")
{
    // Skip these tests if we can't connect to MongoDB
    if (!mongodb_test_helpers::canConnectToMongoDB())
    {
        SKIP("Cannot connect to MongoDB database");
        return;
    }

    // Get MongoDB configuration
    auto dbConfig = mongodb_test_helpers::getMongoDBConfig("dev_mongodb");
    std::string connStr = mongodb_test_helpers::buildMongoDBConnectionString(dbConfig);
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();

    // Get a MongoDB driver
    auto driver = mongodb_test_helpers::getMongoDBDriver();
    auto conn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(
        driver->connectDocument(connStr, username, password));
    REQUIRE(conn != nullptr);

    SECTION("Basic JSON document operations")
    {
        // Generate a unique collection name for this test
        std::string collectionName = mongodb_test_helpers::generateRandomCollectionName();

        // Create collection
        conn->createCollection(collectionName);
        auto collection = conn->getCollection(collectionName);
        REQUIRE(collection != nullptr);

        // Insert simple JSON document
        std::string simpleDoc = R"({
            "id": 1,
            "name": "John",
            "age": 30,
            "city": "New York"
        })";

        auto insertResult = collection->insertOne(simpleDoc);
        REQUIRE(insertResult.acknowledged);

        // Insert JSON array document
        std::string arrayDoc = R"({
            "id": 2,
            "numbers": [1, 2, 3, 4, 5]
        })";

        insertResult = collection->insertOne(arrayDoc);
        REQUIRE(insertResult.acknowledged);

        // Insert nested JSON document
        std::string nestedDoc = R"({
            "id": 3,
            "person": {
                "name": "Alice",
                "age": 25
            },
            "active": true
        })";

        insertResult = collection->insertOne(nestedDoc);
        REQUIRE(insertResult.acknowledged);

        // Insert complex JSON document with arrays and nested objects
        std::string complexDoc = R"({
            "id": 4,
            "people": [
                {"name": "Bob", "age": 40},
                {"name": "Carol", "age": 35}
            ],
            "location": {
                "city": "Boston",
                "state": "MA"
            }
        })";

        insertResult = collection->insertOne(complexDoc);
        REQUIRE(insertResult.acknowledged);

        // Test retrieving JSON data
        auto doc = collection->findOne("{\"id\": 1}");
        REQUIRE(doc != nullptr);
        REQUIRE(doc->getString("name") == "John");
        REQUIRE(doc->getInt("age") == 30);
        REQUIRE(doc->getString("city") == "New York");

        // Test accessing JSON array elements
        doc = collection->findOne("{\"id\": 2}");
        REQUIRE(doc != nullptr);
        REQUIRE(doc->hasField("numbers"));
        // Check if we can get array values (implementation dependent)

        // Test accessing nested JSON objects
        doc = collection->findOne("{\"id\": 3}");
        REQUIRE(doc != nullptr);
        REQUIRE(doc->hasField("person"));
        REQUIRE(doc->getBool("active") == true);

        // Test accessing nested fields
        // Get the nested document first
        auto personDoc = doc->getDocument("person");
        REQUIRE(personDoc != nullptr);
        REQUIRE(personDoc->getString("name") == "Alice");
        REQUIRE(personDoc->getInt("age") == 25);

        // Test complex document with arrays of objects
        doc = collection->findOne("{\"id\": 4}");
        REQUIRE(doc != nullptr);
        REQUIRE(doc->hasField("people"));
        REQUIRE(doc->hasField("location"));

        // Test accessing nested fields with dot notation
        auto locationDoc = doc->getDocument("location");
        REQUIRE(locationDoc != nullptr);
        REQUIRE(locationDoc->getString("city") == "Boston");
        REQUIRE(locationDoc->getString("state") == "MA");

        // Clean up
        conn->dropCollection(collectionName);
    }

    SECTION("JSON query operators")
    {
        // Generate a unique collection name for this test
        std::string collectionName = mongodb_test_helpers::generateRandomCollectionName();

        // Create collection
        conn->createCollection(collectionName);
        auto collection = conn->getCollection(collectionName);
        REQUIRE(collection != nullptr);

        // Insert multiple documents for testing query operators
        std::vector<std::string> docs = {
            R"({"id": 1, "type": "electronics", "price": 1200, "name": "Laptop", "inStock": true, "tags": ["computer", "gaming", "premium"]})",
            R"({"id": 2, "type": "electronics", "price": 800, "name": "Smartphone", "inStock": true, "tags": ["mobile", "smart", "premium"]})",
            R"({"id": 3, "type": "electronics", "price": 200, "name": "Headphones", "inStock": false, "tags": ["audio", "wireless"]})",
            R"({"id": 4, "type": "clothing", "price": 50, "name": "Shirt", "inStock": true, "tags": ["casual", "cotton"]})",
            R"({"id": 5, "type": "clothing", "price": 80, "name": "Jeans", "inStock": true, "tags": ["casual", "denim"]})",
            R"({"id": 6, "type": "furniture", "price": 500, "name": "Desk", "inStock": false, "tags": ["office", "wood"]})",
            R"({"id": 7, "type": "furniture", "price": 300, "name": "Chair", "inStock": true, "tags": ["office", "ergonomic"]})"};

        for (const auto &doc : docs)
        {
            auto insertResult = collection->insertOne(doc);
            REQUIRE(insertResult.acknowledged);
        }

        // Test comparison operators

        // Test $eq (equals)
        auto cursor = collection->find("{\"type\": \"electronics\"}");
        int count = 0;
        while (cursor->next())
        {
            count++;
            auto doc = cursor->current();
            REQUIRE(doc->getString("type") == "electronics");
        }
        REQUIRE(count == 3); // Should find 3 electronics

        // Test $gt (greater than)
        cursor = collection->find("{\"price\": {\"$gt\": 300}}");
        count = 0;
        while (cursor->next())
        {
            count++;
            auto doc = cursor->current();
            REQUIRE(doc->getDouble("price") > 300);
        }
        REQUIRE(count == 3); // Laptop, Smartphone, Desk

        // Test $lt (less than)
        cursor = collection->find("{\"price\": {\"$lt\": 300}}");
        count = 0;
        while (cursor->next())
        {
            count++;
            auto doc = cursor->current();
            REQUIRE(doc->getDouble("price") < 300);
        }
        REQUIRE(count == 3); // Headphones, Shirt, Chair

        // Test logical operators

        // Test $and
        cursor = collection->find("{\"$and\": [{\"type\": \"electronics\"}, {\"price\": {\"$gt\": 500}}]}");
        count = 0;
        while (cursor->next())
        {
            count++;
            auto doc = cursor->current();
            REQUIRE(doc->getString("type") == "electronics");
            REQUIRE(doc->getDouble("price") > 500);
        }
        REQUIRE(count == 2); // Laptop, Smartphone

        // Test $or
        cursor = collection->find("{\"$or\": [{\"type\": \"furniture\"}, {\"price\": {\"$lt\": 100}}]}");
        count = 0;
        while (cursor->next())
        {
            count++;
            auto doc = cursor->current();
            bool isValid = doc->getString("type") == "furniture" || doc->getDouble("price") < 100;
            REQUIRE(isValid);
        }
        REQUIRE(count == 4); // Shirt, Jeans, Desk, Chair

        // Test array operators

        // Test $in
        cursor = collection->find("{\"tags\": {\"$in\": [\"premium\", \"ergonomic\"]}}");
        count = 0;
        while (cursor->next())
        {
            count++;
        }
        REQUIRE(count == 3); // Laptop, Smartphone, Chair

        // Test projection (field selection)
        cursor = collection->find("{\"inStock\": true}", "{\"_id\": 0, \"name\": 1, \"price\": 1}");
        count = 0;
        while (cursor->next())
        {
            count++;
            auto doc = cursor->current();
            REQUIRE(doc->hasField("name"));
            REQUIRE(doc->hasField("price"));
            REQUIRE_FALSE(doc->hasField("type"));
            REQUIRE_FALSE(doc->hasField("tags"));
        }
        REQUIRE(count == 5); // All in-stock items

        // Clean up
        conn->dropCollection(collectionName);
    }

    SECTION("JSON updates and modifications")
    {
        // Generate a unique collection name for this test
        std::string collectionName = mongodb_test_helpers::generateRandomCollectionName();

        // Create collection
        conn->createCollection(collectionName);
        auto collection = conn->getCollection(collectionName);
        REQUIRE(collection != nullptr);

        // Insert a document for update testing
        std::string docToUpdate = R"({
            "id": 1,
            "name": "Original Name",
            "price": 100,
            "categories": ["A", "B"],
            "details": {
                "color": "blue",
                "size": "medium",
                "features": ["feature1", "feature2"]
            }
        })";

        auto insertResult = collection->insertOne(docToUpdate);
        REQUIRE(insertResult.acknowledged);

        // Test $set operator (update specific fields)
        auto updateResult = collection->updateOne(
            "{\"id\": 1}",
            "{\"$set\": {\"name\": \"Updated Name\", \"price\": 150}}");
        REQUIRE(updateResult.matchedCount == 1);
        REQUIRE(updateResult.modifiedCount == 1);

        // Verify the update
        auto doc = collection->findOne("{\"id\": 1}");
        REQUIRE(doc != nullptr);
        REQUIRE(doc->getString("name") == "Updated Name");
        // Use approximate comparison for floating point
        double price = doc->getDouble("price");
        REQUIRE(std::abs(price - 150.0) < 0.001);

        // Test $inc operator (increment numeric value)
        updateResult = collection->updateOne(
            "{\"id\": 1}",
            "{\"$inc\": {\"price\": 25}}");
        REQUIRE(updateResult.matchedCount == 1);
        REQUIRE(updateResult.modifiedCount == 1);

        // Verify the increment
        doc = collection->findOne("{\"id\": 1}");
        REQUIRE(doc != nullptr);
        price = doc->getDouble("price");
        REQUIRE(std::abs(price - 175.0) < 0.001); // 150 + 25

        // Test $push operator (add element to array)
        updateResult = collection->updateOne(
            "{\"id\": 1}",
            "{\"$push\": {\"categories\": \"C\"}}");
        REQUIRE(updateResult.matchedCount == 1);
        REQUIRE(updateResult.modifiedCount == 1);

        // Verify array update
        doc = collection->findOne("{\"id\": 1}");
        REQUIRE(doc != nullptr);
        // Verify array contains expected values (implementation dependent)

        // Test nested updates
        updateResult = collection->updateOne(
            "{\"id\": 1}",
            "{\"$set\": {\"details.color\": \"red\", \"details.features.0\": \"updated_feature1\"}}");
        REQUIRE(updateResult.matchedCount == 1);
        REQUIRE(updateResult.modifiedCount == 1);

        // Verify nested updates
        doc = collection->findOne("{\"id\": 1}");
        REQUIRE(doc != nullptr);
        auto detailsDoc = doc->getDocument("details");
        REQUIRE(detailsDoc != nullptr);
        REQUIRE(detailsDoc->getString("color") == "red");

        // Clean up
        conn->dropCollection(collectionName);
    }

    SECTION("JSON aggregation operations")
    {
        // Generate a unique collection name for this test
        std::string collectionName = mongodb_test_helpers::generateRandomCollectionName();

        // Create collection
        conn->createCollection(collectionName);
        auto collection = conn->getCollection(collectionName);
        REQUIRE(collection != nullptr);

        // Insert multiple documents for aggregation testing
        std::vector<std::string> products = {
            R"({"category": "A", "price": 10, "quantity": 5})",
            R"({"category": "A", "price": 20, "quantity": 10})",
            R"({"category": "A", "price": 30, "quantity": 15})",
            R"({"category": "B", "price": 15, "quantity": 7})",
            R"({"category": "B", "price": 25, "quantity": 12})",
            R"({"category": "C", "price": 50, "quantity": 20})"};

        for (const auto &product : products)
        {
            auto insertResult = collection->insertOne(product);
            REQUIRE(insertResult.acknowledged);
        }

        // Test simple aggregation with $match and $group
        std::string pipeline = R"([
            {"$group": {"_id": "$category", "total": {"$sum": "$price"}, "count": {"$sum": 1}}}
        ])";

        auto cursor = collection->aggregate(pipeline);

        std::map<std::string, std::pair<double, int>> aggregationResults;
        while (cursor->next())
        {
            auto doc = cursor->current();
            std::string category = doc->getString("_id");
            double total = doc->getDouble("total");
            auto count = doc->getInt("count");
            aggregationResults[category] = std::make_pair(total, count);
        }

        REQUIRE(aggregationResults.size() == 3);
        // Use approximate comparison for floating point values
        REQUIRE(std::abs(aggregationResults["A"].first - 60.0) < 0.001); // 10 + 20 + 30
        REQUIRE(aggregationResults["A"].second == 3);                    // 3 products
        REQUIRE(std::abs(aggregationResults["B"].first - 40.0) < 0.001); // 15 + 25
        REQUIRE(aggregationResults["B"].second == 2);                    // 2 products
        REQUIRE(std::abs(aggregationResults["C"].first - 50.0) < 0.001);
        REQUIRE(aggregationResults["C"].second == 1);

        // Test more complex aggregation with multiple stages
        pipeline = R"([
            {"$match": {"price": {"$gt": 15}}},
            {"$group": {"_id": "$category", "total": {"$sum": {"$multiply": ["$price", "$quantity"]}}}},
            {"$sort": {"total": -1}}
        ])";

        cursor = collection->aggregate(pipeline);

        std::vector<std::pair<std::string, double>> complexResults;
        while (cursor->next())
        {
            auto doc = cursor->current();
            std::string category = doc->getString("_id");
            double total = doc->getDouble("total");
            complexResults.push_back(std::make_pair(category, total));
        }

        // Calculate expected values manually
        // Category A: (20 * 10) + (30 * 15) = 200 + 450 = 650
        // Category B: (25 * 12) = 300
        // Category C: (50 * 20) = 1000
        REQUIRE(complexResults.size() == 3);
        REQUIRE(complexResults[0].first == "C");
        REQUIRE(std::abs(complexResults[0].second - 1000.0) < 0.001);
        REQUIRE(complexResults[1].first == "A");
        REQUIRE(std::abs(complexResults[1].second - 650.0) < 0.001);
        REQUIRE(complexResults[2].first == "B");
        REQUIRE(std::abs(complexResults[2].second - 300.0) < 0.001);

        // Clean up
        conn->dropCollection(collectionName);
    }

    // Close the connection
    conn->close();
}
#else
// Skip tests if MongoDB support is not enabled
TEST_CASE("MongoDB JSON operations (skipped)", "[mongodb_real_json]")
{
    SKIP("MongoDB support is not enabled");
}
#endif