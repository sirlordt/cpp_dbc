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
 * @file document_connection_pool_example.cpp
 * @brief Example of using the document database connection pool with MongoDB
 */

#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <chrono>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/document/document_db_connection_pool.hpp>
#include <cpp_dbc/drivers/document/driver_mongodb.hpp>
#include <cpp_dbc/config/database_config.hpp>

using namespace std;
using namespace cpp_dbc;

// Helper function to create a simple test document
string createTestDocument(int id, const string &name, double value)
{
    return "{\"id\": " + to_string(id) +
           ", \"name\": \"" + name + "\"" +
           ", \"value\": " + to_string(value) + "}";
}

int main(int argc, char *argv[])
{
    try
    {
        cout << "Document Database Connection Pool Example" << endl;
        cout << "----------------------------------------" << endl;

        // Step 1: Register the MongoDB driver
        cout << "Registering MongoDB driver..." << endl;
        auto driver = make_shared<MongoDB::MongoDBDriver>();
        DriverManager::registerDriver(driver);

        // Step 2: Create MongoDB connection configuration
        cout << "Creating connection configuration..." << endl;
        config::DBConnectionPoolConfig poolConfig;

        // Set connection parameters - adjust these for your MongoDB server
        poolConfig.setUrl("cpp_dbc:mongodb://localhost:27017/test_db");
        poolConfig.setUsername("root");     // Set to empty string if no authentication is required
        poolConfig.setPassword("dsystems"); // Set to empty string if no authentication is required

        // Set connection pool parameters
        poolConfig.setInitialSize(5);                   // Start with 5 connections
        poolConfig.setMaxSize(10);                      // Allow up to 10 connections
        poolConfig.setMinIdle(3);                       // Keep at least 3 idle connections
        poolConfig.setConnectionTimeout(5000);          // Wait up to 5 seconds for a connection
        poolConfig.setValidationInterval(30000);        // Validate idle connections every 30 seconds
        poolConfig.setIdleTimeout(60000);               // Close idle connections after 60 seconds
        poolConfig.setMaxLifetimeMillis(300000);        // Maximum connection lifetime of 5 minutes
        poolConfig.setTestOnBorrow(true);               // Test connections before giving them to clients
        poolConfig.setTestOnReturn(false);              // Don't test when returning to the pool
        poolConfig.setValidationQuery("{\"ping\": 1}"); // Use MongoDB ping command for validation

        // Step 3: Create the connection pool
        cout << "Creating MongoDB connection pool..." << endl;
        MongoDB::MongoDBConnectionPool pool(poolConfig);

        // Step 4: Create a test collection
        const string testCollectionName = "connection_pool_example";

        {
            cout << "Setting up test collection..." << endl;
            auto conn = pool.getDocumentDBConnection();

            // Drop the collection if it exists (cleanup from previous runs)
            if (conn->collectionExists(testCollectionName))
            {
                conn->dropCollection(testCollectionName);
            }

            // Create a new collection
            auto collection = conn->createCollection(testCollectionName);
            cout << "Created collection: " << testCollectionName << endl;

            // Return connection to the pool
            conn->close();
        }

        // Step 5: Use connections from the pool for basic operations
        {
            cout << "\nPerforming basic document operations..." << endl;

            // Get a connection from the pool
            auto conn = pool.getDocumentDBConnection();
            auto collection = conn->getCollection(testCollectionName);

            // Insert a document
            string docJson = createTestDocument(1, "Test Document", 42.5);
            auto insertResult = collection->insertOne(docJson);

            cout << "Inserted document with ID: " << insertResult.insertedId << endl;

            // Find the document
            auto doc = collection->findById(insertResult.insertedId);

            if (doc)
            {
                cout << "Found document: " << doc->toJson() << endl;
            }
            else
            {
                cout << "Document not found!" << endl;
            }

            // Update the document
            string updateJson = "{\"$set\": {\"value\": 99.9}}";
            auto updateResult = collection->updateOne(
                "{\"id\": 1}", // filter
                updateJson     // update
            );

            cout << "Updated " << updateResult.modifiedCount << " document(s)" << endl;

            // Find the document again to verify update
            doc = collection->findOne("{\"id\": 1}");

            if (doc)
            {
                cout << "Updated document: " << doc->toJson() << endl;
            }

            // Return connection to the pool
            conn->close();
        }

        // Step 6: Demonstrate concurrent use with multiple threads
        {
            cout << "\nDemonstrating concurrent connections..." << endl;

            const int numThreads = 5;
            vector<thread> threads;

            for (int i = 0; i < numThreads; i++)
            {
                threads.push_back(thread([&pool, i, testCollectionName]()
                                         {
                    try {
                        // Get connection from pool
                        auto conn = pool.getDocumentDBConnection();
                        
                        // Get collection
                        auto collection = conn->getCollection(testCollectionName);
                        
                        // Insert a document
                        string docJson = createTestDocument(
                            i + 10,
                            "Thread Document " + to_string(i),
                            i * 10.5
                        );
                        
                        auto result = collection->insertOne(docJson);
                        
                        cout << "Thread " << i << " inserted document with ID: " 
                             << result.insertedId << endl;
                        
                        // Small delay to simulate work
                        this_thread::sleep_for(chrono::milliseconds(100));
                        
                        // Return connection to pool
                        conn->close();
                    }
                    catch (const DBException& e) {
                        cout << "Thread " << i << " error: " << e.what_s() << endl;
                    } }));
            }

            // Wait for all threads to complete
            for (auto &t : threads)
            {
                if (t.joinable())
                {
                    t.join();
                }
            }

            // Check results
            auto conn = pool.getDocumentDBConnection();
            auto collection = conn->getCollection(testCollectionName);
            auto count = collection->countDocuments();

            cout << "Final document count in collection: " << count << endl;
            cout << "\nDocument operations through pool completed successfully" << endl;

            conn->close();
        }

        // Step 7: Print pool statistics
        cout << "\nConnection pool statistics:" << endl;
        cout << "Active connections: " << pool.getActiveDBConnectionCount() << endl;
        cout << "Idle connections: " << pool.getIdleDBConnectionCount() << endl;
        cout << "Total connections: " << pool.getTotalDBConnectionCount() << endl;

        // Step 8: Clean up
        {
            cout << "\nCleaning up..." << endl;
            auto conn = pool.getDocumentDBConnection();

            if (conn->collectionExists(testCollectionName))
            {
                conn->dropCollection(testCollectionName);
                cout << "Dropped collection: " << testCollectionName << endl;
            }

            conn->close();
        }

        // Close the pool
        cout << "Closing connection pool..." << endl;
        pool.close();

        cout << "\nExample completed successfully." << endl;
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