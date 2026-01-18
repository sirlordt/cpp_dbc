/**
 * Redis Example
 *
 * This example demonstrates how to use the Redis key-value database driver
 * to perform common Redis operations.
 *
 * To run this example, make sure Redis is installed and running, and that
 * the USE_REDIS option is enabled in CMake.
 *
 * Build with: cmake -DUSE_REDIS=ON -DCPP_DBC_BUILD_EXAMPLES=ON ..
 * Run with: ./redis_example
 */

#include "cpp_dbc/cpp_dbc.hpp"
#include "cpp_dbc/drivers/kv/driver_redis.hpp"
#include <iostream>
#include <vector>

using namespace cpp_dbc;
using namespace cpp_dbc::Redis;

/**
 * @brief Demonstrates common Redis key-value operations using the cpp_dbc Redis driver.
 *
 * Performs a sequence of example operations against a Redis server:
 * connects to Redis, exercises string, counter, list, hash, set, and sorted-set commands,
 * scans keys, retrieves basic server info (including ping), cleans up example keys, and closes the connection.
 *
 * @return int 0 on successful completion, 1 if an exception (DBException or std::exception) is encountered.
 */
int main()
{
    try
    {
        std::cout << "Connecting to Redis...\n";

        // Create Redis driver
        auto driver = std::make_shared<RedisDriver>();

        // Connect to Redis server
        // Replace with your Redis server details if needed
        auto conn = driver->connectKV(
            "redis://localhost:6379", // URL
            "",                       // Username (may be empty)
            ""                        // Password (may be empty)
        );

        std::cout << "Connected to Redis successfully\n\n";

        // Basic String Operations
        std::cout << "===== String Operations =====\n";

        // Set a string value
        std::string key = "example_string";
        std::string value = "Hello, Redis!";

        if (conn->setString(key, value))
        {
            std::cout << "Set key: " << key << " = " << value << "\n";
        }

        // Get the string value
        std::string retrievedValue = conn->getString(key);
        std::cout << "Retrieved value: " << retrievedValue << "\n";

        // Set with expiration
        if (conn->setString(key + "_exp", value, 60))
        {
            std::cout << "Set key with 60-second expiration\n";
        }

        // Check TTL
        int64_t ttl = conn->getTTL(key + "_exp");
        std::cout << "TTL: " << ttl << " seconds\n\n";

        // Counter Operations
        std::cout << "===== Counter Operations =====\n";

        // Set counter
        std::string counterKey = "example_counter";
        conn->setString(counterKey, "10");

        // Increment
        int64_t newValue = conn->increment(counterKey);
        std::cout << "Counter incremented to: " << newValue << "\n";

        // Increment by specific amount
        newValue = conn->increment(counterKey, 5);
        std::cout << "Counter incremented by 5 to: " << newValue << "\n";

        // Decrement
        newValue = conn->decrement(counterKey);
        std::cout << "Counter decremented to: " << newValue << "\n\n";

        // List Operations
        std::cout << "===== List Operations =====\n";

        // Push elements to a list
        std::string listKey = "example_list";

        // Clear any existing list
        conn->deleteKey(listKey);

        // Push to right
        conn->listPushRight(listKey, "first");
        conn->listPushRight(listKey, "second");

        // Push to left
        conn->listPushLeft(listKey, "zero");

        // Get list length
        std::cout << "List length: " << conn->listLength(listKey) << "\n";

        // Get range
        auto listValues = conn->listRange(listKey, 0, -1);
        std::cout << "List contents: ";
        for (const auto &item : listValues)
        {
            std::cout << item << " ";
        }
        std::cout << "\n";

        // Pop from list
        std::string popValue = conn->listPopLeft(listKey);
        std::cout << "Popped from left: " << popValue << "\n";

        popValue = conn->listPopRight(listKey);
        std::cout << "Popped from right: " << popValue << "\n\n";

        // Hash Operations
        std::cout << "===== Hash Operations =====\n";

        // Set hash fields
        std::string hashKey = "example_hash";

        // Clear any existing hash
        conn->deleteKey(hashKey);

        conn->hashSet(hashKey, "field1", "value1");
        conn->hashSet(hashKey, "field2", "value2");
        conn->hashSet(hashKey, "field3", "value3");

        // Get hash field
        std::cout << "Hash field1: " << conn->hashGet(hashKey, "field1") << "\n";

        // Get all hash fields
        auto hashValues = conn->hashGetAll(hashKey);
        std::cout << "Hash contents: \n";
        for (const auto &[field, val] : hashValues)
        {
            std::cout << "  " << field << ": " << val << "\n";
        }

        // Delete hash field
        conn->hashDelete(hashKey, "field2");

        // Get hash length
        std::cout << "Hash length after delete: " << conn->hashLength(hashKey) << "\n\n";

        // Set Operations
        std::cout << "===== Set Operations =====\n";

        // Add to set
        std::string setKey = "example_set";

        // Clear any existing set
        conn->deleteKey(setKey);

        conn->setAdd(setKey, "member1");
        conn->setAdd(setKey, "member2");
        conn->setAdd(setKey, "member3");

        // Check set membership
        std::cout << "Is 'member2' in set? " << (conn->setIsMember(setKey, "member2") ? "yes" : "no") << "\n";

        // Get set members
        auto setMembers = conn->setMembers(setKey);
        std::cout << "Set members: ";
        for (const auto &member : setMembers)
        {
            std::cout << member << " ";
        }
        std::cout << "\n";

        // Get set size
        std::cout << "Set size: " << conn->setSize(setKey) << "\n";

        // Remove from set
        conn->setRemove(setKey, "member2");
        std::cout << "Set size after removal: " << conn->setSize(setKey) << "\n\n";

        // Sorted Set Operations
        std::cout << "===== Sorted Set Operations =====\n";

        // Add to sorted set
        std::string zsetKey = "example_zset";

        // Clear any existing sorted set
        conn->deleteKey(zsetKey);

        conn->sortedSetAdd(zsetKey, 1.0, "item1");
        conn->sortedSetAdd(zsetKey, 2.5, "item2");
        conn->sortedSetAdd(zsetKey, 3.7, "item3");

        // Get score
        auto score = conn->sortedSetScore(zsetKey, "item2");
        if (score)
        {
            std::cout << "Score of 'item2': " << *score << "\n";
        }

        // Get range by rank
        auto zsetMembers = conn->sortedSetRange(zsetKey, 0, -1);
        std::cout << "Sorted set members (by rank): ";
        for (const auto &member : zsetMembers)
        {
            std::cout << member << " ";
        }
        std::cout << "\n";

        // Get sorted set size
        std::cout << "Sorted set size: " << conn->sortedSetSize(zsetKey) << "\n\n";

        // Scan Keys
        std::cout << "===== Key Scan =====\n";

        // Scan for keys matching a pattern
        auto keys = conn->scanKeys("example_*");
        std::cout << "Keys matching 'example_*': ";
        for (const auto &k : keys)
        {
            std::cout << k << " ";
        }
        std::cout << "\n\n";

        // Server Info
        std::cout << "===== Server Info =====\n";

        // Ping server
        std::string pong = conn->ping();
        std::cout << "Ping response: " << pong << "\n";

        // Server info
        auto serverInfo = conn->getServerInfo();
        std::cout << "Redis server info (excerpt):\n";
        int count = 0;
        for (const auto &[k, info] : serverInfo)
        {
            std::cout << "  " << k << ": " << info << "\n";
            if (++count >= 5)
            {
                std::cout << "  ... (more info available)\n";
                break;
            }
        }

        // Clean up - delete all example keys
        std::cout << "\nCleaning up example keys...\n";
        std::vector<std::string> keysToDelete = {
            key, key + "_exp", counterKey, listKey, hashKey, setKey, zsetKey};
        int64_t deleted = conn->deleteKeys(keysToDelete);
        std::cout << "Deleted " << deleted << " keys\n";

        // Close connection
        conn->close();

        std::cout << "\nExample completed successfully!\n";
        return 0;
    }
    catch (const DBException &e)
    {
        std::cerr << "Error: " << e.what_s() << "\n";
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Standard exception: " << e.what() << "\n";
        return 1;
    }
}