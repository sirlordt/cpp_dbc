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

 @file 20_021_test_mysql_real_driver.cpp
 @brief Tests for MySQL driver

*/

#include <string>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#if USE_MYSQL
// Test case for MySQL driver
TEST_CASE("MySQL driver tests", "[20_021_01_mysql_real_driver]")
{
    SECTION("MySQL driver URL acceptance")
    {
        // Create a MySQL driver
        cpp_dbc::MySQL::MySQLDBDriver driver;

        // Check that it accepts MySQL URLs
        REQUIRE(driver.acceptsURL("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:mysql://127.0.0.1:3306/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:mysql://db.example.com:3306/testdb"));

        // Check that it rejects non-MySQL URLs
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("jdbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("mysql://localhost:3306/testdb"));
    }

    SECTION("MySQL driver connection string parsing")
    {
        // Create a MySQL driver
        cpp_dbc::MySQL::MySQLDBDriver driver;

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a DBException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver.connect("cpp_dbc:mysql://localhost:3306/non_existent_db", "user", "pass"),
            cpp_dbc::DBException);
    }

    SECTION("MySQL driver parseURL - valid URLs")
    {
        cpp_dbc::MySQL::MySQLDBDriver driver;
        std::string host;
        int port = 0;
        std::string database;

        // Full URL with host, port, and database
        REQUIRE(driver.parseURL("cpp_dbc:mysql://localhost:3306/testdb", host, port, database));
        REQUIRE(host == "localhost");
        REQUIRE(port == 3306);
        REQUIRE(database == "testdb");

        // URL with custom port
        REQUIRE(driver.parseURL("cpp_dbc:mysql://dbserver:9999/mydb", host, port, database));
        REQUIRE(host == "dbserver");
        REQUIRE(port == 9999);
        REQUIRE(database == "mydb");

        // URL without port (should default to 3306)
        REQUIRE(driver.parseURL("cpp_dbc:mysql://localhost/testdb", host, port, database));
        REQUIRE(host == "localhost");
        REQUIRE(port == 3306);
        REQUIRE(database == "testdb");

        // URL with host only (no port, no database)
        REQUIRE(driver.parseURL("cpp_dbc:mysql://localhost", host, port, database));
        REQUIRE(host == "localhost");
        REQUIRE(port == 3306);
        REQUIRE(database == "");

        // URL with host and port but no database
        REQUIRE(driver.parseURL("cpp_dbc:mysql://localhost:3307", host, port, database));
        REQUIRE(host == "localhost");
        REQUIRE(port == 3307);
    }

    SECTION("MySQL driver parseURL - invalid URLs")
    {
        cpp_dbc::MySQL::MySQLDBDriver driver;
        std::string host;
        int port = 0;
        std::string database;

        // Wrong scheme
        REQUIRE_FALSE(driver.parseURL("cpp_dbc:postgresql://localhost:5432/testdb", host, port, database));
        REQUIRE_FALSE(driver.parseURL("jdbc:mysql://localhost:3306/testdb", host, port, database));

        // Invalid port
        REQUIRE_FALSE(driver.parseURL("cpp_dbc:mysql://localhost:notaport/testdb", host, port, database));
    }
}
#endif
