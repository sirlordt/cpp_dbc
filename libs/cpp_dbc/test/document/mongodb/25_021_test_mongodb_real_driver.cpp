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

 @file 25_021_test_mongodb_real_driver.cpp
 @brief Tests for MongoDB driver

*/

#include <string>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include "25_001_test_mongodb_real_common.hpp"

#if USE_MONGODB
// Test case for MongoDB driver
TEST_CASE("MongoDB driver tests", "[25_021_01_mongodb_real_driver]")
{
    SECTION("MongoDB driver URL acceptance")
    {
        // Create a MongoDB driver
        auto driver = mongodb_test_helpers::getMongoDBDriver();
        REQUIRE(driver != nullptr);

        // Check that it accepts MongoDB URIs
        REQUIRE_NOTHROW(driver->acceptURI("cpp_dbc:mongodb://localhost:27017/testdb"));
        REQUIRE_NOTHROW(driver->acceptURI("cpp_dbc:mongodb://127.0.0.1:27017/testdb"));
        REQUIRE_NOTHROW(driver->acceptURI("cpp_dbc:mongodb://db.example.com:27017/testdb"));

        // Check that it rejects non-MongoDB URIs
        REQUIRE_THROWS_AS(driver->acceptURI("cpp_dbc:mysql://localhost:3306/testdb"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver->acceptURI("cpp_dbc:postgresql://localhost:5432/testdb"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver->acceptURI("mongodb://localhost:27017/testdb"), cpp_dbc::DBException);
        REQUIRE_THROWS_AS(driver->acceptURI("jdbc:mongodb://localhost:27017/testdb"), cpp_dbc::DBException);
    }

    SECTION("MongoDB driver URI acceptance (nothrow)")
    {
        auto driver = mongodb_test_helpers::getMongoDBDriver();
        REQUIRE(driver != nullptr);

        // Valid MongoDB URIs — has_value() returns true
        auto ok1 = driver->acceptURI(std::nothrow, "cpp_dbc:mongodb://localhost:27017/testdb");
        REQUIRE(ok1.has_value());
        auto ok2 = driver->acceptURI(std::nothrow, "cpp_dbc:mongodb://127.0.0.1:27017/testdb");
        REQUIRE(ok2.has_value());
        auto ok3 = driver->acceptURI(std::nothrow, "cpp_dbc:mongodb://db.example.com:27017/testdb");
        REQUIRE(ok3.has_value());

        // Wrong scheme — has_value() returns false with scheme mismatch error
        auto no1 = driver->acceptURI(std::nothrow, "cpp_dbc:mysql://localhost:3306/testdb");
        REQUIRE_FALSE(no1.has_value());
        auto no2 = driver->acceptURI(std::nothrow, "mongodb://localhost:27017/testdb");
        REQUIRE_FALSE(no2.has_value());
        auto no3 = driver->acceptURI(std::nothrow, "jdbc:mongodb://localhost:27017/testdb");
        REQUIRE_FALSE(no3.has_value());
    }

    SECTION("MongoDB driver connection string parsing")
    {
        // Create a MongoDB driver
        auto driver = mongodb_test_helpers::getMongoDBDriver();
        REQUIRE(driver != nullptr);

        // We can't actually connect to a database in unit tests,
        // but we can verify that the driver correctly parses connection strings

        // This would normally throw a DBException if the database doesn't exist,
        // but we're just testing the URL parsing logic
        REQUIRE_THROWS_AS(
            driver->connect("cpp_dbc:mongodb://localhost:27017/non_existent_db", "user", "pass"),
            cpp_dbc::DBException);
    }

    SECTION("MongoDB driver parseURI - valid URIs")
    {
        auto driver = mongodb_test_helpers::getMongoDBDriver();
        REQUIRE(driver != nullptr);

        // parseURI expects cpp_dbc:mongodb:// prefix
        auto params = driver->parseURI("cpp_dbc:mongodb://localhost:27017/testdb");
        REQUIRE(params.at("host") == "localhost");
        REQUIRE(params.at("port") == "27017");
        REQUIRE(params.at("database") == "testdb");

        // URI with IP address
        params = driver->parseURI("cpp_dbc:mongodb://127.0.0.1:27017/mydb");
        REQUIRE(params.at("host") == "127.0.0.1");
        REQUIRE(params.at("port") == "27017");
        REQUIRE(params.at("database") == "mydb");

        // URI with custom port
        params = driver->parseURI("cpp_dbc:mongodb://dbserver:28017/proddb");
        REQUIRE(params.at("host") == "dbserver");
        REQUIRE(params.at("port") == "28017");
        REQUIRE(params.at("database") == "proddb");

        // URI with IPv6 address
        params = driver->parseURI("cpp_dbc:mongodb://[::1]:27017/testdb");
        REQUIRE(params.at("host") == "::1");
        REQUIRE(params.at("port") == "27017");
        REQUIRE(params.at("database") == "testdb");
    }

    SECTION("MongoDB driver parseURI - invalid URIs")
    {
        auto driver = mongodb_test_helpers::getMongoDBDriver();
        REQUIRE(driver != nullptr);

        // Completely invalid URI
        REQUIRE_THROWS_AS(
            driver->parseURI("not_a_valid_uri"),
            cpp_dbc::DBException);
    }
}
#endif
