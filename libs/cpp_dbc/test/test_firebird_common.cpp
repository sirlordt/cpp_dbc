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

 @file test_firebird_common.cpp
 @brief Implementation of Firebird test helpers

*/

#include "test_firebird_common.hpp"

namespace firebird_test_helpers
{

#if USE_FIREBIRD

    cpp_dbc::config::DatabaseConfig getFirebirdConfig(const std::string &databaseName)
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

            // Get test queries from YAML config
            const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();

            // Add queries as options to DatabaseConfig
            // Firebird uses different syntax - no IF NOT EXISTS, uses DOUBLE PRECISION
            dbConfig.setOption("query__connection_test",
                               testQueries.getQuery("firebird", "connection_test",
                                                    "SELECT 1 AS TEST_VALUE FROM RDB$DATABASE"));

            // Use RECREATE TABLE - Firebird's equivalent of DROP TABLE IF EXISTS + CREATE TABLE
            dbConfig.setOption("query__create_table",
                               testQueries.getQuery("firebird", "create_table",
                                                    "RECREATE TABLE test_table (id INTEGER NOT NULL PRIMARY KEY, name VARCHAR(100), num_value DOUBLE PRECISION)"));

            dbConfig.setOption("query__insert_data",
                               testQueries.getQuery("firebird", "insert_data",
                                                    "INSERT INTO test_table (id, name, num_value) VALUES (?, ?, ?)"));

            dbConfig.setOption("query__select_data",
                               testQueries.getQuery("firebird", "select_data",
                                                    "SELECT * FROM test_table WHERE id = ?"));

            dbConfig.setOption("query__drop_table",
                               testQueries.getQuery("firebird", "drop_table",
                                                    "DROP TABLE test_table"));
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("firebird");
            dbConfig.setHost("localhost");
            dbConfig.setPort(3050);
            dbConfig.setDatabase("/tmp/test_firebird.fdb");
            dbConfig.setUsername("SYSDBA");
            dbConfig.setPassword("dsystems");

            // Add default queries as options
            dbConfig.setOption("query__connection_test",
                               "SELECT 1 AS TEST_VALUE FROM RDB$DATABASE");
            // Use RECREATE TABLE - Firebird's equivalent of DROP TABLE IF EXISTS + CREATE TABLE
            dbConfig.setOption("query__create_table",
                               "RECREATE TABLE test_table (id INTEGER NOT NULL PRIMARY KEY, name VARCHAR(100), num_value DOUBLE PRECISION)");
            dbConfig.setOption("query__insert_data",
                               "INSERT INTO test_table (id, name, num_value) VALUES (?, ?, ?)");
            dbConfig.setOption("query__select_data",
                               "SELECT * FROM test_table WHERE id = ?");
            dbConfig.setOption("query__drop_table",
                               "DROP TABLE test_table");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);
        dbConfig.setType("firebird");
        dbConfig.setHost("localhost");
        dbConfig.setPort(3050);
        dbConfig.setDatabase("/tmp/test_firebird.fdb");
        dbConfig.setUsername("SYSDBA");
        dbConfig.setPassword("dsystems");

        // Add default queries as options
        dbConfig.setOption("query__connection_test",
                           "SELECT 1 AS TEST_VALUE FROM RDB$DATABASE");
        // Use RECREATE TABLE - Firebird's equivalent of DROP TABLE IF EXISTS + CREATE TABLE
        dbConfig.setOption("query__create_table",
                           "RECREATE TABLE test_table (id INTEGER NOT NULL PRIMARY KEY, name VARCHAR(100), num_value DOUBLE PRECISION)");
        dbConfig.setOption("query__insert_data",
                           "INSERT INTO test_table (id, name, num_value) VALUES (?, ?, ?)");
        dbConfig.setOption("query__select_data",
                           "SELECT * FROM test_table WHERE id = ?");
        dbConfig.setOption("query__drop_table",
                           "DROP TABLE test_table");
