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

 @file test_mongodb_thread_safe.cpp
 @brief Tests for MongoDB thread safety

*/

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "test_mongodb_common.hpp"

#if USE_MONGODB
// Test case for MongoDB thread safety
TEST_CASE("MongoDB thread safety tests", "[mongodb_thread_safe]")
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

    SECTION("Multiple threads using separate connections")
    {
        // Generate a unique collection name for this test
        std::string collectionName = mongodb_test_helpers::generateRandomCollectionName();

        // Get a connection for initial setup
        auto driver = mongodb_test_helpers::getMongoDBDriver();
        auto setupConn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(
            driver->connectDocument(connStr, username, password));

        // Create collection
        setupConn->createCollection(collectionName);
        auto collection = setupConn->getCollection(collectionName);
        REQUIRE(collection != nullptr);

        // Close the setup connection
        setupConn->close();

        // Number of threads and operations
        const int numThreads = 10;
        const int opsPerThread = 20;

        // Shared variables for tracking results
        std::atomic<int> successCount(0);
        std::mutex outputMutex;

        // Create and launch threads
        std::vector<std::thread> threads;
        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([&, i]()
                                          {
                try {
                    // Each thread creates its own connection
                    auto threadDriver = mongodb_test_helpers::getMongoDBDriver();
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(
                        threadDriver->connectDocument(connStr, username, password));
                    
                    // Get the collection
                    auto threadCollection = conn->getCollection(collectionName);
                    
                    for (int j = 0; j < opsPerThread; j++) {
                        // Generate a unique ID for this operation
                        int id = i * 1000 + j;
                        
                        // Create a document to insert
                        std::string doc = mongodb_test_helpers::generateTestDocument(id, "Thread " + std::to_string(i) + " Op " + std::to_string(j), id * 0.1);
                        
                        // Insert the document
                        auto insertResult = threadCollection->insertOne(doc);
                        
                        // Verify the document was inserted
                        auto query = "{\"id\": " + std::to_string(id) + "}";
                        auto foundDoc = threadCollection->findOne(query);
                        
                        if (foundDoc && foundDoc->getInt("id") == id) {
                            successCount++;
                        } else {
                            std::lock_guard<std::mutex> lock(outputMutex);
                            std::cerr << "Thread " << i << " failed to verify document " << id << std::endl;
                        }
                    }
                    
                    // Close the connection
                    conn->close();
                }
                catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(outputMutex);
                    std::cerr << "Thread " << i << " failed with exception: " << e.what() << std::endl;
                } }));
        }

        // Wait for all threads to complete
        for (auto &t : threads)
        {
            t.join();
        }

        // Verify all operations succeeded
        REQUIRE(successCount.load() == numThreads * opsPerThread);

        // Clean up - drop the collection
        auto cleanupConn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(
            driver->connectDocument(connStr, username, password));
        cleanupConn->dropCollection(collectionName);
        cleanupConn->close();
    }

    SECTION("Concurrent operations on shared connection")
    {
        // Generate a unique collection name for this test
        std::string collectionName = mongodb_test_helpers::generateRandomCollectionName();

        // Get a connection for all threads to share
        auto driver = mongodb_test_helpers::getMongoDBDriver();
        auto sharedConn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(
            driver->connectDocument(connStr, username, password));

        // Create collection
        sharedConn->createCollection(collectionName);
        auto collection = sharedConn->getCollection(collectionName);
        REQUIRE(collection != nullptr);

        // Number of threads and operations (reduced for shared connection)
        const int numThreads = 5;
        const int opsPerThread = 10;

        // Shared variables
        std::atomic<int> successCount(0);
        std::mutex connMutex; // Mutex for synchronizing access to the shared connection

        // Create and launch threads
        std::vector<std::thread> threads;
        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([&, i]()
                                          {
                for (int j = 0; j < opsPerThread; j++) {
                    // Generate a unique ID for this operation
                    int id = i * 1000 + j;
                    
                    // Create a document to insert
                    std::string doc = mongodb_test_helpers::generateTestDocument(id, "Shared " + std::to_string(i) + " Op " + std::to_string(j), id * 0.5);
                    
                    // Lock the mutex to synchronize access to the shared connection
                    {
                        std::lock_guard<std::mutex> lock(connMutex);
                        
                        // Insert the document
                        auto insertResult = collection->insertOne(doc);
                        
                        // Verify the document was inserted
                        auto query = "{\"id\": " + std::to_string(id) + "}";
                        auto foundDoc = collection->findOne(query);
                        
                        if (foundDoc && foundDoc->getInt("id") == id) {
                            successCount++;
                        }
                    } // Mutex is released here
                    
                    // Sleep briefly to simulate work and allow other threads to run
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } }));
        }

        // Wait for all threads to complete
        for (auto &t : threads)
        {
            t.join();
        }

        // Verify all operations succeeded
        REQUIRE(successCount.load() == numThreads * opsPerThread);

        // Clean up - drop the collection
        sharedConn->dropCollection(collectionName);
        sharedConn->close();
    }
}
#else
// Skip tests if MongoDB support is not enabled
TEST_CASE("MongoDB thread safety tests (skipped)", "[mongodb_thread_safe]")
{
    SKIP("MongoDB support is not enabled");
}
#endif