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

 @file blob_operations_example.cpp
 @brief Example demonstrating BLOB operations with databases

*/

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/blob.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/relational/driver_postgresql.hpp>
#endif
#if USE_SQLITE
#include <cpp_dbc/drivers/relational/driver_sqlite.hpp>
#endif
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

// Helper function to get the executable path and name
std::string getExecutablePathAndName()
{
    std::vector<char> buffer(2048);
    ssize_t len = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (len != -1)
    {
        buffer[len] = '\0';
        return std::string(buffer.data());
    }
    return {};
}

// Helper function to get only the executable path
std::string getOnlyExecutablePath()
{
    std::filesystem::path p(getExecutablePathAndName());
    return p.parent_path().string() + "/"; // Returns only the directory with trailing slash
}

// Helper function to get the path to the test.jpg file
std::string getTestImagePath()
{
    // The test.jpg file is copied to the build directory by CMake
    return getOnlyExecutablePath() + "test.jpg";
}

// Helper function to generate a random temporary filename
std::string generateRandomTempFilename()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(10000, 99999);

    return "/tmp/test_image_" + std::to_string(distrib(gen)) + ".jpg";
}

// Helper function to generate random binary data
std::vector<uint8_t> generateRandomData(size_t size)
{
    std::vector<uint8_t> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);

    for (size_t i = 0; i < size; ++i)
    {
        data[i] = static_cast<uint8_t>(distrib(gen));
    }

    return data;
}

// Helper function to read a binary file
std::vector<uint8_t> readBinaryFile(const std::string &filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Cannot open file: " + filePath);
    }

    // Get file size
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the data
    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char *>(buffer.data()), size))
    {
        throw std::runtime_error("Error reading file: " + filePath);
    }

    return buffer;
}

// Helper function to write a binary file
void writeBinaryFile(const std::string &filePath, const std::vector<uint8_t> &data)
{
    std::ofstream file(filePath, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Cannot create file: " + filePath);
    }

    file.write(reinterpret_cast<const char *>(data.data()), data.size());
    if (!file)
    {
        throw std::runtime_error("Error writing file: " + filePath);
    }
}

// Helper function to create a temporary file with random data
std::string createTempFile(const std::string &prefix, size_t size)
{
    // Generate a unique filename
    std::string tempFilename = prefix + "_" +
                               std::to_string(std::time(nullptr)) + "_" +
                               std::to_string(std::rand()) + ".bin";

    // Generate random data
    auto data = generateRandomData(size);

    // Write to file
    writeBinaryFile(tempFilename, data);

    return tempFilename;
}

// Helper function to compare two binary data vectors
bool compareBinaryData(const std::vector<uint8_t> &data1, const std::vector<uint8_t> &data2)
{
    if (data1.size() != data2.size())
    {
        return false;
    }

    return std::equal(data1.begin(), data1.end(), data2.begin());
}

// Function to demonstrate basic BLOB operations
void demonstrateBasicBlobOperations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== Basic BLOB Operations ===\n"
              << std::endl;

    try
    {
        // Create a test table with BLOB columns
        conn->executeUpdate("DROP TABLE IF EXISTS test_blobs");
        conn->executeUpdate(
            "CREATE TABLE test_blobs ("
            "id INT PRIMARY KEY, "
            "name VARCHAR(100), "
            "description TEXT, "
            "small_data BLOB, " // For small binary data
            "large_data BLOB"   // For large binary data
            ")");

        std::cout << "Table created successfully." << std::endl;

        // Generate test data
        std::vector<uint8_t> smallData = generateRandomData(1000);   // 1KB
        std::vector<uint8_t> largeData = generateRandomData(100000); // 100KB

        // Insert data using a prepared statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, description, small_data, large_data) "
            "VALUES (?, ?, ?, ?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setString(2, "Test BLOB");
        pstmt->setString(3, "This is a test of BLOB data storage and retrieval");
        pstmt->setBytes(4, smallData);
        pstmt->setBytes(5, largeData);

        auto rowsAffected = pstmt->executeUpdate(); // auto will infer uint64_t
        std::cout << rowsAffected << " row(s) inserted." << std::endl;

        // Retrieve the data
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 1");

        if (rs->next())
        {
            std::cout << "Retrieved row with ID: " << rs->getInt("id") << std::endl;
            std::cout << "Name: " << rs->getString("name") << std::endl;
            std::cout << "Description: " << rs->getString("description") << std::endl;

            // Get the BLOB data
            auto retrievedSmallData = rs->getBytes("small_data");
            auto retrievedLargeData = rs->getBytes("large_data");

            // Verify the data
            bool smallDataMatch = compareBinaryData(smallData, retrievedSmallData);
            bool largeDataMatch = compareBinaryData(largeData, retrievedLargeData);

            std::cout << "Small data size: " << retrievedSmallData.size() << " bytes" << std::endl;
            std::cout << "Small data matches original: " << (smallDataMatch ? "Yes" : "No") << std::endl;

            std::cout << "Large data size: " << retrievedLargeData.size() << " bytes" << std::endl;
            std::cout << "Large data matches original: " << (largeDataMatch ? "Yes" : "No") << std::endl;
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what_s() << std::endl;
    }
}

