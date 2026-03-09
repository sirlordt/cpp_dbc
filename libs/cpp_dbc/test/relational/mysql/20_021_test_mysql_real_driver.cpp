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
#include <map>
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
        REQUIRE(driver.acceptURI("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE(driver.acceptURI("cpp_dbc:mysql://127.0.0.1:3306/testdb"));
        REQUIRE(driver.acceptURI("cpp_dbc:mysql://db.example.com:3306/testdb"));

        // Check that it rejects non-MySQL URLs
        REQUIRE_FALSE(driver.acceptURI("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE_FALSE(driver.acceptURI("jdbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptURI("mysql://localhost:3306/testdb"));
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

    SECTION("MySQL driver parseURI - valid URLs")
    {
        cpp_dbc::MySQL::MySQLDBDriver driver;

        // Full URL with host, port, and database
        auto result1 = driver.parseURI("cpp_dbc:mysql://localhost:3306/testdb");
        REQUIRE(result1.at("host") == "localhost");
        REQUIRE(result1.at("port") == "3306");
        REQUIRE(result1.at("database") == "testdb");

        // URL with custom port
        auto result2 = driver.parseURI("cpp_dbc:mysql://dbserver:9999/mydb");
        REQUIRE(result2.at("host") == "dbserver");
        REQUIRE(result2.at("port") == "9999");
        REQUIRE(result2.at("database") == "mydb");

        // URL without port (should default to 3306)
        auto result3 = driver.parseURI("cpp_dbc:mysql://localhost/testdb");
        REQUIRE(result3.at("host") == "localhost");
        REQUIRE(result3.at("port") == "3306");
        REQUIRE(result3.at("database") == "testdb");

        // URL with host only (no port, no database)
        auto result4 = driver.parseURI("cpp_dbc:mysql://localhost");
        REQUIRE(result4.at("host") == "localhost");
        REQUIRE(result4.at("port") == "3306");
        REQUIRE(result4.at("database").empty());

        // URL with host and port but no database
        auto result5 = driver.parseURI("cpp_dbc:mysql://localhost:3307");
        REQUIRE(result5.at("host") == "localhost");
        REQUIRE(result5.at("port") == "3307");
        REQUIRE(result5.at("database").empty());

        // URL with IPv6 address
        auto result6 = driver.parseURI("cpp_dbc:mysql://[::1]:3306/testdb");
        REQUIRE(result6.at("host") == "::1");
        REQUIRE(result6.at("port") == "3306");
        REQUIRE(result6.at("database") == "testdb");
    }

    SECTION("MySQL driver parseURI - invalid URLs")
    {
        cpp_dbc::MySQL::MySQLDBDriver driver;

        // Wrong scheme — returns unexpected (not accepted)
        auto r1 = driver.parseURI(std::nothrow, "cpp_dbc:postgresql://localhost:5432/testdb");
        REQUIRE_FALSE(r1.has_value());

        auto r2 = driver.parseURI(std::nothrow, "jdbc:mysql://localhost:3306/testdb");
        REQUIRE_FALSE(r2.has_value());

        // Invalid port — returns unexpected (parse error)
        auto r3 = driver.parseURI(std::nothrow, "cpp_dbc:mysql://localhost:notaport/testdb");
        REQUIRE_FALSE(r3.has_value());
    }

    SECTION("MySQL driver buildURI")
    {
        cpp_dbc::MySQL::MySQLDBDriver driver;

        // Build a standard URI
        auto uri = driver.buildURI("localhost", 3306, "testdb");
        REQUIRE(uri == "cpp_dbc:mysql://localhost:3306/testdb");

        // Build URI without database
        auto uri2 = driver.buildURI("dbserver", 9999, "");
        REQUIRE(uri2 == "cpp_dbc:mysql://dbserver:9999");

        // Roundtrip: buildURI -> parseURI
        auto parsed = driver.parseURI(uri);
        REQUIRE(parsed.at("host") == "localhost");
        REQUIRE(parsed.at("port") == "3306");
        REQUIRE(parsed.at("database") == "testdb");
    }
}
#endif
