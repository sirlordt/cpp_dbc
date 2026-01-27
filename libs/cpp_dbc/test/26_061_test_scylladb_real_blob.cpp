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
 * @file test_scylla_real_blob.cpp
 * @brief Tests for ScyllaDB BLOB data operations
 */

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "26_001_test_scylladb_real_common.hpp"
#include "10_000_test_main.hpp"

#if USE_SCYLLADB
// Test case for ScyllaDB BLOB operations
TEST_CASE("ScyllaDB BLOB operations", "[26_061_01_scylladb_real_blob]")
{
    // Skip these tests if we can't connect to ScyllaDB
    if (!scylla_test_helpers::canConnectToScylla())
    {
        SKIP("Cannot connect to ScyllaDB database");
        return;
    }

    // Get ScyllaDB configuration
    auto dbConfig = scylla_test_helpers::getScyllaConfig();

    // Extract connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string host = dbConfig.getHost();
    int port = dbConfig.getPort();
    std::string keyspace = dbConfig.getDatabase();
    std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;

    // Register the ScyllaDB driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

    // Get a connection
    auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
    REQUIRE(conn != nullptr);

    // Create keyspace if not exists
    std::string createKeyspaceQuery = dbConfig.getOption("query__create_keyspace",
                                                         "CREATE KEYSPACE IF NOT EXISTS " + keyspace + " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    conn->executeUpdate(createKeyspaceQuery);

    // Create test table with BLOB column
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_blobs");
    conn->executeUpdate(
        "CREATE TABLE " + keyspace + ".test_blobs ("
                                     "id int PRIMARY KEY, "
                                     "name text, "
                                     "small_data blob, "
                                     "medium_data blob, "
                                     "large_data blob"
                                     ")");

    SECTION("Basic BLOB operations")
    {
        // Generate test data
        std::vector<uint8_t> smallData = common_test_helpers::generateRandomBinaryData(1000);
        std::vector<uint8_t> mediumData = common_test_helpers::generateRandomBinaryData(10000);
        std::vector<uint8_t> largeData = common_test_helpers::generateRandomBinaryData(100000);

        // Insert data using PreparedStatement
        auto stmt = conn->prepareStatement(
            "INSERT INTO " + keyspace + ".test_blobs (id, name, small_data, medium_data, large_data) "
                                        "VALUES (?, ?, ?, ?, ?)");

        stmt->setInt(1, 1);
        stmt->setString(2, "Test BLOB");
        stmt->setBytes(3, smallData);
        stmt->setBytes(4, mediumData);
        stmt->setBytes(5, largeData);

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve data using ResultSet
        auto rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_blobs WHERE id = 1");
        REQUIRE(rs->next());

        // Verify retrieved data
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getString("name") == "Test BLOB");

        auto retrievedSmallData = rs->getBytes("small_data");
        REQUIRE(common_test_helpers::compareBinaryData(smallData, retrievedSmallData));

        auto retrievedMediumData = rs->getBytes("medium_data");
        REQUIRE(common_test_helpers::compareBinaryData(mediumData, retrievedMediumData));

        auto retrievedLargeData = rs->getBytes("large_data");
        REQUIRE(common_test_helpers::compareBinaryData(largeData, retrievedLargeData));
    }

    // Note: BLOB streaming operations and BLOB object operations are not implemented
    // because the required classes (MemoryInputStream, MemoryBlob) and methods (setBinaryStream, setBlob, getBlob)
    // are not available in the ColumnarDBPreparedStatement and ColumnarDBResultSet interfaces.

    SECTION("Image file BLOB operations")
    {
        // Get the path to the test image
        std::string imagePath = common_test_helpers::getTestImagePath();

        // Read the image file
        std::vector<uint8_t> imageData = common_test_helpers::readBinaryFile(imagePath);
        REQUIRE(!imageData.empty());

        // Insert the image data into the database
        auto stmt = conn->prepareStatement(
            "INSERT INTO " + keyspace + ".test_blobs (id, name, large_data) "
                                        "VALUES (?, ?, ?)");

        stmt->setInt(1, 5);
        stmt->setString(2, "Test Image");
        stmt->setBytes(3, imageData);

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve the image data from the database
        auto rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_blobs WHERE id = 5");
        REQUIRE(rs->next());

        // Verify the image metadata
        REQUIRE(rs->getInt("id") == 5);
        REQUIRE(rs->getString("name") == "Test Image");

        // Get the image data
        auto retrievedImageData = rs->getBytes("large_data");
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

    SECTION("BLOB update operations")
    {
        // Generate test data
        std::vector<uint8_t> originalData = common_test_helpers::generateRandomBinaryData(5000);
        std::vector<uint8_t> updatedData = common_test_helpers::generateRandomBinaryData(8000);

        // Insert original data
        auto stmt = conn->prepareStatement(
            "INSERT INTO " + keyspace + ".test_blobs (id, name, small_data) "
                                        "VALUES (?, ?, ?)");

        stmt->setInt(1, 6);
        stmt->setString(2, "Update Test");
        stmt->setBytes(3, originalData);

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Verify the original data was inserted correctly
        auto rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_blobs WHERE id = 6");
        REQUIRE(rs->next());
        auto retrievedOriginalData = rs->getBytes("small_data");
        REQUIRE(common_test_helpers::compareBinaryData(originalData, retrievedOriginalData));

        // Update the BLOB data
        auto updateStmt = conn->prepareStatement(
            "UPDATE " + keyspace + ".test_blobs SET small_data = ? WHERE id = ?");
        updateStmt->setBytes(1, updatedData);
        updateStmt->setInt(2, 6);

        rowsAffected = updateStmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Verify the data was updated correctly
        rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_blobs WHERE id = 6");
        REQUIRE(rs->next());
        auto retrievedUpdatedData = rs->getBytes("small_data");
        REQUIRE(common_test_helpers::compareBinaryData(updatedData, retrievedUpdatedData));
        REQUIRE_FALSE(common_test_helpers::compareBinaryData(originalData, retrievedUpdatedData));
    }

    SECTION("Multiple BLOB operations")
    {
        // Create multiple BLOBs of different sizes
        const int numBlobs = 5;
        std::vector<std::vector<uint8_t>> blobDataArray;

        // Insert multiple BLOBs
        for (int i = 0; i < numBlobs; i++)
        {
            // Generate data with increasing size
            std::vector<uint8_t> blobData = common_test_helpers::generateRandomBinaryData(1000 * (i + 1));
            blobDataArray.push_back(blobData);

            auto stmt = conn->prepareStatement(
                "INSERT INTO " + keyspace + ".test_blobs (id, name, medium_data) "
                                            "VALUES (?, ?, ?)");

            stmt->setInt(1, 10 + i);
            stmt->setString(2, "Multi BLOB " + std::to_string(i));
            stmt->setBytes(3, blobData);

            auto rowsAffected = stmt->executeUpdate();
            REQUIRE(rowsAffected == 1);
        }

        // Retrieve and verify all BLOBs
        auto rs = conn->executeQuery("SELECT * FROM " + keyspace + ".test_blobs WHERE id IN (10, 11, 12, 13, 14)");

        // Create a map to track which blobs we've found
        std::map<int, bool> foundBlobs;
        for (int i = 0; i < numBlobs; i++)
        {
            foundBlobs[10 + i] = false;
        }

        int count = 0;
        while (rs->next() && count < numBlobs)
        {
            int id = rs->getInt("id");
            // Verify the ID is in our expected range
            REQUIRE(id >= 10);
            REQUIRE(id < 10 + numBlobs);

            // Mark this blob as found
            foundBlobs[id] = true;

            // Verify the blob data matches what we expect
            int index = id - 10;
            auto retrievedData = rs->getBytes("medium_data");
            REQUIRE(common_test_helpers::compareBinaryData(blobDataArray[index], retrievedData));

            count++;
        }

        // Verify we found all blobs
        REQUIRE(count == numBlobs);
        for (const auto &pair : foundBlobs)
        {
            REQUIRE(pair.second == true);
        }
    }

    // Clean up
    conn->executeUpdate("DROP TABLE IF EXISTS " + keyspace + ".test_blobs");

    // Close the connection
    conn->close();
}
#else
// Skip tests if ScyllaDB support is not enabled
TEST_CASE("ScyllaDB BLOB operations (skipped)", "[26_061_02_scylladb_real_blob]")
{
    SKIP("ScyllaDB support is not enabled");
}
#endif
