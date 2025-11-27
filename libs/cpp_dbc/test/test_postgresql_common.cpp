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

 @file test_postgresql_common.cpp
 @brief Implementation of PostgreSQL test helpers

*/

#include "test_postgresql_common.hpp"

namespace postgresql_test_helpers
{

#if USE_POSTGRESQL

    bool tryCreateDatabase()
    {
        try
        {
#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
            // Load the configuration using DatabaseConfigManager
            std::string config_path = common_test_helpers::getConfigFilePath();
            cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

            // Find the dev_postgresql configuration
            auto dbConfigOpt = configManager.getDatabaseByName("dev_postgresql");
            if (!dbConfigOpt.has_value())
            {
                std::cerr << "PostgreSQL configuration 'dev_postgresql' not found in config file" << std::endl;
                return false;
            }
            const cpp_dbc::config::DatabaseConfig &dbConfig = dbConfigOpt.value().get();

            // Create connection parameters
            std::string type = dbConfig.getType();
            std::string host = dbConfig.getHost();
            int port = dbConfig.getPort();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Get the create database query
            const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();
            std::string createDatabaseQuery = testQueries.getQuery("postgresql", "create_database");
#else
            // Hardcoded values when YAML is not available
            std::string type = "postgresql";
            std::string host = "localhost";
            int port = 5432;
            std::string username = "root";
            std::string password = "dsystems";
            std::string createDatabaseQuery = "CREATE DATABASE IF NOT EXISTS Test01DB";
#endif

            // Create connection string without database name to connect to PostgreSQL server
            std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/postgres";

            // Register the PostgreSQL driver
            cpp_dbc::DriverManager::registerDriver("postgresql", std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDriver>());

            // Attempt to connect to PostgreSQL server
            std::cout << "Attempting to connect to PostgreSQL server to create database..." << std::endl;
            auto conn = cpp_dbc::DriverManager::getConnection(connStr, username, password);

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

    bool canConnectToPostgreSQL()
    {
        try
        {
            // First, try to create the database if it doesn't exist
            if (!postgresql_test_helpers::tryCreateDatabase())
            {
                std::cerr << "Failed to create database, but continuing with connection test..." << std::endl;
            }

#if defined(USE_CPP_YAML) && USE_CPP_YAML == 1
            // Load the configuration using DatabaseConfigManager
            std::string config_path = common_test_helpers::getConfigFilePath();
            cpp_dbc::config::DatabaseConfigManager configManager = cpp_dbc::config::YamlConfigLoader::loadFromFile(config_path);

            // Find the dev_postgresql configuration
            auto dbConfigOpt = configManager.getDatabaseByName("dev_postgresql");
            if (!dbConfigOpt.has_value())
            {
                std::cerr << "PostgreSQL configuration 'dev_postgresql' not found in config file" << std::endl;
                return false;
            }
            const cpp_dbc::config::DatabaseConfig &dbConfig = dbConfigOpt.value().get();

            // Create connection parameters
            std::string type = dbConfig.getType();
            std::string host = dbConfig.getHost();
            int port = dbConfig.getPort();
            std::string database = dbConfig.getDatabase();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();
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

#endif

} // namespace postgresql_test_helpers