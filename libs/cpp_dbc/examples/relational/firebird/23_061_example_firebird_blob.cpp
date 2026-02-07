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
 * @file 23_061_example_firebird_blob.cpp
 * @brief Firebird-specific example demonstrating BLOB operations
 *
 * This example demonstrates:
 * - Creating tables with BLOB columns
 * - Inserting binary data into BLOB columns
 * - Reading binary data from BLOB columns
 * - BLOB sub_type differences (TEXT vs BINARY)
 *
 * Usage:
 *   ./23_061_example_firebird_blob [--config=<path>] [--db=<name>] [--help]
 *
 * Exit codes:
 *   0   - Success
 *   1   - Runtime error
 *   100 - Firebird support not enabled at compile time
 */

#include "../../common/example_common.hpp"
#include <iomanip>
#include <sstream>
#include <vector>
#include <cstring>

#if USE_FIREBIRD
#include <cpp_dbc/drivers/relational/driver_firebird.hpp>
#endif

using namespace cpp_dbc::examples;

#if USE_FIREBIRD
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

/**
 * @brief Demonstrates BLOB operations in Firebird
 *
 * IMPORTANT FIREBIRD-SPECIFIC RESOURCE MANAGEMENT:
 * =================================================
 * This example shows EXPLICIT closing of ResultSet and PreparedStatement objects.
 *
 * WHY THIS MATTERS FOR FIREBIRD:
 * -------------------------------
 * Firebird maintains active METADATA LOCKS on tables as long as there are open:
 * - ResultSet objects (even after next() returns false)
 * - PreparedStatement objects (even after executeUpdate() completes)
 *
 * PROBLEM WITHOUT EXPLICIT CLOSE:
 * --------------------------------
 * If you don't explicitly close these objects before DROP TABLE, you'll get:
 *   "SQLCODE -607: unsuccessful metadata update - object TABLE is in use"
 *
 * This happens because Firebird's transaction isolation prevents DDL operations
 * while there are active references to the table, even if you've finished reading
 * or modifying data.
 *
 * PATTERN USED IN THIS EXAMPLE:
 * -----------------------------
 * 1. Create PreparedStatement → Use it → CLOSE IT (pstmt->close())
 * 2. Execute Query → Process ResultSet → CLOSE IT (rs->close())
 * 3. Only after ALL objects are closed → DROP TABLE succeeds
 *
 * This is a Firebird-specific requirement. Other databases may auto-close via
 * shared_ptr destructors, but Firebird requires explicit cleanup for DDL safety.
 */
