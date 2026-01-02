/**
 * Key-Value Database Connection Pool Example
 *
 * This example demonstrates how to use the connection pool for key-value databases
 * such as Redis. It shows basic connection pooling functionality and how to
 * perform key-value operations with connections from the pool.
 *
 * To run this example, make sure Redis is installed and running, and that
 * the USE_REDIS option is enabled in CMake.
 *
 * Build with: cmake -DUSE_REDIS=ON -DCPP_DBC_BUILD_EXAMPLES=ON ..
 * Run with: ./kv_connection_pool_example
 */

#include "cpp_dbc/cpp_dbc.hpp"
#include "cpp_dbc/core/kv/kv_db_connection_pool.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <future>

using namespace cpp_dbc;
using namespace cpp_dbc::Redis;

// Function to test a connection from the pool
void testConnection(std::shared_ptr<KVDBConnectionPool> pool, int id)
{
    std::cout << "Thread " << id << " getting connection from pool...\n";

    try
    {
        // Get a connection from the pool
        auto conn = pool->getKVDBConnection();

        // Create a test key unique to this thread
        std::string key = "test_key_" + std::to_string(id);
        std::string value = "Hello from thread " + std::to_string(id);

        // Set a string value
        if (conn->setString(key, value))
        {
            std::cout << "Thread " << id << " set key: " << key << " = " << value << "\n";
        }

        // Read it back
        std::string retrieved = conn->getString(key);
        std::cout << "Thread " << id << " got value: " << retrieved << "\n";

        // Increment a counter
        std::string counterKey = "counter_" + std::to_string(id);
        conn->setString(counterKey, "0");

        for (int i = 0; i < 5; i++)
        {
            int64_t newValue = conn->increment(counterKey);
            std::cout << "Thread " << id << " incremented counter to: " << newValue << "\n";
        }

        // Clean up - delete the test keys
        conn->deleteKey(key);
        conn->deleteKey(counterKey);

        // Connection automatically returns to pool when it goes out of scope
        std::cout << "Thread " << id << " finished and released connection back to pool\n";
    }
    catch (const DBException &e)
    {
        std::cerr << "Thread " << id << " error: " << e.what() << "\n";
    }
}

int main()
{
    try
    {
        std::cout << "Creating Redis connection pool...\n";

        // Create a Redis connection pool
        // Replace with your Redis server details if needed
        auto pool = RedisConnectionPool::create(
            "redis://localhost:6379", // URL
            "",                       // Username (may be empty)
            ""                        // Password (may be empty)
        );

        // Alternatively, use a configuration object:
        /*
        config::DBConnectionPoolConfig config;
        config.setUrl("redis://localhost:6379");
        config.setUsername("");
        config.setPassword("");
        config.setInitialSize(5);
        config.setMaxSize(10);
        config.setMinIdle(3);
        config.setConnectionTimeout(5000);
        config.setValidationInterval(5000);
        config.setIdleTimeout(300000);
        config.setMaxLifetimeMillis(1800000);
        config.setTestOnBorrow(true);
        config.setTestOnReturn(false);
        config.setValidationQuery("PING");
        auto pool = RedisConnectionPool::create(config);
        */

        std::cout << "Pool created successfully\n";

        // Display initial pool statistics
        std::cout << "Initial pool statistics:\n";
        std::cout << "  Active connections: " << pool->getActiveDBConnectionCount() << "\n";
        std::cout << "  Idle connections: " << pool->getIdleDBConnectionCount() << "\n";
        std::cout << "  Total connections: " << pool->getTotalDBConnectionCount() << "\n\n";

        // Test the pool with multiple threads
        const int numThreads = 8;
        std::vector<std::future<void>> futures;

        std::cout << "Starting " << numThreads << " threads to test connection pool...\n\n";

        for (int i = 0; i < numThreads; i++)
        {
            futures.push_back(std::async(std::launch::async, testConnection, pool, i));
        }

        // Wait for all threads to complete
        for (auto &future : futures)
        {
            future.wait();
        }

        std::cout << "\nAll threads completed\n";

        // Display final pool statistics
        std::cout << "Final pool statistics:\n";
        std::cout << "  Active connections: " << pool->getActiveDBConnectionCount() << "\n";
        std::cout << "  Idle connections: " << pool->getIdleDBConnectionCount() << "\n";
        std::cout << "  Total connections: " << pool->getTotalDBConnectionCount() << "\n";

        // Close the pool
        std::cout << "Closing connection pool...\n";
        pool->close();

        std::cout << "Pool closed successfully\n";
        return 0;
    }
    catch (const DBException &e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Standard exception: " << e.what() << "\n";
        return 1;
    }
}