// Function to demonstrate BLOB streaming operations
void demonstrateBlobStreaming(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== BLOB Streaming Operations ===\n"
              << std::endl;

    try
    {
        // Create a temporary file with random data (1MB)
        std::string tempFilename = createTempFile("stream_test", 1024 * 1024);
        std::cout << "Created temporary file: " << tempFilename << std::endl;

        // Read the file data for later comparison
        std::vector<uint8_t> fileData = readBinaryFile(tempFilename);
        std::cout << "File size: " << fileData.size() << " bytes" << std::endl;

        // Insert the file data using streaming
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, description, large_data) "
            "VALUES (?, ?, ?, ?)");

        pstmt->setInt(1, 2);
        pstmt->setString(2, "Streamed BLOB");
        pstmt->setString(3, "This BLOB was inserted using streaming");

        // Create a file input stream
        std::ifstream fileStream(tempFilename, std::ios::binary);
        if (!fileStream)
        {
            throw std::runtime_error("Cannot open file for streaming: " + tempFilename);
        }

        // Get file size
        fileStream.seekg(0, std::ios::end);
        // std::streamsize fileSize = fileStream.tellg();
        fileStream.seekg(0, std::ios::beg);

        // Create a memory input stream from the file data
        auto inputStream = std::make_shared<cpp_dbc::MemoryInputStream>(fileData);
        pstmt->setBinaryStream(4, inputStream, fileData.size());

        auto rowsAffected = pstmt->executeUpdate();
        std::cout << rowsAffected << " row(s) inserted using streaming." << std::endl;

        // Retrieve the data using streaming
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 2");

        if (rs->next())
        {
            std::cout << "Retrieved row with ID: " << rs->getInt("id") << std::endl;

            // Get the BLOB as a stream
            auto blobStream = rs->getBinaryStream("large_data");

            if (blobStream)
            {
                // Create a new file to save the retrieved data
                std::string retrievedFilename = "retrieved_" + tempFilename;
                std::ofstream outFile(retrievedFilename, std::ios::binary);

                if (!outFile)
                {
                    throw std::runtime_error("Cannot create output file: " + retrievedFilename);
                }

                // Read from the stream and write to the file
                uint8_t buffer[4096];
                int bytesRead;
                size_t totalBytes = 0;

                while ((bytesRead = blobStream->read(buffer, sizeof(buffer))) > 0)
                {
                    outFile.write(reinterpret_cast<char *>(buffer), bytesRead);
                    totalBytes += bytesRead;
                }

                outFile.close();

                std::cout << "Retrieved " << totalBytes << " bytes and saved to: " << retrievedFilename << std::endl;

                // Verify the retrieved file
                std::vector<uint8_t> retrievedData = readBinaryFile(retrievedFilename);
                bool dataMatch = compareBinaryData(fileData, retrievedData);

                std::cout << "Retrieved data matches original: " << (dataMatch ? "Yes" : "No") << std::endl;

                // Clean up the retrieved file
                fs::remove(retrievedFilename);
                std::cout << "Removed temporary retrieved file." << std::endl;
            }
        }

        // Clean up the temporary file
        fs::remove(tempFilename);
        std::cout << "Removed temporary source file." << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what_s() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// Function to demonstrate BLOB object operations
