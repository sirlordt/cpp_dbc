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

 @file json_operations_example.cpp
 @brief Example demonstrating JSON operations with MySQL and PostgreSQL

*/

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <iomanip>

// Helper function to print JSON query results
void printJsonResults(std::shared_ptr<cpp_dbc::RelationalDBResultSet> rs)
{
    // Get column names
    auto columnNames = rs->getColumnNames();

    // Print header
    for (const auto &column : columnNames)
    {
        std::cout << std::setw(20) << std::left << column;
    }
    std::cout << std::endl;

    // Print separator
    for (size_t i = 0; i < columnNames.size(); ++i)
    {
        std::cout << std::string(20, '-');
    }
    std::cout << std::endl;

    // Print data
    while (rs->next())
    {
        for (const auto &column : columnNames)
        {
            std::cout << std::setw(20) << std::left << rs->getString(column);
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

// Function to demonstrate JSON operations with MySQL
void demonstrateMySQLJson(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== MySQL JSON Operations ===\n"
              << std::endl;

    try
    {
        // Create a table with a JSON column
        conn->executeUpdate("DROP TABLE IF EXISTS product_catalog");
        conn->executeUpdate(
            "CREATE TABLE product_catalog ("
            "id INT PRIMARY KEY, "
            "product_data JSON"
            ")");

        std::cout << "Table created successfully." << std::endl;

        // Insert JSON data using prepared statements
        auto pstmt = conn->prepareStatement(
            "INSERT INTO product_catalog (id, product_data) VALUES (?, ?)");

        // Simple JSON object
        pstmt->setInt(1, 1);
        pstmt->setString(2, R"({
            "name": "Laptop",
            "price": 1299.99,
            "specs": {
                "cpu": "Intel i7",
                "ram": "16GB",
                "storage": "512GB SSD"
            },
            "colors": ["Silver", "Space Gray", "Black"]
        })");
        pstmt->executeUpdate();

        // Another JSON object
        pstmt->setInt(1, 2);
        pstmt->setString(2, R"({
            "name": "Smartphone",
            "price": 799.99,
            "specs": {
                "cpu": "Snapdragon 8",
                "ram": "8GB",
                "storage": "256GB"
            },
            "colors": ["Black", "White", "Blue", "Red"]
        })");
        pstmt->executeUpdate();

        // One more JSON object
        pstmt->setInt(1, 3);
        pstmt->setString(2, R"({
            "name": "Tablet",
            "price": 499.99,
            "specs": {
                "cpu": "A14 Bionic",
                "ram": "4GB",
                "storage": "128GB"
            },
            "colors": ["Silver", "Gold"]
        })");
        pstmt->executeUpdate();

        std::cout << "Data inserted successfully." << std::endl;

        // Example 1: Extract specific JSON fields
        std::cout << "\nExample 1: Extracting specific JSON fields" << std::endl;
        auto rs = conn->executeQuery(
            "SELECT id, "
            "JSON_EXTRACT(product_data, '$.name') AS product_name, "
            "JSON_EXTRACT(product_data, '$.price') AS price, "
            "JSON_EXTRACT(product_data, '$.specs.cpu') AS cpu "
            "FROM product_catalog");
        printJsonResults(rs);

        // Example 2: Filter based on JSON values
        std::cout << "Example 2: Filtering based on JSON values" << std::endl;
        rs = conn->executeQuery(
            "SELECT id, JSON_EXTRACT(product_data, '$.name') AS product_name "
            "FROM product_catalog "
            "WHERE JSON_EXTRACT(product_data, '$.price') > 700");
        printJsonResults(rs);

        // Example 3: Check if JSON array contains a value
        std::cout << "Example 3: Checking if JSON array contains a value" << std::endl;
        rs = conn->executeQuery(
            "SELECT id, JSON_EXTRACT(product_data, '$.name') AS product_name, "
            "JSON_EXTRACT(product_data, '$.colors') AS colors, "
            "JSON_CONTAINS(JSON_EXTRACT(product_data, '$.colors'), '\"Silver\"') AS has_silver "
            "FROM product_catalog");
        printJsonResults(rs);

        // Example 4: Modify JSON data
        std::cout << "Example 4: Modifying JSON data" << std::endl;
        conn->executeUpdate(
            "UPDATE product_catalog "
            "SET product_data = JSON_SET(product_data, '$.price', 1199.99, '$.on_sale', true) "
            "WHERE id = 1");

        rs = conn->executeQuery("SELECT id, product_data FROM product_catalog WHERE id = 1");
        printJsonResults(rs);

        // Example 5: Add elements to JSON array
        std::cout << "Example 5: Adding elements to JSON array" << std::endl;
        conn->executeUpdate(
            "UPDATE product_catalog "
            "SET product_data = JSON_ARRAY_APPEND(product_data, '$.colors', '\"Green\"') "
            "WHERE id = 2");

        rs = conn->executeQuery(
            "SELECT id, JSON_EXTRACT(product_data, '$.name') AS product_name, "
            "JSON_EXTRACT(product_data, '$.colors') AS colors "
            "FROM product_catalog WHERE id = 2");
        printJsonResults(rs);

        // Clean up
        conn->executeUpdate("DROP TABLE product_catalog");
        std::cout << "Table dropped successfully." << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "MySQL JSON operation error: " << e.what_s() << std::endl;
    }
}

