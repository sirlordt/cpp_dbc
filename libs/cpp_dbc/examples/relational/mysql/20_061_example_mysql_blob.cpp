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
 * @file 20_061_example_mysql_blob.cpp
 * @brief MySQL-specific example demonstrating BLOB operations
 *
 * This example demonstrates:
 * - Basic BLOB operations (insert, retrieve, compare)
 * - BLOB streaming operations
 * - BLOB object operations with partial retrieval
 * - Image file BLOB operations
 *
 * Usage:
 *   ./20_061_example_mysql_blob [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - MySQL support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <cpp_dbc/blob.hpp>
#include <fstream>
#include <random>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

#if USE_MYSQL
#include <cpp_dbc/drivers/relational/driver_mysql.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_MYSQL
// Helper function to get the path to the test.jpg file
std::string getTestImagePath()
{
    return cpp_dbc::system_utils::getExecutablePath() + "test.jpg";
}

// Helper function to generate a random temporary filename
std::string generateRandomTempFilename()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(10000, 99999);

    return (fs::temp_directory_path() / ("test_image_" + std::to_string(distrib(gen)) + ".jpg")).string();
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

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

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
    std::string tempFilename = (fs::temp_directory_path() /
                                (prefix + "_" +
                                 std::to_string(std::time(nullptr)) + "_" +
                                 std::to_string(std::rand()) + ".bin"))
                                   .string();

    auto data = generateRandomData(size);
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
    logMsg("");
    logMsg("--- Basic BLOB Operations ---");

    try
    {
        logStep("Creating test_blobs table...");
        conn->executeUpdate("DROP TABLE IF EXISTS test_blobs");
        conn->executeUpdate(
            "CREATE TABLE test_blobs ("
            "id INT PRIMARY KEY, "
            "name VARCHAR(100), "
            "description TEXT, "
            "small_data BLOB, "
            "large_data LONGBLOB"
            ")");
        logOk("Table created");

        // Generate test data
        std::vector<uint8_t> smallData = generateRandomData(1000);   // 1KB
        std::vector<uint8_t> largeData = generateRandomData(100000); // 100KB

        logStep("Inserting BLOB data...");
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, description, small_data, large_data) "
            "VALUES (?, ?, ?, ?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setString(2, "Test BLOB");
        pstmt->setString(3, "This is a test of BLOB data storage and retrieval");
        pstmt->setBytes(4, smallData);
        pstmt->setBytes(5, largeData);

        auto rowsAffected = pstmt->executeUpdate();
        logOk(std::to_string(rowsAffected) + " row(s) inserted");

        // Retrieve the data
        logStep("Retrieving BLOB data...");
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 1");

        if (rs->next())
        {
            logData("Retrieved row ID: " + std::to_string(rs->getInt("id")));
            logData("Name: " + rs->getString("name"));

            auto retrievedSmallData = rs->getBytes("small_data");
            auto retrievedLargeData = rs->getBytes("large_data");

            bool smallDataMatch = compareBinaryData(smallData, retrievedSmallData);
            bool largeDataMatch = compareBinaryData(largeData, retrievedLargeData);

            logData("Small data size: " + std::to_string(retrievedSmallData.size()) +
                    " bytes, matches: " + (smallDataMatch ? "Yes" : "No"));
            logData("Large data size: " + std::to_string(retrievedLargeData.size()) +
                    " bytes, matches: " + (largeDataMatch ? "Yes" : "No"));
            logOk("BLOB data verified");
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
    }
}