#endif

        return dbConfig;
    }

    bool tryCreateDatabase()
    {
        try
        {
            // Get configuration
            auto dbConfig = getFirebirdConfig("dev_firebird");

            // Get connection parameters
            std::string type = dbConfig.getType();
            std::string host = dbConfig.getHost();
            int port = dbConfig.getPort();
            std::string database = dbConfig.getDatabase();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Register the Firebird driver
            cpp_dbc::DriverManager::registerDriver("firebird", std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

            // Build connection string for cpp_dbc
            std::string connStr = "cpp_dbc:" + type + "://" + host + ":" + std::to_string(port) + "/" + database;

            // First, try to connect to see if database already exists
            try
            {
                auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));
                std::cout << "Firebird database exists and connection successful!" << std::endl;
                conn->close();
                return true;
            }
            catch (const std::exception &)
            {
                // Database doesn't exist, try to create it
                std::cout << "Database doesn't exist, attempting to create it..." << std::endl;
            }

            // Build the Firebird connection string for CREATE DATABASE
            // Format: host/port:database_path or just database_path for local
            std::string fbConnStr;
            if (!host.empty() && host != "localhost" && host != "127.0.0.1")
            {
                fbConnStr = host;
                if (port != 3050 && port != 0)
                {
                    fbConnStr += "/" + std::to_string(port);
                }
                fbConnStr += ":";
            }
            fbConnStr += database;

            // Build CREATE DATABASE SQL command
            // Format: CREATE DATABASE 'path' USER 'username' PASSWORD 'password'
            std::string createDbSql = "CREATE DATABASE '" + fbConnStr + "' "
                                                                        "USER '" +
                                      username + "' "
                                                 "PASSWORD '" +
                                      password + "' "
                                                 "PAGE_SIZE 4096 "
                                                 "DEFAULT CHARACTER SET UTF8";

            std::cout << "Executing: " << createDbSql << std::endl;

            ISC_STATUS_ARRAY status;
            isc_db_handle db = 0;
            isc_tr_handle tr = 0;

            // Execute CREATE DATABASE using isc_dsql_execute_immediate
            // Note: For CREATE DATABASE, we pass null handles and the SQL creates the database
            if (isc_dsql_execute_immediate(status, &db, &tr, 0, createDbSql.c_str(), SQL_DIALECT_V6, nullptr))
            {
                std::string errorMsg = cpp_dbc::Firebird::interpretStatusVector(status);
                std::cerr << "Failed to create database: " << errorMsg << std::endl;
                std::cerr << std::endl;
                std::cerr << "To fix this, you may need to:" << std::endl;
                std::cerr << "1. Ensure the directory exists and is writable by the Firebird server" << std::endl;
                std::cerr << "2. Configure Firebird to allow database creation in the target directory" << std::endl;
                std::cerr << "   Edit /etc/firebird/3.0/firebird.conf (or similar path)" << std::endl;
                std::cerr << "   Set: DatabaseAccess = Full" << std::endl;
                std::cerr << "3. Restart Firebird: sudo systemctl restart firebird3.0" << std::endl;
                std::cerr << std::endl;
                std::cerr << "Alternatively, create the database manually:" << std::endl;
                std::cerr << "   isql-fb -user " << username << " -password " << password << std::endl;
                std::cerr << "   SQL> CREATE DATABASE '" << database << "';" << std::endl;
                std::cerr << "   SQL> quit;" << std::endl;
                return false;
            }

            std::cout << "Firebird database created successfully!" << std::endl;

            // Detach from the newly created database
            if (db)
            {
                isc_detach_database(status, &db);
            }

            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Database creation error: " << e.what() << std::endl;
            return false;
        }
    }

    bool canConnectToFirebird()
    {
        try
        {
            // First, try to create the database if it doesn't exist
            if (!firebird_test_helpers::tryCreateDatabase())
            {
                std::cerr << "Failed to create database, but continuing with connection test..." << std::endl;
            }

            // Get database configuration
            auto dbConfig = getFirebirdConfig("dev_firebird");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Register the Firebird driver
            cpp_dbc::DriverManager::registerDriver("firebird", std::make_shared<cpp_dbc::Firebird::FirebirdDBDriver>());

            // Attempt to connect to Firebird
            std::cout << "Attempting to connect to Firebird with connection string: " << connStr << std::endl;
            std::cout << "Username: " << username << ", Password: " << password << std::endl;

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            // If we get here, the connection was successful
            std::cout << "Firebird connection successful!" << std::endl;

            // Execute a simple query to verify the connection
            // Get the connection_test query from config
            // Note: Firebird ignores aliases for constant expressions and returns "CONSTANT" as column name
            // So we use column index (0) instead of column name
            std::string connectionTestQuery = dbConfig.getOption("query__connection_test", "SELECT 1 FROM RDB$DATABASE");
            auto resultSet = conn->executeQuery(connectionTestQuery);
            bool success = resultSet->next() && resultSet->getInt(0) == 1;

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Firebird connection error: " << e.what() << std::endl;
            return false;
        }
    }

#endif

} // namespace firebird_test_helpers