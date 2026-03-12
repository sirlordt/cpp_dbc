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

 @file 20_001_test_mysql_real_common.cpp
 @brief Implementation of MySQL test helpers

*/

#include "20_001_test_mysql_real_common.hpp"

#include <cpp_dbc/common/system_utils.hpp>

namespace mysql_test_helpers
{

#if USE_MYSQL

    cpp_dbc::config::DatabaseConfig getMySQLConfig(const std::string &databaseName, bool useEmptyDatabase)
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

            // Get test queries from YAML config
            const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();

            // Add queries as options to DatabaseConfig
            dbConfig.setOption("query__create_table",
                               testQueries.getQuery("mysql", "create_table",
                                                    "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE)"));

            dbConfig.setOption("query__insert_data",
                               testQueries.getQuery("mysql", "insert_data",
                                                    "INSERT INTO test_table (id, name, value) VALUES (?, ?, ?)"));

            dbConfig.setOption("query__select_data",
                               testQueries.getQuery("mysql", "select_data",
                                                    "SELECT * FROM test_table WHERE id = ?"));

            dbConfig.setOption("query__drop_table",
                               testQueries.getQuery("mysql", "drop_table",
                                                    "DROP TABLE IF EXISTS test_table"));
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("mysql");
            dbConfig.setHost("localhost");
            dbConfig.setPort(3306);
            dbConfig.setDatabase(useEmptyDatabase ? "" : "Test01DB");
            dbConfig.setUsername("root");
            dbConfig.setPassword("dsystems");

            // Add default queries as options
            dbConfig.setOption("query__create_table",
                               "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE)");
            dbConfig.setOption("query__insert_data",
                               "INSERT INTO test_table (id, name, value) VALUES (?, ?, ?)");
            dbConfig.setOption("query__select_data",
                               "SELECT * FROM test_table WHERE id = ?");
            dbConfig.setOption("query__drop_table",
                               "DROP TABLE IF EXISTS test_table");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("mysql");
        dbConfig.setHost("localhost");
        dbConfig.setPort(3306);
        dbConfig.setDatabase(useEmptyDatabase ? "" : "Test01DB");
        dbConfig.setUsername("root");
        dbConfig.setPassword("dsystems");

        // Add default queries as options
        dbConfig.setOption("query__create_table",
                           "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE)");
        dbConfig.setOption("query__insert_data",
                           "INSERT INTO test_table (id, name, value) VALUES (?, ?, ?)");
        dbConfig.setOption("query__select_data",
                           "SELECT * FROM test_table WHERE id = ?");
        dbConfig.setOption("query__drop_table",
                           "DROP TABLE IF EXISTS test_table");
#endif

        return dbConfig;
    }

    std::shared_ptr<cpp_dbc::MySQL::MySQLDBDriver> getMySQLDriver()
    {
        static std::shared_ptr<cpp_dbc::MySQL::MySQLDBDriver> driver =
            cpp_dbc::MySQL::MySQLDBDriver::getInstance(std::nothrow).value_or(nullptr);
        return driver;
    }

    std::shared_ptr<cpp_dbc::RelationalDBConnection> getMySQLConnection()
    {
        auto dbConfig = getMySQLConfig("dev_mysql");

        std::string connStr = dbConfig.createConnectionString();
        std::string username = dbConfig.getUsername();
        std::string password = dbConfig.getPassword();

        auto driver = getMySQLDriver();
        cpp_dbc::DriverManager::registerDriver(driver);
        auto conn = cpp_dbc::DriverManager::getDBConnection(connStr, username, password);

        return std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(conn);
    }

    bool tryCreateDatabase()
    {
        try
        {
            // Get configuration with empty database name (we'll connect to mysql)
            auto dbConfig = getMySQLConfig("dev_mysql", true);

            // Get connection parameters
            std::string type = dbConfig.getType();
            std::string host = dbConfig.getHost();
            int port = dbConfig.getPort();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Get database name to create (default: Test01DB)
            std::string dbName = dbConfig.getOption("database_name", "Test01DB");

            // Get or create the CREATE DATABASE query
            std::string createDatabaseQuery = dbConfig.getOption("query__create_database",
                                                                 "CREATE DATABASE IF NOT EXISTS " + dbName);

            // Create connection string without database name to connect to MySQL server
            std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/mysql";

            // Register the MySQL driver singleton
            auto driver = getMySQLDriver();
            cpp_dbc::DriverManager::registerDriver(driver);

            // Attempt to connect to MySQL server
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to MySQL server to create database...");
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            // Execute the create database query
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Executing: " + createDatabaseQuery);
            conn->executeUpdate(createDatabaseQuery);
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Database creation successful or database already exists!");

            // Close the connection
            conn->close();

            return true;
        }
        catch (const std::exception &ex)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Database creation error: " + std::string(ex.what()));
            return false;
        }
    }

    bool canConnectToMySQL()
    {
        try
        {
            // First, try to create the database if it doesn't exist
            if (!mysql_test_helpers::tryCreateDatabase())
            {
                cpp_dbc::system_utils::logWithTimesMillis("TEST", "Failed to create database, but continuing with connection test...");
            }

            // Get database configuration
            auto dbConfig = getMySQLConfig("dev_mysql");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Register the MySQL driver singleton
            auto driver = getMySQLDriver();
            cpp_dbc::DriverManager::registerDriver(driver);

            // Attempt to connect to MySQL
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to MySQL with connection string: " + connStr);
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Username: " + username + ", Password: " + password);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "MySQL connection successful!");

            // Verify the connection with ping()
            bool success = conn->ping();
            cpp_dbc::system_utils::logWithTimesMillis("TEST", std::string("MySQL ping ") + (success ? "successful!" : "returned false"));

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &ex)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "MySQL connection error: " + std::string(ex.what()));
            return false;
        }
    }

#endif

} // namespace mysql_test_helpers
