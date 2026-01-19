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
 * @file scylladb_connection_pool_example.cpp
 * @brief Example demonstrating ScyllaDB connection pool usage
 *
 * This example demonstrates how to use the connection pool for columnar databases,
 * specifically ScyllaDB. It shows basic connection pooling functionality and how to
 * perform columnar database operations with connections from the pool.
 *
 * To run this example, make sure ScyllaDB is installed and running, and that
 * the USE_SCYLLADB option is enabled in CMake.
 *
 * Build with: cmake -DUSE_SCYLLADB=ON -DCPP_DBC_BUILD_EXAMPLES=ON ..
 * Run with: ./scylladb_connection_pool_example
 */

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/columnar/columnar_db_connection_pool.hpp>
#if USE_SCYLLADB
#include <cpp_dbc/drivers/columnar/driver_scylladb.hpp>
#endif
#include <cpp_dbc/config/database_config.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <future>

using namespace std;
using namespace cpp_dbc;

#if USE_SCYLLADB

// Function to test a connection from the pool
void testConnection(shared_ptr<ColumnarDBConnectionPool> pool, int id)
{
    cout << "Thread " << id << " getting connection from pool..." << endl;

    try
    {
        // Get a connection from the pool
        auto conn = pool->getColumnarDBConnection();

        // Define keyspace and table
        string keyspace = "test_pool_keyspace";
        string table = keyspace + ".thread_table_" + to_string(id);

        // Create keyspace if not exists (only for the first thread)
        if (id == 0)
        {
            conn->executeUpdate(
                "CREATE KEYSPACE IF NOT EXISTS " + keyspace +
                " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
            cout << "Thread " << id << " created keyspace: " << keyspace << endl;
        }

        // Small delay to ensure keyspace is created
        this_thread::sleep_for(chrono::milliseconds(100));

        // Create a test table for this thread
        conn->executeUpdate("DROP TABLE IF EXISTS " + table);
        conn->executeUpdate(
            "CREATE TABLE " + table + " ("
            "id int PRIMARY KEY, "
            "thread_id int, "
            "name text, "
            "value double, "
            "timestamp bigint"
            ")");

        cout << "Thread " << id << " created table: " << table << endl;

        // Insert data using prepared statements
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + table + " (id, thread_id, name, value, timestamp) VALUES (?, ?, ?, ?, ?)");

        for (int i = 1; i <= 5; i++)
        {
            pstmt->setInt(1, i);
            pstmt->setInt(2, id);
            pstmt->setString(3, "Item " + to_string(i) + " from thread " + to_string(id));
            pstmt->setDouble(4, i * 1.5 + id);
            pstmt->setLong(5, chrono::system_clock::now().time_since_epoch().count());
            pstmt->executeUpdate();
        }

        cout << "Thread " << id << " inserted 5 rows" << endl;

        // Query the data
        auto rs = conn->executeQuery("SELECT * FROM " + table);
        int rowCount = 0;
        while (rs->next())
        {
            rowCount++;
            cout << "Thread " << id << " - Row " << rowCount << ": ID=" << rs->getInt("id")
                 << ", Name=" << rs->getString("name")
                 << ", Value=" << rs->getDouble("value") << endl;
        }

        // Update a row
        auto updateStmt = conn->prepareStatement(
            "UPDATE " + table + " SET name = ?, value = ? WHERE id = ?");
        updateStmt->setString(1, "Updated by thread " + to_string(id));
        updateStmt->setDouble(2, 999.99);
        updateStmt->setInt(3, 3);
        updateStmt->executeUpdate();

        cout << "Thread " << id << " updated row with id=3" << endl;

        // Verify update
        auto selectStmt = conn->prepareStatement("SELECT * FROM " + table + " WHERE id = ?");
        selectStmt->setInt(1, 3);
        rs = selectStmt->executeQuery();

        if (rs->next())
        {
            cout << "Thread " << id << " - Updated row: Name=" << rs->getString("name")
                 << ", Value=" << rs->getDouble("value") << endl;
        }

        // Clean up - drop the table
        conn->executeUpdate("DROP TABLE " + table);
        cout << "Thread " << id << " dropped table: " << table << endl;

        // Connection automatically returns to pool when it goes out of scope
        conn->close();
        cout << "Thread " << id << " finished and released connection back to pool" << endl;
    }
    catch (const DBException &e)
    {
        cerr << "Thread " << id << " error: " << e.what_s() << endl;
    }
}

// Function to demonstrate batch operations
void batchOperations(shared_ptr<ColumnarDBConnectionPool> pool)
{
    cout << "\n=== Demonstrating Batch Operations ===" << endl;

    try
    {
        auto conn = pool->getColumnarDBConnection();

        string keyspace = "test_pool_keyspace";
        string table = keyspace + ".batch_test_table";

        // Create table
        conn->executeUpdate("DROP TABLE IF EXISTS " + table);
        conn->executeUpdate(
            "CREATE TABLE " + table + " ("
            "id int PRIMARY KEY, "
            "category text, "
            "amount decimal"
            ")");

        cout << "Created batch test table" << endl;

        // Insert multiple rows using prepared statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + table + " (id, category, amount) VALUES (?, ?, ?)");

        for (int i = 1; i <= 10; i++)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Category_" + to_string(i % 3));
            pstmt->setDouble(3, i * 10.5);
            pstmt->executeUpdate();
        }

        cout << "Inserted 10 rows in batch" << endl;

        // Query with filtering
        auto rs = conn->executeQuery("SELECT * FROM " + table);
        int count = 0;
        while (rs->next())
        {
            count++;
        }

        cout << "Total rows: " << count << endl;

        // Clean up
        conn->executeUpdate("DROP TABLE " + table);
        conn->close();

        cout << "Batch operations completed successfully" << endl;
    }
    catch (const DBException &e)
    {
        cerr << "Batch operations error: " << e.what_s() << endl;
    }
}