// Function to demonstrate BLOB streaming operations
void demonstrateBlobStreaming(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- BLOB Streaming Operations ---");

    try
    {
        logStep("Creating temporary file with 1MB of data...");
        std::string tempFilename = createTempFile("stream_test", 1024 * 1024);
        logOk("Created: " + tempFilename);

        std::vector<uint8_t> fileData = readBinaryFile(tempFilename);
        logData("File size: " + std::to_string(fileData.size()) + " bytes");

        logStep("Inserting data using streaming...");
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, description, large_data) "
            "VALUES (?, ?, ?, ?)");

        pstmt->setInt(1, 2);
        pstmt->setString(2, "Streamed BLOB");
        pstmt->setString(3, "This BLOB was inserted using streaming");

        auto inputStream = std::make_shared<cpp_dbc::MemoryInputStream>(fileData);
        pstmt->setBinaryStream(4, inputStream, fileData.size());

        auto rowsAffected = pstmt->executeUpdate();
        logOk(std::to_string(rowsAffected) + " row(s) inserted using streaming");

        // Retrieve the data using streaming
        logStep("Retrieving data using streaming...");
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 2");

        if (rs->next())
        {
            auto blobStream = rs->getBinaryStream("large_data");

            if (blobStream)
            {
                // Construct the retrieved filename in the same directory as the original file
                fs::path p(tempFilename);
                std::string retrievedFilename = (p.parent_path() / ("retrieved_" + p.filename().string())).string();
                std::ofstream outFile(retrievedFilename, std::ios::binary);

                uint8_t buffer[4096];
                int bytesRead;
                size_t totalBytes = 0;

                while ((bytesRead = blobStream->read(buffer, sizeof(buffer))) > 0)
                {
                    outFile.write(reinterpret_cast<char *>(buffer), bytesRead);
                    totalBytes += bytesRead;
                }
                outFile.close();

                logData("Retrieved " + std::to_string(totalBytes) + " bytes");

                std::vector<uint8_t> retrievedData = readBinaryFile(retrievedFilename);
                bool dataMatch = compareBinaryData(fileData, retrievedData);
                logData("Data matches original: " + std::string(dataMatch ? "Yes" : "No"));

                fs::remove(retrievedFilename);
                logOk("Streaming verified");
            }
        }

        fs::remove(tempFilename);
        logOk("Temporary files cleaned up");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
    }
    catch (const std::exception &e)
    {
        logError("Error: " + std::string(e.what()));
    }
}

// Function to demonstrate BLOB object operations
void demonstrateBlobObjects(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- BLOB Object Operations ---");

    try
    {
        std::vector<uint8_t> blobData = generateRandomData(50000); // 50KB

        logStep("Inserting data using BLOB object...");
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, description, large_data) "
            "VALUES (?, ?, ?, ?)");

        pstmt->setInt(1, 3);
        pstmt->setString(2, "BLOB Object");
        pstmt->setString(3, "This BLOB was inserted using a BLOB object");

        auto blob = std::make_shared<cpp_dbc::MemoryBlob>(blobData);
        pstmt->setBlob(4, blob);

        auto rowsAffected = pstmt->executeUpdate();
        logOk(std::to_string(rowsAffected) + " row(s) inserted using BLOB object");

        // Retrieve the data as a BLOB object
        logStep("Retrieving BLOB object...");
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 3");

        if (rs->next())
        {
            auto retrievedBlob = rs->getBlob("large_data");

            if (retrievedBlob)
            {
                logData("Retrieved BLOB length: " + std::to_string(retrievedBlob->length()) + " bytes");

                auto retrievedData = retrievedBlob->getBytes(0, retrievedBlob->length());
                bool dataMatch = compareBinaryData(blobData, retrievedData);
                logData("Data matches original: " + std::string(dataMatch ? "Yes" : "No"));

                // Demonstrate partial retrieval
                size_t offset = 1000;
                size_t length = 500;
                logStep("Retrieving partial data (offset=" + std::to_string(offset) +
                        ", length=" + std::to_string(length) + ")...");

                auto partialData = retrievedBlob->getBytes(offset, length);
                std::vector<uint8_t> expectedPartial(blobData.begin() + offset, blobData.begin() + offset + length);
                bool partialMatch = compareBinaryData(expectedPartial, partialData);
                logData("Partial data matches: " + std::string(partialMatch ? "Yes" : "No"));

                // Demonstrate BLOB modification
                logStep("Modifying BLOB data...");
                std::vector<uint8_t> newData = generateRandomData(200);
                size_t modifyOffset = 2000;

                retrievedBlob->setBytes(modifyOffset, newData);

                auto modifiedSection = retrievedBlob->getBytes(modifyOffset, newData.size());
                bool modificationMatch = compareBinaryData(newData, modifiedSection);
                logData("Modified section matches: " + std::string(modificationMatch ? "Yes" : "No"));
                logOk("BLOB object operations verified");
            }
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
    }
}