void demonstrateBlobOperations(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    log("");
    log("=== Firebird BLOB Operations ===");
    log("");

    try
    {
        // ===== Create Tables =====
        logStep("Creating tables with BLOB columns...");
        conn->executeUpdate(
            "RECREATE TABLE blob_test ("
            "id INTEGER NOT NULL PRIMARY KEY, "
            "name VARCHAR(100) NOT NULL, "
            "binary_data BLOB SUB_TYPE 0, "       // Binary BLOB
            "text_data BLOB SUB_TYPE TEXT, "      // Text BLOB
            "file_size INTEGER"
            ")");
        logOk("Table created");

        // ===== Insert Binary Data =====
        log("");
        log("--- Insert BLOB Data ---");

        // Create test data of various sizes
        auto smallData = createTestBinaryData(100);
        auto mediumData = createTestBinaryData(10000);
        auto largeData = createTestBinaryData(100000);

        logStep("Inserting small binary data (100 bytes)...");
        auto pstmt = conn->prepareStatement(
            "INSERT INTO blob_test (id, name, binary_data, text_data, file_size) "
            "VALUES (?, ?, ?, ?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setString(2, "Small File");
        pstmt->setBytes(3, smallData);
        pstmt->setString(4, "This is text content stored in a TEXT BLOB");
        pstmt->setInt(5, static_cast<int>(smallData.size()));
        pstmt->executeUpdate();
        logOk("Small data inserted");

        logStep("Inserting medium binary data (10 KB)...");
        pstmt->setInt(1, 2);
        pstmt->setString(2, "Medium File");
        pstmt->setBytes(3, mediumData);
        pstmt->setString(4, "Medium sized text content for testing BLOB operations in Firebird database");
        pstmt->setInt(5, static_cast<int>(mediumData.size()));
        pstmt->executeUpdate();
        logOk("Medium data inserted");

        logStep("Inserting large binary data (100 KB)...");
        pstmt->setInt(1, 3);
        pstmt->setString(2, "Large File");
        pstmt->setBytes(3, largeData);
        pstmt->setString(4, "Large text content that demonstrates Firebird's capability to handle substantial amounts of text data in BLOB columns");
        pstmt->setInt(5, static_cast<int>(largeData.size()));
        pstmt->executeUpdate();
        logOk("Large data inserted");
        pstmt->close();  // Close prepared statement

        // ===== Query BLOB Data =====
        log("");
        log("--- Query BLOB Data ---");

        logStep("Querying blob metadata...");
        auto rs = conn->executeQuery(
            "SELECT id, name, file_size "
            "FROM blob_test ORDER BY id");

        while (rs->next())
        {
            logData("ID: " + std::to_string(rs->getInt("ID")) +
                    ", Name: " + rs->getString("NAME") +
                    ", File Size: " + std::to_string(rs->getInt("FILE_SIZE")));
        }
        rs->close();  // Close result set

        // ===== Retrieve and Verify Binary Data =====
        log("");
        log("--- Retrieve and Verify BLOB Data ---");

        logStep("Retrieving and verifying small data...");
        rs = conn->executeQuery("SELECT binary_data FROM blob_test WHERE id = 1");
        if (rs->next())
        {
            auto retrieved = rs->getBytes("BINARY_DATA");
            if (verifyBinaryData(smallData, retrieved))
            {
                logOk("Small data verified successfully (" + std::to_string(retrieved.size()) + " bytes)");
            }
            else
            {
                logError("Small data verification failed!");
            }
        }
        rs->close();  // Close result set

        logStep("Retrieving and verifying medium data...");
        rs = conn->executeQuery("SELECT binary_data FROM blob_test WHERE id = 2");
        if (rs->next())
        {
            auto retrieved = rs->getBytes("BINARY_DATA");
            if (verifyBinaryData(mediumData, retrieved))
            {
                logOk("Medium data verified successfully (" + std::to_string(retrieved.size()) + " bytes)");
            }
            else
            {
                logError("Medium data verification failed!");
            }
        }
        rs->close();  // Close result set

        logStep("Retrieving and verifying large data...");
        rs = conn->executeQuery("SELECT binary_data FROM blob_test WHERE id = 3");
        if (rs->next())
        {
            auto retrieved = rs->getBytes("BINARY_DATA");
            if (verifyBinaryData(largeData, retrieved))
            {
                logOk("Large data verified successfully (" + std::to_string(retrieved.size()) + " bytes)");
            }
            else
            {
                logError("Large data verification failed!");
            }
        }
        rs->close();  // Close result set

        // ===== Text BLOB Operations =====
        log("");
        log("--- Text BLOB Operations ---");

        logStep("Retrieving text BLOB data...");
        rs = conn->executeQuery("SELECT id, name, text_data FROM blob_test ORDER BY id");
        while (rs->next())
        {
            std::string textData = rs->getString("TEXT_DATA");
            logData("ID " + std::to_string(rs->getInt("ID")) +
                    " text (first 50 chars): " +
                    (textData.length() > 50 ? textData.substr(0, 50) + "..." : textData));
        }
        rs->close();  // Close result set

        // ===== Update BLOB Data =====
        log("");
        log("--- Update BLOB Data ---");

        auto updatedData = createTestBinaryData(500);
        logStep("Updating BLOB data for ID 1...");
        pstmt = conn->prepareStatement(
            "UPDATE blob_test SET binary_data = ?, file_size = ? WHERE id = ?");
        pstmt->setBytes(1, updatedData);
        pstmt->setInt(2, static_cast<int>(updatedData.size()));
        pstmt->setInt(3, 1);
        pstmt->executeUpdate();
        logOk("BLOB data updated");
        pstmt->close();  // Close prepared statement

        logStep("Verifying updated data...");
        rs = conn->executeQuery("SELECT binary_data FROM blob_test WHERE id = 1");
        if (rs->next())
        {
            auto retrieved = rs->getBytes("BINARY_DATA");
            if (verifyBinaryData(updatedData, retrieved))
            {
                logOk("Updated data verified successfully (" + std::to_string(retrieved.size()) + " bytes)");
            }
            else
            {
                logError("Updated data verification failed!");
            }
        }
        rs->close();  // Close result set

        // ===== NULL BLOB Handling =====
        log("");
        log("--- NULL BLOB Handling ---");

        logStep("Inserting row with NULL BLOB...");
        conn->executeUpdate(
            "INSERT INTO blob_test (id, name, binary_data, text_data, file_size) "
            "VALUES (4, 'Empty File', NULL, NULL, 0)");
        logOk("Row with NULL BLOB inserted");

        logStep("Querying NULL BLOB...");
        rs = conn->executeQuery("SELECT id, name, binary_data FROM blob_test WHERE id = 4");
        if (rs->next())
        {
            if (rs->isNull("BINARY_DATA"))
            {
                logOk("NULL BLOB correctly detected");
            }
            else
            {
                logError("NULL BLOB not detected!");
            }
        }
        rs->close();  // Close result set

        // ===== Cleanup =====
        log("");
        logStep("Cleaning up...");
        conn->executeUpdate("DROP TABLE blob_test");
        logOk("Table dropped");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Firebird BLOB operation error: " + e.what_s());
        throw;
    }
}
#endif

int main(int argc, char *argv[])
{
    log("========================================");
    log("cpp_dbc Firebird BLOB Operations Example");
    log("========================================");
    log("");

#if !USE_FIREBIRD
    logError("Firebird support is not enabled");
    logInfo("Build with -DUSE_FIREBIRD=ON to enable Firebird support");
    logInfo("Or use: ./helper.sh --run-build=rebuild,firebird");
    return EXIT_DRIVER_NOT_ENABLED_;
#else

    logStep("Parsing command line arguments...");
    auto args = parseArgs(argc, argv);

    if (args.showHelp)
    {
        printHelp("23_061_example_firebird_blob", "firebird");
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

    logStep("Getting Firebird database configuration...");
    auto firebirdResult = getDbConfig(configManager, args.dbName.empty() ? "" : args.dbName, "firebird");

    if (!firebirdResult)
    {
        logError("Failed to get Firebird config: " + firebirdResult.error().what_s());
        return EXIT_ERROR_;
    }

    if (!firebirdResult.value())
    {
        logError("Firebird configuration not found");
        return EXIT_ERROR_;
    }

    auto &firebirdConfig = *firebirdResult.value();
    logOk("Using database: " + firebirdConfig.getName() +
          " (" + firebirdConfig.getType() + "://" +
          firebirdConfig.getHost() + ":" + std::to_string(firebirdConfig.getPort()) +
          "/" + firebirdConfig.getDatabase() + ")");

    logStep("Registering Firebird driver...");
    registerDriver("firebird");
    logOk("Driver registered");

    try
    {
        logStep("Connecting to Firebird...");
        auto firebirdConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(
            firebirdConfig.createDBConnection());

        logOk("Connected to Firebird");

        demonstrateBlobOperations(firebirdConn);

        logStep("Closing Firebird connection...");
        firebirdConn->close();
        logOk("Firebird connection closed");
    }
    catch (const cpp_dbc::DBException &e)
    {
        logError("Firebird error: " + e.what_s());
        return EXIT_ERROR_;
    }

    log("");
    log("========================================");
    logOk("Example completed successfully");
    log("========================================");

    return EXIT_OK_;
#endif
}
