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
 * @file benchmark_common.hpp
 * @brief Common utilities for cpp_dbc benchmarks
 */

#pragma once

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/benchmark/catch_benchmark_all.hpp>

#include <cpp_dbc/cpp_dbc.hpp>
#include <cpp_dbc/connection_pool.hpp>
#include <cpp_dbc/transaction_manager.hpp>
#include <cpp_dbc/config/database_config.hpp>

#if USE_MYSQL
#include <cpp_dbc/drivers/driver_mysql.hpp>
#endif

#if USE_POSTGRESQL
#include <cpp_dbc/drivers/driver_postgresql.hpp>
#endif

#if USE_SQLITE
#include <cpp_dbc/drivers/driver_sqlite.hpp>
#endif

#include <string>
#include <memory>
#include <vector>
#include <chrono>
#include <iostream>
#include <random>

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
#include <cpp_dbc/config/yaml_config_loader.hpp>
#endif

// Import helper functions from test directory
extern std::string getConfigFilePath();

namespace benchmark_helpers
{

    // Data sizes for benchmarks
    constexpr int SMALL_SIZE = 10;
    constexpr int MEDIUM_SIZE = 100;
    constexpr int LARGE_SIZE = 1000;
    constexpr int XLARGE_SIZE = 10000;

    // Helper function to generate random string data
    inline std::string generateRandomString(size_t length)
    {
        static const std::string chars =
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789";

        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<std::string::size_type> dist(0, chars.size() - 1);

        std::string result;
        result.reserve(length);

        for (size_t i = 0; i < length; ++i)
        {
            result += chars[dist(gen)];
        }

        return result;
    }

    // Helper function to create test tables for benchmarks
    template <typename ConnectionType>
    void createBenchmarkTable(ConnectionType &conn, const std::string &tableName)
    {
        // Drop table if it exists
        try
        {
            conn->executeUpdate("DROP TABLE IF EXISTS " + tableName);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error dropping table: " << e.what() << std::endl;
        }

        // Create table with standard columns for benchmarks
        try
        {
            conn->executeUpdate(
                "CREATE TABLE " + tableName + " ("
                                              "id INT PRIMARY KEY, "
                                              "name VARCHAR(100), "
                                              "value DOUBLE, "
                                              "description TEXT, "
                                              "created_at TIMESTAMP"
                                              ")");
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error creating table: " << e.what() << std::endl;
            throw; // Re-throw to fail the test
        }
    }

    // Helper function to drop test tables after benchmarks
    template <typename ConnectionType>
    void dropBenchmarkTable(ConnectionType &conn, const std::string &tableName)
    {
        try
        {
            conn->executeUpdate("DROP TABLE IF EXISTS " + tableName);
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error dropping table: " << e.what() << std::endl;
        }
    }

    // Helper function to populate a table with a specific number of rows
    template <typename ConnectionType>
    void populateTable(ConnectionType &conn, const std::string &tableName, int rowCount)
    {
        // Prepare the insert statement
        auto pstmt = conn->prepareStatement(
            "INSERT INTO " + tableName + " (id, name, value, description, created_at) VALUES (?, ?, ?, ?, CURRENT_TIMESTAMP)");

        // Insert the specified number of rows
        for (int i = 1; i <= rowCount; ++i)
        {
            pstmt->setInt(1, i);
            pstmt->setString(2, "Name " + std::to_string(i));
            pstmt->setDouble(3, i * 1.5);
            pstmt->setString(4, generateRandomString(50));
            pstmt->executeUpdate();
        }
    }

// Helper function to check if we can connect to MySQL
#if USE_MYSQL
    inline bool canConnectToMySQL()
    {
        try
        {
#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
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
#else
            // Hardcoded values when YAML is not available
            std::string type = "mysql";
            std::string host = "localhost";
            int port = 3306;
            std::string database = "Test01DB";
            std::string username = "root";
            std::string password = "dsystems";
#endif

            std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

            // Register the MySQL driver
            cpp_dbc::DriverManager::registerDriver("mysql", std::make_shared<cpp_dbc::MySQL::MySQLDriver>());

            // Attempt to connect to MySQL
            auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

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
#endif

// Helper function to check if we can connect to PostgreSQL
#if USE_POSTGRESQL
    inline bool canConnectToPostgreSQL()
    {
        try
        {
#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
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
#else
            // Hardcoded values when YAML is not available
            std::string type = "postgresql";
            std::string host = "localhost";
            int port = 5432;
            std::string database = "Test01DB";
            std::string username = "root";
            std::string password = "dsystems";
#endif

            std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

            // Register the PostgreSQL driver
            cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

            // Attempt to connect to PostgreSQL
            auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

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
#endif

// Helper function to check if we can connect to SQLite
#if USE_SQLITE
    inline bool canConnectToSQLite()
    {
        try
        {
#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
            // Load the YAML configuration
            std::string config_path = getConfigFilePath();
            YAML::Node config = YAML::LoadFile(config_path);

            // Find the dev_sqlite configuration
            YAML::Node dbConfig;
            for (size_t i = 0; i < config["databases"].size(); i++)
            {
                YAML::Node db = config["databases"][i];
                if (db["name"].as<std::string>() == "dev_sqlite")
                {
                    dbConfig = YAML::Node(db);
                    break;
                }
            }

            if (!dbConfig.IsDefined())
            {
                std::cerr << "SQLite configuration not found in test_db_connections.yml" << std::endl;
                return false;
            }

            // Create connection string
            std::string type = dbConfig["type"].as<std::string>();
            std::string database = dbConfig["database"].as<std::string>();
#else
            // Hardcoded values when YAML is not available
            std::string type = "sqlite";
            std::string database = ":memory:";
#endif

            std::string connStr = "cpp_dbc:" + type + "://" + database;

            // Register the SQLite driver
            cpp_dbc::DriverManager::registerDriver("sqlite", std::make_shared<cpp_dbc::SQLite::SQLiteDriver>());

            // Attempt to connect to SQLite
            auto conn = cpp_dbc::DriverManager::getConnection(connStr, "", "");

            // Execute a simple query to verify the connection
            auto resultSet = conn->executeQuery("SELECT 1 as test_value");
            bool success = resultSet->next() && resultSet->getInt("test_value") == 1;

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &e)
        {
            std::cerr << "SQLite connection error: " << e.what() << std::endl;
            return false;
        }
    }
#endif

} // namespace benchmark_helpers