// CPPDBC_ConnectionPool_Example.cpp
// Example of using the CPPDBC connection pool

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#include "cpp_dbc/connection_pool.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>

// Mutex for thread-safe console output
std::mutex consoleMutex;

// Function to simulate a database operation
void performDatabaseOperation(cpp_dbc::ConnectionPool &pool, int threadId)
{
    try
    {
        // Simulate random delay before requesting connection
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 500));

        // Get connection from pool
        auto conn = pool.getConnection();

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "Thread " << threadId << ": Got connection from pool" << std::endl;
        }

        // Simulate database operation
        auto resultSet = conn->executeQuery("SELECT * FROM employees LIMIT 5");

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "Thread " << threadId << ": Query executed" << std::endl;

            if (resultSet->next())
            {
                std::cout << "Thread " << threadId << ": Found employee: "
                          << resultSet->getString("name") << std::endl;
            }
            else
            {
                std::cout << "Thread " << threadId << ": No employees found" << std::endl;
            }
        }

        // Simulate more work with the connection
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 1000));

        // Connection will be returned to pool when it goes out of scope
        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "Thread " << threadId << ": Returning connection to pool" << std::endl;
        }
    }
    catch (const cpp_dbc::SQLException &e)
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cerr << "Thread " << threadId << " encountered error: " << e.what() << std::endl;
    }
}

int main()
{
    // Seed the random number generator
    srand(static_cast<unsigned int>(time(nullptr)));

    try
    {
        // Configure MySQL connection pool
        cpp_dbc::ConnectionPoolConfig mysqlConfig;
        mysqlConfig.url = "cppdbc:mysql://localhost:3306/testdb";
        mysqlConfig.username = "username";
        mysqlConfig.password = "password";
        mysqlConfig.initialSize = 3;
        mysqlConfig.maxSize = 10;
        mysqlConfig.validationQuery = "SELECT 1";

        // Create MySQL connection pool
#if USE_MYSQL
        cpp_dbc::MySQL::MySQLConnectionPool mysqlPool(mysqlConfig);
#else
        std::cout << "MySQL support is not enabled. Skipping MySQL example." << std::endl;
        return 0;
#endif

        std::cout << "MySQL connection pool created with "
                  << mysqlPool.getIdleConnectionCount() << " idle connections" << std::endl;

        // Demonstrate concurrent usage with multiple threads
        std::vector<std::thread> threads;
        const int numThreads = 15; // More threads than connections to demonstrate waiting

        std::cout << "Starting " << numThreads << " threads..." << std::endl;

        for (int i = 0; i < numThreads; ++i)
        {
            threads.push_back(std::thread(performDatabaseOperation, std::ref(mysqlPool), i));
        }

        // Wait for all threads to complete
        for (auto &thread : threads)
        {
            thread.join();
        }

        std::cout << "All threads completed." << std::endl;
        std::cout << "Final pool statistics:" << std::endl;
        std::cout << "  Active connections: " << mysqlPool.getActiveConnectionCount() << std::endl;
        std::cout << "  Idle connections: " << mysqlPool.getIdleConnectionCount() << std::endl;
        std::cout << "  Total connections: " << mysqlPool.getTotalConnectionCount() << std::endl;

        // Close the pool
        mysqlPool.close();
        std::cout << "MySQL connection pool closed." << std::endl;

#if USE_POSTGRESQL
        // PostgreSQL example would be similar
        std::cout << "\nNow demonstrating PostgreSQL connection pool..." << std::endl;

        // Configure PostgreSQL connection pool
        cpp_dbc::ConnectionPoolConfig pgConfig;
        pgConfig.url = "cppdbc:postgresql://localhost:5432/testdb";
        pgConfig.username = "username";
        pgConfig.password = "password";
        pgConfig.initialSize = 3;
        pgConfig.maxSize = 10;
        pgConfig.validationQuery = "SELECT 1";

        // Create PostgreSQL connection pool
        cpp_dbc::PostgreSQL::PostgreSQLConnectionPool pgPool(pgConfig);

        std::cout << "PostgreSQL connection pool created with "
                  << pgPool.getIdleConnectionCount() << " idle connections" << std::endl;

        // Run a few operations with the PostgreSQL pool
        // (Similar to MySQL example but with fewer threads for brevity)
        std::vector<std::thread> pgThreads;
        const int pgNumThreads = 5;

        for (int i = 0; i < pgNumThreads; ++i)
        {
            pgThreads.push_back(std::thread(performDatabaseOperation, std::ref(pgPool), i));
        }

        for (auto &thread : pgThreads)
        {
            thread.join();
        }

        pgPool.close();
        std::cout << "PostgreSQL connection pool closed." << std::endl;
#else
        std::cout << "\nPostgreSQL support is not enabled. Skipping PostgreSQL example." << std::endl;
#endif
    }
    catch (const cpp_dbc::SQLException &e)
    {
        std::cerr << "Database Error: " << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
