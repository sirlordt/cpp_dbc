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
 * @file 26_001_test_scylladb_real_common.cpp
 * @brief Implementation of Scylla test helpers
 */

#include "26_001_test_scylladb_real_common.hpp"

#include <cpp_dbc/common/system_utils.hpp>

namespace scylla_test_helpers
{

#if USE_SCYLLADB

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

    std::shared_ptr<cpp_dbc::ScyllaDB::ScyllaDBDriver> getScyllaDriver()
    {
        static std::shared_ptr<cpp_dbc::ScyllaDB::ScyllaDBDriver> driver =
            cpp_dbc::ScyllaDB::ScyllaDBDriver::getInstance(std::nothrow).value_or(nullptr);
        return driver;
    }

    std::shared_ptr<cpp_dbc::ColumnarDBConnection> getScyllaConnection()
    {
        auto dbConfig = getScyllaConfig("dev_scylla");

        std::string host = dbConfig.getHost();
        int port = dbConfig.getPort();
        std::string keyspace = dbConfig.getDatabase();
        std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;
        std::string username = dbConfig.getUsername();
        std::string password = dbConfig.getPassword();

        auto driver = getScyllaDriver();
        cpp_dbc::DriverManager::registerDriver(driver);
        auto conn = cpp_dbc::DriverManager::getDBConnection(connStr, username, password);

        return std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(conn);
    }

    bool tryCreateKeyspace()
    {
        try
        {
            auto dbConfig = getScyllaConfig("dev_scylla");
            std::string host = dbConfig.getHost();
            int port = dbConfig.getPort();
            std::string keyspace = dbConfig.getDatabase();

            // Connect without keyspace first
            std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port);

            // Get the CREATE KEYSPACE query
            std::string createKeyspaceQuery = dbConfig.getOption("query__create_keyspace",
                                                                 "CREATE KEYSPACE IF NOT EXISTS " + keyspace + " WITH replication = {'class': 'SimpleStrategy', 'replication_factor': 1}");

            // Register the ScyllaDB driver singleton
            auto driver = getScyllaDriver();
            cpp_dbc::DriverManager::registerDriver(driver);

            // Attempt to connect to ScyllaDB
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to ScyllaDB to create keyspace...");
            auto conn = cpp_dbc::DriverManager::getDBConnection(connStr, dbConfig.getUsername(), dbConfig.getPassword());

            // Execute the create keyspace query
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Executing: " + createKeyspaceQuery);
            auto columnarConn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(conn);
            columnarConn->executeUpdate(createKeyspaceQuery);
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Keyspace creation successful or keyspace already exists!");

            // Close the connection
            conn->close();
            return true;
        }
        catch (const std::exception &ex)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Keyspace creation error: " + std::string(ex.what()));
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
                cpp_dbc::system_utils::logWithTimesMillis("TEST", "Failed to create keyspace, but continuing with connection test...");
            }

            auto dbConfig = getScyllaConfig("dev_scylla");
            std::string host = dbConfig.getHost();
            int port = dbConfig.getPort();
            std::string keyspace = dbConfig.getDatabase();
            std::string connStr = "cpp_dbc:scylladb://" + host + ":" + std::to_string(port) + "/" + keyspace;
            std::string username = dbConfig.getUsername();
            std::string password = dbConfig.getPassword();

            // Register the ScyllaDB driver singleton
            auto driver = getScyllaDriver();
            cpp_dbc::DriverManager::registerDriver(driver);

            // Attempt to connect to ScyllaDB via DriverManager (consistent with all other drivers)
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Attempting to connect to ScyllaDB with connection string: " + connStr);
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Username: " + username + ", Password: " + password);

            auto conn = std::dynamic_pointer_cast<cpp_dbc::ColumnarDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, username, password));

            if (!conn)
            {
                cpp_dbc::system_utils::logWithTimesMillis("TEST", "ScyllaDB connection returned null");
                return false;
            }

            // If we get here, the connection was successful
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "ScyllaDB connection successful!");

            // Verify the connection with ping()
            bool success = conn->ping();
            cpp_dbc::system_utils::logWithTimesMillis("TEST", std::string("ScyllaDB ping ") + (success ? "successful!" : "returned false"));

            // Close the connection
            conn->close();

            return success;
        }
        catch (const std::exception &ex)
        {
            cpp_dbc::system_utils::logWithTimesMillis("TEST", "Cannot connect to ScyllaDB: " + std::string(ex.what()));
            return false;
        }
    }

#endif

} // namespace scylla_test_helpers