// Function to demonstrate JSON operations with PostgreSQL
void demonstratePostgreSQLJson(std::shared_ptr<cpp_dbc::RelationalDBConnection> conn)
{
    std::cout << "\n=== PostgreSQL JSON Operations ===\n"
              << std::endl;

    try
    {
        // Create a table with a JSONB column (preferred over JSON in PostgreSQL)
        conn->executeUpdate("DROP TABLE IF EXISTS user_profiles");
        conn->executeUpdate(
            "CREATE TABLE user_profiles ("
            "id INT PRIMARY KEY, "
            "profile JSONB"
            ")");

        std::cout << "Table created successfully." << std::endl;

        // Insert JSON data using prepared statements
        auto pstmt = conn->prepareStatement(
            "INSERT INTO user_profiles (id, profile) VALUES (?, ?)");

        // User profile 1
        pstmt->setInt(1, 1);
        pstmt->setString(2, R"({
            "username": "johndoe",
            "email": "john@example.com",
            "age": 32,
            "interests": ["programming", "hiking", "photography"],
            "address": {
                "city": "San Francisco",
                "state": "CA",
                "country": "USA"
            }
        })");
        pstmt->executeUpdate();

        // User profile 2
        pstmt->setInt(1, 2);
        pstmt->setString(2, R"({
            "username": "janedoe",
            "email": "jane@example.com",
            "age": 28,
            "interests": ["design", "travel", "cooking"],
            "address": {
                "city": "New York",
                "state": "NY",
                "country": "USA"
            }
        })");
        pstmt->executeUpdate();

        // User profile 3
        pstmt->setInt(1, 3);
        pstmt->setString(2, R"({
            "username": "bobsmith",
            "email": "bob@example.com",
            "age": 45,
            "interests": ["gardening", "woodworking", "hiking"],
            "address": {
                "city": "Seattle",
                "state": "WA",
                "country": "USA"
            }
        })");
        pstmt->executeUpdate();

        std::cout << "Data inserted successfully." << std::endl;

        // Example 1: Extract specific JSON fields (PostgreSQL syntax)
        std::cout << "\nExample 1: Extracting specific JSON fields" << std::endl;
        auto rs = conn->executeQuery(
            "SELECT id, "
            "profile->>'username' AS username, "
            "profile->>'email' AS email, "
            "profile->>'age' AS age, "
            "profile->'address'->>'city' AS city "
            "FROM user_profiles");
        printJsonResults(rs);

        // Example 2: Filter based on JSON values
        std::cout << "Example 2: Filtering based on JSON values" << std::endl;
        rs = conn->executeQuery(
            "SELECT id, profile->>'username' AS username, profile->>'age' AS age "
            "FROM user_profiles "
            "WHERE (profile->>'age')::int > 30");
        printJsonResults(rs);

        // Example 3: Check if JSON array contains a value
        std::cout << "Example 3: Checking if JSON array contains a value" << std::endl;
        rs = conn->executeQuery(
            "SELECT id, profile->>'username' AS username, "
            "profile->'interests' AS interests, "
            "profile->'interests' ? 'hiking' AS likes_hiking "
            "FROM user_profiles");
        printJsonResults(rs);

        // Example 4: Modify JSON data
        std::cout << "Example 4: Modifying JSON data" << std::endl;
        conn->executeUpdate(
            "UPDATE user_profiles "
            "SET profile = profile || '{\"premium_member\": true, \"age\": 33}'::jsonb "
            "WHERE id = 1");

        rs = conn->executeQuery("SELECT id, profile FROM user_profiles WHERE id = 1");
        printJsonResults(rs);

        // Example 5: Add elements to JSON array
        std::cout << "Example 5: Adding elements to JSON array" << std::endl;
        conn->executeUpdate(
            "UPDATE user_profiles "
            "SET profile = jsonb_set(profile, '{interests}', "
            "profile->'interests' || '\"music\"'::jsonb) "
            "WHERE id = 2");

        rs = conn->executeQuery(
            "SELECT id, profile->>'username' AS username, "
            "profile->'interests' AS interests "
            "FROM user_profiles WHERE id = 2");
        printJsonResults(rs);

        // Example 6: JSON path queries (PostgreSQL 12+)
        std::cout << "Example 6: JSON path queries (PostgreSQL 12+)" << std::endl;
        try
        {
            rs = conn->executeQuery(
                "SELECT id, profile->>'username' AS username, "
                "jsonb_path_query_array(profile, '$.interests[*]') AS interest_list "
                "FROM user_profiles");
            printJsonResults(rs);
        }
        catch (const cpp_dbc::DBException &e)
        {
            std::cout << "JSON path query not supported in this PostgreSQL version." << std::endl;
        }

        // Clean up
        conn->executeUpdate("DROP TABLE user_profiles");
        std::cout << "Table dropped successfully." << std::endl;
    }
    catch (const cpp_dbc::DBException &e)
    {
        std::cerr << "PostgreSQL JSON operation error: " << e.what_s() << std::endl;
    }
}

int main()
{
    try
    {
        // Register database drivers
#if USE_MYSQL
        cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());

        // Connect to MySQL
        std::cout << "Connecting to MySQL..." << std::endl;
        auto mysqlConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(
            "cpp_dbc:mysql://localhost:3306/testdb",
            "username",
            "password"));

        // Demonstrate MySQL JSON operations
        demonstrateMySQLJson(mysqlConn);

        // Close MySQL connection
        mysqlConn->close();
#else
        std::cout << "MySQL support is not enabled." << std::endl;
#endif

#if USE_POSTGRESQL
        cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());

        // Connect to PostgreSQL
        std::cout << "\nConnecting to PostgreSQL..." << std::endl;
        auto pgConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(
            "cpp_dbc:postgresql://localhost:5432/testdb",
            "username",
            "password"));

        // Demonstrate PostgreSQL JSON operations
        demonstratePostgreSQLJson(pgConn);

        // Close PostgreSQL connection
        pgConn->close();
#else
        std::cout << "PostgreSQL support is not enabled." << std::endl;
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