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

 @file 21_021_test_postgresql_real_driver.cpp
 @brief Tests for PostgreSQL driver

*/

#include <string>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#if USE_POSTGRESQL
// Test case for PostgreSQL driver
TEST_CASE("PostgreSQL driver tests", "[21_021_01_postgresql_real_driver]")
{
    SECTION("PostgreSQL driver URL acceptance")
    {
        // Create a PostgreSQL driver
        cpp_dbc::PostgreSQL::PostgreSQLDBDriver driver;

        // Check that it accepts PostgreSQL URLs
        REQUIRE(driver.acceptsURL("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:postgresql://127.0.0.1:5432/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:postgresql://db.example.com:5432/testdb"));

        // Check that it rejects non-PostgreSQL URLs
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("jdbc:postgresql://localhost:5432/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("postgresql://localhost:5432/testdb"));
    }

    SECTION("PostgreSQL driver connection string parsing")
    {
        // Create a PostgreSQL driver
        cpp_dbc::PostgreSQL::PostgreSQLDBDriver driver;

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a DBException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver.connect("cpp_dbc:postgresql://localhost:5432/non_existent_db", "user", "pass"),
            cpp_dbc::DBException);
    }

    SECTION("PostgreSQL driver parseURL - valid URLs")
    {
        cpp_dbc::PostgreSQL::PostgreSQLDBDriver driver;
        std::string host;
        int port = 0;
        std::string database;

        // Full URL with host, port, and database
        REQUIRE(driver.parseURL("cpp_dbc:postgresql://localhost:5432/testdb", host, port, database));
        REQUIRE(host == "localhost");
        REQUIRE(port == 5432);
        REQUIRE(database == "testdb");

        // URL with custom port
        REQUIRE(driver.parseURL("cpp_dbc:postgresql://dbserver:9999/mydb", host, port, database));
        REQUIRE(host == "dbserver");
        REQUIRE(port == 9999);
        REQUIRE(database == "mydb");

        // URL with IP address
        REQUIRE(driver.parseURL("cpp_dbc:postgresql://127.0.0.1:5432/proddb", host, port, database));
        REQUIRE(host == "127.0.0.1");
        REQUIRE(port == 5432);
        REQUIRE(database == "proddb");

        // URL without port (should default to 5432)
        REQUIRE(driver.parseURL("cpp_dbc:postgresql://localhost/testdb", host, port, database));
        REQUIRE(host == "localhost");
        REQUIRE(port == 5432);
        REQUIRE(database == "testdb");
    }

    SECTION("PostgreSQL driver parseURL - invalid URLs")
    {
        cpp_dbc::PostgreSQL::PostgreSQLDBDriver driver;
        std::string host;
        int port = 0;
        std::string database;

        // Wrong scheme
        REQUIRE_FALSE(driver.parseURL("cpp_dbc:mysql://localhost:3306/testdb", host, port, database));
        REQUIRE_FALSE(driver.parseURL("jdbc:postgresql://localhost:5432/testdb", host, port, database));

        // Missing database (PostgreSQL requires it)
        REQUIRE_FALSE(driver.parseURL("cpp_dbc:postgresql://localhost:5432", host, port, database));

        // Invalid port
        REQUIRE_FALSE(driver.parseURL("cpp_dbc:postgresql://localhost:notaport/testdb", host, port, database));
    }
}
#endif
