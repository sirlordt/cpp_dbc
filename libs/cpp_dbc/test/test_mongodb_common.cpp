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

 @file test_mongodb_common.cpp
 @brief Implementation of MongoDB test helpers

*/

#include "test_mongodb_common.hpp"

#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace mongodb_test_helpers
{

#if USE_MONGODB

    cpp_dbc::config::DatabaseConfig getMongoDBConfig(const std::string &databaseName, bool useEmptyDatabase)
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

            // If useEmptyDatabase is true, set the database name to empty
            if (useEmptyDatabase)
            {
                dbConfig.setDatabase("");
            }

            // Add test collection name as option
            dbConfig.setOption("collection__test", "test_collection");
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("mongodb");
            dbConfig.setHost("localhost");
            dbConfig.setPort(27017);
            dbConfig.setDatabase(useEmptyDatabase ? "" : "test_db");
            dbConfig.setUsername(""); // MongoDB often doesn't require auth for local dev
            dbConfig.setPassword("");

            // Add default collection name as option
            dbConfig.setOption("collection__test", "test_collection");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("mongodb");
        dbConfig.setHost("localhost");
        dbConfig.setPort(27017);
        dbConfig.setDatabase(useEmptyDatabase ? "" : "test_db");
        dbConfig.setUsername(""); // MongoDB often doesn't require auth for local dev
        dbConfig.setPassword("");

        // Add default collection name as option
        dbConfig.setOption("collection__test", "test_collection");
#endif

        return dbConfig;
    }

    std::shared_ptr<cpp_dbc::MongoDB::MongoDBDriver> getMongoDBDriver()
    {
        static std::shared_ptr<cpp_dbc::MongoDB::MongoDBDriver> driver =
            std::make_shared<cpp_dbc::MongoDB::MongoDBDriver>();
        return driver;
    }

    std::string buildMongoDBConnectionString(const cpp_dbc::config::DatabaseConfig &dbConfig)
    {
        std::string host = dbConfig.getHost();
        int port = dbConfig.getPort();
        std::string database = dbConfig.getDatabase();
        std::string username = dbConfig.getUsername();
        std::string password = dbConfig.getPassword();

        // Build connection string for MongoDB
        // Format: mongodb://[username:password@]host:port/database?options
        std::string connStr = "mongodb://";
        if (!username.empty() && !password.empty())
        {
            connStr += username + ":" + password + "@";
        }
        connStr += host + ":" + std::to_string(port) + "/" + database;

        // Add query parameters from options
        std::string queryParams;

        // Check for auth_source option
        auto authSource = dbConfig.getOption("auth_source");
        if (!authSource.empty())
        {
            if (!queryParams.empty())
                queryParams += "&";
            queryParams += "authSource=" + authSource;
        }

        // Check for direct_connection option
        auto directConnection = dbConfig.getOption("direct_connection");
        if (!directConnection.empty() && directConnection == "true")
        {
            if (!queryParams.empty())
                queryParams += "&";
            queryParams += "directConnection=true";
        }

        // Check for connect_timeout option
        auto connectTimeout = dbConfig.getOption("connect_timeout");
        if (!connectTimeout.empty())
        {
            if (!queryParams.empty())
                queryParams += "&";
            queryParams += "connectTimeoutMS=" + connectTimeout;
        }

        // Check for server_selection_timeout option
        auto serverSelectionTimeout = dbConfig.getOption("server_selection_timeout");
        if (!serverSelectionTimeout.empty())
        {
            if (!queryParams.empty())
                queryParams += "&";
            queryParams += "serverSelectionTimeoutMS=" + serverSelectionTimeout;
        }

        // Append query parameters if any
        if (!queryParams.empty())
        {
            connStr += "?" + queryParams;
        }

        return connStr;
    }

    std::shared_ptr<cpp_dbc::MongoDB::MongoDBConnection> getMongoDBConnection()
    {
        auto dbConfig = getMongoDBConfig("dev_mongodb");

        std::string username = dbConfig.getUsername();
        std::string password = dbConfig.getPassword();

        // Build connection string for MongoDB
        std::string connStr = buildMongoDBConnectionString(dbConfig);

        auto driver = getMongoDBDriver();
        auto conn = driver->connectDocument(connStr, username, password);

        return std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(conn);
    }

    bool canConnectToMongoDB()
    {
        try
        {
            // Get database configuration
            auto dbConfig = getMongoDBConfig("dev_mongodb");

            // Get connection parameters
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Build connection string for MongoDB
            std::string connStr = buildMongoDBConnectionString(dbConfig);

            // Attempt to connect to MongoDB
            std::cout << "Attempting to connect to MongoDB with connection string: " << connStr << std::endl;

            auto driver = getMongoDBDriver();
            auto conn = driver->connectDocument(connStr, username, password);

            if (!conn)
            {
                std::cerr << "MongoDB connection returned null" << std::endl;
                return false;
            }

            // If we get here, the connection was successful
            std::cout << "MongoDB connection successful!" << std::endl;

            // Try to ping the database
            auto mongoConn = std::dynamic_pointer_cast<cpp_dbc::MongoDB::MongoDBConnection>(conn);
            if (mongoConn)
            {
                // Try to list collections as a simple connectivity test
                auto collections = mongoConn->listCollections();
                std::cout << "MongoDB has " << collections.size() << " collections" << std::endl;
            }

            // Close the connection
            conn->close();

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "MongoDB connection error: " << e.what() << std::endl;
            return false;
        }
    }

    std::string generateTestDocument(int id, const std::string &name, double value)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "{\"id\": " << id
            << ", \"name\": \"" << name << "\""
            << ", \"value\": " << value
            << ", \"timestamp\": " << std::chrono::system_clock::now().time_since_epoch().count()
            << "}";
        return oss.str();
    }

    std::string generateRandomCollectionName()
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);

        std::ostringstream oss;
        oss << "test_collection_";
        for (int i = 0; i < 8; ++i)
        {
            oss << std::hex << dis(gen);
        }
        return oss.str();
    }

#endif

} // namespace mongodb_test_helpers