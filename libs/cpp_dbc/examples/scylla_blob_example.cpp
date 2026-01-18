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
 * @file scylla_blob_example.cpp
 * @brief Example demonstrating ScyllaDB BLOB operations
 */

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/core/columnar/columnar_db_connection.hpp>
#if USE_SCYLLADB
#include <cpp_dbc/drivers/columnar/driver_scylladb.hpp>
#endif
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <iomanip>

#if USE_SCYLLADB

/**
 * @brief Prints binary data as a hexadecimal dump.
 *
 * Outputs each byte of `data` as a two-digit hex value separated by spaces, inserts a newline after every 16 bytes, and restores numeric formatting to decimal at the end.
 *
 * @param data Binary data to print as hex bytes.
 */
void printHex(const std::vector<uint8_t> &data)
{
    for (size_t i = 0; i < data.size(); ++i)
    {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
        if ((i + 1) % 16 == 0)
            std::cout << std::endl;
    }
    std::cout << std::dec << std::endl;
}

/**
 * @brief Demonstrates inserting, retrieving, and verifying a BLOB in ScyllaDB.
 *
 * Sets up a test keyspace and table, inserts a sample binary vector as a BLOB,
 * reads the row back, compares the retrieved bytes with the original, and
 * cleans up by dropping the table. Progress, the hex dump of the data, and
 * verification results are written to standard output; errors are reported to
 * standard error.
 */
void demonstrateScyllaDBBlob(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    std::cout << "\n=== ScyllaDB BLOB Operations ===\n"
              << std::endl;

    try
    {
        std::string keyspace = "test_keyspace";
        std::string table = keyspace + ".blob_example";

        // Setup
        conn->executeUpdate("CREATE KEYSPACE IF NOT EXISTS " + keyspace + " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
        conn->executeUpdate("DROP TABLE IF EXISTS " + table);
        conn->executeUpdate(
            "CREATE TABLE " + table + " ("
                                      "id int PRIMARY KEY, "
                                      "description text, "
                                      "data blob"
                                      ")");

        std::cout << "Table created." << std::endl;

        // Create some binary data
        std::vector<uint8_t> binaryData = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
        for (int i = 0; i < 10; ++i)
            binaryData.push_back(static_cast<uint8_t>(i));

        std::cout << "Original Data (" << binaryData.size() << " bytes):" << std::endl;
        printHex(binaryData);

        // Insert BLOB
        auto pstmt = conn->prepareStatement("INSERT INTO " + table + " (id, description, data) VALUES (?, ?, ?)");
        pstmt->setInt(1, 1);
        pstmt->setString(2, "Test Blob 1");
        pstmt->setBytes(3, binaryData);
        pstmt->executeUpdate();
        std::cout << "BLOB inserted." << std::endl;

        // Retrieve BLOB
        auto rs = conn->executeQuery("SELECT * FROM " + table + " WHERE id = 1");
        if (rs->next())
        {
            std::cout << "Retrieved row: " << rs->getString("description") << std::endl;
            std::vector<uint8_t> retrievedData = rs->getBytes("data");

            std::cout << "Retrieved Data (" << retrievedData.size() << " bytes):" << std::endl;
            printHex(retrievedData);

            // Verify
            if (binaryData == retrievedData)
            {
                std::cout << "SUCCESS: Retrieved data matches original data." << std::endl;
            }
            else
            {
                std::cout << "FAILURE: Data mismatch!" << std::endl;
            }
        }

        // Cleanup
        conn->executeUpdate("DROP TABLE " + table);
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "ScyllaDB BLOB operation error: " << e.what_s() << std::endl;
    }
}
#endif

/**
 * @brief Program entry point that demonstrates ScyllaDB BLOB operations when enabled.
 *
 * When compiled with USE_SCYLLADB defined, registers the ScyllaDB driver, connects to a
 * ScyllaDB instance using a fixed connection string and credentials, runs the BLOB demonstration,
 * and closes the connection. When USE_SCYLLADB is not defined, prints a message indicating that
 * ScyllaDB support is not enabled.
 *
 * @return int 0 on success, 1 if an exception is caught.
 */
int main()
{
    try
    {
#if USE_SCYLLADB
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

        std::cout << "Connecting to ScyllaDB..." << std::endl;
        std::string connStr = "cpp_dbc:scylladb://localhost:9042/test_keyspace";
        std::string username = "cassandra";
        std::string password = "dsystems";

        auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

        demonstrateScyllaDBBlob(conn);

        conn->close();
#else
        std::cout << "ScyllaDB support is not enabled." << std::endl;
#endif
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}