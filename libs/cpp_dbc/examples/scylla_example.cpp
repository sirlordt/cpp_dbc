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
 * @file scylla_example.cpp
 * @brief Example demonstrating basic ScyllaDB operations
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
#include <cmath>

#if USE_SCYLLADB
/**
 * @brief Perform a sequence of basic ScyllaDB operations using the provided connection.
 *
 * Executes a demonstration workflow against ScyllaDB that creates a keyspace and table,
 * inserts multiple rows via prepared statements, selects and prints a single row,
 * updates and verifies that row, selects and prints all rows, deletes a row and verifies
 * the remaining count, then drops the table as cleanup.
 *
 * @param conn Shared pointer to an active cpp_dbc::ColumnarDBConnection used to run the demo operations; must not be null.
 *
 * Note: Database errors of type `cpp_dbc::DBException` are caught inside the function and reported to stderr.
 */
void demonstrateScyllaDB(std::shared_ptr<cpp_dbc::ColumnarDBConnection> conn)
{
    std::cout << "\n=== ScyllaDB Basic Operations ===\n"
              << std::endl;

    try
    {
        // Define keyspace and table
        std::string keyspace = "test_keyspace";
        std::string table = keyspace + ".example_table";

        // Create keyspace if not exists
        std::cout << "Creating keyspace if not exists..." << std::endl;
        conn->executeUpdate(
            "CREATE KEYSPACE IF NOT EXISTS " + keyspace + " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");

        // Create table
        std::cout << "Creating table..." << std::endl;
        conn->executeUpdate("DROP TABLE IF EXISTS " + table);
        conn->executeUpdate(
            "CREATE TABLE " + table + " ("
                                      "id int PRIMARY KEY, "
                                      "name text, "
                                      "value double"
                                      ")");

        std::cout << "Table created successfully." << std::endl;

        // Insert data using prepared statements
        std::cout << "Inserting data..." << std::endl;
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + table + " (id, name, value) VALUES (?, ?, ?)");

        for (int i = 1; i <= 5; ++i)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Item " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->executeUpdate();
        }
        std::cout << "Data inserted successfully." << std::endl;

        // Select specific row
        std::cout << "Selecting row with id = 3..." << std::endl;
        auto selectStmt = conn->prepareStatement("SELECT * FROM " + table + " WHERE id = ?");
        selectStmt->setInt(1, 3);
        auto rs = selectStmt->executeQuery();

        if (rs->next())
        {
            std::cout << "Found: ID=" << rs->getInt("id")
                      << ", Name=" << rs->getString("name")
                      << ", Value=" << rs->getDouble("value") << std::endl;
        }
        else
        {
            std::cout << "Row not found!" << std::endl;
        }

        // Update data
        std::cout << "Updating row with id = 3..." << std::endl;
        auto updateStmt = conn->prepareStatement("UPDATE " + table + " SET name = ? WHERE id = ?");
        updateStmt->setString(1, "Updated Item 3");
        updateStmt->setInt(2, 3);
        updateStmt->executeUpdate();

        // Verify update
        selectStmt->setInt(1, 3);
        rs = selectStmt->executeQuery();
        if (rs->next())
        {
            std::cout << "Updated: ID=" << rs->getInt("id")
                      << ", Name=" << rs->getString("name") << std::endl;
        }

        // Select all rows
        std::cout << "Selecting all rows..." << std::endl;
        rs = conn->executeQuery("SELECT * FROM " + table);
        while (rs->next())
        {
            std::cout << "Row: ID=" << rs->getInt("id")
                      << ", Name=" << rs->getString("name")
                      << ", Value=" << rs->getDouble("value") << std::endl;
        }

        // Delete data
        std::cout << "Deleting row with id = 5..." << std::endl;
        conn->executeUpdate("DELETE FROM " + table + " WHERE id = 5");

        // Verify delete (using count)
        rs = conn->executeQuery("SELECT COUNT(*) as count FROM " + table);
        if (rs->next())
        {
            // Note: In Scylla/Cassandra count is returned as a 64-bit integer (long)
            std::cout << "Remaining rows: " << rs->getLong("count") << std::endl;
        }

        // Clean up
        conn->executeUpdate("DROP TABLE " + table);
        std::cout << "Table dropped successfully." << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "ScyllaDB operation error: " << e.what_s() << std::endl;
    }
}
#endif

/**
 * @brief Program entry point that runs the ScyllaDB demonstration when built with ScyllaDB support.
 *
 * If the binary is compiled with ScyllaDB enabled, this function registers the ScyllaDB driver,
 * establishes a connection using configured credentials, invokes the demonstration routine, and
 * closes the connection. If ScyllaDB support is not enabled at build time, it prints an informational message.
 * Observable errors are reported to stderr.
 *
 * @return int `0` on success, `1` if an exception is thrown during initialization or execution.
 */
int main()
{
    try
    {
#if USE_SCYLLADB
        // Register ScyllaDB driver
        cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>());

        std::cout << "Connecting to ScyllaDB..." << std::endl;
        // Connection string format: cpp_dbc:scylladb://host:port/keyspace
        // Using values from example_config.yml
        std::string connStr = "cpp_dbc:scylladb://localhost:9042/test_keyspace";
        std::string username = "cassandra";
        std::string password = "dsystems";

        auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(
            cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

        demonstrateScyllaDB(conn);

        conn->close();
        std::cout << "Connection closed." << std::endl;
#else
        std::cout << "ScyllaDB support is not enabled." << std::endl;
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