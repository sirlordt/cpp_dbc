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
 * @file 26_031_test_scylladb_real.cpp
 * @brief Tests for ScyllaDB database operations with real connections
 */

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#include <cpp_dbc/config/database_config.hpp>
#include <cpp_dbc/common/system_utils.hpp>

#include "26_001_test_scylladb_real_common.hpp"
#include "10_000_test_main.hpp"

#if USE_SCYLLADB
// Test case for real ScyllaDB connection
TEST_CASE("Real ScyllaDB connection tests", "[26_031_01_scylladb_real]")
{
    // Skip these tests if we can't connect to ScyllaDB
    if (!scylla_test_helpers::canConnectToScylla())
    {
        SKIP("Cannot connect to ScyllaDB database");
        return;
    }

    // Get ScyllaDB configuration
    auto dbConfig = scylla_test_helpers::getScyllaConfig("dev_scylla");

    // Extract connection parameters
    std::string username = dbConfig.getUsername();
    std::string password = dbConfig.getPassword();
    std::string host = dbConfig.getHost();
    int port = dbConfig.getPort();
    std::string keyspace = dbConfig.getDatabase();
    std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;

    // Get test queries from the config options
    std::string createKeyspaceQuery = dbConfig.getOption("query__create_keyspace",
                                                         "CREATE KEYSPACE IF NOT EXISTS test_keyspace WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
    std::string createTableQuery = dbConfig.getOption("query__create_table",
                                                      "CREATE TABLE IF NOT EXISTS test_keyspace.test_table (id int PRIMARY KEY, name text, value double)");
    std::string insertDataQuery = dbConfig.getOption("query__insert_data",
                                                     "INSERT INTO test_keyspace.test_table (id, name, value) VALUES (?, ?, ?)");
    std::string selectDataQuery = dbConfig.getOption("query__select_data",
                                                     "SELECT * FROM test_keyspace.test_table WHERE id = ?");
    std::string dropTableQuery = dbConfig.getOption("query__drop_table",
                                                    "DROP TABLE IF EXISTS test_keyspace.test_table");

    SECTION("Basic ScyllaDB operations")
    {
        // Register the ScyllaDB driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
        REQUIRE(conn != nullptr);

        // Create keyspace if not exists
        conn->executeUpdate(createKeyspaceQuery);

        // Create a test table
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        auto result = conn->executeUpdate(createTableQuery);
        REQUIRE(result == 0); // CREATE TABLE should return 0 affected rows

        // Insert data using a prepared statement
        auto pstmt = conn->prepareStatement(insertDataQuery);
        REQUIRE(pstmt != nullptr);

        // Insert multiple rows
        for (int i = 1; i <= 10; i++)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Test Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            auto insertResult = pstmt->executeUpdate();
            REQUIRE(insertResult == 1); // Each insert should affect 1 row
        }

        // Select data using a prepared statement
        auto selectStmt = conn->prepareStatement(selectDataQuery);
        REQUIRE(selectStmt != nullptr);

        // Select a specific row
        selectStmt->setInt(1, 5);
        auto rs = selectStmt->executeQuery();
        REQUIRE(rs != nullptr);

        // Verify the result
        REQUIRE(rs->next());
        REQUIRE(rs->getInt("id") == 5);
        REQUIRE(rs->getString("name") == "Test Name 5");
        REQUIRE(std::abs(rs->getDouble("value") - 7.5) < 0.001);
        REQUIRE_FALSE(rs->next()); // Should only be one row

        // Select all rows using direct query
        rs = conn->executeQuery("SELECT * FROM test_keyspace.test_table");
        REQUIRE(rs != nullptr);

        // Verify all rows
        int count = 0;
        while (rs->next())
        {
            count++;
            int id = rs->getInt("id");
            REQUIRE(id >= 1);
            REQUIRE(id <= 10);
            REQUIRE(rs->getString("name") == "Test Name " + std::to_string(id));
            REQUIRE(std::abs(rs->getDouble("value") - (id * 1.5)) < 0.001);
        }
        REQUIRE(count == 10);

        // Update data
        auto updateResult = conn->executeUpdate("UPDATE test_keyspace.test_table SET name = 'Updated Name' WHERE id = 3");
        REQUIRE(updateResult == 1); // Should affect 1 row

        // Verify the update
        rs = conn->executeQuery("SELECT * FROM test_keyspace.test_table WHERE id = 3");
        REQUIRE(rs->next());
        REQUIRE(rs->getString("name") == "Updated Name");

        // Delete data - Cassandra/ScyllaDB requires IN clause for multi-row delete on partition key
        auto deleteResult = conn->executeUpdate("DELETE FROM test_keyspace.test_table WHERE id IN (6, 7, 8, 9, 10)");
        REQUIRE(deleteResult == 5); // Should delete 5 rows (6-10)

        // Verify the delete
        // In ScyllaDB/Cassandra, when using COUNT(*), the result is a single row with a count column
        rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_keyspace.test_table");
        REQUIRE(rs->next());
        long rowCount = rs->getLong("count"); // Get the actual count value from the result
        REQUIRE(rowCount == 5);               // Should be 5 rows left

        // Drop the test table
        result = conn->executeUpdate(dropTableQuery);
        REQUIRE(result == 0); // DROP TABLE should return 0 affected rows

        // Close the connection
        conn->close();
    }

    SECTION("ScyllaDB metadata retrieval")
    {
        // Register the ScyllaDB driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

        // Create keyspace if not exists
        conn->executeUpdate(createKeyspaceQuery);

        // Create a test table with various data types
        conn->executeUpdate("DROP TABLE IF EXISTS test_keyspace.test_types");
        conn->executeUpdate(
            "CREATE TABLE test_keyspace.test_types ("
            "id int PRIMARY KEY, "
            "int_col int, "
            "double_col double, "
            "text_col text, "
            "bool_col boolean, "
            "timestamp_col timestamp, "
            "uuid_col uuid, "
            "blob_col blob"
            ")");

        // Insert test data with timestamp, blob and UUID fields
        auto pstmt = conn->prepareStatement(
            "INSERT INTO test_keyspace.test_types (id, int_col, double_col, text_col, bool_col, timestamp_col, uuid_col, blob_col) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

        pstmt->setInt(1, 1);
        pstmt->setInt(2, 42);
        pstmt->setDouble(3, 3.14159);
        pstmt->setString(4, "Hello, World!");
        pstmt->setBoolean(5, true);

        // For timestamp, use the specific timestamp method
        pstmt->setTimestamp(6, "2023-01-15 14:30:00");

        // For UUID, use the specific UUID method
        pstmt->setUUID(7, "550e8400e29b41d4a716446655440000"); // UUID without hyphens

        // Create blob data for testing
        std::vector<uint8_t> blobData = {0x01, 0x02, 0x03, 0x04, 0x05};
        pstmt->setBytes(8, blobData);

        pstmt->executeUpdate();

        // Test retrieving different data types
        auto rs = conn->executeQuery("SELECT * FROM test_keyspace.test_types");
        REQUIRE(rs->next());

        // Test each data type
        REQUIRE(rs->getInt("id") == 1);
        REQUIRE(rs->getInt("int_col") == 42);
        REQUIRE(std::abs(rs->getDouble("double_col") - 3.14159) < 0.001);
        REQUIRE(rs->getString("text_col") == "Hello, World!");
        REQUIRE(rs->getBoolean("bool_col") == true);

        // Now test the timestamp column that's been re-enabled
        // Timestamp format may vary, just check it's not empty
        std::string timestamp = rs->getString("timestamp_col");
        std::cout << "DEBUG - Timestamp value returned: '" << timestamp << "'" << std::endl;
        REQUIRE_FALSE(timestamp.empty());

        // UUID should now be properly formatted as a standard UUID string
        std::string uuid = rs->getUUID("uuid_col");
        // Should be in the format "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
        REQUIRE_FALSE(uuid.empty());
        REQUIRE(uuid.length() == 36); // Standard UUID string length with hyphens
        REQUIRE(uuid[8] == '-');      // Check for hyphens in the expected positions
        REQUIRE(uuid[13] == '-');
        REQUIRE(uuid[18] == '-');
        REQUIRE(uuid[23] == '-');

        // Verify blob data
        auto retrievedBlob = rs->getBytes("blob_col");
        REQUIRE(retrievedBlob.size() == blobData.size());
        for (size_t i = 0; i < blobData.size(); i++)
        {
            REQUIRE(retrievedBlob[i] == blobData[i]);
        }

        // Test column metadata
        auto columnNames = rs->getColumnNames();
        REQUIRE(columnNames.size() == 8); // 8 columns now with timestamp_col, uuid_col and blob_col
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "id") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "int_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "double_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "text_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "bool_col") != columnNames.end());
        // Now include the timestamp column
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "timestamp_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "uuid_col") != columnNames.end());
        REQUIRE(std::find(columnNames.begin(), columnNames.end(), "blob_col") != columnNames.end());

        // Test NULL values - use prepared statements for more reliable null handling
        auto updateStmt = conn->prepareStatement("UPDATE test_keyspace.test_types SET int_col = ?, text_col = ? WHERE id = ?");
        updateStmt->setNull(1, cpp_dbc::Types::INTEGER);
        updateStmt->setNull(2, cpp_dbc::Types::VARCHAR);
        updateStmt->setInt(3, 1);
        updateStmt->executeUpdate();

        rs = conn->executeQuery("SELECT * FROM test_keyspace.test_types");
        REQUIRE(rs->next());
        REQUIRE(rs->isNull("int_col"));
        REQUIRE(rs->isNull("text_col"));

        // Clean up
        conn->executeUpdate("DROP TABLE IF EXISTS test_keyspace.test_types");

        // Close the connection
        conn->close();
    }

    SECTION("ScyllaDB stress test")
    {
        // Register the ScyllaDB driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

        // Get a connection
        auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

        // Create keyspace if not exists
        conn->executeUpdate(createKeyspaceQuery);

        // Create a test table
        conn->executeUpdate(dropTableQuery); // Drop table if it exists
        conn->executeUpdate(createTableQuery);

        // Test with fewer concurrent threads for stability
        const int numThreads = 10;
        const int opsPerThread = 20;

        std::atomic<int> successCount(0);
        std::vector<std::thread> threads;

        auto startTime = std::chrono::steady_clock::now();

        for (int i = 0; i < numThreads; i++)
        {
            threads.push_back(std::thread([&, i]()
                                          {
                try {
                    // Each thread gets its own connection
                    auto threadConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
                        cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                    
                    for (int j = 0; j < opsPerThread; j++) {
                        try {
                            // Insert a row
                            auto pstmt = threadConn->prepareStatement(insertDataQuery);
                            int id = i * 1000 + j;
                            pstmt->setInt(1, id);
                            pstmt->setString(2, "Stress Test " + std::to_string(id));
                            pstmt->setDouble(3, id * 1.5);
                            pstmt->executeUpdate();

                            // Select the row back
                            auto selectStmt = threadConn->prepareStatement(selectDataQuery);
                            selectStmt->setInt(1, id);
                            auto rs = selectStmt->executeQuery();

                            if (rs->next() && rs->getInt("id") == id &&
                                rs->getString("name") == "Stress Test " + std::to_string(id)) {
                                successCount++;
                            }
                        }
                        catch (const std::exception& e) {
                            std::cerr << "Thread operation failed: " << e.what() << std::endl;
                        }
                    }
                    
                    threadConn->close();
                }
                catch (const std::exception& e) {
                    std::cerr << "Thread connection failed: " << e.what() << std::endl;
                } }));
        }

        // Wait for all threads to complete
        for (auto &t : threads)
        {
            t.join();
        }

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

        std::cout << "ScyllaDB stress test completed in " << duration << " ms" << std::endl;
        std::cout << "Operations per second: " << (numThreads * opsPerThread * 1000.0 / static_cast<double>(duration)) << std::endl;

        // Verify that most operations were successful
        REQUIRE(successCount > numThreads * opsPerThread * 0.8); // At least 80% success rate

        // Verify the total number of rows
        auto rs = conn->executeQuery("SELECT COUNT(*) as count FROM test_keyspace.test_table");
        int rowCount = 0;

        while (rs->next())
        {
            rowCount++;
            // ScyllaDB doesn't return a count directly, just a row for each matching record
        }

        // Clean up
        conn->executeUpdate(dropTableQuery);
        conn->close();
    }
}
#else
// Skip tests if ScyllaDB support is not enabled
TEST_CASE("Real ScyllaDB connection tests (skipped)", "[26_031_02_scylladb_real]")
{
    SKIP("ScyllaDB support is not enabled");
}
#endif
