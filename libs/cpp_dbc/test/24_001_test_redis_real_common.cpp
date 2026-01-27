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
 *
 * This file is part of the cpp_dbc project and is licensed under the GNU GPL v3.
 * See the LICENSE.md file in the project root for more information.
 *
 * @file test_redis_common.cpp
 * @brief Implementation of Redis test helpers
 *
 */

#include "24_001_test_redis_real_common.hpp"

#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace redis_test_helpers
{

#if USE_REDIS

    cpp_dbc::config::DatabaseConfig getRedisConfig(const std::string &databaseName)
    {
        cpp_dbc::config::DatabaseConfig dbConfig;

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
        // Load the configuration using DatabaseConfigManager
        std::string config_path = common_test_helpers::getConfigFilePath();
        cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

        // Find the requested database configuration
        auto dbConfigOpt = configManager.getDatabaseByName(databaseName);

        if (dbConfigOpt.has_value())
        {
            // Use the configuration from the YAML file
            dbConfig = dbConfigOpt.value().get();
        }
        else
        {
            // Fallback to default values
            dbConfig.setName(databaseName);
            dbConfig.setType("redis");
            dbConfig.setHost("localhost");
            dbConfig.setPort(6379);
            dbConfig.setDatabase("0"); // Redis uses database numbers
            dbConfig.setUsername("");  // Redis authentication
            dbConfig.setPassword("");  // Redis password
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("redis");
        dbConfig.setHost("localhost");
        dbConfig.setPort(6379);
        dbConfig.setDatabase("0"); // Redis uses database numbers
        dbConfig.setUsername("");  // Redis authentication
        dbConfig.setPassword("");  // Redis password
#endif

        return dbConfig;
    }

    std::shared_ptr<cpp_dbc::Redis::RedisDriver> getRedisDriver()
    {
        static std::shared_ptr<cpp_dbc::Redis::RedisDriver> driver =
            std::make_shared<cpp_dbc::Redis::RedisDriver>();
        return driver;
    }

    std::string buildRedisConnectionString(const cpp_dbc::config::DatabaseConfig &dbConfig)
    {
        std::string host = dbConfig.getHost();
        int port = dbConfig.getPort();
        std::string database = dbConfig.getDatabase();

        // Build connection string for Redis
        // Format: cpp_dbc:redis://host:port/database
        std::string connStr = "cpp_dbc:redis://" + host + ":" + std::to_string(port) + "/" + database;

        return connStr;
    }

    std::shared_ptr<cpp_dbc::KVDBConnection> getRedisConnection()
    {
        auto dbConfig = getRedisConfig("test_redis");

        std::string username = dbConfig.getUsername();
        std::string password = dbConfig.getPassword();

        // Build connection string for Redis
        std::string connStr = buildRedisConnectionString(dbConfig);

        auto driver = getRedisDriver();
        return driver->connectKV(connStr, username, password);
    }

    bool canConnectToRedis()
    {
        try
        {
            // Get database configuration
            auto dbConfig = getRedisConfig("test_redis");

            // Get connection parameters
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Build connection string for Redis
            std::string connStr = buildRedisConnectionString(dbConfig);

            // Attempt to connect to Redis
            std::cout << "Attempting to connect to Redis with connection string: " << connStr << std::endl;

            auto driver = getRedisDriver();
            auto conn = driver->connectKV(connStr, username, password);

            if (!conn)
            {
                std::cerr << "Redis connection returned null" << std::endl;
                return false;
            }

            // If we get here, the connection was successful
            std::cout << "Redis connection successful!" << std::endl;

            // Try to ping the server
            std::string pingResult = conn->ping();
            std::cout << "Redis ping result: " << pingResult << std::endl;

            // Close the connection
            conn->close();

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Redis connection error: " << e.what() << std::endl;
            return false;
        }
    }

#endif

} // namespace redis_test_helpers