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

 @file 25_081_test_mongodb_real_left_join.cpp
 @brief Tests for MongoDB LEFT JOIN operations using aggregation pipelines

*/

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <set>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "25_001_test_mongodb_real_common.hpp"

#if USE_MONGODB
// Test case for MongoDB LEFT JOIN operations
TEST_CASE("MongoDB LEFT JOIN operations", "[25_081_01_mongodb_real_left_join]")
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

    // Generate unique collection names for this test
    std::string customersCollectionName = mongodb_test_helpers::generateRandomCollectionName() + "_customers";
    std::string ordersCollectionName = mongodb_test_helpers::generateRandomCollectionName() + "_orders";

    // Create collections
    conn->createCollection(customersCollectionName);
    auto customersCollection = conn->getCollection(customersCollectionName);
    REQUIRE(customersCollection != nullptr);

    conn->createCollection(ordersCollectionName);
    auto ordersCollection = conn->getCollection(ordersCollectionName);
    REQUIRE(ordersCollection != nullptr);

    // Insert test data for customers
    std::vector<std::string> customers = {
        R"({"customer_id": 1, "name": "John Doe", "email": "john@example.com"})",
        R"({"customer_id": 2, "name": "Jane Smith", "email": "jane@example.com"})",
        R"({"customer_id": 3, "name": "Bob Johnson", "email": "bob@example.com"})",
        R"({"customer_id": 4, "name": "Alice Williams", "email": "alice@example.com"})",
        R"({"customer_id": 5, "name": "Charlie Brown", "email": "charlie@example.com"})"};

    for (const auto &customer : customers)
    {
        auto result = customersCollection->insertOne(customer);
        REQUIRE(result.acknowledged);
    }

    // Insert test data for orders
    std::vector<std::string> orders = {
        R"({"order_id": 101, "customer_id": 1, "amount": 200, "product": "Laptop"})",
        R"({"order_id": 102, "customer_id": 1, "amount": 50, "product": "Mouse"})",
        R"({"order_id": 103, "customer_id": 2, "amount": 100, "product": "Monitor"})",
        R"({"order_id": 104, "customer_id": 3, "amount": 30, "product": "Keyboard"})",
        R"({"order_id": 105, "customer_id": 3, "amount": 150, "product": "Printer"})",
        R"({"order_id": 106, "customer_id": null, "amount": 75, "product": "External Drive"})",
        R"({"order_id": 107, "customer_id": 7, "amount": 60, "product": "Headphones"})"};

    for (const auto &order : orders)
    {
        auto result = ordersCollection->insertOne(order);
        REQUIRE(result.acknowledged);
    }

    SECTION("Left Join with $lookup")
    {
        // MongoDB $lookup (SQL equivalent: LEFT JOIN)
        // The $lookup itself is equivalent to a LEFT JOIN
        std::string pipeline = R"([
            {
                "$lookup": {
                    "from": ")" +
                               customersCollectionName + R"(",
                    "localField": "customer_id",
                    "foreignField": "customer_id",
                    "as": "customer_info"
                }
            },
            {
                "$project": {
                    "order_id": 1,
                    "product": 1,
                    "amount": 1,
                    "customer_id": 1,
                    "customer_name": { "$arrayElemAt": ["$customer_info.name", 0] },
                    "customer_email": { "$arrayElemAt": ["$customer_info.email", 0] }
                }
            }
        ])";

        auto cursor = ordersCollection->aggregate(pipeline);

        // Count results and verify data
        int count = 0;
        std::set<int> orderIds;
        int nullOrMissingCustomerCount = 0;

        while (cursor->next())
        {
            count++;
            auto doc = cursor->current();
            auto orderId = doc->getInt("order_id");
            orderIds.insert(static_cast<int>(orderId));

            // Check if customer info is null/empty
            if (doc->isNull("customer_name") || doc->getString("customer_name").empty())
            {
                nullOrMissingCustomerCount++;
            }
        }

        // Should find all 7 orders
        REQUIRE(count == 7);
        // Should include orders 106 (null customer_id) and 107 (non-existent customer_id)
        REQUIRE(orderIds.count(106) == 1);
        REQUIRE(orderIds.count(107) == 1);
        // 2 orders should have null/empty customer info
        REQUIRE(nullOrMissingCustomerCount == 2);
    }

    // Clean up
    conn->dropCollection(customersCollectionName);
    conn->dropCollection(ordersCollectionName);
    conn->close();
}
#else
// Skip tests if MongoDB support is not enabled
TEST_CASE("MongoDB LEFT JOIN operations (skipped)", "[25_081_02_mongodb_real_left_join]")
{
    SKIP("MongoDB support is not enabled");
}
#endif
