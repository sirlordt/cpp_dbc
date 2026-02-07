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

 @file 22_001_test_sqlite_real_common.cpp
 @brief Implementation of SQLite test helpers

*/

#include "22_001_test_sqlite_real_common.hpp"

namespace sqlite_test_helpers
{

#if USE_SQLITE

    cpp_dbc::config::DatabaseConfig getSQLiteConfig(const std::string &databaseName, bool useInMemory)
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

            // If useInMemory is true, override the database to in-memory
            if (useInMemory)
            {
                dbConfig.setDatabase(":memory:");
            }

            // Get test queries from YAML config if available
            const cpp_dbc::config::TestQueries &testQueries = configManager.getTestQueries();

            // Add queries as options to DatabaseConfig
            dbConfig.setOption("query__create_table",
                               testQueries.getQuery("sqlite", "create_table",
                                                    "CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT, value REAL)"));

            dbConfig.setOption("query__insert_data",
                               testQueries.getQuery("sqlite", "insert_data",
                                                    "INSERT INTO test_table (id, name, value) VALUES (?, ?, ?)"));

            dbConfig.setOption("query__select_data",
                               testQueries.getQuery("sqlite", "select_data",
                                                    "SELECT * FROM test_table WHERE id = ?"));

            dbConfig.setOption("query__drop_table",
                               testQueries.getQuery("sqlite", "drop_table",
                                                    "DROP TABLE IF EXISTS test_table"));
        }
        else
        {
            // Fallback to default values if configuration not found
            dbConfig.setName(databaseName);
            dbConfig.setType("sqlite");
            dbConfig.setDatabase(useInMemory ? ":memory:" : "sqlite_test.db");

            // Add default queries as options
            dbConfig.setOption("query__create_table",
                               "CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT, value REAL)");
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
        dbConfig.setType("sqlite");
        dbConfig.setDatabase(useInMemory ? ":memory:" : "sqlite_test.db");

        // Add default queries as options
        dbConfig.setOption("query__create_table",
                           "CREATE TABLE IF NOT EXISTS test_table (id INTEGER PRIMARY KEY, name TEXT, value REAL)");
        dbConfig.setOption("query__insert_data",
                           "INSERT INTO test_table (id, name, value) VALUES (?, ?, ?)");
        dbConfig.setOption("query__select_data",
                           "SELECT * FROM test_table WHERE id = ?");
        dbConfig.setOption("query__drop_table",
                           "DROP TABLE IF EXISTS test_table");
#endif

        return dbConfig;
    }

    bool canConnectToSQLite()
    {
        try
        {
            // Get database configuration
            auto dbConfig = getSQLiteConfig("dev_sqlite");

            // Get connection parameters
            std::string connStr = dbConfig.createConnectionString();

            // Register the SQLite driver
            cpp_dbc::DriverManager::registerDriver(std::make_shared<cpp_dbc::SQLite::SQLiteDBDriver>());

            // Attempt to connect to SQLite
            std::cout << "Attempting to connect to SQLite with connection string: " << connStr << std::endl;

            auto conn = std::dynamic_pointer_cast<cpp_dbc::RelationalDBConnection>(cpp_dbc::DriverManager::getDBConnection(connStr, "", ""));

            // If we get here, the connection was successful
            std::cout << "SQLite connection successful!" << std::endl;

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

} // namespace sqlite_test_helpers