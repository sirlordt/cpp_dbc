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

 @file test_firebird_real_blob.cpp
 @brief Tests for Firebird BLOB database operations

*/

#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/relational/relational_db_connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "23_001_test_firebird_real_common.hpp"

// Using common_test_helpers namespace for helper functions

#if USE_FIREBIRD
// Test case for Firebird BLOB operations
TEST_CASE("Firebird BLOB operations", "[23_061_01_firebird_real_blob]")
{
    // Skip these tests if we can't connect to Firebird
    if (!firebird_test_helpers::canConnectToFirebird())
    {
        SKIP("Cannot connect to Firebird database");
        return;
    }

    // Get Firebird configuration using the centralized function
    auto dbConfig = firebird_test_helpers::getFirebirdConfig("dev_firebird");

    // Extract connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Register the Firebird driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

    // Get a connection
    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
    REQUIRE(conn != nullptr);

    // Create test table with BLOB column
    // Firebird uses BLOB SUB_TYPE 0 for binary data
    try
    {
        conn->executeUpdate("DROP TABLE test_blobs");
    }
    catch (...)
    {
        // Table might not exist, ignore error
    }

    conn->executeUpdate(
        "CREATE TABLE test_blobs ("
        "id INTEGER NOT NULL PRIMARY KEY, "
        "name VARCHAR(100), "
        "data BLOB SUB_TYPE 0, "
        "small_data BLOB SUB_TYPE 0, "
        "large_data BLOB SUB_TYPE 0"
        ")");

    SECTION("Basic BLOB operations")
    {
        // Generate test data
        std::vector<uint8_t> smallData = common_test_helpers::generateRandomBinaryData(1000);
        std::vector<uint8_t> mediumData = common_test_helpers::generateRandomBinaryData(10000);
        std::vector<uint8_t> largeData = common_test_helpers::generateRandomBinaryData(100000);

        // Insert data using PreparedStatement
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, data, small_data, large_data) "
            "VALUES (?, ?, ?, ?, ?)");

        stmt->setInt(1, 1);
        stmt->setString(2, "Test BLOB");
        stmt->setBytes(3, smallData);
        stmt->setBytes(4, mediumData);
        stmt->setBytes(5, largeData);

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve data using ResultSet
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 1");
        REQUIRE(rs->next());

        // Verify retrieved data
        REQUIRE(rs->getInt("ID") == 1);
        REQUIRE(rs->getString("NAME") == "Test BLOB");

        auto retrievedSmallData = rs->getBytes("DATA");
        REQUIRE(common_test_helpers::compareBinaryData(smallData, retrievedSmallData));

        auto retrievedMediumData = rs->getBytes("SMALL_DATA");
        REQUIRE(common_test_helpers::compareBinaryData(mediumData, retrievedMediumData));

        auto retrievedLargeData = rs->getBytes("LARGE_DATA");
        REQUIRE(common_test_helpers::compareBinaryData(largeData, retrievedLargeData));
    }

    SECTION("BLOB streaming operations")
    {
        // Generate test data
        std::vector<uint8_t> largeData = common_test_helpers::generateRandomBinaryData(200000);

        // Insert data using PreparedStatement with streaming
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, large_data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 2);
        stmt->setString(2, "Streaming BLOB");

        // Create a memory input stream
        auto inputStream = std::make_shared<cpp_dbc::MemoryInputStream>(largeData);
        stmt->setBinaryStream(3, inputStream, largeData.size());

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve data using ResultSet with streaming
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 2");
        REQUIRE(rs->next());

        // Get the BLOB as a stream
        auto blobStream = rs->getBinaryStream("LARGE_DATA");
        REQUIRE(blobStream != nullptr);

        // Read the data from the stream
        std::vector<uint8_t> retrievedData;
        uint8_t buffer[4096];
        int bytesRead;

        while ((bytesRead = blobStream->read(buffer, sizeof(buffer))) > 0)
        {
            retrievedData.insert(retrievedData.end(), buffer, buffer + bytesRead);
        }

        // Verify the data
        REQUIRE(common_test_helpers::compareBinaryData(largeData, retrievedData));
    }

    SECTION("BLOB object operations")
    {
        // Generate test data
        std::vector<uint8_t> blobData = common_test_helpers::generateRandomBinaryData(50000);

        // Insert data using PreparedStatement with BLOB object
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, large_data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 3);
        stmt->setString(2, "BLOB Object");

        // Create a memory BLOB
        auto blob = std::make_shared<cpp_dbc::MemoryBlob>(blobData);
        stmt->setBlob(3, blob);

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve data using ResultSet with BLOB object
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 3");
        REQUIRE(rs->next());

        // Get the BLOB object
        auto retrievedBlob = rs->getBlob("LARGE_DATA");
        REQUIRE(retrievedBlob != nullptr);

        // Verify the BLOB length
        REQUIRE(retrievedBlob->length() == blobData.size());

        // Get the BLOB data
        auto retrievedData = retrievedBlob->getBytes(0, retrievedBlob->length());
        REQUIRE(common_test_helpers::compareBinaryData(blobData, retrievedData));

        // Test partial retrieval
        size_t partialSize = 1000;
        auto partialData = retrievedBlob->getBytes(1000, partialSize);
        REQUIRE(partialData.size() == partialSize);
        REQUIRE(common_test_helpers::compareBinaryData(
            std::vector<uint8_t>(blobData.begin() + 1000, blobData.begin() + 1000 + partialSize),
            partialData));

        // Test modification
        std::vector<uint8_t> newData = common_test_helpers::generateRandomBinaryData(1000);
        retrievedBlob->setBytes(2000, newData);

        // Verify the modification
        auto modifiedData = retrievedBlob->getBytes(2000, newData.size());
        REQUIRE(common_test_helpers::compareBinaryData(newData, modifiedData));
    }

    SECTION("Image file BLOB operations")
    {
        // Get the path to the test image
        std::string imagePath = common_test_helpers::getTestImagePath();

        // Read the image file
        std::vector<uint8_t> imageData = common_test_helpers::readBinaryFile(imagePath);
        REQUIRE(!imageData.empty());

        // Insert the image data into the database
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, large_data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 5);
        stmt->setString(2, "Test Image");
        stmt->setBytes(3, imageData);

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve the image data from the database
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 5");
        REQUIRE(rs->next());

        // Verify the image metadata
        REQUIRE(rs->getInt("ID") == 5);
        REQUIRE(rs->getString("NAME") == "Test Image");

        // Get the image data
        auto retrievedImageData = rs->getBytes("LARGE_DATA");
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

    // Clean up
    try
    {
        conn->executeUpdate("DROP TABLE test_blobs");
    }
    catch (...)
    {
        // Ignore cleanup errors
    }

    // Close the connection
    conn->close();
}
#endif