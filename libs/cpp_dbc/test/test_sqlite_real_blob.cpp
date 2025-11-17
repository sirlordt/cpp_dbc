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

 @file test_sqlite_real_blob.cpp
 @brief Tests for SQLite database operations

*/

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/config/yaml_config_loader.hpp>
#if USE_SQLITE
#include <cpp_dbc/drivers/driver_sqlite.hpp>
#include <cpp_dbc/drivers/sqlite_blob.hpp>
#endif
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <filesystem>
#include "test_blob_common.hpp"

// External helper functions from test_main.cpp
extern std::vector<uint8_t> readBinaryFile(const std::string &filePath);
extern void writeBinaryFile(const std::string &filePath, const std::vector<uint8_t> &data);
extern std::string getTestImagePath();
extern std::string generateRandomTempFilename();

// Helper function to get the path to the test_db_connections.yml file
extern std::string getConfigFilePath();

#if USE_SQLITE
// Test case for SQLite BLOB operations
TEST_CASE("SQLite BLOB operations", "[sqlite_real_blob]")
{
    // Load the YAML configuration
    std::string config_path = getConfigFilePath();
    YAML::Node config = YAML::LoadFile(config_path);

    // Find the test_sqlite configuration
    YAML::Node dbConfig;
    if (config["databases"].IsSequence())
    {
        for (std::size_t i = 0; i < config["databases"].size(); ++i)
        {
            YAML::Node db = config["databases"][i];
            if (db["name"].as<std::string>() == "test_sqlite")
            {
                dbConfig = db;
                break;
            }
        }
    }

    // Check that the database configuration was found
    REQUIRE(dbConfig.IsDefined());

    // Create connection string
    std::string type = dbConfig["type"].as<std::string>();
    std::string database = dbConfig["database"].as<std::string>();

    // Test connection
    std::string connStr = "cpp_dbc:" + type + "://" + database;

    // Register the SQLite driver
    cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDriver>());

    // Get a connection
    auto conn = cpp_dbc::DriverManager::getConnection(connStr, "", "");
    REQUIRE(conn != nullptr);

    // Create test table with BLOB column
    conn->executeUpdate("DROP TABLE IF EXISTS test_blobs");
    conn->executeUpdate(
        "CREATE TABLE test_blobs ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT, "
        "data BLOB"
        ")");

    SECTION("Basic BLOB operations")
    {
        // Generate test data
        std::vector<uint8_t> smallData = generateRandomBinaryData(1000);
        std::vector<uint8_t> largeData = generateRandomBinaryData(100000);

        // Insert data using PreparedStatement
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 1);
        stmt->setString(2, "Test BLOB");
        stmt->setBytes(3, smallData);

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Insert large data
        stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 2);
        stmt->setString(2, "Large BLOB");
        stmt->setBytes(3, largeData);

        rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve small data
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 1");
        REQUIRE(rs->next());

        // Verify retrieved data
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getString("name") == "Test BLOB");

        auto retrievedSmallData = rs->getBytes("data");
        REQUIRE(compareBinaryData(smallData, retrievedSmallData));

        // Retrieve large data
        rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 2");
        REQUIRE(rs->next());

        // Verify retrieved data
        REQUIRE(rs->getInt("id") == 2);
        REQUIRE(rs->getString("name") == "Large BLOB");

        auto retrievedLargeData = rs->getBytes("data");
        REQUIRE(compareBinaryData(largeData, retrievedLargeData));
    }

    SECTION("BLOB streaming operations")
    {
        // Generate test data
        std::vector<uint8_t> largeData = generateRandomBinaryData(200000);

        // Insert data using PreparedStatement with streaming
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 3);
        stmt->setString(2, "Streaming BLOB");

        // Create a memory input stream
        auto inputStream = std::make_shared<cpp_dbc::MemoryInputStream>(largeData);
        stmt->setBinaryStream(3, inputStream, largeData.size());

        auto rowsAffected = stmt->executeUpdate();
        REQUIRE(rowsAffected == 1);

        // Retrieve data using ResultSet with streaming
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 3");
        REQUIRE(rs->next());

        // Get the BLOB as a stream
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
        REQUIRE(compareBinaryData(largeData, retrievedData));
    }

    SECTION("BLOB object operations")
    {
        // Generate test data
        std::vector<uint8_t> blobData = generateRandomBinaryData(50000);

        // Insert data using PreparedStatement with BLOB object
        auto stmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, data) "
            "VALUES (?, ?, ?)");

        stmt->setInt(1, 4);
        stmt->setString(2, "BLOB Object");

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
        REQUIRE(compareBinaryData(blobData, retrievedData));

        // Test partial retrieval
        size_t partialSize = 1000;
        auto partialData = retrievedBlob->getBytes(1000, partialSize);
        REQUIRE(partialData.size() == partialSize);
        REQUIRE(compareBinaryData(
            std::vector<uint8_t>(blobData.begin() + 1000, blobData.begin() + 1000 + partialSize),
            partialData));
    }

    SECTION("Image file BLOB operations")
    {
        // Get the path to the test image
        std::string imagePath = getTestImagePath();

        // Read the image file
        std::vector<uint8_t> imageData = readBinaryFile(imagePath);
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
        REQUIRE(compareBinaryData(imageData, retrievedImageData));

        // Write the retrieved image to a temporary file
        std::string tempImagePath = generateRandomTempFilename();
        writeBinaryFile(tempImagePath, retrievedImageData);

        // Read back the temporary file
        std::vector<uint8_t> tempImageData = readBinaryFile(tempImagePath);

        // Verify the temporary file data is the same as the original
        REQUIRE(tempImageData.size() == imageData.size());
        REQUIRE(compareBinaryData(imageData, tempImageData));

        // Clean up the temporary file
        std::remove(tempImagePath.c_str());
    }

    // Clean up
    conn->executeUpdate("DROP TABLE IF EXISTS test_blobs");

    // Close the connection
    conn->close();
}
#endif