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
#include <map>
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

        // Check that it accepts PostgreSQL URIs
        REQUIRE_NOTHROW(driver.acceptURI("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE_NOTHROW(driver.acceptURI("cpp_dbc:postgresql://127.0.0.1:5432/testdb"));
        REQUIRE_NOTHROW(driver.acceptURI("cpp_dbc:postgresql://db.example.com:5432/testdb"));

        // Check that it rejects non-PostgreSQL URIs
        REQUIRE_THROWS_AS(driver.acceptURI("cpp_dbc:mysql://localhost:3306/testdb"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver.acceptURI("jdbc:postgresql://localhost:5432/testdb"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver.acceptURI("postgresql://localhost:5432/testdb"), cpp_dbc::DBException);
    }

    SECTION("PostgreSQL driver URI acceptance (nothrow)")
    {
        cpp_dbc::PostgreSQL::PostgreSQLDBDriver driver;

        // Valid PostgreSQL URIs — has_value() returns true
        auto ok1 = driver.acceptURI(std::nothrow, "cpp_dbc:postgresql://localhost:5432/testdb");
        REQUIRE(ok1.has_value());
        auto ok2 = driver.acceptURI(std::nothrow, "cpp_dbc:postgresql://127.0.0.1:5432/testdb");
        REQUIRE(ok2.has_value());
        auto ok3 = driver.acceptURI(std::nothrow, "cpp_dbc:postgresql://db.example.com:5432/testdb");
        REQUIRE(ok3.has_value());

        // Wrong scheme — has_value() returns false with scheme mismatch error
        auto no1 = driver.acceptURI(std::nothrow, "cpp_dbc:mysql://localhost:3306/testdb");
        REQUIRE_FALSE(no1.has_value());
        auto no2 = driver.acceptURI(std::nothrow, "jdbc:postgresql://localhost:5432/testdb");
        REQUIRE_FALSE(no2.has_value());
        auto no3 = driver.acceptURI(std::nothrow, "postgresql://localhost:5432/testdb");
        REQUIRE_FALSE(no3.has_value());
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

    SECTION("PostgreSQL driver parseURI - valid URLs")
    {
        cpp_dbc::PostgreSQL::PostgreSQLDBDriver driver;

        // Full URL with host, port, and database
        auto result1 = driver.parseURI("cpp_dbc:postgresql://localhost:5432/testdb");
        REQUIRE(result1.at("host") == "localhost");
        REQUIRE(result1.at("port") == "5432");
        REQUIRE(result1.at("database") == "testdb");

        // URL with custom port
        auto result2 = driver.parseURI("cpp_dbc:postgresql://dbserver:9999/mydb");
        REQUIRE(result2.at("host") == "dbserver");
        REQUIRE(result2.at("port") == "9999");
        REQUIRE(result2.at("database") == "mydb");

        // URL with IP address
        auto result3 = driver.parseURI("cpp_dbc:postgresql://127.0.0.1:5432/proddb");
        REQUIRE(result3.at("host") == "127.0.0.1");
        REQUIRE(result3.at("port") == "5432");
        REQUIRE(result3.at("database") == "proddb");

        // URL without port (should default to 5432)
        auto result4 = driver.parseURI("cpp_dbc:postgresql://localhost/testdb");
        REQUIRE(result4.at("host") == "localhost");
        REQUIRE(result4.at("port") == "5432");
        REQUIRE(result4.at("database") == "testdb");

        // URL with IPv6 address
        auto result5 = driver.parseURI("cpp_dbc:postgresql://[::1]:5432/testdb");
        REQUIRE(result5.at("host") == "::1");
        REQUIRE(result5.at("port") == "5432");
        REQUIRE(result5.at("database") == "testdb");
    }

    SECTION("PostgreSQL driver parseURI - invalid URLs")
    {
        cpp_dbc::PostgreSQL::PostgreSQLDBDriver driver;

        // Wrong scheme — returns unexpected (not accepted)
        auto r1 = driver.parseURI(std::nothrow, "cpp_dbc:mysql://localhost:3306/testdb");
        REQUIRE_FALSE(r1.has_value());

        auto r2 = driver.parseURI(std::nothrow, "jdbc:postgresql://localhost:5432/testdb");
        REQUIRE_FALSE(r2.has_value());

        // Missing database (PostgreSQL requires it) — returns unexpected (parse error)
        auto r3 = driver.parseURI(std::nothrow, "cpp_dbc:postgresql://localhost:5432");
        REQUIRE_FALSE(r3.has_value());

        // Invalid port — returns unexpected (parse error)
        auto r4 = driver.parseURI(std::nothrow, "cpp_dbc:postgresql://localhost:notaport/testdb");
        REQUIRE_FALSE(r4.has_value());
    }

    SECTION("PostgreSQL driver buildURI")
    {
        cpp_dbc::PostgreSQL::PostgreSQLDBDriver driver;

        // Build a standard URL
        auto uri = driver.buildURI("localhost", 5432, "testdb");
        REQUIRE(uri == "cpp_dbc:postgresql://localhost:5432/testdb");

        // Build URL with custom port
        auto uri2 = driver.buildURI("dbserver", 9999, "mydb");
        REQUIRE(uri2 == "cpp_dbc:postgresql://dbserver:9999/mydb");

        // Roundtrip: buildURI -> parseURI
        auto parsed = driver.parseURI(uri);
        REQUIRE(parsed.at("host") == "localhost");
        REQUIRE(parsed.at("port") == "5432");
        REQUIRE(parsed.at("database") == "testdb");
    }
}
#endif
