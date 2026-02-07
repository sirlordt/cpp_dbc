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

 @file 10_061_test_integration.cpp
 @brief Real database integration tests for all available drivers

*/

#include <string>
#include <memory>
#include <iostream>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/config/database_config.hpp>

#include "20_001_test_mysql_real_common.hpp"
#include "21_001_test_postgresql_real_common.hpp"
#include "22_001_test_sqlite_real_common.hpp"
#include "23_001_test_firebird_real_common.hpp"
#include "24_001_test_redis_real_common.hpp"
#include "25_001_test_mongodb_real_common.hpp"
#include "26_001_test_scylladb_real_common.hpp"

// Test case for loading the actual test_db_connections.yml file
TEST_CASE("Load and use test_db_connections.yml", "[10_061_01_integration]")
{
    SECTION("Load test_db_connections.yml")
    {
        // Load the YAML configuration
        std::string config_path = common_test_helpers::getConfigFilePath();
        std::ifstream file(config_path);
        REQUIRE(file.good());
        file.close();

        // Check that the file exists and can be opened
        REQUIRE(std::ifstream(config_path).good());

        // We'll skip the actual YAML parsing here since it depends on the YAML-CPP library
        // which might not be available in all builds
    }
}

// Test case for real database integration with all available drivers
TEST_CASE("Real database integration with all drivers", "[10_061_02_integration]")
{
// Register all available drivers
#if USE_MYSQL
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::MySQL::MySQLDBDriver>());
    // std::cout << "MySQL driver registered" << std::endl;
#endif

#if USE_POSTGRESQL
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>());
    // std::cout << "PostgreSQL driver registered" << std::endl;
#endif

#if USE_SQLITE
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());
    // std::cout << "SQLite driver registered" << std::endl;
#endif

#if USE_FIREBIRD
    cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());
    // std::cout << "Firebird driver registered" << std::endl;
#endif

#if USE_MONGODB
    auto mongoDriver = std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>();
    cpp_dbc::DriverManager::registerDriver(mongoDriver);
    // std::cout << "MongoDB driver registered" << std::endl;
#endif

#if USE_SCYLLADB
    auto scyllaDriver = std::make_shared<cpp_dbc::ScyllaDB::ScyllaDBDriver>();
    cpp_dbc::DriverManager::registerDriver(scyllaDriver);
    // std::cout << "ScyllaDB driver registered" << std::endl;
#endif

#if USE_REDIS
    auto redisDriver = std::make_shared<cpp_dbc::Redis::RedisDriver>();
    cpp_dbc::DriverManager::registerDriver(redisDriver);
    // std::cout << "Redis driver registered" << std::endl;
#endif

    SECTION("Test connection to all available databases")
    {
#if USE_CPP_YAML
        // Load the configuration using DatabaseConfigManager
        std::string config_path = common_test_helpers::getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Get all database configurations
        const auto &allDatabases = configManager.getAllDatabases();

        for (const auto &dbConfig : allDatabases)
        {
            std::string name = dbConfig.getName();
            std::string type = dbConfig.getType();

            // Get the driver to check its database type
            auto driverOpt = cpp_dbc::DriverManager::getDriver(type);
            if (!driverOpt.has_value())
            {
                WARN("Driver not found for type: " << type);
                continue;
            }

            // Skip non-relational databases (they cannot be tested with SQL queries)
            if (driverOpt.value()->getDBType() != cpp_dbc::DBType::RELATIONAL)
            {
                WARN("Skipping non-relational database: " << name << " (" << type << ")");
                continue;
            }

            // std::cout << "Testing connection to " << name << " (" << type << ")" << std::endl;

            try
            {
                // Create connection string
                std::string connStr = dbConfig.createConnectionString();
                std::string username = (type == "sqlite") ? "" : dbConfig.getUsername();
                std::string password = (type == "sqlite") ? "" : dbConfig.getPassword();

                // Note: MongoDB authSource handling was removed as MongoDB is non-relational
                // and is skipped earlier in this test (line 124-128)

                if (type == "sqlite")
                {
                    try
                    {
                        // Attempt to connect
                        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                        REQUIRE(conn != nullptr);

                        // Execute a simple query to verify the connection
                        auto resultSet = conn->executeQuery("SELECT 1 as test_value");

                        // Assert that we got a result row
                        REQUIRE(resultSet->next());
                        REQUIRE(resultSet->getInt("test_value") == 1);

                        // Close the connection
                        conn->close();
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "DEBUG: SQLite exception: " << e.what() << std::endl;
                        throw; // Re-throw to be caught by the outer catch block
                    }
                    catch (...)
                    {
                        std::cerr << "DEBUG: Unknown SQLite exception occurred" << std::endl;
                        throw; // Re-throw to be caught by the outer catch block
                    }
                }
                else if (type == "firebird")
                {
                    try
                    {
                        // Attempt to connect
                        auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                        REQUIRE(conn != nullptr);

                        // Execute a simple query to verify the connection
                        // Firebird uses "SELECT 1 FROM RDB$DATABASE" for a simple test query
                        auto resultSet = conn->executeQuery("SELECT 1 as test_value FROM RDB$DATABASE");

                        // Assert that we got a result row
                        REQUIRE(resultSet->next());
                        // Firebird returns uppercase column names
                        REQUIRE(resultSet->getInt("TEST_VALUE") == 1);

                        // Close the connection
                        conn->close();
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "DEBUG: Firebird exception: " << e.what() << std::endl;
                        throw; // Re-throw to be caught by the outer catch block
                    }
                    catch (...)
                    {
                        std::cerr << "DEBUG: Unknown Firebird exception occurred" << std::endl;
                        throw; // Re-throw to be caught by the outer catch block
                    }
                }
                else
                {
                    // Attempt to connect
                    auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                    REQUIRE(conn != nullptr);

                    // Execute a simple query to verify the connection
                    auto resultSet = conn->executeQuery("SELECT 1 as test_value");

                    // Assert that we got a result row
                    REQUIRE(resultSet->next());
                    REQUIRE(resultSet->getInt("test_value") == 1);

                    // Close the connection
                    conn->close();
                }
            }
            catch (const cpp_dbc::DBException &e)
            {
                // std::cout << "Connection to " << name << " failed: " << e.what() << std::endl;
                // We'll just warn instead of failing the test, since the database might not be available
                WARN("Connection to " << name << " failed: " << e.what_s());
            }
        }
#else
        // Skip this test if YAML is not enabled
        SKIP("YAML support is not enabled, cannot load database configurations");
#endif
    }
}
