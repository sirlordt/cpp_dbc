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

 @file 26_021_test_scylladb_real_driver.cpp
 @brief Tests for ScyllaDB driver

*/

#include <string>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#if USE_SCYLLADB
// Test case for ScyllaDB driver
TEST_CASE("ScyllaDB driver tests", "[26_021_01_scylladb_real_driver]")
{
    SECTION("ScyllaDB driver URL acceptance")
    {
        // Create a ScyllaDB driver
        cpp_dbc::ScyllaDB::ScyllaDBDriver driver;

        // Check that it accepts ScyllaDB URLs
        REQUIRE(driver.acceptsURL("cpp_dbc:scylladb://localhost:9042/testdb"));
        REQUIRE(driver.acceptsURL("cpp_dbc:scylladb://127.0.0.1:9042/testdb"));

        // Check that it rejects non-ScyllaDB URLs
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("scylladb://localhost:9042/testdb"));
    }

    SECTION("ScyllaDB driver parseURI - valid URIs")
    {
        cpp_dbc::ScyllaDB::ScyllaDBDriver driver;

        // Full URI with host, port, and keyspace
        auto params = driver.parseURI("cpp_dbc:scylladb://localhost:9042/mydb");
        REQUIRE(params.at("host") == "localhost");
        REQUIRE(params.at("port") == "9042");
        REQUIRE(params.at("database") == "mydb");

        // URI with host and port but no keyspace
        params = driver.parseURI("cpp_dbc:scylladb://server:1234");
        REQUIRE(params.at("host") == "server");
        REQUIRE(params.at("port") == "1234");
        REQUIRE(params.at("database") == "");

        // URI with host only (should default port to 9042)
        params = driver.parseURI("cpp_dbc:scylladb://localhost");
        REQUIRE(params.at("host") == "localhost");
        REQUIRE(params.at("port") == "9042");
        REQUIRE(params.at("database") == "");

        // URI with host and keyspace but no port
        params = driver.parseURI("cpp_dbc:scylladb://localhost/mykeyspace");
        REQUIRE(params.at("host") == "localhost");
        REQUIRE(params.at("port") == "9042");
        REQUIRE(params.at("database") == "mykeyspace");

        // URI with IP address
        params = driver.parseURI("cpp_dbc:scylladb://127.0.0.1:9042/testks");
        REQUIRE(params.at("host") == "127.0.0.1");
        REQUIRE(params.at("port") == "9042");
        REQUIRE(params.at("database") == "testks");
    }

    SECTION("ScyllaDB driver parseURI - invalid URIs")
    {
        cpp_dbc::ScyllaDB::ScyllaDBDriver driver;

        // Wrong scheme
        REQUIRE_THROWS_AS(
            driver.parseURI("cpp_dbc:mysql://localhost:3306/testdb"),
            cpp_dbc::DBException);
        REQUIRE_THROWS_AS(
            driver.parseURI("scylladb://localhost:9042/testdb"),
            cpp_dbc::DBException);
    }
}
#endif
