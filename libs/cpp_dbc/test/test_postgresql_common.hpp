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

 @file test_postgresql_common.hpp
 @brief Tests for PostgreSQL database operations

*/

#pragma once

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif
#include <string>
#include <memory>
#include <iostream>
#include <yaml-cpp/yaml.h>
#include <optional>

// Helper function to get the path to the test_db_connections.yml file
std::string getConfigFilePath();

namespace postgresql_test_helpers
{

#if USE_POSTGRESQL
    // Helper function to try to create the database if it doesn't exist
    static bool tryCreateDatabase()
    {
        try
        {
            // Load the YAML configuration
            std::string config_path = getConfigFilePath();
            YAML::Node config = YAML::LoadFile(config_path);

            // Find the dev_postgresql configuration
            YAML::Node dbConfig;
            for (size_t i = 0; i < config["databases"].size(); i++)
            {
                YAML::Node db = config["databases"][i];
                if (db["name"].as<std::string>() == "dev_postgresql")
                {
                    dbConfig = YAML::Node(db);
                    break;
                }
            }

            if (!dbConfig.IsDefined())
            {
                std::cerr << "PostgreSQL configuration not found in test_db_connections.yml" << std::endl;
                return false;
            }

            // Create connection parameters
            std::string type = dbConfig["type"].as<std::string>();
            std::string host = dbConfig["host"].as<std::string>();
            int port = dbConfig["port"].as<int>();
            std::string username = dbConfig["username"].as<std::string>();
            std::string password = dbConfig["password"].as<std::string>();

            // Create connection string without database name to connect to PostgreSQL server
            std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/postgres";

            // Register the PostgreSQL driver
            cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

            // Attempt to connect to PostgreSQL server
            std::cout << "Attempting to connect to PostgreSQL server to create database..." << std::endl;
            auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

            // Get the create database query
            YAML::Node testQueries = config["test_queries"]["postgresql"];
            std::string createDatabaseQuery = testQueries["create_database"].as<std::string>();

            // Execute the create database query
            std::cout << "Executing: " << createDatabaseQuery << std::endl;
            conn->executeUpdate(createDatabaseQuery);
            std::cout << "Database creation successful or database already exists!" << std::endl;

            // Close the connection
            conn->close();

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Database creation error: " << e.what() << std::endl;
            return false;
        }
    }

    // Helper function to check if we can connect to PostgreSQL
    static bool canConnectToPostgreSQL()
    {
        try
        {
            // First, try to create the database if it doesn't exist
            if (!tryCreateDatabase())
            {
                std::cerr << "Failed to create database, but continuing with connection test..." << std::endl;
            }

            // Load the YAML configuration
            std::string config_path = getConfigFilePath();
            YAML::Node config = YAML::LoadFile(config_path);

            // Find the dev_postgresql configuration
            YAML::Node dbConfig;
            for (size_t i = 0; i < config["databases"].size(); i++)
            {
                YAML::Node db = config["databases"][i];
                if (db["name"].as<std::string>() == "dev_postgresql")
                {
                    dbConfig = YAML::Node(db);
                    break;
                }
            }

            if (!dbConfig.IsDefined())
            {
                std::cerr << "PostgreSQL configuration not found in test_db_connections.yml" << std::endl;
                return false;
            }

            // Create connection string
            std::string type = dbConfig["type"].as<std::string>();
            std::string host = dbConfig["host"].as<std::string>();
            int port = dbConfig["port"].as<int>();
            std::string database = dbConfig["database"].as<std::string>();
            std::string username = dbConfig["username"].as<std::string>();
            std::string password = dbConfig["password"].as<std::string>();

            std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

            // Register the PostgreSQL driver
            cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

            // Attempt to connect to PostgreSQL
            std::cout << "Attempting to connect to PostgreSQL with connection string: " << connStr << std::endl;
            std::cout << "Username: " << username << ", Password: " << password << std::endl;

            auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

            // If we get here, the connection was successful
            std::cout << "PostgreSQL connection successful!" << std::endl;

            // Execute a simple query to verify the connection
            auto resultSet = conn->executeQuery("SELECT 1 as test_value");
            bool success = resultSet->next() && resultSet->getInt("test_value") == 1;

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &e)
        {
            std::cerr << "PostgreSQL connection error: " << e.what() << std::endl;
            return false;
        }
    }

#else
    static bool tryCreateDatabase()
    {
        std::cerr << "PostgreSQL support is not enabled" << std::endl;
        return false;
    }

    static bool canConnectToPostgreSQL()
    {
        std::cerr << "PostgreSQL support is not enabled" << std::endl;
        return false;
    }
#endif

} // namespace postgresql_test_helpers