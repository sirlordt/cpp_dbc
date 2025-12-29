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

 @file test_mongodb_real_join.cpp
 @brief Tests for MongoDB join operations using aggregation pipelines

*/

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <set>
#include <map>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "test_mongodb_common.hpp"

#if USE_MONGODB
// Test case for MongoDB join operations
TEST_CASE("MongoDB join operations", "[mongodb_real_join]")
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
        R"({"order_id": 106, "customer_id": null, "amount": 75, "product": "External Drive"})", // Order with null customer_id
        R"({"order_id": 107, "customer_id": 7, "amount": 60, "product": "Headphones"})"         // Order with non-existent customer_id
    };

    for (const auto &order : orders)
    {
        auto result = ordersCollection->insertOne(order);
        REQUIRE(result.acknowledged);
    }

    SECTION("Inner Join (Equivalent) with $lookup")
    {
        // MongoDB $lookup with inner join-like match (SQL equivalent: INNER JOIN)
        // The $lookup performs the join, then we use $match to filter only documents where there was a match
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
                "$match": {
                    "customer_info": { "$ne": [] }
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
        std::set<int64_t> orderIds;

        while (cursor->next())
        {
            count++;
            auto doc = cursor->current();
            auto orderId = doc->getInt("order_id");
            orderIds.insert(orderId);

            // Verify join data is present
            REQUIRE(doc->hasField("customer_name"));
            REQUIRE(doc->hasField("customer_email"));
            REQUIRE(!doc->getString("customer_name").empty());
        }

        // Should find 5 orders with matching customers (101, 102, 103, 104, 105)
        REQUIRE(count == 5);
        REQUIRE(orderIds.count(101) == 1);
        REQUIRE(orderIds.count(102) == 1);
        REQUIRE(orderIds.count(103) == 1);
        REQUIRE(orderIds.count(104) == 1);
        REQUIRE(orderIds.count(105) == 1);
        // Should NOT include orders 106 (null customer_id) and 107 (non-existent customer_id)
        REQUIRE(orderIds.count(106) == 0);
        REQUIRE(orderIds.count(107) == 0);
    }

    SECTION("Left Join (Equivalent) with $lookup")
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

    SECTION("Right Join (Equivalent) with $lookup")
    {
        // MongoDB doesn't have a direct equivalent to RIGHT JOIN, but we can simulate it
        // by switching the collections and using $lookup (from customers to orders)
        std::string pipeline = R"([
            {
                "$lookup": {
                    "from": ")" +
                               ordersCollectionName + R"(",
                    "localField": "customer_id",
                    "foreignField": "customer_id",
                    "as": "orders"
                }
            },
            {
                "$project": {
                    "customer_id": 1,
                    "name": 1,
                    "email": 1,
                    "order_count": { "$size": "$orders" },
                    "orders": 1
                }
            }
        ])";

        auto cursor = customersCollection->aggregate(pipeline);

        // Count results and verify data
        int totalCustomers = 0;
        int customersWithOrders = 0;
        int customersWithoutOrders = 0;

        std::map<int, int> customerOrderCounts;

        while (cursor->next())
        {
            totalCustomers++;
            auto doc = cursor->current();
            auto customerId = doc->getInt("customer_id");
            auto orderCount = doc->getInt("order_count");

            customerOrderCounts[static_cast<int>(customerId)] = static_cast<int>(orderCount);

            if (orderCount > 0)
            {
                customersWithOrders++;
            }
            else
            {
                customersWithoutOrders++;
            }
        }

        // Should find all 5 customers
        REQUIRE(totalCustomers == 5);
        // 3 customers have orders, 2 don't
        REQUIRE(customersWithOrders == 3);
        REQUIRE(customersWithoutOrders == 2);

        // Verify specific order counts
        REQUIRE(customerOrderCounts[1] == 2); // 2 orders
        REQUIRE(customerOrderCounts[2] == 1); // 1 order
        REQUIRE(customerOrderCounts[3] == 2); // 2 orders
        REQUIRE(customerOrderCounts[4] == 0); // No orders
        REQUIRE(customerOrderCounts[5] == 0); // No orders
    }

    SECTION("Full Join (Equivalent) with $facet and $unionWith")
    {
        // MongoDB doesn't have a direct equivalent to FULL JOIN
        // We need to create two separate datasets and combine them

        // First part: Get all customers with a null row for each
        std::string allCustomersPipeline = R"([
            {
                "$project": {
                    "_id": 0,
                    "source": "customer_only",
                    "customer_id": 1,
                    "customer_name": "$name",
                    "customer_email": "$email",
                    "order_id": null,
                    "product": null,
                    "amount": null
                }
            }
        ])";

        // Second part: Get all customer-order pairs
        std::string customerOrderPairsPipeline = R"([
            {
                "$lookup": {
                    "from": ")" + ordersCollectionName +
                                                 R"(",
                    "localField": "customer_id",
                    "foreignField": "customer_id",
                    "as": "matched_orders"
                }
            },
            {
                "$unwind": {
                    "path": "$matched_orders",
                    "preserveNullAndEmptyArrays": false
                }
            },
            {
                "$project": {
                    "_id": 0,
                    "source": "customer_order_pair",
                    "customer_id": 1,
                    "customer_name": "$name",
                    "customer_email": "$email",
                    "order_id": "$matched_orders.order_id",
                    "product": "$matched_orders.product",
                    "amount": "$matched_orders.amount"
                }
            }
        ])";

        // Third part: Get all orders without matching customers
        std::string ordersWithoutCustomersPipeline = R"([
            {
                "$lookup": {
                    "from": ")" + customersCollectionName +
                                                     R"(",
                    "localField": "customer_id",
                    "foreignField": "customer_id",
                    "as": "customers"
                }
            },
            {
                "$match": {
                    "customers": { "$eq": [] }
                }
            },
            {
                "$project": {
                    "_id": 0,
                    "source": "order_only",
                    "customer_id": null,
                    "customer_name": null,
                    "customer_email": null,
                    "order_id": "$order_id",
                    "product": "$product",
                    "amount": "$amount"
                }
            }
        ])";

        // Combine everything into a single pipeline
        std::string fullJoinPipeline = R"([
            {
                "$facet": {
                    "all_customers": )" +
                                       allCustomersPipeline + R"(
                }
            },
            {
                "$lookup": {
                    "from": ")" + customersCollectionName +
                                       R"(",
                    "pipeline": )" + customerOrderPairsPipeline +
                                       R"(,
                    "as": "customer_order_pairs"
                }
            },
            {
                "$lookup": {
                    "from": ")" + ordersCollectionName +
                                       R"(",
                    "pipeline": )" + ordersWithoutCustomersPipeline +
                                       R"(,
                    "as": "orders_without_customers"
                }
            },
            {
                "$project": {
                    "all_rows": {
                        "$concatArrays": [
                            "$all_customers",
                            "$customer_order_pairs",
                            "$orders_without_customers"
                        ]
                    }
                }
            },
            {
                "$unwind": "$all_rows"
            },
            {
                "$replaceRoot": {
                    "newRoot": "$all_rows"
                }
            }
        ])";

        auto cursor = customersCollection->aggregate(fullJoinPipeline);

        // Count results and verify data
        int totalRows = 0;
        int rowsWithCustomerAndOrder = 0;
        int rowsWithCustomerOnly = 0;
        int rowsWithOrderOnly = 0;

        std::set<int> uniqueCustomers;
        std::set<int> uniqueOrders;

        while (cursor->next())
        {
            totalRows++;
            auto doc = cursor->current();

            bool hasCustomer = !doc->isNull("customer_id") &&
                               !doc->isNull("customer_name") &&
                               !doc->getString("customer_name").empty();

            bool hasOrder = !doc->isNull("order_id");

            if (hasCustomer && !doc->isNull("customer_id"))
            {
                uniqueCustomers.insert(static_cast<int>(doc->getInt("customer_id")));
            }

            if (hasOrder)
            {
                uniqueOrders.insert(static_cast<int>(doc->getInt("order_id")));
            }

            if (hasCustomer && hasOrder)
            {
                rowsWithCustomerAndOrder++;
            }
            else if (hasCustomer)
            {
                rowsWithCustomerOnly++;
            }
            else if (hasOrder)
            {
                rowsWithOrderOnly++;
            }
        }

        // Verify counts
        // 5 customers (2 with no orders) + 5 orders with customers + 2 orders with no customers = 12 total rows
        REQUIRE(totalRows == 12);
        REQUIRE(rowsWithCustomerAndOrder == 5); // 5 orders with matching customers
        REQUIRE(rowsWithCustomerOnly == 5);     // 5 customers (including those with no orders)
        REQUIRE(rowsWithOrderOnly == 2);        // 2 orders with no matching customer

        // Should include all 5 customers
        REQUIRE(uniqueCustomers.size() == 5);

        // Should include all 7 orders
        REQUIRE(uniqueOrders.size() == 7);
    }

    // Clean up
    conn->dropCollection(customersCollectionName);
    conn->dropCollection(ordersCollectionName);
    conn->close();
}
#else
// Skip tests if MongoDB support is not enabled
TEST_CASE("MongoDB join operations (skipped)", "[mongodb_real_join]")
{
    SKIP("MongoDB support is not enabled");
}
#endif