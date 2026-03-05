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

 @file 21_001_test_postgresql_real_common.cpp
 @brief Implementation of PostgreSQL test helpers

*/

#include "21_001_test_postgresql_real_common.hpp"

#include <cpp_dbc/common/system_utils.hpp>

namespace postgresql_test_helpers
{

#if USE_POSTGRESQL

    cpp_dbc::config::DatabaseConfig getPostgreSQLConfig(const std::string &databaseName, bool useEmptyDatabase)
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
                               testQueries.getQuery("postgresql", "create_table",
                                                    "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE PRECISION)"));

            dbConfig.setOption("query__insert_data",
                               testQueries.getQuery("postgresql", "insert_data",
                                                    "INSERT INTO test_table (id, name, value) VALUES ($1, $2, $3)"));

            dbConfig.setOption("query__select_data",
                               testQueries.getQuery("postgresql", "select_data",
                                                    "SELECT * FROM test_table WHERE id = $1"));

            dbConfig.setOption("query__drop_table",
                               testQueries.getQuery("postgresql", "drop_table",
                                                    "DROP TABLE IF EXISTS test_table"));
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("postgresql");
            dbConfig.setHost("localhost");
            dbConfig.setPort(5432);
            dbConfig.setDatabase(useEmptyDatabase ? "" : "Test01DB");
            dbConfig.setUsername("postgres");
            dbConfig.setPassword("dsystems");

            // Add default queries as options
            dbConfig.setOption("query__create_table",
                               "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE PRECISION)");
            dbConfig.setOption("query__insert_data",
                               "INSERT INTO test_table (id, name, value) VALUES ($1, $2, $3)");
            dbConfig.setOption("query__select_data",
                               "SELECT * FROM test_table WHERE id = $1");
            dbConfig.setOption("query__drop_table",
                               "DROP TABLE IF EXISTS test_table");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("postgresql");
        dbConfig.setHost("localhost");
        dbConfig.setPort(5432);
        dbConfig.setDatabase(useEmptyDatabase ? "" : "Test01DB");
        dbConfig.setUsername("postgres");
        dbConfig.setPassword("dsystems");

        // Add default queries as options
        dbConfig.setOption("query__create_table",
                           "CREATE TABLE IF NOT EXISTS test_table (id INT PRIMARY KEY, name VARCHAR(100), value DOUBLE PRECISION)");
        dbConfig.setOption("query__insert_data",
                           "INSERT INTO test_table (id, name, value) VALUES ($1, $2, $3)");
        dbConfig.setOption("query__select_data",
                           "SELECT * FROM test_table WHERE id = $1");
        dbConfig.setOption("query__drop_table",
                           "DROP TABLE IF EXISTS test_table");
#endif

        return dbConfig;
    }

    std::shared_ptr<cpp_dbc::PostgreSQL::PostgreSQLDBDriver> getPostgreSQLDriver()
    {
        static std::shared_ptr<cpp_dbc::PostgreSQL::PostgreSQLDBDriver> driver =
            std::make_shared<cpp_dbc::PostgreSQL::PostgreSQLDBDriver>();
        return driver;
    }

    std::shared_ptr<cpp_dbc::RelationalDBConnection> getPostgreSQLConnection()
    {
        auto dbConfig = getPostgreSQLConfig("dev_postgresql");

        std::string connStr = dbConfig.createConnectionString();
        std::string username = dbConfig.getUsername();
        std::string password = dbConfig.getPassword();

        auto driver = getPostgreSQLDriver();
        cpp_dbc::DriverManager::registerDriver(driver);
        auto conn = cpp_dbc::DriverManager::getDBConnection(connStr, username, password);

        return std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(conn);
    }

    bool tryCreateDatabase()
    {
        try
        {
            // Get configuration with empty database name (we'll connect to postgres)
            auto dbConfig = getPostgreSQLConfig("dev_postgresql", false);

            // Get connection parameters
            std::string type = dbConfig.getType();
            std::string host = dbConfig.getHost();
            int port = dbConfig.getPort();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Get database name to create (default: Test01DB)
            std::string dbName = dbConfig.getDatabase();

            // Create connection string with postgres database name to connect to PostgreSQL server
            std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/postgres";

            // Register the PostgreSQL driver singleton
            auto driver = getPostgreSQLDriver();
            cpp_dbc::DriverManager::registerDriver(driver);

            // Attempt to connect to PostgreSQL server
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to PostgreSQL server to create database...");
            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            // First check if the database already exists
            std::string checkDatabaseQuery = "SELECT 1 FROM pg_database WHERE datname = '" + dbName + "'";
            auto resultSet = conn->executeQuery(checkDatabaseQuery);

            if (!resultSet->next())
            {
                // Database doesn't exist, create it
                std::string createDatabaseQuery = dbConfig.getOption("query__create_database",
                                                                     "CREATE DATABASE " + dbName);
                try
                {
                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Executing: " + createDatabaseQuery);
                    conn->executeUpdate(createDatabaseQuery);
                    cpp_dbc::system_utils::logWithTimesMillis("TEST", "Database creation successful!");
                }
                catch (const std::exception &ex)
                {
                    // Check if error contains "already exists" message (race condition)
                    std::string error = ex.what();
                    if (error.find("already exists") != std::string::npos)
                    {
                        cpp_dbc::system_utils::logWithTimesMillis("TEST", "Database already exists, continuing...");
                    }
                    else
                    {
                        // This is a different error, rethrow it
                        throw;
                    }
                }
            }
            else
            {
                cpp_dbc::system_utils::logWithTimesMillis("TEST", "Database '" + dbName + "' already exists.");
            }

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

    bool canConnectToPostgreSQL()
    {
        try
        {
            // First, try to create the database if it doesn't exist
            if (!postgresql_test_helpers::tryCreateDatabase())
            {
                cpp_dbc::system_utils::logWithTimesMillis("TEST", "Failed to create database, but continuing with connection test...");
            }

            // Get database configuration
            auto dbConfig = getPostgreSQLConfig("dev_postgresql");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Register the PostgreSQL driver singleton
            auto driver = getPostgreSQLDriver();
            cpp_dbc::DriverManager::registerDriver(driver);

            // Attempt to connect to PostgreSQL
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to PostgreSQL with connection string: " + connStr);
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Username: " + username + ", Password: " + password);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "PostgreSQL connection successful!");

            // Verify the connection with ping()
            bool success = conn->ping();
            cpp_dbc::system_utils::logWithTimesMillis("TEST", std::string("PostgreSQL ping ") + (success ? "successful!" : "returned false"));

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &ex)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "PostgreSQL connection error: " + std::string(ex.what()));
            return false;
        }
    }

#endif

} // namespace postgresql_test_helpers