void demonstrateBlobObjects(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== BLOB Object Operations ===\n"
              << std::endl;

    try
    {
        // Generate test data
        std::vector<uint8_t> blobData = generateRandomData(50000); // 50KB

        // Insert data using a BLOB object
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, description, large_data) "
            "VALUES (?, ?, ?, ?)");

        pstmt->setInt(1, 3);
        pstmt->setString(2, "BLOB Object");
        pstmt->setString(3, "This BLOB was inserted using a BLOB object");

        // Create a memory BLOB
        auto blob = std::make_shared<cpp_dbc::MemoryBlob>(blobData);
        pstmt->setBlob(4, blob);

        auto rowsAffected = pstmt->executeUpdate();
        std::cout << rowsAffected << " row(s) inserted using BLOB object." << std::endl;

        // Retrieve the data as a BLOB object
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 3");

        if (rs->next())
        {
            std::cout << "Retrieved row with ID: " << rs->getInt("id") << std::endl;

            // Get the BLOB object
            auto retrievedBlob = rs->getBlob("large_data");

            if (retrievedBlob)
            {
                std::cout << "Retrieved BLOB length: " << retrievedBlob->length() << " bytes" << std::endl;

                // Get the entire BLOB data
                auto retrievedData = retrievedBlob->getBytes(0, retrievedBlob->length());

                // Verify the data
                bool dataMatch = compareBinaryData(blobData, retrievedData);
                std::cout << "Retrieved data matches original: " << (dataMatch ? "Yes" : "No") << std::endl;

                // Demonstrate partial retrieval
                size_t offset = 1000;
                size_t length = 500;
                std::cout << "\nDemonstrating partial BLOB retrieval:" << std::endl;
                std::cout << "Retrieving " << length << " bytes starting at offset " << offset << std::endl;

                auto partialData = retrievedBlob->getBytes(offset, length);
                std::cout << "Retrieved " << partialData.size() << " bytes" << std::endl;

                // Verify the partial data
                std::vector<uint8_t> expectedPartial(blobData.begin() + offset, blobData.begin() + offset + length);
                bool partialMatch = compareBinaryData(expectedPartial, partialData);
                std::cout << "Partial data matches expected: " << (partialMatch ? "Yes" : "No") << std::endl;

                // Demonstrate BLOB modification
                std::cout << "\nDemonstrating BLOB modification:" << std::endl;

                // Generate new data to replace a section
                std::vector<uint8_t> newData = generateRandomData(200);
                size_t modifyOffset = 2000;

                std::cout << "Modifying " << newData.size() << " bytes at offset " << modifyOffset << std::endl;
                retrievedBlob->setBytes(modifyOffset, newData);

                // Verify the modification
                auto modifiedSection = retrievedBlob->getBytes(modifyOffset, newData.size());
                bool modificationMatch = compareBinaryData(newData, modifiedSection);
                std::cout << "Modified section matches new data: " << (modificationMatch ? "Yes" : "No") << std::endl;
            }
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what_s() << std::endl;
    }
}

