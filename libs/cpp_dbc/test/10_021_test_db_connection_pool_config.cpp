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

 @file test_db_connection_pool_config.cpp
 @brief Tests for DBConnectionPoolConfig class

*/

#include <string>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

// Test case for DBConnectionPoolConfig
TEST_CASE("DBConnectionPoolConfig tests", "[10_021_01_connection_pool_config]")
{
    SECTION("Default constructor sets default values")
    {
        INFO("Default constructor sets default values");
        cpp_dbc::config::DBConnectionPoolConfig config;

        REQUIRE(config.getInitialSize() == 5);
        REQUIRE(config.getMaxSize() == 20);
        REQUIRE(config.getMinIdle() == 3);
        REQUIRE(config.getConnectionTimeout() == 30000);
        REQUIRE(config.getIdleTimeout() == 300000);
        REQUIRE(config.getValidationInterval() == 5000);
        REQUIRE(config.getMaxLifetimeMillis() == 1800000);
        REQUIRE(config.getTestOnBorrow() == true);
        REQUIRE(config.getTestOnReturn() == false);
        REQUIRE(config.getValidationQuery() == "SELECT 1");
    }

    SECTION("Constructor with basic parameters")
    {
        INFO("Constructor with basic parameters");
        cpp_dbc::config::DBConnectionPoolConfig config(
            "test_pool", 10, 50, 10000, 60000, 15000);

        REQUIRE(config.getName() == "test_pool");
        REQUIRE(config.getInitialSize() == 10);
        REQUIRE(config.getMaxSize() == 50);
        REQUIRE(config.getConnectionTimeout() == 10000);
        REQUIRE(config.getIdleTimeout() == 60000);
        REQUIRE(config.getValidationInterval() == 15000);

        // Check default values for parameters not specified
        REQUIRE(config.getMinIdle() == 3);
        REQUIRE(config.getMaxLifetimeMillis() == 1800000);
        REQUIRE(config.getTestOnBorrow() == true);
        REQUIRE(config.getTestOnReturn() == false);
        REQUIRE(config.getValidationQuery() == "SELECT 1");
    }

    SECTION("Full constructor with all parameters")
    {
        INFO("Full constructor with all parameters");
        cpp_dbc::config::DBConnectionPoolConfig config(
            "full_pool", "cpp_dbc:mysql://localhost:3306/test", "user", "pass",
            15, 100, 5, 20000, 120000, 30000, 3600000, false, true, "SELECT version()");

        REQUIRE(config.getName() == "full_pool");
        REQUIRE(config.getUrl() == "cpp_dbc:mysql://localhost:3306/test");
        REQUIRE(config.getUsername() == "user");
        REQUIRE(config.getPassword() == "pass");
        REQUIRE(config.getInitialSize() == 15);
        REQUIRE(config.getMaxSize() == 100);
        REQUIRE(config.getMinIdle() == 5);
        REQUIRE(config.getConnectionTimeout() == 20000);
        REQUIRE(config.getIdleTimeout() == 120000);
        REQUIRE(config.getValidationInterval() == 30000);
        REQUIRE(config.getMaxLifetimeMillis() == 3600000);
        REQUIRE(config.getTestOnBorrow() == false);
        REQUIRE(config.getTestOnReturn() == true);
        REQUIRE(config.getValidationQuery() == "SELECT version()");
    }

    SECTION("Setters and getters")
    {
        INFO("Setters and getters");
        cpp_dbc::config::DBConnectionPoolConfig config;

        config.setName("setter_test");
        config.setUrl("cpp_dbc:postgresql://localhost:5432/test");
        config.setUsername("postgres");
        config.setPassword("postgres");
        config.setInitialSize(8);
        config.setMaxSize(30);
        config.setMinIdle(4);
        config.setConnectionTimeout(15000);
        config.setIdleTimeout(90000);
        config.setValidationInterval(10000);
        config.setMaxLifetimeMillis(2400000);
        config.setTestOnBorrow(false);
        config.setTestOnReturn(true);
        config.setValidationQuery("SELECT 2");

        REQUIRE(config.getName() == "setter_test");
        REQUIRE(config.getUrl() == "cpp_dbc:postgresql://localhost:5432/test");
        REQUIRE(config.getUsername() == "postgres");
        REQUIRE(config.getPassword() == "postgres");
        REQUIRE(config.getInitialSize() == 8);
        REQUIRE(config.getMaxSize() == 30);
        REQUIRE(config.getMinIdle() == 4);
        REQUIRE(config.getConnectionTimeout() == 15000);
        REQUIRE(config.getIdleTimeout() == 90000);
        REQUIRE(config.getValidationInterval() == 10000);
        REQUIRE(config.getMaxLifetimeMillis() == 2400000);
        REQUIRE(config.getTestOnBorrow() == false);
        REQUIRE(config.getTestOnReturn() == true);
        REQUIRE(config.getValidationQuery() == "SELECT 2");
    }

    SECTION("withDatabaseConfig method")
    {
        INFO("withDatabaseConfig method");
        // Create a database config
        cpp_dbc::config::DatabaseConfig dbConfig(
            "test_db", "mysql", "localhost", 3306, "testdb", "root", "password");

        // Create a connection pool config and apply the database config
        cpp_dbc::config::DBConnectionPoolConfig poolConfig;
        poolConfig.withDatabaseConfig(dbConfig);

        // Check that the database config values were applied
        REQUIRE(poolConfig.getUrl() == "cpp_dbc:mysql://localhost:3306/testdb");
        REQUIRE(poolConfig.getUsername() == "root");
        REQUIRE(poolConfig.getPassword() == "password");
    }
}
