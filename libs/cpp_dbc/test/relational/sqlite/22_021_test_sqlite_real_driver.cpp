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

 @file 22_021_test_sqlite_real_driver.cpp
 @brief Tests for SQLite driver

*/

#include <string>
#include <map>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#if USE_SQLITE
// Test case for SQLite driver
TEST_CASE("SQLite driver tests", "[22_021_01_sqlite_real_driver]")
{
    SECTION("SQLite driver URL acceptance")
    {
        // Create a SQLite driver
        cpp_dbc::SQLite::SQLiteDBDriver driver;

        // Check that it accepts SQLite URIs
        REQUIRE_NOTHROW(driver.acceptURI("cpp_dbc:sqlite://:memory:"));
        REQUIRE_NOTHROW(driver.acceptURI("cpp_dbc:sqlite://test.db"));
        REQUIRE_NOTHROW(driver.acceptURI("cpp_dbc:sqlite:///path/to/database.db"));

        // Check that it rejects non-SQLite URIs
        REQUIRE_THROWS_AS(driver.acceptURI("cpp_dbc:mysql://localhost:3306/testdb"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver.acceptURI("cpp_dbc:postgresql://localhost:5432/testdb"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver.acceptURI("jdbc:sqlite://test.db"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver.acceptURI("sqlite://test.db"), cpp_dbc::DBException);
    }

    SECTION("SQLite driver URI acceptance (nothrow)")
    {
        cpp_dbc::SQLite::SQLiteDBDriver driver;

        // Valid SQLite URIs — has_value() returns true
        auto ok1 = driver.acceptURI(std::nothrow, "cpp_dbc:sqlite://:memory:");
        REQUIRE(ok1.has_value());
        auto ok2 = driver.acceptURI(std::nothrow, "cpp_dbc:sqlite://test.db");
        REQUIRE(ok2.has_value());
        auto ok3 = driver.acceptURI(std::nothrow, "cpp_dbc:sqlite:///path/to/database.db");
        REQUIRE(ok3.has_value());

        // Wrong scheme — has_value() returns false with scheme mismatch error
        auto no1 = driver.acceptURI(std::nothrow, "cpp_dbc:mysql://localhost:3306/testdb");
        REQUIRE_FALSE(no1.has_value());
        auto no2 = driver.acceptURI(std::nothrow, "jdbc:sqlite://test.db");
        REQUIRE_FALSE(no2.has_value());
        auto no3 = driver.acceptURI(std::nothrow, "sqlite://test.db");
        REQUIRE_FALSE(no3.has_value());
    }

    SECTION("SQLite driver parseURI - valid URLs")
    {
        cpp_dbc::SQLite::SQLiteDBDriver driver;

        // In-memory database
        auto result1 = driver.parseURI("cpp_dbc:sqlite://:memory:");
        REQUIRE(result1.at("host").empty());
        REQUIRE(result1.at("port") == "0");
        REQUIRE(result1.at("database") == ":memory:");

        // Simple file path
        auto result2 = driver.parseURI("cpp_dbc:sqlite://test.db");
        REQUIRE(result2.at("database") == "test.db");

        // Absolute file path
        auto result3 = driver.parseURI("cpp_dbc:sqlite:///path/to/database.db");
        REQUIRE(result3.at("database") == "/path/to/database.db");
    }

    SECTION("SQLite driver parseURI - invalid URLs")
    {
        cpp_dbc::SQLite::SQLiteDBDriver driver;

        // Wrong scheme — returns unexpected (not accepted)
        auto r1 = driver.parseURI(std::nothrow, "cpp_dbc:mysql://localhost:3306/testdb");
        REQUIRE_FALSE(r1.has_value());

        auto r2 = driver.parseURI(std::nothrow, "jdbc:sqlite://test.db");
        REQUIRE_FALSE(r2.has_value());

        auto r3 = driver.parseURI(std::nothrow, "sqlite://test.db");
        REQUIRE_FALSE(r3.has_value());
    }

    SECTION("SQLite driver buildURI")
    {
        cpp_dbc::SQLite::SQLiteDBDriver driver;

        // Build an in-memory URL
        auto uri = driver.buildURI("", 0, ":memory:");
        REQUIRE(uri == "cpp_dbc:sqlite://:memory:");

        // Build a file path URL
        auto uri2 = driver.buildURI("", 0, "/path/to/database.db");
        REQUIRE(uri2 == "cpp_dbc:sqlite:///path/to/database.db");

        // Roundtrip: buildURI -> parseURI
        auto parsed = driver.parseURI(uri);
        REQUIRE(parsed.at("database") == ":memory:");
    }

    SECTION("SQLite driver connection to in-memory database")
    {
        // Create a SQLite driver
        cpp_dbc::SQLite::SQLiteDBDriver driver;

        // SQLite can connect to in-memory databases without external dependencies
        auto conn = driver.connect("cpp_dbc:sqlite://:memory:", "", "");
        REQUIRE(conn != nullptr);

        // Verify the connection works
        auto relConn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(conn);
        REQUIRE(relConn != nullptr);
        REQUIRE(relConn->isClosed() == false);

        // Close the connection
        relConn->close();
        REQUIRE(relConn->isClosed() == true);
    }
}
#endif
