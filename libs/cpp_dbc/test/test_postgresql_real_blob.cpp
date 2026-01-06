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

 @file test_postgresql_real_blob.cpp
 @brief Tests for PostgreSQL database operations

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

#include "test_postgresql_common.hpp"

// Using common_test_helpers namespace for helper functions

#if USE_POSTGRESQL
// Test case for PostgreSQL BLOB operations
TEST_CASE("PostgreSQL BLOB operations", "[postgresql_real_blob]")
{
    // Skip these tests if we can't connect to PostgreSQL
    if (!postgresql_test_helpers::canConnectToPostgreSQL())
    {
        SKIP("Cannot connect to PostgreSQL database");
        return;
    }

    // Get PostgreSQL configuration
    auto dbConfig = postgresql_test_helpers::getPostgreSQLConfig("dev_postgresql");

    // Get connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string connStr = dbConfig.createConnectionString();

    // Register the PostgreSQL driver
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());

    // Get a connection
    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
    REQUIRE(conn != nullptr);

    // Create test table with BYTEA column
    conn->executeUpdate("DROP TABLE IF EXISTS test_blobs");
    conn->executeUpdate(
        "CREATE TABLE test_blobs ("
        "id INT PRIMARY KEY, "
        "name VARCHAR(100), "
        "data BYTEA"
        ")");

    SECTION("Basic BYTEA operations")
    {
        // Generate test data
        std::vector<uint8_t> smallData = common_test_helpers::generateRandomBinaryData(1000);
        std::vector<uint8_t> largeData = common_test_helpers::generateRandomBinaryData(100000);

        // Insert data using PreparedStatement
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 1);
        stmt->setString(2, "Test BYTEA");
        stmt->setBytes(3, smallData);

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Insert large data
        stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 2);
        stmt->setString(2, "Large BYTEA");
        stmt->setBytes(3, largeData);

        rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve small data
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 1");
        REQUIRE(rs->next());

        // Verify retrieved data
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getString("name") == "Test BYTEA");

        auto retrievedSmallData = rs->getBytes("data");
        REQUIRE(common_test_helpers::compareBinaryData(smallData, retrievedSmallData));

        // Retrieve large data
        rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 2");
        REQUIRE(rs->next());

        // Verify retrieved data
        REQUIRE(rs->getInt("id") == 2);
        REQUIRE(rs->getString("name") == "Large BYTEA");

        auto retrievedLargeData = rs->getBytes("data");
        REQUIRE(common_test_helpers::compareBinaryData(largeData, retrievedLargeData));
    }

    SECTION("BYTEA streaming operations")
    {
        // Generate test data
        std::vector<uint8_t> largeData = common_test_helpers::generateRandomBinaryData(200000);

        // Insert data using PreparedStatement with streaming
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 3);
        stmt->setString(2, "Streaming BYTEA");

        // Create a memory input stream
        auto inputStream = std::make_shared<cpp_dbc::MemoryInputStream>(largeData);
        stmt->setBinaryStream(3, inputStream, largeData.size());

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve data using ResultSet with streaming
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 3");
        REQUIRE(rs->next());

        // Get the BYTEA as a stream
        auto blobStream = rs->getBinaryStream("data");
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

    SECTION("BYTEA object operations")
    {
        // Generate test data
        std::vector<uint8_t> blobData = common_test_helpers::generateRandomBinaryData(50000);

        // Insert data using PreparedStatement with BLOB object
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 4);
        stmt->setString(2, "BYTEA Object");

        // Create a memory BLOB
        auto blob = std::make_shared<cpp_dbc::MemoryBlob>(blobData);
        stmt->setBlob(3, blob);

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve data using ResultSet with BLOB object
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 4");
        REQUIRE(rs->next());

        // Get the BLOB object
        auto retrievedBlob = rs->getBlob("data");
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
    }

    SECTION("Image file BYTEA operations")
    {
        // Get the path to the test image
        std::string imagePath = common_test_helpers::getTestImagePath();

        // Read the image file
        std::vector<uint8_t> imageData = common_test_helpers::readBinaryFile(imagePath);
        REQUIRE(!imageData.empty());

        // Insert the image data into the database
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, data) "
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
        REQUIRE(rs->getInt("id") == 5);
        REQUIRE(rs->getString("name") == "Test Image");

        // Get the image data
        auto retrievedImageData = rs->getBytes("data");
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
    conn->executeUpdate("DROP TABLE IF EXISTS test_blobs");

    // Close the connection
    conn->close();
}
#endif