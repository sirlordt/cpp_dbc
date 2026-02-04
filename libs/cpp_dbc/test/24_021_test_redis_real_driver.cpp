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

 @file 24_021_test_redis_real_driver.cpp
 @brief Tests for Redis driver

*/

#include <string>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>

#if USE_REDIS

#include "24_001_test_redis_real_common.hpp"
// Test case for Redis driver
TEST_CASE("Redis driver tests", "[24_021_01_redis_real_driver]")
{
    SECTION("Redis driver URL acceptance")
    {
        // Create a Redis driver
        cpp_dbc::Redis::RedisDriver driver;

        // Check that it accepts Redis URLs
        REQUIRE(driver.acceptsURL("cpp_dbc:redis://localhost:6379/0"));
        REQUIRE(driver.acceptsURL("cpp_dbc:redis://127.0.0.1:6379/0"));
        REQUIRE(driver.acceptsURL("cpp_dbc:redis://db.example.com:6379/1"));
        REQUIRE(driver.acceptsURL("cpp_dbc:redis://localhost:6379/15"));

        // Check that it rejects non-Redis URLs
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:mysql://localhost:3306/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:postgresql://localhost:5432/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("cpp_dbc:mongodb://localhost:27017/testdb"));
        REQUIRE_FALSE(driver.acceptsURL("redis://localhost:6379/0"));
        REQUIRE_FALSE(driver.acceptsURL("jdbc:redis://localhost:6379/0"));
    }

    SECTION("Redis driver connection with config credentials")
    {
        // Skip if we can't connect to Redis
        if (!redis_test_helpers::canConnectToRedis())
        {
            SKIP("Cannot connect to Redis database");
        }

        // Get Redis configuration using the helper function
        auto dbConfig = redis_test_helpers::getRedisConfig("dev_redis");

        // Create connection parameters
        std::string connStr = redis_test_helpers::buildRedisConnectionString(dbConfig);
        std::string username = dbConfig.getUsername();
        std::string password = dbConfig.getPassword();

        // Create a Redis driver and connect
        cpp_dbc::Redis::RedisDriver driver;
        auto conn = driver.connectKV(connStr, username, password);

        // Verify connection is valid
        REQUIRE(conn != nullptr);

        // Test ping
        std::string pingResult = conn->ping();
        REQUIRE(pingResult == "PONG");

        // Close the connection
        conn->close();
    }

    SECTION("Redis driver rejects invalid URL format")
    {
        cpp_dbc::Redis::RedisDriver driver;

        // Invalid URL format should throw
        REQUIRE_THROWS_AS(
            driver.connectKV("invalid://localhost:6379/0", "", ""),
            cpp_dbc::DBException);
    }

    SECTION("Redis driver parseURI - valid URIs")
    {
        cpp_dbc::Redis::RedisDriver driver;

        // Full URI with host, port, and db index
        auto params = driver.parseURI("cpp_dbc:redis://localhost:6379/0");
        REQUIRE(params.at("host") == "localhost");
        REQUIRE(params.at("port") == "6379");
        REQUIRE(params.at("db") == "0");

        // URI with custom port and db index
        params = driver.parseURI("cpp_dbc:redis://myhost:6380/5");
        REQUIRE(params.at("host") == "myhost");
        REQUIRE(params.at("port") == "6380");
        REQUIRE(params.at("db") == "5");

        // URI without db index (should default to 0)
        params = driver.parseURI("cpp_dbc:redis://localhost:6379");
        REQUIRE(params.at("host") == "localhost");
        REQUIRE(params.at("port") == "6379");
        REQUIRE(params.at("db") == "0");

        // URI with IPv6 address
        params = driver.parseURI("cpp_dbc:redis://[::1]:6379/0");
        REQUIRE(params.at("host") == "::1");
        REQUIRE(params.at("port") == "6379");
        REQUIRE(params.at("db") == "0");
    }

    SECTION("Redis driver parseURI - invalid URIs")
    {
        cpp_dbc::Redis::RedisDriver driver;

        // Completely invalid URI
        REQUIRE_THROWS_AS(
            driver.parseURI("not_a_valid_uri"),
            cpp_dbc::DBException);
    }
}
#endif
