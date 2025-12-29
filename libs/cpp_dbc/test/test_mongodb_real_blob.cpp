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

 @file test_mongodb_real_blob.cpp
 @brief Tests for MongoDB binary data operations

*/

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "test_mongodb_common.hpp"

#if USE_MONGODB
// Test case for MongoDB binary data operations
TEST_CASE("MongoDB binary data operations", "[mongodb_real_blob]")
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

    // Generate a unique collection name for this test
    std::string collectionName = mongodb_test_helpers::generateRandomCollectionName();

    // Create collection
    conn->createCollection(collectionName);
    auto collection = conn->getCollection(collectionName);
    REQUIRE(collection != nullptr);

    SECTION("Basic binary data operations")
    {
        // Generate test data of different sizes
        std::vector<uint8_t> smallData = common_test_helpers::generateRandomBinaryData(1000);
        std::vector<uint8_t> mediumData = common_test_helpers::generateRandomBinaryData(10000);
        std::vector<uint8_t> largeData = common_test_helpers::generateRandomBinaryData(100000);

        // MongoDB stores binary data as Base64 encoded strings in document

        // Insert a document with binary data - small binary
        auto smallDoc = conn->createDocument();
        smallDoc->setInt("id", 1);
        smallDoc->setString("name", "Small Binary");
        smallDoc->setBinary("binary_data", smallData);

        std::string doc1 = smallDoc->toJson();

        auto insertResult = collection->insertOne(doc1);
        REQUIRE(insertResult.acknowledged);

        // Insert a document with medium binary data
        auto mediumDoc = conn->createDocument();
        mediumDoc->setInt("id", 2);
        mediumDoc->setString("name", "Medium Binary");
        mediumDoc->setBinary("binary_data", mediumData);

        insertResult = collection->insertOne(mediumDoc);
        REQUIRE(insertResult.acknowledged);

        // Insert a document with large binary data
        auto largeDoc = conn->createDocument();
        largeDoc->setInt("id", 3);
        largeDoc->setString("name", "Large Binary");
        largeDoc->setBinary("binary_data", largeData);

        insertResult = collection->insertOne(largeDoc);
        REQUIRE(insertResult.acknowledged);

        // Retrieve the documents and verify binary data
        auto doc = collection->findOne("{\"id\": 1}");
        REQUIRE(doc != nullptr);
        REQUIRE(doc->getString("name") == "Small Binary");
        REQUIRE(doc->hasField("binary_data"));

        std::vector<uint8_t> retrievedSmallData = doc->getBinary("binary_data");
        REQUIRE(!retrievedSmallData.empty());
        REQUIRE(common_test_helpers::compareBinaryData(smallData, retrievedSmallData));

        // Retrieve medium data
        doc = collection->findOne("{\"id\": 2}");
        REQUIRE(doc != nullptr);
        std::vector<uint8_t> retrievedMediumData = doc->getBinary("binary_data");
        REQUIRE(common_test_helpers::compareBinaryData(mediumData, retrievedMediumData));

        // Retrieve large data
        doc = collection->findOne("{\"id\": 3}");
        REQUIRE(doc != nullptr);
        std::vector<uint8_t> retrievedLargeData = doc->getBinary("binary_data");
        REQUIRE(common_test_helpers::compareBinaryData(largeData, retrievedLargeData));
    }

    SECTION("Image file binary data operations")
    {
        // Use the test image helper function to get the correct path
        std::string imagePath = common_test_helpers::getTestImagePath();

        // Read the image file
        std::vector<uint8_t> imageData = common_test_helpers::readBinaryFile(imagePath);
        REQUIRE(!imageData.empty());

        // Create document with image data using the MongoDB driver's document creation
        auto imageDoc = conn->createDocument();
        imageDoc->setInt("id", 5);
        imageDoc->setString("name", "Test Image");
        imageDoc->setBinary("image_data", imageData);

        // Insert the document
        auto insertResult = collection->insertOne(imageDoc);
        REQUIRE(insertResult.acknowledged);

        // Retrieve the document
        auto retrievedDoc = collection->findOne("{\"id\": 5}");
        REQUIRE(retrievedDoc != nullptr);
        REQUIRE(retrievedDoc->getString("name") == "Test Image");
        REQUIRE(retrievedDoc->hasField("image_data"));

        // Get the binary data directly
        std::vector<uint8_t> retrievedImageData = retrievedDoc->getBinary("image_data");
        REQUIRE(!retrievedImageData.empty());

        // Verify the image data is the same as the original
        REQUIRE(retrievedImageData.size() == imageData.size());
        REQUIRE(common_test_helpers::compareBinaryData(imageData, retrievedImageData));

        // Write the retrieved image to a temporary file
        std::string tempImagePath = common_test_helpers::generateRandomTempFilename();
        common_test_helpers::writeBinaryFile(tempImagePath, retrievedImageData);

        // Read back the temporary file
        std::vector<uint8_t> tempImageData = common_test_helpers::readBinaryFile(tempImagePath);

        // Verify the temporary file data is the same as the original
        REQUIRE(tempImageData.size() == imageData.size());
        REQUIRE(common_test_helpers::compareBinaryData(imageData, tempImageData));

        // Clean up the temporary file
        std::remove(tempImagePath.c_str());
    }

    SECTION("Large binary data storage")
    {
        // Note: GridFS functionality is not directly exposed in the cpp_dbc API
        // This test demonstrates handling large binary data using regular collections

        // Skip if the MongoDB driver doesn't support large binary storage
        if (!conn->supportsTransactions()) // Using this as a proxy for modern MongoDB features
        {
            SKIP("Skipping large binary test for older MongoDB versions");
            return;
        }

        // Use a separate collection for this test
        std::string largeCollName = collectionName + "_large";
        conn->createCollection(largeCollName);
        auto largeColl = conn->getCollection(largeCollName);
        REQUIRE(largeColl != nullptr);

        try
        {
            // Create moderately large binary data (2MB to avoid test slowness)
            // Note: In a real application, data >16MB would require GridFS or chunking
            const size_t dataSize = 2 * 1024 * 1024; // 2MB
            std::vector<uint8_t> largeData = common_test_helpers::generateRandomBinaryData(dataSize);

            // Store large binary data in chunks of 512KB
            const size_t chunkSize = 512 * 1024;                    // 512KB chunks
            int numChunks = (dataSize + chunkSize - 1) / chunkSize; // Ceiling division

            // Insert chunks
            for (int i = 0; i < numChunks; i++)
            {
                size_t offset = i * chunkSize;
                size_t thisChunkSize = std::min(chunkSize, dataSize - offset);

                std::vector<uint8_t> chunk(largeData.begin() + offset,
                                           largeData.begin() + offset + thisChunkSize);

                auto chunkDoc = conn->createDocument();
                chunkDoc->setInt("file_id", 1);              // All chunks belong to file_id 1
                chunkDoc->setInt("chunk_index", i);          // Position in the sequence
                chunkDoc->setInt("total_chunks", numChunks); // Total number of chunks
                chunkDoc->setBinary("data", chunk);          // Actual binary chunk

                auto result = largeColl->insertOne(chunkDoc);
                REQUIRE(result.acknowledged);
            }

            // Query to get all chunks for our file
            auto cursor = largeColl->find("{\"file_id\": 1}");
            REQUIRE(cursor != nullptr);

            // Reassemble the chunks
            std::vector<std::shared_ptr<cpp_dbc::DocumentDBData>> chunks;
            while (cursor->next())
            {
                chunks.push_back(cursor->current());
            }

            // Sort chunks by index
            std::sort(chunks.begin(), chunks.end(),
                      [](const auto &a, const auto &b)
                      {
                          return a->getInt("chunk_index") < b->getInt("chunk_index");
                      });

            // Verify we have all chunks
            REQUIRE(chunks.size() == static_cast<size_t>(numChunks));

            // Reassemble the data
            std::vector<uint8_t> retrievedData;
            for (const auto &chunk : chunks)
            {
                auto chunkData = chunk->getBinary("data");
                retrievedData.insert(retrievedData.end(), chunkData.begin(), chunkData.end());
            }

            // Verify the reassembled data matches the original
            REQUIRE(retrievedData.size() == largeData.size());
            REQUIRE(common_test_helpers::compareBinaryData(largeData, retrievedData));

            // Clean up
            conn->dropCollection(largeCollName);
        }
        catch (const cpp_dbc::DBException &e)
        {
            // Attempt to clean up even if test fails
            try
            {
                conn->dropCollection(largeCollName);
            }
            catch (...)
            {
            }
            throw; // Re-throw the original exception
        }
    }

    // Clean up - drop the collection
    conn->dropCollection(collectionName);
    conn->close();
}
#else
// Skip tests if MongoDB support is not enabled
TEST_CASE("MongoDB binary data operations (skipped)", "[mongodb_real_blob]")
{
    SKIP("MongoDB support is not enabled");
}
#endif