// Function to demonstrate image file BLOB operations
void demonstrateImageBlob(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    logMsg("");
    logMsg("--- Image BLOB Operations ---");

    try
    {
        std::string imagePath = getTestImagePath();

        if (!fs::exists(imagePath))
        {
            logInfo("Image file not found: " + imagePath);
            logInfo("Skipping image BLOB demonstration");
            return;
        }

        logStep("Loading image file: " + imagePath);
        std::vector<uint8_t> imageData = readBinaryFile(imagePath);
        logData("Image size: " + std::to_string(imageData.size()) + " bytes");

        logStep("Inserting image into database...");
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_blobs (id, name, description, large_data) "
            "VALUES (?, ?, ?, ?)");

        pstmt->setInt(1, 4);
        pstmt->setString(2, "Image BLOB");
        pstmt->setString(3, "This BLOB contains an image file");
        pstmt->setBytes(4, imageData);

        auto rowsAffected = pstmt->executeUpdate();
        logOk(std::to_string(rowsAffected) + " row(s) inserted with image data");

        // Retrieve the image
        logStep("Retrieving image from database...");
        auto rs = conn->executeQuery("SELECT * FROM test_blobs WHERE id = 4");

        if (rs->next())
        {
            auto retrievedImageData = rs->getBytes("large_data");
            logData("Retrieved image size: " + std::to_string(retrievedImageData.size()) + " bytes");

            bool imageMatch = compareBinaryData(imageData, retrievedImageData);
            logData("Retrieved image matches original: " + std::string(imageMatch ? "Yes" : "No"));

            // Save the retrieved image to a new file
            std::string retrievedImagePath = generateRandomTempFilename();
            writeBinaryFile(retrievedImagePath, retrievedImageData);
            logData("Saved retrieved image to: " + retrievedImagePath);

            fs::remove(retrievedImagePath);
            logOk("Image BLOB operations verified");
        }
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
    }
    catch (const std::exception &e)
    {
        logError("Error: " + std::string(e.what()));
    }
}

// Function to run all BLOB demonstrations for MySQL
void runAllDemonstrations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    demonstrateBasicBlobOperations(conn);
    demonstrateBlobStreaming(conn);
    demonstrateBlobObjects(conn);
    demonstrateImageBlob(conn);

    logMsg("");
    logStep("Cleaning up tables...");
    conn->executeUpdate("DROP TABLE IF EXISTS test_blobs");
    logOk("Tables dropped");
}
#endif

int main(int argc, char *argv[])
{
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    logMsg("========================================");
    logMsg("cpp_dbc MySQL BLOB Operations Example");
    logMsg("========================================");
    logMsg("");

#if !USE_MYSQL
    logError("MySQL support is not enabled");
    logInfo("Build with -DUSE_MYSQL=ON to enable MySQL support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,mysql");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("20_061_example_mysql_blob", "mysql");
        return EXIT_OK_;
    }
    logOk("Arguments parsed");

    logStep("Loading configuration from: " + args.configPath);
    auto configResult = loadConfig(args.configPath);

    if (!configResult)
    {
        logError("Failed to load configuration: " + configResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!configResult.value())
    {
        logError("Configuration file not found: " + args.configPath);
        logInfo("Use --config=<path> to specify config file");
        return EXIT_ERROR_;
    }
    logOk("Configuration loaded successfully");

    auto &configManager = *configResult.value();

    logStep("Registering MySQL driver...");
    registerDriver("mysql");
    logOk("Driver registered");

    try
    {
        logStep("Getting MySQL configuration...");
        auto mysqlResult = getDbConfig(configManager, args.dbName.empty() ? "" : args.dbName, "mysql");

        if (!mysqlResult)
        {
            logError("Failed to get MySQL config: " + mysqlResult.error().what_s());
            return EXIT_ERROR_;
        }

        if (!mysqlResult.value())
        {
            logError("MySQL configuration not found");
            return EXIT_ERROR_;
        }

        auto &mysqlConfig = *mysqlResult.value();
        logOk("Using: " + mysqlConfig.getName());

        logStep("Connecting to MySQL...");
        auto mysqlConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            mysqlConfig.createDBConnection());
        logOk("Connected to MySQL");

        runAllDemonstrations(mysqlConn);

        logStep("Closing MySQL connection...");
        mysqlConn->close();
        logOk("MySQL connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        e.printCallStack();
        return EXIT_ERROR_;
    }
    catch (const std::exception &e)
    {
        logError("Error: " + std::string(e.what()));
        return EXIT_ERROR_;
    }

    logMsg("");
    logMsg("========================================");
    logOk("Example completed successfully");
    logMsg("========================================");

    return EXIT_OK_;
#endif
}
