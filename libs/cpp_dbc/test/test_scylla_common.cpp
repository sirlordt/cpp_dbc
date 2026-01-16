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
 * @file test_scylla_common.cpp
 * @brief Implementation of Scylla test helpers
 */

#include "test_scylla_common.hpp"

namespace scylla_test_helpers
{

#if USE_SCYLLA

    cpp_dbc::config::DatabaseConfig getScyllaConfig(const std::string &databaseName)
    {
        cpp_dbc::config::DatabaseConfig dbConfig;

        // Defaults
        dbConfig.setType("scylladb");
        dbConfig.setHost("localhost");
        dbConfig.setPort(9042);
        dbConfig.setDatabase("test_keyspace");
        dbConfig.setUsername("cassandra");
        dbConfig.setPassword("dsystems");

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
            dbConfig.setOption("query__create_keyspace",
                               testQueries.getQuery("scylladb", "create_keyspace",
                                                    "CREATE KEYSPACE IF NOT EXISTS test_keyspace WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}"));

            dbConfig.setOption("query__create_table",
                               testQueries.getQuery("scylladb", "create_table",
                                                    "CREATE TABLE IF NOT EXISTS test_keyspace.test_table (id int PRIMARY KEY, name text, value double)"));

            dbConfig.setOption("query__insert_data",
                               testQueries.getQuery("scylladb", "insert_data",
                                                    "INSERT INTO test_keyspace.test_table (id, name, value) VALUES (?, ?, ?)"));

            dbConfig.setOption("query__select_data",
                               testQueries.getQuery("scylladb", "select_data",
                                                    "SELECT * FROM test_keyspace.test_table WHERE id = ?"));

            dbConfig.setOption("query__drop_table",
                               testQueries.getQuery("scylladb", "drop_table",
                                                    "DROP TABLE IF EXISTS test_keyspace.test_table"));
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);

            // Set test queries as default options
            dbConfig.setOption("query__create_keyspace",
                               "CREATE KEYSPACE IF NOT EXISTS test_keyspace WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
            dbConfig.setOption("query__create_table",
                               "CREATE TABLE IF NOT EXISTS test_keyspace.test_table (id int PRIMARY KEY, name text, value double)");
            dbConfig.setOption("query__insert_data",
                               "INSERT INTO test_keyspace.test_table (id, name, value) VALUES (?, ?, ?)");
            dbConfig.setOption("query__select_data",
                               "SELECT * FROM test_keyspace.test_table WHERE id = ?");
            dbConfig.setOption("query__drop_table",
                               "DROP TABLE IF EXISTS test_keyspace.test_table");
        }
#else
        // Hardcoded values when YAML is not available
        dbConfig.setName(databaseName);

        // Set test queries as default options
        dbConfig.setOption("query__create_keyspace",
                           "CREATE KEYSPACE IF NOT EXISTS test_keyspace WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");
        dbConfig.setOption("query__create_table",
                           "CREATE TABLE IF NOT EXISTS test_keyspace.test_table (id int PRIMARY KEY, name text, value double)");
        dbConfig.setOption("query__insert_data",
                           "INSERT INTO test_keyspace.test_table (id, name, value) VALUES (?, ?, ?)");
        dbConfig.setOption("query__select_data",
                           "SELECT * FROM test_keyspace.test_table WHERE id = ?");
        dbConfig.setOption("query__drop_table",
                           "DROP TABLE IF EXISTS test_keyspace.test_table");
#endif

        return dbConfig;
    }

    bool tryCreateKeyspace()
    {
        try
        {
            auto dbConfig = getScyllaConfig("dev_scylla");
            std::string connStr = "cpp_dbc:scylladb://" + dbConfig.getHost() + ":" + std::to_string(dbConfig.getPort()); // Connect without keyspace first
            std::string keyspace = dbConfig.getDatabase();

            // Get the CREATE KEYSPACE query
            std::string createKeyspaceQuery = dbConfig.getOption("query__create_keyspace",
                                                                 "CREATE KEYSPACE IF NOT EXISTS " + keyspace + " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");

            // Register the ScyllaDB driver
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Scylla::ScyllaDBDriver>());

            // Attempt to connect to ScyllaDB
            std::cout << "Attempting to connect to ScyllaDB to create keyspace..." << std::endl;
            auto conn = cpp_dbc::DriverManager::getDBConnection(connStr, dbConfig.getUsername(), dbConfig.getPassword());

            // Execute the create keyspace query
            std::cout << "Executing: " << createKeyspaceQuery << std::endl;
            auto columnarConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(conn);
            columnarConn->executeUpdate(createKeyspaceQuery);
            std::cout << "Keyspace creation successful or keyspace already exists!" << std::endl;

            // Close the connection
            conn->close();
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Keyspace creation error: " << e.what() << std::endl;
            return false;
        }
    }

    bool canConnectToScylla()
    {
        try
        {
            // First, try to create the keyspace if it doesn't exist
            if (!tryCreateKeyspace())
            {
                std::cerr << "Failed to create keyspace, but continuing with connection test..." << std::endl;
            }

            auto dbConfig = getScyllaConfig("dev_scylla");
            std::string host = dbConfig.getHost();
            int port = dbConfig.getPort();
            std::string keyspace = dbConfig.getDatabase();
            std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;

            // Register the ScyllaDB driver
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::Scylla::ScyllaDBDriver>());

            // Attempt to connect to ScyllaDB
            std::cout << "Attempting to connect to ScyllaDB with connection string: " << connStr << std::endl;
            std::cout << "Username: " << dbConfig.getUsername() << ", Password: " << dbConfig.getPassword() << std::endl;

            auto conn = cpp_dbc::DriverManager::getDBConnection(connStr, dbConfig.getUsername(), dbConfig.getPassword());

            // If we get here, the connection was successful
            if (conn != nullptr)
            {
                std::cout << "ScyllaDB connection successful!" << std::endl;

                // Close the connection
                conn->close();
                return true;
            }
            return false;
        }
        catch (const std::exception &e)
        {
            std::cout << "Cannot connect to ScyllaDB: " << e.what() << std::endl;
            return false;
        }
    }

#endif

} // namespace scylla_test_helpers