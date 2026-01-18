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

 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.

 @file connection_pool_example.cpp
 @brief Tests for database connections

*/
// CPPDBC_ConnectionPool_Example.cpp
// Example of using the CPPDBC connection pool

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif
#include "cpp_dbc/core/relational/relational_db_connection_pool.hpp"
#include "cpp_dbc/config/database_config.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>

// Mutex for thread-safe console output
std::mutex consoleMutex;

// Function to simulate a database operation
void performDatabaseOperation(cpp_dbc::RelationalDBConnectionPool &pool, int threadId)
{
    try
    {
        // Simulate random delay before requesting connection
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 500));

        // Get connection from pool
        auto conn = pool.getRelationalDBConnection();

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
    catch (const cpp_dbc::DBException &e)
    {
        std::lock_guard<std::mutex> lock(consoleMutex);
        std::cerr << "Thread " << threadId << " encountered error: " << e.what_s() << std::endl;
    }
}

/**
 * @brief Demonstrates creating and using relational connection pools with concurrent worker threads.
 *
 * Configures and creates MySQL and PostgreSQL connection pools when their support is enabled,
 * launches multiple threads that obtain connections from the pools to run example queries,
 * prints pool statistics, and closes the pools. Exceptions of type `cpp_dbc::DBException`
 * and `std::exception` are caught, reported to stderr, and cause the program to exit with a
 * non-zero status.
 *
 * @return int `0` on successful completion; `1` if a caught exception caused early termination.
 */
int main()
{
    // Seed the random number generator
    srand(static_cast<unsigned int>(time(nullptr)));

    try
    {
        // Configure MySQL connection pool
        cpp_dbc::config::DBConnectionPoolConfig mysqlConfig;
        mysqlConfig.setUrl("cpp_dbc:mysql://localhost:3306/testdb");
        mysqlConfig.setUsername("username");
        mysqlConfig.setPassword("password");
        mysqlConfig.setInitialSize(3);
        mysqlConfig.setMaxSize(10);
        mysqlConfig.setValidationQuery("SELECT 1");

        // Create MySQL connection pool
#if USE_MYSQL
        auto mysqlPool = cpp_dbc::MySQL::MySQLConnectionPool::create(mysqlConfig);

        std::cout << "MySQL connection pool created with "
                  << mysqlPool->getIdleDBConnectionCount() << " idle connections" << std::endl;

        // Demonstrate concurrent usage with multiple threads
        std::vector<std::thread> threads;
        const int numThreads = 15; // More threads than connections to demonstrate waiting

        std::cout << "Starting " << numThreads << " threads..." << std::endl;

        for (int i = 0; i < numThreads; ++i)
        {
            threads.push_back(std::thread(performDatabaseOperation, std::ref(*mysqlPool), i));
        }

        // Wait for all threads to complete
        for (auto &thread : threads)
        {
            thread.join();
        }

        std::cout << "All threads completed." << std::endl;
        std::cout << "Final pool statistics:" << std::endl;
        std::cout << "  Active connections: " << mysqlPool->getActiveDBConnectionCount() << std::endl;
        std::cout << "  Idle connections: " << mysqlPool->getIdleDBConnectionCount() << std::endl;
        std::cout << "  Total connections: " << mysqlPool->getTotalDBConnectionCount() << std::endl;

        // Close the pool
        mysqlPool->close();
        std::cout << "MySQL connection pool closed." << std::endl;
#else
        std::cout << "MySQL support is not enabled. Skipping MySQL example." << std::endl;
#endif

#if USE_POSTGRESQL
        // PostgreSQL example would be similar
        std::cout << "\nNow demonstrating PostgreSQL connection pool..." << std::endl;

        // Configure PostgreSQL connection pool
        cpp_dbc::config::DBConnectionPoolConfig pgConfig;
        pgConfig.setUrl("cpp_dbc:postgresql://localhost:5432/testdb");
        pgConfig.setUsername("username");
        pgConfig.setPassword("password");
        pgConfig.setInitialSize(3);
        pgConfig.setMaxSize(10);
        pgConfig.setValidationQuery("SELECT 1");

        // Create PostgreSQL connection pool
        auto pgPool = cpp_dbc::PostgreSQL::PostgreSQLConnectionPool::create(pgConfig);

        std::cout << "PostgreSQL connection pool created with "
                  << pgPool->getIdleDBConnectionCount() << " idle connections" << std::endl;

        // Run a few operations with the PostgreSQL pool
        // (Similar to MySQL example but with fewer threads for brevity)
        std::vector<std::thread> pgThreads;
        const int pgNumThreads = 5;

        for (int i = 0; i < pgNumThreads; ++i)
        {
            pgThreads.push_back(std::thread(performDatabaseOperation, std::ref(*pgPool), i));
        }

        for (auto &thread : pgThreads)
        {
            thread.join();
        }

        pgPool->close();
        std::cout << "PostgreSQL connection pool closed." << std::endl;
#else
        std::cout << "\nPostgreSQL support is not enabled. Skipping PostgreSQL example." << std::endl;
#endif
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << e.what_s() << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}