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
 * @file 24_061_example_redis_blob.cpp
 * @brief Redis-specific example demonstrating binary data operations
 *
 * This example demonstrates:
 * - Storing binary data as Redis values
 * - Retrieving and verifying binary data integrity
 * - Binary-safe string operations
 * - Large binary data handling
 *
 * Note: Redis is binary-safe, meaning keys and values can be any binary sequence.
 *
 * Usage:
 *   ./24_061_example_redis_blob [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - Redis support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <vector>
#include <cstring>

#if USE_REDIS
#include <cpp_dbc/drivers/kv/driver_redis.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_REDIS
// Helper function to create test binary data
std::vector<uint8_t> createTestBinaryData(size_t size)
{
    std::vector<uint8_t> data(size);
    for (size_t i = 0; i < size; ++i)
    {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    return data;
}

// Helper function to verify binary data
bool verifyBinaryData(const std::vector<uint8_t> &original, const std::vector<uint8_t> &retrieved)
{
    if (original.size() != retrieved.size())
    {
        return false;
    }
    return std::memcmp(original.data(), retrieved.data(), original.size()) == 0;
}

// Convert vector to string for storage
std::string vectorToString(const std::vector<uint8_t> &data)
{
    return std::string(reinterpret_cast<const char *>(data.data()), data.size());
}

// Convert string to vector
std::vector<uint8_t> stringToVector(const std::string &str)
{
    return std::vector<uint8_t>(str.begin(), str.end());
}

void demonstrateBinaryStorage(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    log("");
    log("--- Binary Data Storage ---");
    logInfo("Redis is binary-safe and can store any byte sequence");

    // Create test data of various sizes
    auto smallData = createTestBinaryData(100);
    auto mediumData = createTestBinaryData(10000);
    auto largeData = createTestBinaryData(100000);

    // Store small binary data
    logStep("Storing small binary data (100 bytes)...");
    std::string smallKey = "blob_small";
    conn->setString(smallKey, vectorToString(smallData));
    logOk("Small data stored");

    // Store medium binary data
    logStep("Storing medium binary data (10 KB)...");
    std::string mediumKey = "blob_medium";
    conn->setString(mediumKey, vectorToString(mediumData));
    logOk("Medium data stored");

    // Store large binary data
    logStep("Storing large binary data (100 KB)...");
    std::string largeKey = "blob_large";
    conn->setString(largeKey, vectorToString(largeData));
    logOk("Large data stored");

    // Retrieve and verify
    log("");
    log("--- Retrieve and Verify ---");

    logStep("Retrieving and verifying small data...");
    auto retrievedSmall = stringToVector(conn->getString(smallKey));
    if (verifyBinaryData(smallData, retrievedSmall))
    {
        logOk("Small data verified (" + std::to_string(retrievedSmall.size()) + " bytes)");
    }
    else
    {
        logError("Small data verification failed!");
    }

    logStep("Retrieving and verifying medium data...");
    auto retrievedMedium = stringToVector(conn->getString(mediumKey));
    if (verifyBinaryData(mediumData, retrievedMedium))
    {
        logOk("Medium data verified (" + std::to_string(retrievedMedium.size()) + " bytes)");
    }
    else
    {
        logError("Medium data verification failed!");
    }

    logStep("Retrieving and verifying large data...");
    auto retrievedLarge = stringToVector(conn->getString(largeKey));
    if (verifyBinaryData(largeData, retrievedLarge))
    {
        logOk("Large data verified (" + std::to_string(retrievedLarge.size()) + " bytes)");
    }
    else
    {
        logError("Large data verification failed!");
    }

    // Cleanup
    conn->deleteKey(smallKey);
    conn->deleteKey(mediumKey);
    conn->deleteKey(largeKey);
}

void demonstrateBinaryWithNullBytes(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    log("");
    log("--- Binary Data with NULL Bytes ---");
    logInfo("Redis handles NULL bytes (\\0) correctly");

    std::string key = "blob_with_nulls";

    // Create data with embedded null bytes
    std::vector<uint8_t> dataWithNulls = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x00, 0x57, 0x6F, 0x72, 0x6C, 0x64, 0x00, 0x21};
    // This is "Hello\0World\0!"

    logStep("Storing data with embedded NULL bytes...");
    conn->setString(key, vectorToString(dataWithNulls));
    logData("Original size: " + std::to_string(dataWithNulls.size()) + " bytes");
    logOk("Data stored");

    logStep("Retrieving data...");
    auto retrieved = stringToVector(conn->getString(key));
    logData("Retrieved size: " + std::to_string(retrieved.size()) + " bytes");

    if (verifyBinaryData(dataWithNulls, retrieved))
    {
        logOk("NULL bytes preserved correctly");
    }
    else
    {
        logError("Data corruption detected!");
    }

    // Cleanup
    conn->deleteKey(key);
}

void demonstrateBinaryInHash(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    log("");
    log("--- Binary Data in Hash Fields ---");
    logInfo("Hash fields can also store binary data");

    std::string hashKey = "blob_hash";

    // Create different binary payloads
    auto imageData = createTestBinaryData(5000);
    auto audioData = createTestBinaryData(8000);
    auto videoData = createTestBinaryData(12000);

    logStep("Clearing existing hash...");
    conn->deleteKey(hashKey);

    logStep("Storing binary data in hash fields...");
    conn->hashSet(hashKey, "image", vectorToString(imageData));
    logData("Stored 'image' field: " + std::to_string(imageData.size()) + " bytes");

    conn->hashSet(hashKey, "audio", vectorToString(audioData));
    logData("Stored 'audio' field: " + std::to_string(audioData.size()) + " bytes");

    conn->hashSet(hashKey, "video", vectorToString(videoData));
    logData("Stored 'video' field: " + std::to_string(videoData.size()) + " bytes");

    logOk("Binary data stored in hash");

    logStep("Retrieving and verifying hash fields...");

    auto retrievedImage = stringToVector(conn->hashGet(hashKey, "image"));
    if (verifyBinaryData(imageData, retrievedImage))
    {
        logOk("Image data verified (" + std::to_string(retrievedImage.size()) + " bytes)");
    }
    else
    {
        logError("Image data verification failed!");
    }

    auto retrievedAudio = stringToVector(conn->hashGet(hashKey, "audio"));
    if (verifyBinaryData(audioData, retrievedAudio))
    {
        logOk("Audio data verified (" + std::to_string(retrievedAudio.size()) + " bytes)");
    }
    else
    {
        logError("Audio data verification failed!");
    }

    auto retrievedVideo = stringToVector(conn->hashGet(hashKey, "video"));
    if (verifyBinaryData(videoData, retrievedVideo))
    {
        logOk("Video data verified (" + std::to_string(retrievedVideo.size()) + " bytes)");
    }
    else
    {
        logError("Video data verification failed!");
    }

    // Cleanup
    conn->deleteKey(hashKey);
}

void demonstrateBinaryWithExpiration(std::shared_ptr<cpp_dbc::KVDBConnection> conn)
{
    log("");
    log("--- Binary Data with TTL ---");
    logInfo("Binary data can have expiration time");

    std::string key = "blob_with_ttl";
    auto binaryData = createTestBinaryData(1000);

    logStep("Storing binary data with 60 second TTL...");
    conn->setString(key, vectorToString(binaryData), 60);
    logOk("Data stored with TTL");

    logStep("Checking TTL...");
    int64_t ttl = conn->getTTL(key);
    logData("TTL: " + std::to_string(ttl) + " seconds");

    logStep("Verifying data integrity...");
    auto retrieved = stringToVector(conn->getString(key));
    if (verifyBinaryData(binaryData, retrieved))
    {
        logOk("Data verified (" + std::to_string(retrieved.size()) + " bytes)");
    }
    else
    {
        logError("Data verification failed!");
    }

    // Cleanup
    conn->deleteKey(key);
}
#endif

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc Redis Binary Data Example");
    log("========================================");
    log("");

#if !USE_REDIS
    logError("Redis support is not enabled");
    logInfo("Build with -DUSE_REDIS=ON to enable Redis support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,redis");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("24_061_example_redis_blob", "redis");
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

    logStep("Getting Redis database configuration...");
    auto dbResult = getDbConfig(configManager, args.dbName, "redis");

    if (!dbResult)
    {
        logError("Failed to get database config: " + dbResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!dbResult.value())
    {
        logError("Redis configuration not found");
        return EXIT_ERROR_;
    }

    auto &dbConfig = *dbResult.value();
    logOk("Using database: " + dbConfig.getName() +
          " (" + dbConfig.getType() + "://" +
          dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) + ")");

    logStep("Registering Redis driver...");
    registerDriver("redis");
    logOk("Driver registered");

    try
    {
        logStep("Connecting to Redis...");

        auto driver = std::make_shared<cpp_dbc::Redis::RedisDriver>();
        std::string url = "cpp_dbc:redis://" + dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()) + "/" + dbConfig.getDatabase();
        auto conn = driver->connectKV(url, dbConfig.getUsername(), dbConfig.getPassword());

        logOk("Connected to Redis");

        demonstrateBinaryStorage(conn);
        demonstrateBinaryWithNullBytes(conn);
        demonstrateBinaryInHash(conn);
        demonstrateBinaryWithExpiration(conn);

        log("");
        logStep("Closing connection...");
        conn->close();
        logOk("Connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Database error: " + e.what_s());
        return EXIT_ERROR_;
    }
    catch (const std::exception &e)
    {
        logError("Error: " + std::string(e.what()));
        return EXIT_ERROR_;
    }

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return EXIT_OK_;
#endif
}
