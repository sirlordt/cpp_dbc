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
 * @file scylla_json_example.cpp
 * @brief Example demonstrating ScyllaDB JSON operations (storing JSON as text)
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

#if USE_SCYLLADB
/**
 * @brief Demonstrates storing and retrieving JSON payloads in ScyllaDB using the provided connection.
 *
 * Creates a keyspace and temporary table, inserts three example JSON values (simple object, array, nested object),
 * selects and prints all rows, and then drops the table. Database errors are caught and reported to stderr.
 */
void demonstrateScyllaDBJson(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    std::cout << "\n=== ScyllaDB JSON Operations ===\n"
              << std::endl;

    try
    {
        std::string keyspace = "test_keyspace";
        std::string table = keyspace + ".json_example";

        // Setup
        conn->executeUpdate("CREATE KEYSPACE IF NOT EXISTS " + keyspace + " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
        conn->executeUpdate("DROP TABLE IF EXISTS " + table);
        conn->executeUpdate(
            "CREATE TABLE " + table + " ("
                                      "id int PRIMARY KEY, "
                                      "json_data text"
                                      ")");

        std::cout << "Table created." << std::endl;

        // Insert JSON data
        auto pstmt = conn->prepareStatement("INSERT INTO " + table + " (id, json_data) VALUES (?, ?)");

        // Simple JSON object
        std::string json1 = "{\"name\": \"John\", \"age\": 30, \"city\": \"New York\"}";
        pstmt->setInt(1, 1);
        pstmt->setString(2, json1);
        pstmt->executeUpdate();
        std::cout << "Inserted JSON 1: " << json1 << std::endl;

        // JSON array
        std::string json2 = "[1, 2, 3, 4, 5]";
        pstmt->setInt(1, 2);
        pstmt->setString(2, json2);
        pstmt->executeUpdate();
        std::cout << "Inserted JSON 2: " << json2 << std::endl;

        // Nested JSON
        std::string json3 = "{\"person\": {\"name\": \"Alice\", \"address\": {\"city\": \"Wonderland\"}}}";
        pstmt->setInt(1, 3);
        pstmt->setString(2, json3);
        pstmt->executeUpdate();
        std::cout << "Inserted JSON 3: " << json3 << std::endl;

        // Retrieve and Verify
        std::cout << "\nRetrieving data..." << std::endl;
        auto rs = conn->executeQuery("SELECT * FROM " + table);
        while (rs->next())
        {
            std::cout << "ID: " << rs->getInt("id") << "\n"
                      << "JSON: " << rs->getString("json_data") << "\n"
                      << std::endl;
        }

        // Cleanup
        conn->executeUpdate("DROP TABLE " + table);
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "ScyllaDB JSON operation error: " << e.what_s() << std::endl;
    }
}
#endif

/**
 * @brief Program entry point that runs the ScyllaDB JSON demonstration when available.
 *
 * When compiled with ScyllaDB support, registers the ScyllaDB driver, establishes a
 * connection using configured credentials, invokes the JSON demonstration routine,
 * and closes the connection. If ScyllaDB support is not enabled, prints a message
 * indicating that the feature is unavailable.
 *
 * @return int 0 on success, 1 if an exception is thrown during execution.
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

        demonstrateScyllaDBJson(conn);

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