// Function to demonstrate image file BLOB operations
void demonstrateImageBlob(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== Image BLOB Operations ===\n"
              << std::endl;

    try
    {
        // Get the path to the test.jpg file
        std::string imagePath = getTestImagePath();

        // Check if the image file exists
        if (!fs::exists(imagePath))
        {
            std::cerr << "Image file not found: " << imagePath << std::endl;
            std::cerr << "Make sure test.jpg is copied to the build directory." << std::endl;
            return;
        }

        std::cout << "Using image file: " << imagePath << std::endl;

        // Read the image file
        std::vector<uint8_t> imageData = readBinaryFile(imagePath);
        std::cout << "Image size: " << imageData.size() << " bytes" << std::endl;

        // Insert the image into the database
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, description, large_data) "
            "VALUES (?, ?, ?, ?)");

        pstmt->setInt(1, 4);
        pstmt->setString(2, "Image BLOB");
        pstmt->setString(3, "This BLOB contains an image file");
        pstmt->setBytes(4, imageData);

        auto rowsAffected = pstmt->executeUpdate();
        std::cout << rowsAffected << " row(s) inserted with image data." << std::endl;

        // Retrieve the image
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 4");

        if (rs->next())
        {
            std::cout << "Retrieved row with ID: " << rs->getInt("id") << std::endl;

            // Get the image data
            auto retrievedImageData = rs->getBytes("large_data");
            std::cout << "Retrieved image size: " << retrievedImageData.size() << " bytes" << std::endl;

            // Verify the image data
            bool imageMatch = compareBinaryData(imageData, retrievedImageData);
            std::cout << "Retrieved image matches original: " << (imageMatch ? "Yes" : "No") << std::endl;

            // Save the retrieved image to a new file
            std::string retrievedImagePath = generateRandomTempFilename();

            writeBinaryFile(retrievedImagePath, retrievedImageData);
            std::cout << "Saved retrieved image to: " << retrievedImagePath << std::endl;

            // Clean up the retrieved image file
            fs::remove(retrievedImagePath);
            std::cout << "Removed retrieved image file." << std::endl;
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what_s() << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main()
{
    // Seed the random number generator
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    try
    {
        // Register database drivers
#if USE_MYSQL
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Connect to MySQL
        std::cout << "Connecting to MySQL..." << std::endl;
        auto mysqlConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(
            "cpp_dbc:mysql://localhost:3306/testdb",
            "username",
            "password"));

        // Demonstrate BLOB operations with MySQL
        demonstrateBasicBlobOperations(mysqlConn);
        demonstrateBlobStreaming(mysqlConn);
        demonstrateBlobObjects(mysqlConn);
        demonstrateImageBlob(mysqlConn);

        // Clean up
        mysqlConn->executeUpdate("DROP TABLE IF EXISTS test_blobs");
        std::cout << "\nDropped test table." << std::endl;

        // Close MySQL connection
        mysqlConn->close();
#else
        std::cout << "MySQL support is not enabled." << std::endl;
#endif

#if USE_POSTGRESQL
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());

        // Connect to PostgreSQL
        std::cout << "\nConnecting to PostgreSQL..." << std::endl;
        auto pgConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(
            "cpp_dbc:postgresql://localhost:5432/testdb",
            "username",
            "password"));

        // Demonstrate BLOB operations with PostgreSQL
        demonstrateBasicBlobOperations(pgConn);
        demonstrateBlobStreaming(pgConn);
        demonstrateBlobObjects(pgConn);
        demonstrateImageBlob(pgConn);

        // Clean up
        pgConn->executeUpdate("DROP TABLE IF EXISTS test_blobs");
        std::cout << "\nDropped test table." << std::endl;

        // Close PostgreSQL connection
        pgConn->close();
#else
        std::cout << "PostgreSQL support is not enabled." << std::endl;
#endif

#if USE_SQLITE
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

        // Connect to SQLite (in-memory database)
        std::cout << "\nConnecting to SQLite..." << std::endl;
        auto sqliteConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(
            "cpp_dbc:sqlite::memory:",
            "",
            ""));

        // Demonstrate BLOB operations with SQLite
        demonstrateBasicBlobOperations(sqliteConn);
        demonstrateBlobStreaming(sqliteConn);
        demonstrateBlobObjects(sqliteConn);
        demonstrateImageBlob(sqliteConn);

        // Clean up
        sqliteConn->executeUpdate("DROP TABLE IF EXISTS test_blobs");
        std::cout << "\nDropped test table." << std::endl;

        // Close SQLite connection
        sqliteConn->close();
#else
        std::cout << "SQLite support is not enabled." << std::endl;
#endif
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "Database error: " << e.what_s() << std::endl;
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}