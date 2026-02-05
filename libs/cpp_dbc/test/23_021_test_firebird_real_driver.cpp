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
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#if USE_FIREBIRD
// Test case for Firebird driver
TEST_CASE("Firebird driver tests", "[23_021_01_firebird_real_driver]")
{
    SECTION("Firebird driver URL acceptance")
    {
        // Create a Firebird driver
        cpp_dbc::Firebird::FirebirdDBDriver driver;

        // Check that it accepts Firebird URLs
        REQUIRE(driver.acceptsURL("cpp_dbc:firebird://localhost:3050/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:firebird://127.0.0.1:3050/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:firebird://db.example.com:3050/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:firebird://localhost:3050//var/lib/firebird/data/testdb.fdb"));

        // Check that it rejects non-Firebird URLs
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("jdbc:firebird://localhost:3050/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("firebird://localhost:3050/testdb"));
    }

    SECTION("Firebird driver connection string parsing")
    {
        // Create a Firebird driver
        cpp_dbc::Firebird::FirebirdDBDriver driver;

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a DBException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver.connect("cpp_dbc:firebird://localhost:3050/non_existent_db", "user", "pass"),
            cpp_dbc::DBException);
    }

    SECTION("Firebird driver parseURL - valid URLs")
    {
        cpp_dbc::Firebird::FirebirdDBDriver driver;
        std::string host;
        int port = 0;
        std::string database;

        // Full URL with host, port, and database path
        REQUIRE(driver.parseURL("cpp_dbc:firebird://localhost:3050/testdb", host, port, database));
        REQUIRE(host == "localhost");
        REQUIRE(port == 3050);
        REQUIRE(database == "/testdb");

        // URL with custom port and absolute path
        REQUIRE(driver.parseURL("cpp_dbc:firebird://dbserver:3051//var/lib/firebird/data/test.fdb", host, port, database));
        REQUIRE(host == "dbserver");
        REQUIRE(port == 3051);
        REQUIRE(database == "//var/lib/firebird/data/test.fdb");

        // URL without port (should default to 3050)
        REQUIRE(driver.parseURL("cpp_dbc:firebird://localhost/testdb.fdb", host, port, database));
        REQUIRE(host == "localhost");
        REQUIRE(port == 3050);
        REQUIRE(database == "/testdb.fdb");

        // Local connection (no host, starts with /)
        REQUIRE(driver.parseURL("cpp_dbc:firebird:///var/lib/firebird/data/test.fdb", host, port, database));
        REQUIRE(host.empty());
        REQUIRE(port == 3050);
        REQUIRE(database == "/var/lib/firebird/data/test.fdb");

        // URL with IPv6 address
        REQUIRE(driver.parseURL("cpp_dbc:firebird://[::1]:3050/testdb.fdb", host, port, database));
        REQUIRE(host == "::1");
        REQUIRE(port == 3050);
        REQUIRE(database == "/testdb.fdb");
    }

    SECTION("Firebird driver parseURL - invalid URLs")
    {
        cpp_dbc::Firebird::FirebirdDBDriver driver;
        std::string host;
        int port = 0;
        std::string database;

        // Wrong scheme
        REQUIRE_FALSE(driver.parseURL("cpp_dbc:mysql://localhost:3306/testdb", host, port, database));
        REQUIRE_FALSE(driver.parseURL("jdbc:firebird://localhost:3050/testdb", host, port, database));

        // Host without database path
        REQUIRE_FALSE(driver.parseURL("cpp_dbc:firebird://localhost", host, port, database));
    }
}
#endif
