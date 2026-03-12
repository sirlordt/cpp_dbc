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

 @file 23_021_test_firebird_real_driver.cpp
 @brief Tests for Firebird driver

*/

#include <string>
#include <map>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#include "23_001_test_firebird_real_common.hpp"

#if USE_FIREBIRD
// Test case for Firebird driver
TEST_CASE("Firebird driver tests", "[23_021_01_firebird_real_driver]")
{
    SECTION("Firebird driver URL acceptance")
    {
        // Create a Firebird driver
        auto driver = firebird_test_helpers::getFirebirdDriver();
        REQUIRE(driver != nullptr);

        // Check that it accepts Firebird URIs
        REQUIRE_NOTHROW(driver->acceptURI("cpp_dbc:firebird://localhost:3050/testdb"));
        REQUIRE_NOTHROW(driver->acceptURI("cpp_dbc:firebird://127.0.0.1:3050/testdb"));
        REQUIRE_NOTHROW(driver->acceptURI("cpp_dbc:firebird://db.example.com:3050/testdb"));
        REQUIRE_NOTHROW(driver->acceptURI("cpp_dbc:firebird://localhost:3050//var/lib/firebird/data/testdb.fdb"));

        // Check that it rejects non-Firebird URIs
        REQUIRE_THROWS_AS(driver->acceptURI("cpp_dbc:mysql://localhost:3306/testdb"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver->acceptURI("cpp_dbc:postgresql://localhost:5432/testdb"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver->acceptURI("jdbc:firebird://localhost:3050/testdb"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver->acceptURI("firebird://localhost:3050/testdb"), cpp_dbc::DBException);
    }

    SECTION("Firebird driver URI acceptance (nothrow)")
    {
        auto driver = firebird_test_helpers::getFirebirdDriver();
        REQUIRE(driver != nullptr);

        // Valid Firebird URIs — has_value() returns true
        auto ok1 = driver->acceptURI(std::nothrow, "cpp_dbc:firebird://localhost:3050/testdb");
        REQUIRE(ok1.has_value());
        auto ok2 = driver->acceptURI(std::nothrow, "cpp_dbc:firebird://127.0.0.1:3050/testdb");
        REQUIRE(ok2.has_value());
        auto ok3 = driver->acceptURI(std::nothrow, "cpp_dbc:firebird://localhost:3050//var/lib/firebird/data/testdb.fdb");
        REQUIRE(ok3.has_value());

        // Wrong scheme — has_value() returns false with scheme mismatch error
        auto no1 = driver->acceptURI(std::nothrow, "cpp_dbc:mysql://localhost:3306/testdb");
        REQUIRE_FALSE(no1.has_value());
        auto no2 = driver->acceptURI(std::nothrow, "jdbc:firebird://localhost:3050/testdb");
        REQUIRE_FALSE(no2.has_value());
        auto no3 = driver->acceptURI(std::nothrow, "firebird://localhost:3050/testdb");
        REQUIRE_FALSE(no3.has_value());
    }

    SECTION("Firebird driver connection string parsing")
    {
        // Create a Firebird driver
        auto driver = firebird_test_helpers::getFirebirdDriver();
        REQUIRE(driver != nullptr);

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a DBException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver->connect("cpp_dbc:firebird://localhost:3050/non_existent_db", "user", "pass"),
            cpp_dbc::DBException);
    }

    SECTION("Firebird driver parseURI - valid URLs")
    {
        auto driver = firebird_test_helpers::getFirebirdDriver();
        REQUIRE(driver != nullptr);

        // Full URL with host, port, and database path
        auto result1 = driver->parseURI("cpp_dbc:firebird://localhost:3050/testdb");
        REQUIRE(result1.at("host") == "localhost");
        REQUIRE(result1.at("port") == "3050");
        REQUIRE(result1.at("database") == "/testdb");

        // URL with custom port and absolute path
        auto result2 = driver->parseURI("cpp_dbc:firebird://dbserver:3051//var/lib/firebird/data/test.fdb");
        REQUIRE(result2.at("host") == "dbserver");
        REQUIRE(result2.at("port") == "3051");
        REQUIRE(result2.at("database") == "//var/lib/firebird/data/test.fdb");

        // URL without port (should default to 3050)
        auto result3 = driver->parseURI("cpp_dbc:firebird://localhost/testdb.fdb");
        REQUIRE(result3.at("host") == "localhost");
        REQUIRE(result3.at("port") == "3050");
        REQUIRE(result3.at("database") == "/testdb.fdb");

        // Local connection (no host, starts with /)
        auto result4 = driver->parseURI("cpp_dbc:firebird:///var/lib/firebird/data/test.fdb");
        REQUIRE(result4.at("host").empty());
        REQUIRE(result4.at("port") == "3050");
        REQUIRE(result4.at("database") == "/var/lib/firebird/data/test.fdb");

        // URL with IPv6 address
        auto result5 = driver->parseURI("cpp_dbc:firebird://[::1]:3050/testdb.fdb");
        REQUIRE(result5.at("host") == "::1");
        REQUIRE(result5.at("port") == "3050");
        REQUIRE(result5.at("database") == "/testdb.fdb");
    }

    SECTION("Firebird driver parseURI - invalid URLs")
    {
        auto driver = firebird_test_helpers::getFirebirdDriver();
        REQUIRE(driver != nullptr);

        // Wrong scheme — returns unexpected (not accepted)
        auto r1 = driver->parseURI(std::nothrow, "cpp_dbc:mysql://localhost:3306/testdb");
        REQUIRE_FALSE(r1.has_value());

        auto r2 = driver->parseURI(std::nothrow, "jdbc:firebird://localhost:3050/testdb");
        REQUIRE_FALSE(r2.has_value());

        // Host without database path — returns unexpected (parse error)
        auto r3 = driver->parseURI(std::nothrow, "cpp_dbc:firebird://localhost");
        REQUIRE_FALSE(r3.has_value());
    }

    SECTION("Firebird driver buildURI")
    {
        auto driver = firebird_test_helpers::getFirebirdDriver();
        REQUIRE(driver != nullptr);

        // Build a standard URL
        auto uri = driver->buildURI("localhost", 3050, "/testdb");
        REQUIRE(uri == "cpp_dbc:firebird://localhost/testdb");

        // Build URL with absolute path
        auto uri2 = driver->buildURI("dbserver", 3051, "//var/lib/firebird/data/test.fdb");
        REQUIRE(uri2 == "cpp_dbc:firebird://dbserver:3051//var/lib/firebird/data/test.fdb");

        // Roundtrip: buildURI -> parseURI
        auto parsed = driver->parseURI(uri);
        REQUIRE(parsed.at("host") == "localhost");
        REQUIRE(parsed.at("port") == "3050");
        REQUIRE(parsed.at("database") == "/testdb");
    }
}
#endif
