#pragma once

#include <cpp_dbc/cpp_dbc.hpp>
#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif
#include <string>
#include <memory>
#include <iostream>
#include <yaml-cpp/yaml.h>

// Helper function to get the path to the test_db_connections.yml file
std::string getConfigFilePath();

namespace mysql_test_helpers
{

#if USE_MYSQL
    // Helper function to try to create the database if it doesn't exist
    static bool tryCreateDatabase()
    {
        try
        {
            // Load the YAML configuration
            std::string config_path = getConfigFilePath();
            YAML::Node config = YAML::LoadFile(config_path);

            // Find the dev_mysql configuration
            YAML::Node dbConfig;
            for (size_t i = 0; i < config["databases"].size(); i++)
            {
                YAML::Node db = config["databases"][i];
                if (db["name"].as<std::string>() == "dev_mysql")
                {
                    dbConfig = YAML::Node(db);
                    break;
                }
            }

            if (!dbConfig.IsDefined())
            {
                std::cerr << "MySQL configuration not found in test_db_connections.yml" << std::endl;
                return false;
            }

            // Create connection parameters
            std::string type = dbConfig["type"].as<std::string>();
            std::string host = dbConfig["host"].as<std::string>();
            int port = dbConfig["port"].as<int>();
            std::string username = dbConfig["username"].as<std::string>();
            std::string password = dbConfig["password"].as<std::string>();

            // Create connection string without database name to connect to MySQL server
            std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/mysql";

            // Register the MySQL driver
            cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDriver>());

            // Attempt to connect to MySQL server
            std::cout << "Attempting to connect to MySQL server to create database..." << std::endl;
            auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

            // Get the create database query
            YAML::Node testQueries = config["test_queries"]["mysql"];
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

    // Helper function to check if we can connect to MySQL
    static bool canConnectToMySQL()
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

            // Find the dev_mysql configuration
            YAML::Node dbConfig;
            for (size_t i = 0; i < config["databases"].size(); i++)
            {
                YAML::Node db = config["databases"][i];
                if (db["name"].as<std::string>() == "dev_mysql")
                {
                    dbConfig = YAML::Node(db);
                    break;
                }
            }

            if (!dbConfig.IsDefined())
            {
                std::cerr << "MySQL configuration not found in test_db_connections.yml" << std::endl;
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

            // Register the MySQL driver
            cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDriver>());

            // Attempt to connect to MySQL
            std::cout << "Attempting to connect to MySQL with connection string: " << connStr << std::endl;
            std::cout << "Username: " << username << ", Password: " << password << std::endl;

            auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

            // If we get here, the connection was successful
            std::cout << "MySQL connection successful!" << std::endl;

            // Execute a simple query to verify the connection
            auto resultSet = conn->executeQuery("SELECT 1 as test_value");
            bool success = resultSet->next() && resultSet->getInt("test_value") == 1;

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &e)
        {
            std::cerr << "MySQL connection error: " << e.what() << std::endl;
            return false;
        }
    }

#else
    static bool tryCreateDatabase()
    {
        std::cerr << "MySQL support is not enabled" << std::endl;
        return false;
    }

    static bool canConnectToMySQL()
    {
        std::cerr << "MySQL support is not enabled" << std::endl;
        return false;
    }
#endif

} // namespace mysql_test_helpers