#endif

int main()
{
    try
    {
#if USE_SCYLLADB
        // Register ScyllaDB driver
        DriverManager::registerDriver(make_shared<ScyllaDB::ScyllaDBDriver>());

        cout << "Creating ScyllaDB connection pool..." << endl;

        // Method 1: Create pool using config object (recommended for production)
        config::DBConnectionPoolConfig config;
        config.setUrl("cpp_dbc:scylladb://localhost:9042/test_pool_keyspace");
        config.setUsername("cassandra");
        config.setPassword("dsystems");

        // Configure pool parameters
        config.setInitialSize(5);            // Start with 5 connections
        config.setMaxSize(10);               // Allow up to 10 connections
        config.setMinIdle(3);                // Keep at least 3 idle connections
        config.setConnectionTimeout(5000);   // Wait up to 5 seconds for a connection
        config.setValidationInterval(30000); // Validate idle connections every 30 seconds
        config.setIdleTimeout(60000);        // Close idle connections after 60 seconds
        config.setMaxLifetimeMillis(300000); // Maximum connection lifetime of 5 minutes
        config.setTestOnBorrow(true);        // Test connections before giving them to clients
        config.setTestOnReturn(false);       // Don't test when returning to the pool
        config.setValidationQuery("SELECT now() FROM system.local"); // ScyllaDB/Cassandra validation query

        auto pool = ColumnarDBConnectionPool::create(config);

        // Alternatively, you can use the simpler factory method:
        /*
        auto pool = ColumnarDBConnectionPool::create(
            "cpp_dbc:scylladb://localhost:9042/test_pool_keyspace",
            "cassandra",
            "dsystems"
        );
        */

        // Or use the ScyllaDB-specific pool:
        /*
        auto pool = Scylla::ScyllaConnectionPool::create(
            "cpp_dbc:scylladb://localhost:9042/test_pool_keyspace",
            "cassandra",
            "dsystems"
        );
        */

        cout << "Pool created successfully" << endl;

        // Display initial pool statistics
        cout << "\nInitial pool statistics:" << endl;
        cout << "  Active connections: " << pool->getActiveDBConnectionCount() << endl;
        cout << "  Idle connections: " << pool->getIdleDBConnectionCount() << endl;
        cout << "  Total connections: " << pool->getTotalDBConnectionCount() << endl;

        // Test 1: Sequential operations
        cout << "\n=== Test 1: Sequential Operations ===" << endl;
        {
            auto conn = pool->getColumnarDBConnection();

            // Create keyspace
            conn->executeUpdate(
                "CREATE KEYSPACE IF NOT EXISTS test_pool_keyspace "
                "WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");

            cout << "Keyspace created/verified" << endl;
            conn->close();
        }

        // Small delay to ensure keyspace is ready
        this_thread::sleep_for(chrono::milliseconds(500));

        // Test 2: Concurrent operations with multiple threads
        cout << "\n=== Test 2: Concurrent Operations ===" << endl;
        const int numThreads = 6;
        vector<future<void>> futures;

        cout << "Starting " << numThreads << " threads to test connection pool..." << endl;

        for (int i = 0; i < numThreads; i++)
        {
            futures.push_back(async(launch::async, testConnection, pool, i));
        }

        // Wait for all threads to complete
        for (auto &future : futures)
        {
            future.wait();
        }

        cout << "\nAll threads completed" << endl;

        // Test 3: Batch operations
        batchOperations(pool);

        // Display pool statistics after operations
        cout << "\nPool statistics after concurrent operations:" << endl;
        cout << "  Active connections: " << pool->getActiveDBConnectionCount() << endl;
        cout << "  Idle connections: " << pool->getIdleDBConnectionCount() << endl;
        cout << "  Total connections: " << pool->getTotalDBConnectionCount() << endl;

        // Test 4: Pool stress test
        cout << "\n=== Test 4: Pool Stress Test ===" << endl;
        cout << "Rapidly acquiring and releasing connections..." << endl;

        for (int i = 0; i < 20; i++)
        {
            auto conn = pool->getColumnarDBConnection();
            // Simulate some work
            this_thread::sleep_for(chrono::milliseconds(10));
            conn->close();
        }

        cout << "Stress test completed" << endl;

        // Final pool statistics
        cout << "\nFinal pool statistics:" << endl;
        cout << "  Active connections: " << pool->getActiveDBConnectionCount() << endl;
        cout << "  Idle connections: " << pool->getIdleDBConnectionCount() << endl;
        cout << "  Total connections: " << pool->getTotalDBConnectionCount() << endl;

        // Clean up - drop keyspace
        {
            cout << "\nCleaning up..." << endl;
            auto conn = pool->getColumnarDBConnection();
            conn->executeUpdate("DROP KEYSPACE IF EXISTS test_pool_keyspace");
            cout << "Dropped keyspace: test_pool_keyspace" << endl;
            conn->close();
        }

        // Close the pool
        cout << "\nClosing connection pool..." << endl;
        pool->close();

        cout << "Pool closed successfully" << endl;
        cout << "\nExample completed successfully." << endl;

#else
        cout << "ScyllaDB support is not enabled." << endl;
        cout << "Build with: cmake -DUSE_SCYLLADB=ON -DCPP_DBC_BUILD_EXAMPLES=ON .." << endl;
#endif
    }
    catch (const DBException &e)
    {
        cerr << "Database error: " << e.what_s() << endl;
        return 1;
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
